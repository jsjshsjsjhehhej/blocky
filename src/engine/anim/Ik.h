#pragma once

/**
 * Cinematica inversa: dato dove deve arrivare la mano (o il piede), trova come
 * ruotare le ossa della catena.
 *
 *   twoBone  — braccio/gamba: soluzione esatta col teorema del coseno, con
 *              pole target (dove punta il gomito/ginocchio) e twist.
 *   fabrik   — catene di qualsiasi lunghezza (code, colonne, tentacoli):
 *              FABRIK, iterativo, poi rotazioni osso per osso.
 *   lookAt   — un osso guarda un punto (testa, occhi), con asse scelto.
 *
 * Tutto lavora sulle posizioni globali e scrive SOLO la posa delle ossa
 * della catena (Armature::setGlobalRotation): il riposo non si tocca mai.
 */

#include <vector>

#include "engine/anim/Armature.h"

namespace blk {
namespace anim {
namespace ik {

/** Ruota l'osso i (nel mondo) cosi' che la direzione verso `from` diventi quella verso `to`. */
inline void aimBone(const Armature& arm, Pose& pose, std::vector<Transform>& g, int i,
                    const glm::vec3& curDir, const glm::vec3& wantDir) {
    if (glm::length(curDir) < 1e-6f || glm::length(wantDir) < 1e-6f) return;
    const glm::quat delta = rotationBetween(glm::normalize(curDir), glm::normalize(wantDir));
    arm.setGlobalRotation(pose, g, i, glm::normalize(delta * g[(size_t)i].rot));
}

/**
 * IK a due ossa: root (spalla/anca) -> mid (gomito/ginocchio) -> end (polso/
 * caviglia). Se il bersaglio e' troppo lontano la catena si stende verso di
 * lui. pole: punto verso cui si piega il giunto centrale (nullptr = mantiene
 * il piano attuale). twistDeg ruota tutta la catena attorno all'asse
 * spalla-bersaglio (Twist Correction).
 */
inline bool twoBone(const Armature& arm, Pose& pose, std::vector<Transform>& g, int root, int mid, int end,
                    const glm::vec3& target, const glm::vec3* pole = nullptr, float twistDeg = 0.f) {
    if (!arm.valid(root) || !arm.valid(mid) || (end >= 0 && !arm.valid(end))) return false;
    // end = -1: il punto finale e' la coda di mid (catena di due ossa senza un terzo osso).
    auto endPos = [&]() { return end >= 0 ? g[(size_t)end].pos : arm.tail(mid, g[(size_t)mid]); };
    const glm::vec3 a = g[(size_t)root].pos, b = g[(size_t)mid].pos, c = endPos();
    const float lab = glm::length(b - a), lbc = glm::length(c - b);
    if (lab < 1e-5f || lbc < 1e-5f) return false;

    glm::vec3 toT = target - a;
    float dist = glm::length(toT);
    if (dist < 1e-5f) return false;
    const glm::vec3 dirT = toT / dist;
    dist = glm::clamp(dist, std::fabs(lab - lbc) + 1e-4f, lab + lbc - 1e-4f);

    // Piano di piegatura: dal pole se c'e', altrimenti quello attuale.
    glm::vec3 bendRef = pole ? (*pole - a) : (b - a);
    glm::vec3 bendDir = bendRef - dirT * glm::dot(bendRef, dirT);
    if (glm::length(bendDir) < 1e-5f) {
        bendDir = glm::cross(dirT, glm::vec3(0.f, 0.f, 1.f));
        if (glm::length(bendDir) < 1e-5f) bendDir = glm::cross(dirT, glm::vec3(1.f, 0.f, 0.f));
    }
    bendDir = glm::normalize(bendDir);
    if (std::fabs(twistDeg) > 1e-4f) bendDir = glm::angleAxis(glm::radians(twistDeg), dirT) * bendDir;

    // Teorema del coseno: angolo alla spalla tra (spalla->bersaglio) e (spalla->gomito).
    const float cosA = glm::clamp((lab * lab + dist * dist - lbc * lbc) / (2.f * lab * dist), -1.f, 1.f);
    const float angA = std::acos(cosA);
    const glm::vec3 wantMid = a + (dirT * std::cos(angA) + bendDir * std::sin(angA)) * lab;

    aimBone(arm, pose, g, root, g[(size_t)mid].pos - g[(size_t)root].pos, wantMid - a);
    const glm::vec3 wantEnd = a + dirT * dist;
    aimBone(arm, pose, g, mid, endPos() - g[(size_t)mid].pos, wantEnd - g[(size_t)mid].pos);
    return true;
}

/**
 * Catena da `tip` risalendo di chainLength ossa (Chain Length di Blender:
 * 2 = l'osso e il padre). FABRIK sulle teste, piu' la coda dell'ultimo osso
 * che deve toccare il bersaglio.
 */
inline bool fabrik(const Armature& arm, Pose& pose, std::vector<Transform>& g, int tip, int chainLength,
                   const glm::vec3& target, int iterations = 16, float tolerance = 1e-3f) {
    if (!arm.valid(tip) || chainLength < 1) return false;
    std::vector<int> chain; // dalla radice della catena alla punta
    for (int b = tip; b >= 0 && (int)chain.size() < chainLength; b = arm.bones[(size_t)b].parent) chain.insert(chain.begin(), b);
    const size_t n = chain.size();

    std::vector<glm::vec3> p(n + 1);
    for (size_t i = 0; i < n; ++i) p[i] = g[(size_t)chain[i]].pos;
    p[n] = arm.tail(tip, g[(size_t)tip]);
    std::vector<float> len(n);
    float total = 0.f;
    for (size_t i = 0; i < n; ++i) { len[i] = glm::length(p[i + 1] - p[i]); total += len[i]; }
    if (total < 1e-5f) return false;

    const glm::vec3 base = p[0];
    if (glm::length(target - base) >= total) {
        const glm::vec3 d = glm::normalize(target - base);
        for (size_t i = 0; i < n; ++i) p[i + 1] = p[i] + d * len[i];
    } else {
        for (int it = 0; it < iterations && glm::length(p[n] - target) > tolerance; ++it) {
            p[n] = target;
            for (size_t i = n; i-- > 0;) p[i] = p[i + 1] + glm::normalize(p[i] - p[i + 1]) * len[i];
            p[0] = base;
            for (size_t i = 0; i < n; ++i) p[i + 1] = p[i] + glm::normalize(p[i + 1] - p[i]) * len[i];
        }
    }
    // Dalle posizioni alle rotazioni: ogni osso punta il suo +Y verso il giunto dopo.
    for (size_t i = 0; i < n; ++i) {
        const int b = chain[i];
        const glm::vec3 cur = arm.tail(b, g[(size_t)b]) - g[(size_t)b].pos;
        aimBone(arm, pose, g, b, cur, p[i + 1] - g[(size_t)b].pos);
    }
    return true;
}

/**
 * L'osso guarda `target` con il suo asse `axis` (locale: +Y come Blender,
 * oppure -Z per una testa stile Roblox). weight 0..1 = quanto (Head Tracking
 * morbido). maxAngleDeg limita quanto puo' girarsi dalla posa attuale.
 */
inline void lookAt(const Armature& arm, Pose& pose, std::vector<Transform>& g, int bone, const glm::vec3& target,
                   const glm::vec3& axis = glm::vec3(0.f, 1.f, 0.f), float weight = 1.f, float maxAngleDeg = 180.f) {
    if (!arm.valid(bone)) return;
    const Transform& t = g[(size_t)bone];
    const glm::vec3 cur = t.rot * glm::normalize(axis);
    glm::vec3 want = target - t.pos;
    if (glm::length(want) < 1e-5f) return;
    want = glm::normalize(want);
    glm::quat delta = rotationBetween(cur, want);
    const float ang = 2.f * std::acos(glm::clamp(std::fabs(delta.w), 0.f, 1.f));
    const float maxA = glm::radians(maxAngleDeg);
    float k = glm::clamp(weight, 0.f, 1.f);
    if (ang > maxA && ang > 1e-5f) k *= maxA / ang;
    delta = glm::slerp(glm::quat(1.f, 0.f, 0.f, 0.f), delta, k);
    arm.setGlobalRotation(pose, g, bone, glm::normalize(delta * t.rot));
}

} // namespace ik
} // namespace anim
} // namespace blk
