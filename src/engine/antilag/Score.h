#pragma once

// Quanto tempo si stima di recuperare, e quanta immagine si perde.
// Non misura la GPU: dice se il piano e' coerente con la causa. Un taglio
// degli script su un collo GPU deve risultare quasi inutile. Un taglio
// della risoluzione sullo stesso collo deve valere dei millisecondi.

#include "Cause.h"

namespace antilag {

struct SceneLoad {
    float renderMs = 8.f;
    float scriptMs = 1.f;
    float physicsMs = 1.5f;
    float shadowMs = 1.2f;
    float postMs = 1.4f;
    float particleMs = 0.4f;
    float netMs = 0.3f;
    int parts = 2000;
};

struct Score {
    float renderMs = 0.f;
    float scriptMs = 0.f;
    float physicsMs = 0.f;
    float otherMs = 0.f;
    float totalMs = 0.f;
    float savedMs = 0.f;
    float quality = 1.f; // 1 = uguale alla qualita' base
    bool playable = true;
};

inline float pixelRatio(float scale) {
    float s = clampf(scale, 0.5f, 1.f);
    return s * s;
}

inline float qualityOf(const BaseQuality& base, const FramePlan& plan) {
    float picture = 0.f;
    float w = 0.f;
    auto acc = [&](float weight, float have, float full) {
        if (full < 1e-4f) return;
        float r = have / full;
        if (r < 0.f) r = 0.f;
        if (r > 1.f) r = 1.f;
        picture += weight * r;
        w += weight;
    };
    acc(0.34f, plan.renderScale, base.renderScale > 0.f ? base.renderScale : 1.f);
    acc(0.22f, (float)plan.maxVisibleParts, (float)std::max(1, base.maxVisibleParts));
    acc(0.14f, plan.maxDrawDistance, std::max(1.f, base.maxDrawDistance));
    acc(0.10f, plan.physicsHz, std::max(1.f, base.physicsHz));
    acc(0.08f, (float)plan.luaBudgetUs, (float)std::max(1, base.luaBudgetUs));
    acc(0.06f, (float)plan.maxParticles, (float)std::max(1, base.maxParticles));
    acc(0.04f, plan.studTextures ? 1.f : 0.f, 1.f);
    acc(0.02f, plan.postFx ? 1.f : 0.f, 1.f);
    if (w < 1e-4f) return 1.f;
    return picture / w;
}

inline Score scorePlan(const BaseQuality& base, const FramePlan& plan, const SceneLoad& load) {
    Score s;
    float partRatio = 1.f;
    if (load.parts > 0)
        partRatio = std::min(1.f, (float)plan.maxVisibleParts / (float)load.parts);
    float distRatio = 1.f;
    if (base.maxDrawDistance > 1.f)
        distRatio = clampf(plan.maxDrawDistance / base.maxDrawDistance, 0.15f, 1.f);

    s.renderMs = load.renderMs * pixelRatio(plan.renderScale) * (0.35f + 0.65f * partRatio) * (0.5f + 0.5f * distRatio);
    int stride = plan.heartbeatStride < 1 ? 1 : plan.heartbeatStride;
    float luaRatio = 1.f;
    if (base.luaBudgetUs > 0)
        luaRatio = std::min(1.f, (float)plan.luaBudgetUs / (float)base.luaBudgetUs);
    s.scriptMs = load.scriptMs * luaRatio / (float)stride;
    float hzRatio = 1.f;
    if (base.physicsHz > 1.f)
        hzRatio = clampf(plan.physicsHz / base.physicsHz, 0.2f, 1.f);
    s.physicsMs = load.physicsMs * hzRatio;

    float shadow = plan.shadowOccluders <= 0 ? 0.15f : std::min(1.f, (float)plan.shadowOccluders / 8.f);
    float post = plan.postFx ? 1.f : 0.2f;
    float particles = 1.f;
    if (base.maxParticles > 0)
        particles = std::min(1.f, (float)plan.maxParticles / (float)base.maxParticles);
    s.otherMs = load.shadowMs * shadow + load.postMs * post + load.particleMs * particles + load.netMs;

    s.totalMs = s.renderMs + s.scriptMs + s.physicsMs + s.otherMs;
    float baseTotal = load.renderMs + load.scriptMs + load.physicsMs
                    + load.shadowMs + load.postMs + load.particleMs + load.netMs;
    s.savedMs = baseTotal - s.totalMs;
    s.quality = qualityOf(base, plan);
    s.playable = plan.physicsHz >= 20.f && plan.renderScale >= 0.5f
              && plan.luaBudgetUs >= 200 && plan.maxVisibleParts >= 32;
    return s;
}

// Il piano "giusto" per una causa deve risparmiare di piu' sulla fase
// colpevole di quanto risparmia il piano pieno. Ritorna i ms stimati
// tagliati su quella fase (non sul totale).
inline float savedOnCause(const BaseQuality& base, const FramePlan& plan,
                          const SceneLoad& load, Cause cause) {
    FramePlan full = planFrom(base, 0, StepReason::None, 16.7f);
    Score a = scorePlan(base, full, load);
    Score b = scorePlan(base, plan, load);
    switch (cause) {
    case Cause::Gpu:     return (a.renderMs + a.otherMs) - (b.renderMs + b.otherMs);
    case Cause::Script:  return a.scriptMs - b.scriptMs;
    case Cause::Physics: return a.physicsMs - b.physicsMs;
    case Cause::Mixed:   return a.totalMs - b.totalMs;
    case Cause::Network: return load.netMs * 0.25f;
    case Cause::None:
    case Cause::OneShot: return 0.f;
    }
    return 0.f;
}

inline SceneLoad sceneBaseplate() {
    SceneLoad s;
    s.renderMs = 3.f;
    s.scriptMs = 0.3f;
    s.physicsMs = 0.8f;
    s.shadowMs = 0.4f;
    s.postMs = 0.6f;
    s.particleMs = 0.1f;
    s.netMs = 0.1f;
    s.parts = 40;
    return s;
}

inline SceneLoad sceneCity() {
    SceneLoad s;
    s.renderMs = 14.f;
    s.scriptMs = 1.2f;
    s.physicsMs = 2.4f;
    s.shadowMs = 2.0f;
    s.postMs = 1.6f;
    s.particleMs = 0.5f;
    s.netMs = 0.3f;
    s.parts = 6000;
    return s;
}

inline SceneLoad sceneScripts() {
    SceneLoad s;
    s.renderMs = 4.f;
    s.scriptMs = 12.f;
    s.physicsMs = 1.f;
    s.shadowMs = 0.4f;
    s.postMs = 0.8f;
    s.particleMs = 0.2f;
    s.netMs = 0.2f;
    s.parts = 300;
    return s;
}

// Il livello piu' basso che riporta il frame stimato sotto il target.
// Se nessuno ci riesce, resta 7: e' il massimo che il governor puo' fare.
inline int cheapestLevel(const BaseQuality& base, const SceneLoad& load, float targetMs) {
    if (targetMs < 8.f) targetMs = 8.f;
    for (int level = 0; level <= Governor::kMaxLevel; ++level) {
        FramePlan plan = planFrom(base, level, StepReason::None, targetMs);
        Score s = scorePlan(base, plan, load);
        if (s.playable && s.totalMs <= targetMs) return level;
    }
    return Governor::kMaxLevel;
}

inline bool worthIt(const Score& s) {
    if (!s.playable) return false;
    if (s.savedMs <= 0.15f) return s.quality > 0.97f;
    // Si accetta di perdere immagine solo se i millisecondi tornano.
    return s.savedMs > (1.f - s.quality) * 8.f;
}

}
