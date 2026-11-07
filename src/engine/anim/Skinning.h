#pragma once

/**
 * Skinning: quanto ogni osso muove ogni vertice di una mesh.
 *
 *   automaticWeights  pesi calcolati dalla distanza dal segmento dell'osso
 *                     (testa-coda), le N ossa piu' vicine, normalizzati.
 *                     Non e' il "heat diffusion" di Blender, ma per mesh
 *                     stile Roblox (blocchi, arti separati) da' gli stessi
 *                     confini; si ritocca col pennello.
 *   paint             Weight Paint: Add, Subtract, Smooth, Blur con raggio e
 *                     forza, su un osso alla volta, come in Blender.
 *   weightColor       blu = 0, verde = 0.5, rosso = 1 (scala di Blender)
 *   skinPositions     deformazione su CPU (anteprima, test, picking)
 *   packTop4          le 4 influenze piu' forti per lo shader GPU, che ha
 *                     4 slot per vertice (uFinalBones in SkinnedModel.h)
 *
 * Si tiene fino a 8 influenze per vertice nei dati; la GPU ne usa 4.
 */

#include <algorithm>
#include <cmath>
#include <vector>

#include "engine/anim/Armature.h"

namespace blk {
namespace anim {

inline constexpr int kMaxInfluences = 8;

struct VertexWeights {
    int bone[kMaxInfluences];
    float weight[kMaxInfluences];
    int count = 0;

    VertexWeights() {
        for (int i = 0; i < kMaxInfluences; ++i) { bone[i] = -1; weight[i] = 0.f; }
    }

    float of(int b) const {
        for (int i = 0; i < count; ++i)
            if (bone[i] == b) return weight[i];
        return 0.f;
    }

    void normalize() {
        float s = 0.f;
        for (int i = 0; i < count; ++i) s += weight[i];
        if (s <= 1e-8f) return;
        for (int i = 0; i < count; ++i) weight[i] /= s;
    }

    /** Tiene le `limit` piu' forti, toglie gli zeri, rinormalizza. */
    void prune(int limit) {
        limit = glm::clamp(limit, 1, kMaxInfluences);
        // ordinamento per peso decrescente (pochi elementi: insertion sort)
        for (int i = 1; i < count; ++i)
            for (int j = i; j > 0 && weight[j] > weight[j - 1]; --j) {
                std::swap(weight[j], weight[j - 1]);
                std::swap(bone[j], bone[j - 1]);
            }
        while (count > 0 && weight[count - 1] <= 1e-5f) --count;
        if (count > limit) count = limit;
        for (int i = count; i < kMaxInfluences; ++i) { bone[i] = -1; weight[i] = 0.f; }
        normalize();
    }

    /** Imposta il peso di un osso; se non c'e' posto prende il posto del piu' debole. */
    void set(int b, float w) {
        for (int i = 0; i < count; ++i)
            if (bone[i] == b) { weight[i] = w; return; }
        if (count < kMaxInfluences) { bone[count] = b; weight[count] = w; ++count; return; }
        int weakest = 0;
        for (int i = 1; i < count; ++i) if (weight[i] < weight[weakest]) weakest = i;
        if (w > weight[weakest]) { bone[weakest] = b; weight[weakest] = w; }
    }
};

inline float distToSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b) {
    const glm::vec3 ab = b - a;
    const float l2 = glm::dot(ab, ab);
    const float t = l2 > 1e-10f ? glm::clamp(glm::dot(p - a, ab) / l2, 0.f, 1.f) : 0.f;
    return glm::length(p - (a + ab * t));
}

/**
 * Pesi automatici. maxInfluences: 1, 2, 4 o 8. falloff alto = confini netti
 * (arti a blocchi), basso = sfumati (mesh organiche).
 */
inline std::vector<VertexWeights> automaticWeights(const Armature& arm, const std::vector<glm::vec3>& positions,
                                                   int maxInfluences = 4, float falloff = 4.f) {
    std::vector<Transform> g;
    arm.restGlobals(g);
    std::vector<VertexWeights> out(positions.size());
    for (size_t v = 0; v < positions.size(); ++v) {
        VertexWeights w;
        for (size_t b = 0; b < arm.bones.size(); ++b) {
            if (!arm.bones[b].deform) continue;
            const float d = distToSegment(positions[v], g[b].pos, arm.tail((int)b, g[b]));
            w.set((int)b, 1.f / std::pow(std::max(d, 1e-3f), falloff));
        }
        w.prune(maxInfluences);
        out[v] = w;
    }
    return out;
}

/** Indice d'osso piu' alto usato (per dimensionare le medie del pennello). */
inline int maxBoneIndex(const std::vector<VertexWeights>& ws) {
    int m = 0;
    for (const auto& w : ws)
        for (int i = 0; i < w.count; ++i) m = std::max(m, w.bone[i]);
    return m;
}

enum class BrushMode { Add, Subtract, Smooth, Blur };

struct Brush {
    BrushMode mode = BrushMode::Add;
    float radius = 1.f;
    float strength = 0.5f;
    int maxInfluences = 4;
};

/**
 * Una pennellata in `center` sull'osso `bone`. Influenza che cala col
 * quadrato della distanza fino al raggio. Smooth = verso la media dei
 * vicini dentro il raggio; Blur = come Smooth ma su tutte le ossa del
 * vertice, non solo quella attiva. Ritorna quanti vertici ha toccato.
 */
inline int paint(std::vector<VertexWeights>& weights, const std::vector<glm::vec3>& positions, int bone,
                 const glm::vec3& center, const Brush& brush) {
    if (weights.size() != positions.size() || brush.radius <= 0.f) return 0;
    std::vector<size_t> inside;
    for (size_t v = 0; v < positions.size(); ++v)
        if (glm::length(positions[v] - center) <= brush.radius) inside.push_back(v);
    if (inside.empty()) return 0;

    auto falloffAt = [&](size_t v) {
        const float t = glm::length(positions[v] - center) / brush.radius;
        return (1.f - t * t) * glm::clamp(brush.strength, 0.f, 1.f);
    };

    if (brush.mode == BrushMode::Add || brush.mode == BrushMode::Subtract) {
        for (size_t v : inside) {
            VertexWeights& w = weights[v];
            const float k = falloffAt(v);
            const float cur = w.of(bone);
            const float next = brush.mode == BrushMode::Add ? cur + (1.f - cur) * k : cur * (1.f - k);
            // Le altre ossa cedono (o riprendono) in proporzione: il totale resta 1.
            const float others = 1.f - cur;
            const float othersNext = 1.f - next;
            for (int i = 0; i < w.count; ++i)
                if (w.bone[i] != bone) w.weight[i] = others > 1e-6f ? w.weight[i] * othersNext / others : 0.f;
            w.set(bone, next);
            if (others <= 1e-6f && othersNext > 1e-6f) {
                // Era tutto di quest'osso: il resto va all'osso piu' vicino gia' presente, se c'e'.
                for (int i = 0; i < w.count; ++i)
                    if (w.bone[i] != bone) { w.weight[i] = othersNext; break; }
            }
            w.prune(brush.maxInfluences);
        }
        return (int)inside.size();
    }

    // Smooth / Blur: media dei pesi dei vertici nel raggio.
    // anche l osso attivo deve starci, pure se nessun vertice lo usa ancora
    std::vector<float> avg((size_t)std::max(maxBoneIndex(weights), bone) + 1, 0.f);
    for (size_t v : inside)
        for (int i = 0; i < weights[v].count; ++i) avg[(size_t)weights[v].bone[i]] += weights[v].weight[i];
    for (float& a : avg) a /= (float)inside.size();
    for (size_t v : inside) {
        VertexWeights& w = weights[v];
        const float k = falloffAt(v);
        if (brush.mode == BrushMode::Smooth) {
            w.set(bone, glm::mix(w.of(bone), avg[(size_t)std::max(0, bone)], k));
        } else {
            for (size_t b = 0; b < avg.size(); ++b)
                if (avg[b] > 1e-6f || w.of((int)b) > 0.f) w.set((int)b, glm::mix(w.of((int)b), avg[b], k));
        }
        w.prune(brush.maxInfluences);
    }
    return (int)inside.size();
}


/** Colore del Weight Paint: blu (0) -> ciano -> verde (0.5) -> giallo -> rosso (1). */
inline glm::vec3 weightColor(float w) {
    w = glm::clamp(w, 0.f, 1.f);
    if (w < 0.25f) return glm::mix(glm::vec3(0, 0, 1), glm::vec3(0, 1, 1), w / 0.25f);
    if (w < 0.5f) return glm::mix(glm::vec3(0, 1, 1), glm::vec3(0, 1, 0), (w - 0.25f) / 0.25f);
    if (w < 0.75f) return glm::mix(glm::vec3(0, 1, 0), glm::vec3(1, 1, 0), (w - 0.5f) / 0.25f);
    return glm::mix(glm::vec3(1, 1, 0), glm::vec3(1, 0, 0), (w - 0.75f) / 0.25f);
}

/** Matrici di skinning: globale(posa) * inversa(globale a riposo), per osso. */
inline std::vector<glm::mat4> skinMatrices(const Armature& arm, const Pose& pose) {
    std::vector<Transform> rest, posed;
    arm.restGlobals(rest);
    arm.poseGlobals(pose, posed);
    std::vector<glm::mat4> m(arm.bones.size());
    for (size_t i = 0; i < m.size(); ++i) m[i] = posed[i].matrix() * glm::inverse(rest[i].matrix());
    return m;
}

inline std::vector<glm::vec3> skinPositions(const std::vector<glm::vec3>& positions, const std::vector<VertexWeights>& weights,
                                            const std::vector<glm::mat4>& mats) {
    std::vector<glm::vec3> out(positions.size());
    for (size_t v = 0; v < positions.size(); ++v) {
        if (v >= weights.size() || weights[v].count == 0) { out[v] = positions[v]; continue; }
        glm::vec4 acc(0.f);
        for (int i = 0; i < weights[v].count; ++i) {
            const int b = weights[v].bone[i];
            if (b < 0 || b >= (int)mats.size()) continue;
            acc += weights[v].weight[i] * (mats[(size_t)b] * glm::vec4(positions[v], 1.f));
        }
        out[v] = glm::vec3(acc);
    }
    return out;
}

/** 4 slot per vertice per lo shader: le piu' forti, rinormalizzate. */
inline void packTop4(const VertexWeights& w, int outBone[4], float outWeight[4]) {
    VertexWeights c = w;
    c.prune(4);
    for (int i = 0; i < 4; ++i) {
        outBone[i] = i < c.count ? c.bone[i] : 0;
        outWeight[i] = i < c.count ? c.weight[i] : 0.f;
    }
}

} // namespace anim
} // namespace blk
