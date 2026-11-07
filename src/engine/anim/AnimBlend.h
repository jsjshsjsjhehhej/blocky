#pragma once

/**
 * Mescolare animazioni: dissolvenze, livelli, additive, blend tree.
 *
 *   blend(a, b, w)          posa a mescolata con b (Cross Fade, Idle + Run)
 *   addAdditive(base, add)  aggiunge una posa additiva (respiro, rinculo)
 *   Layer                   un'Action con peso e maschera di ossa: Run sulle
 *                           gambe e Aim sulle braccia, Walk + Tool
 *   BlendTree1D             piu' Action su un parametro (velocita': idle,
 *                           walk, run), con le fasi sincronizzate
 *
 * Le pose sono deformazioni sopra il riposo (Pose), quindi "additiva"
 * significa semplicemente: applicata sopra un'altra invece che al posto.
 */

#include <string>
#include <vector>

#include "engine/anim/AnimCurve.h"

namespace blk {
namespace anim {

inline Pose blend(const Pose& a, const Pose& b, float w) {
    Pose out;
    const size_t n = std::max(a.local.size(), b.local.size());
    out.local.resize(n);
    for (size_t i = 0; i < n; ++i) out.local[i] = lerp(a.get((int)i), b.get((int)i), glm::clamp(w, 0.f, 1.f));
    return out;
}

/** base + add*w: posizione sommata, rotazione composta, scala moltiplicata. */
inline Pose addAdditive(const Pose& base, const Pose& add, float w, const std::vector<char>* mask = nullptr) {
    Pose out = base;
    const size_t n = std::max(base.local.size(), add.local.size());
    out.local.resize(n);
    for (size_t i = 0; i < n; ++i) {
        if (mask && (i >= mask->size() || !(*mask)[i])) continue;
        const Transform a = lerp(Transform{}, add.get((int)i), glm::clamp(w, 0.f, 1.f));
        Transform& o = out.local[i];
        o.pos += a.pos;
        o.rot = glm::normalize(o.rot * a.rot);
        o.scale *= a.scale;
    }
    return out;
}

/** Maschera di ossa: l'osso `root` e tutti i suoi discendenti (es. "Waist" = busto in su). */
inline std::vector<char> boneMask(const Armature& arm, const std::vector<std::string>& roots) {
    std::vector<char> m(arm.bones.size(), 0);
    for (size_t i = 0; i < arm.bones.size(); ++i) {
        for (const auto& r : roots) {
            const int ri = arm.find(r);
            if (ri < 0) continue;
            if ((int)i == ri || arm.isAncestor(ri, (int)i)) { m[i] = 1; break; }
        }
    }
    return m;
}

struct Layer {
    const Action* action = nullptr;
    float weight = 1.f;
    float time = 0.f;          // secondi nella sua Action
    float speed = 1.f;
    bool additive = false;
    std::vector<char> mask;    // vuota = tutte le ossa
    bool mute = false;
};

/** Livelli dall'alto in basso: ognuno sovrascrive (o si somma) dove la maschera lo permette. */
inline Pose evaluateLayers(const Armature& arm, const std::vector<Layer>& layers) {
    Pose out;
    out.reset(arm.bones.size());
    for (const auto& l : layers) {
        if (!l.action || l.mute || l.weight <= 0.f) continue;
        const Pose p = l.action->sample(arm, l.action->frameAt(l.time));
        if (l.additive) {
            out = addAdditive(out, p, l.weight, l.mask.empty() ? nullptr : &l.mask);
            continue;
        }
        for (size_t i = 0; i < arm.bones.size(); ++i) {
            if (!l.mask.empty() && (i >= l.mask.size() || !l.mask[i])) continue;
            out.local[i] = lerp(out.local[i], p.get((int)i), glm::clamp(l.weight, 0.f, 1.f));
        }
    }
    return out;
}

/** Cross Fade da una posa che si congela a una che parte, in `duration` secondi. */
struct CrossFade {
    Pose from;
    float duration = 0.2f;
    float elapsed = 0.f;
    bool active = false;

    void start(const Pose& current, float seconds) {
        from = current;
        duration = std::max(0.f, seconds);
        elapsed = 0.f;
        active = duration > 0.f;
    }

    Pose apply(const Pose& to, float dt) {
        if (!active) return to;
        elapsed += dt;
        const float t = glm::clamp(elapsed / std::max(1e-4f, duration), 0.f, 1.f);
        // smoothstep: entra e esce morbida invece che a velocita' costante.
        const float s = t * t * (3.f - 2.f * t);
        if (t >= 1.f) active = false;
        return blend(from, to, s);
    }
};

/**
 * Blend tree 1D: Action messe su una linea (es. velocita' 0 idle, 8 walk,
 * 16 run). Al valore del parametro si mescolano le due piu' vicine, con il
 * tempo normalizzato condiviso: i piedi di walk e run toccano terra insieme.
 */
struct BlendTree1D {
    struct Entry { float threshold; const Action* action; };
    std::vector<Entry> entries; // in ordine di soglia
    float phase = 0.f;          // 0..1, avanza con la durata pesata

    Pose sample(const Armature& arm, float param, float dt) {
        Pose none;
        none.reset(arm.bones.size());
        if (entries.empty()) return none;
        size_t hi = 0;
        while (hi < entries.size() && entries[hi].threshold < param) ++hi;
        size_t lo = hi == 0 ? 0 : hi - 1;
        if (hi >= entries.size()) hi = lo = entries.size() - 1;
        float w = 0.f;
        if (hi != lo) w = glm::clamp((param - entries[lo].threshold) / std::max(1e-4f, entries[hi].threshold - entries[lo].threshold), 0.f, 1.f);
        const Action* a = entries[lo].action;
        const Action* b = entries[hi].action;
        if (!a || !b) return none;
        const float len = glm::mix(a->durationSeconds(), b->durationSeconds(), w);
        if (len > 1e-4f) phase = std::fmod(phase + dt / len, 1.f);
        auto at = [&](const Action* x) { return x->frameStart + phase * (x->frameEnd - x->frameStart); };
        return blend(a->sample(arm, at(a)), b->sample(arm, at(b)), w);
    }
};

} // namespace anim
} // namespace blk
