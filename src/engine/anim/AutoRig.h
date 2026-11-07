#pragma once

/**
 * Auto Rig: dalle parti di un personaggio fatto di Part (R6 o R15) genera lo
 * scheletro e il legame osso -> parte, come Auto Rig Pro fa da una mesh.
 *
 * Le parti si riconoscono dal nome, con i nomi di Roblox:
 *   R6:  Head, Torso, Left Arm, Right Arm, Left Leg, Right Leg
 *   R15: Head, UpperTorso, LowerTorso, Left/Right UpperArm, LowerArm, Hand,
 *        Left/Right UpperLeg, LowerLeg, Foot
 * Le ossa prendono i nomi dei giunti Motor6D di Roblox (Neck, Left Shoulder,
 * RightElbow...), cosi' le animazioni restano leggibili da chi viene da li'.
 *
 * Ogni osso nasce nel giunto (spalla in cima al braccio, anca in cima alla
 * gamba...) e punta verso la parte che muove, lungo come lei. Il legame
 * salva la parte rispetto all'osso a riposo: mondo(parte) = globale(osso) * offset.
 */

#include <string>
#include <vector>

#include "engine/anim/Armature.h"

namespace blk {
namespace anim {

/** Una parte del personaggio, nel mondo. rot = rotazione della parte. */
struct PartBox {
    std::string name;
    glm::vec3 pos{0.f};
    glm::vec3 size{1.f};
    glm::quat rot{1.f, 0.f, 0.f, 0.f};
};

struct PartBinding {
    int bone = -1;
    std::string part;
    Transform offset; // parte rispetto all'osso a riposo
};

enum class RigKind { None, R6, R15 };

struct AutoRigResult {
    RigKind kind = RigKind::None;
    Armature armature;
    std::vector<PartBinding> bindings;
    std::string error;
};

namespace detail {

inline const PartBox* findPart(const std::vector<PartBox>& parts, const char* name) {
    for (const auto& p : parts)
        if (p.name == name) return &p;
    return nullptr;
}

/** Punto della parte in coordinate relative (-1..1 per asse) -> mondo. */
inline glm::vec3 partPoint(const PartBox& p, const glm::vec3& rel) { return p.pos + p.rot * (rel * p.size * 0.5f); }

/**
 * Osso da `head` verso `toward`: il +Y dell'osso punta li', il resto
 * dell'orientamento segue la parte (cosi' la rotazione X dell'osso piega
 * davvero in avanti come la parte).
 */
inline int addJoint(Armature& arm, const std::string& name, int parent, const std::vector<Transform>& parentGlobals,
                    const glm::vec3& head, const glm::vec3& toward, const glm::quat& partRot) {
    const glm::vec3 dir = toward - head;
    const float len = std::max(0.05f, glm::length(dir));
    // Orientamento: la rotazione della parte, poi corretta perche' +Y punti lungo l'osso.
    const glm::vec3 yAxis = partRot * glm::vec3(0.f, 1.f, 0.f);
    const glm::quat globalRot = glm::normalize(rotationBetween(yAxis, glm::normalize(dir)) * partRot);
    Transform g;
    g.pos = head;
    g.rot = globalRot;
    const Transform parentG = parent >= 0 ? parentGlobals[(size_t)parent] : Transform{};
    const int idx = arm.addBone(name, parent, compose(inverse(parentG), g), len);
    (void)idx;
    return idx;
}

inline void bindPart(AutoRigResult& r, const std::vector<Transform>& g, int bone, const PartBox& p) {
    Transform world;
    world.pos = p.pos;
    world.rot = p.rot;
    r.bindings.push_back({bone, p.name, compose(inverse(g[(size_t)bone]), world)});
}

} // namespace detail

inline RigKind detectRig(const std::vector<PartBox>& parts) {
    using detail::findPart;
    if (findPart(parts, "UpperTorso") && findPart(parts, "LowerTorso") && findPart(parts, "Head")) return RigKind::R15;
    if (findPart(parts, "Torso") && findPart(parts, "Head")) return RigKind::R6;
    return RigKind::None;
}

inline AutoRigResult autoRig(const std::vector<PartBox>& parts) {
    using detail::addJoint;
    using detail::bindPart;
    using detail::findPart;
    using detail::partPoint;
    AutoRigResult r;
    r.kind = detectRig(parts);
    Armature& arm = r.armature;
    arm.name = r.kind == RigKind::R15 ? "R15 Rig" : "R6 Rig";
    std::vector<Transform> g;
    auto refresh = [&]() { arm.restGlobals(g); };

    if (r.kind == RigKind::None) {
        r.error = "Servono almeno Head e Torso (R6) oppure Head, UpperTorso e LowerTorso (R15)";
        return r;
    }

    if (r.kind == RigKind::R6) {
        const PartBox* torso = findPart(parts, "Torso");
        const PartBox* head = findPart(parts, "Head");
        // Root alla base del torso, Torso (= Spine/RootJoint) dal bacino al collo.
        const int root = addJoint(arm, "Root", -1, g, partPoint(*torso, {0, -1, 0}), partPoint(*torso, {0, -0.5f, 0}), torso->rot);
        refresh();
        const int spine = addJoint(arm, "Torso", root, g, partPoint(*torso, {0, -1, 0}), partPoint(*torso, {0, 1, 0}), torso->rot);
        refresh();
        bindPart(r, g, spine, *torso);
        const int neck = addJoint(arm, "Neck", spine, g, partPoint(*torso, {0, 1, 0}), partPoint(*head, {0, 1, 0}), head->rot);
        refresh();
        bindPart(r, g, neck, *head);
        struct Limb { const char* part; const char* joint; };
        const Limb limbs[] = {{"Left Arm", "Left Shoulder"}, {"Right Arm", "Right Shoulder"},
                              {"Left Leg", "Left Hip"}, {"Right Leg", "Right Hip"}};
        for (const auto& l : limbs) {
            const PartBox* p = findPart(parts, l.part);
            if (!p) continue;
            const int b = addJoint(arm, l.joint, spine, g, partPoint(*p, {0, 1, 0}), partPoint(*p, {0, -1, 0}), p->rot);
            refresh();
            bindPart(r, g, b, *p);
        }
        return r;
    }

    // R15
    const PartBox* lower = findPart(parts, "LowerTorso");
    const PartBox* upper = findPart(parts, "UpperTorso");
    const PartBox* head = findPart(parts, "Head");
    const int root = addJoint(arm, "Root", -1, g, partPoint(*lower, {0, -1, 0}), partPoint(*lower, {0, 0, 0}), lower->rot);
    refresh();
    bindPart(r, g, root, *lower);
    const int waist = addJoint(arm, "Waist", root, g, partPoint(*lower, {0, 1, 0}), partPoint(*upper, {0, 1, 0}), upper->rot);
    refresh();
    bindPart(r, g, waist, *upper);
    const int neck = addJoint(arm, "Neck", waist, g, partPoint(*upper, {0, 1, 0}), partPoint(*head, {0, 1, 0}), head->rot);
    refresh();
    bindPart(r, g, neck, *head);

    struct Seg { const char* part; const char* joint; };
    struct Chain { int parentBone; Seg segs[3]; };
    const Chain chains[] = {
        {waist, {{"LeftUpperArm", "LeftShoulder"}, {"LeftLowerArm", "LeftElbow"}, {"LeftHand", "LeftWrist"}}},
        {waist, {{"RightUpperArm", "RightShoulder"}, {"RightLowerArm", "RightElbow"}, {"RightHand", "RightWrist"}}},
        {root, {{"LeftUpperLeg", "LeftHip"}, {"LeftLowerLeg", "LeftKnee"}, {"LeftFoot", "LeftAnkle"}}},
        {root, {{"RightUpperLeg", "RightHip"}, {"RightLowerLeg", "RightKnee"}, {"RightFoot", "RightAnkle"}}},
    };
    for (const auto& ch : chains) {
        int parent = ch.parentBone;
        for (const auto& s : ch.segs) {
            const PartBox* p = findPart(parts, s.part);
            if (!p) break; // catena interrotta: niente ossa sotto
            const int b = addJoint(arm, s.joint, parent, g, partPoint(*p, {0, 1, 0}), partPoint(*p, {0, -1, 0}), p->rot);
            refresh();
            bindPart(r, g, b, *p);
            parent = b;
        }
    }
    return r;
}

/**
 * Le parti nel mondo per una posa: mondo(parte) = globale(osso) * offset.
 * Cosi' l'animazione muove le Part vere del personaggio (anche di qualsiasi
 * Model, come Moon Animator).
 */
inline std::vector<Transform> partWorlds(const Armature& arm, const Pose& pose, const std::vector<PartBinding>& binds) {
    std::vector<Transform> g;
    arm.poseGlobals(pose, g);
    std::vector<Transform> out;
    out.reserve(binds.size());
    for (const auto& b : binds) out.push_back(arm.valid(b.bone) ? compose(g[(size_t)b.bone], b.offset) : Transform{});
    return out;
}

} // namespace anim
} // namespace blk
