#pragma once

/**
 * Vincoli sulle ossa (Bone Constraints di Blender), valutati dopo la posa
 * dell'animazione, in ordine: ognuno parte dal risultato del precedente.
 *
 *   Copy Location / Rotation / Scale   copia dall'osso bersaglio (per asse)
 *   Look At / Track To                 l'osso guarda il bersaglio
 *   Child Of                           segue il bersaglio come se fosse il padre
 *   Limit Rotation / Scale / Location  tiene la posa dentro un intervallo
 *   IK                                 catena verso il bersaglio (2 ossa o FABRIK)
 *
 * Ogni vincolo ha un'influenza 0..1 (miscela con la posa di prima) e si puo'
 * spegnere. Bersaglio = un osso del rig oppure un punto nel mondo.
 */

#include <string>
#include <vector>

#include "engine/anim/Armature.h"
#include "engine/anim/Ik.h"

namespace blk {
namespace anim {

enum class ConstraintType : int {
    CopyLocation, CopyRotation, CopyScale, LookAt, ChildOf,
    LimitRotation, LimitScale, LimitLocation, Ik, Count
};

inline const char* constraintName(ConstraintType t) {
    static const char* n[] = {"Copy Location", "Copy Rotation", "Copy Scale", "Look At", "Child Of",
                              "Limit Rotation", "Limit Scale", "Limit Location", "IK"};
    return (int)t >= 0 && (int)t < (int)ConstraintType::Count ? n[(int)t] : "?";
}

struct BoneConstraint {
    ConstraintType type = ConstraintType::CopyLocation;
    std::string owner;             // osso che il vincolo muove
    std::string target;            // osso bersaglio ("" = targetPoint nel mondo)
    glm::vec3 targetPoint{0.f};
    float influence = 1.f;
    bool enabled = true;
    bool axis[3] = {true, true, true};  // Copy/Limit: X Y Z
    glm::vec3 offset{0.f};              // Copy Location: aggiunto dopo la copia
    glm::vec3 minV{-180.f}, maxV{180.f};// Limit: gradi (Rotation) o stud/fattori
    glm::vec3 lookAxis{0.f, 1.f, 0.f};  // Look At: asse locale che punta
    int chainLength = 2;                // IK: 2 = osso e padre (risolto esatto)
    std::string pole;                   // IK: osso pole target (facoltativo)
    float twist = 0.f;                  // IK: gradi
    Transform childOfInverse;           // Child Of: "Set Inverse" (relazione al momento del vincolo)
};

namespace detail {

inline bool targetTransform(const Armature& arm, const std::vector<Transform>& g, const BoneConstraint& c, Transform& out) {
    if (c.target.empty()) {
        out = Transform{};
        out.pos = c.targetPoint;
        return true;
    }
    const int t = arm.find(c.target);
    if (t < 0) return false;
    out = g[(size_t)t];
    return true;
}

inline glm::vec3 eulerDeg(const glm::quat& q) { return glm::degrees(glm::eulerAngles(q)); }
inline glm::quat fromEulerDeg(const glm::vec3& e) { return glm::quat(glm::radians(e)); }

} // namespace detail

/** Applica un vincolo. Ritorna falso se owner/target non esistono. */
inline bool applyConstraint(const Armature& arm, Pose& pose, std::vector<Transform>& g, const BoneConstraint& c) {
    if (!c.enabled || c.influence <= 0.f) return true;
    const int o = arm.find(c.owner);
    if (o < 0) return false;
    const float w = glm::clamp(c.influence, 0.f, 1.f);
    Transform tgt;
    const bool hasTarget = detail::targetTransform(arm, g, c, tgt);
    const Transform before = g[(size_t)o];
    const Transform poseBefore = pose.get(o);

    switch (c.type) {
    case ConstraintType::CopyLocation: {
        if (!hasTarget) return false;
        glm::vec3 p = before.pos;
        for (int k = 0; k < 3; ++k) if (c.axis[k]) p[k] = tgt.pos[k] + c.offset[k];
        arm.setGlobalPosition(pose, g, o, glm::mix(before.pos, p, w));
        break;
    }
    case ConstraintType::CopyRotation: {
        if (!hasTarget) return false;
        glm::vec3 e = detail::eulerDeg(before.rot);
        const glm::vec3 te = detail::eulerDeg(tgt.rot);
        for (int k = 0; k < 3; ++k) if (c.axis[k]) e[k] = te[k];
        const glm::quat want = (c.axis[0] && c.axis[1] && c.axis[2]) ? tgt.rot : detail::fromEulerDeg(e);
        arm.setGlobalRotation(pose, g, o, lerp(before, Transform{before.pos, want, before.scale}, w).rot);
        break;
    }
    case ConstraintType::CopyScale: {
        if (!hasTarget) return false;
        glm::vec3 s = poseBefore.scale;
        const glm::vec3 ratio = tgt.scale / glm::max(before.scale, glm::vec3(1e-6f));
        for (int k = 0; k < 3; ++k) if (c.axis[k]) s[k] = poseBefore.scale[k] * ratio[k];
        pose.local[(size_t)o].scale = glm::mix(poseBefore.scale, s, w);
        arm.updateSubtree(pose, o, g);
        break;
    }
    case ConstraintType::LookAt: {
        if (!hasTarget) return false;
        ik::lookAt(arm, pose, g, o, tgt.pos, c.lookAxis, w);
        break;
    }
    case ConstraintType::ChildOf: {
        if (!hasTarget) return false;
        // Mondo voluto = bersaglio * inversa salvata * posa attuale (relativa al bersaglio).
        const Transform want = compose(compose(tgt, c.childOfInverse), before);
        const Transform mixed = lerp(before, want, w);
        arm.setGlobalRotation(pose, g, o, mixed.rot);
        arm.setGlobalPosition(pose, g, o, mixed.pos);
        break;
    }
    case ConstraintType::LimitRotation: {
        // Limiti sulla rotazione della posa (spazio locale dell'osso), come Blender.
        glm::vec3 e = detail::eulerDeg(poseBefore.rot);
        glm::vec3 lim = e;
        for (int k = 0; k < 3; ++k) if (c.axis[k]) lim[k] = glm::clamp(e[k], c.minV[k], c.maxV[k]);
        pose.local[(size_t)o].rot = glm::normalize(glm::slerp(poseBefore.rot, detail::fromEulerDeg(lim), w));
        arm.updateSubtree(pose, o, g);
        break;
    }
    case ConstraintType::LimitScale: {
        glm::vec3 s = poseBefore.scale;
        for (int k = 0; k < 3; ++k) if (c.axis[k]) s[k] = glm::clamp(s[k], c.minV[k], c.maxV[k]);
        pose.local[(size_t)o].scale = glm::mix(poseBefore.scale, s, w);
        arm.updateSubtree(pose, o, g);
        break;
    }
    case ConstraintType::LimitLocation: {
        glm::vec3 p = before.pos;
        for (int k = 0; k < 3; ++k) if (c.axis[k]) p[k] = glm::clamp(p[k], c.minV[k], c.maxV[k]);
        arm.setGlobalPosition(pose, g, o, glm::mix(before.pos, p, w));
        break;
    }
    case ConstraintType::Ik: {
        if (!hasTarget) return false;
        Pose solved = pose;
        std::vector<Transform> sg = g;
        const int poleBone = c.pole.empty() ? -1 : arm.find(c.pole);
        const glm::vec3 polePos = poleBone >= 0 ? g[(size_t)poleBone].pos : glm::vec3(0.f);
        const int parent = arm.bones[(size_t)o].parent;
        bool ok;
        if (c.chainLength == 2 && parent >= 0) {
            // owner = l'ultimo osso (avambraccio): padre (braccio) -> owner -> coda
            // dell'owner sul bersaglio. Soluzione esatta, pole e twist compresi.
            ok = ik::twoBone(arm, solved, sg, parent, o, -1, tgt.pos, poleBone >= 0 ? &polePos : nullptr, c.twist);
        } else {
            ok = ik::fabrik(arm, solved, sg, o, std::max(1, c.chainLength), tgt.pos);
        }
        if (!ok) return false;
        for (size_t i = 0; i < pose.local.size() && i < solved.local.size(); ++i)
            pose.local[i] = lerp(pose.local[i], solved.local[i], w);
        arm.poseGlobals(pose, g);
        break;
    }
    default:
        break;
    }
    return true;
}

/** Tutti i vincoli in ordine; g esce aggiornato con la posa finale. */
inline int applyConstraints(const Armature& arm, Pose& pose, std::vector<Transform>& g,
                            const std::vector<BoneConstraint>& cs) {
    if (pose.local.size() < arm.bones.size()) pose.local.resize(arm.bones.size());
    int failed = 0;
    for (const auto& c : cs)
        if (!applyConstraint(arm, pose, g, c)) ++failed;
    return failed;
}

/** "Set Inverse" di Child Of: la relazione attuale owner/bersaglio resta com'e'. */
inline void childOfSetInverse(const Armature& arm, const std::vector<Transform>& g, BoneConstraint& c) {
    Transform tgt;
    if (detail::targetTransform(arm, g, c, tgt)) c.childOfInverse = inverse(tgt);
}

} // namespace anim
} // namespace blk
