#pragma once

// Perche' il frame e' lungo. Il livello da solo non distingue un alt-tab
// (uno stallo, poi tutto bene) da una GPU che non ce la fa ogni frame.
// Solo il secondo deve inchiodare la zona.

#include "FrameClock.h"
#include "Governor.h"
#include "PhaseMeter.h"

namespace antilag {

enum class Cause : uint8_t {
    None = 0,
    Gpu,
    Script,
    Physics,
    Network,
    Mixed,
    OneShot
};

inline const char* causeName(Cause c) {
    switch (c) {
    case Cause::None:     return "niente";
    case Cause::Gpu:      return "gpu";
    case Cause::Script:   return "script";
    case Cause::Physics:  return "fisica";
    case Cause::Network:  return "rete";
    case Cause::Mixed:    return "misto";
    case Cause::OneShot:  return "una volta";
    }
    return "?";
}

struct CauseReport {
    Cause cause = Cause::None;
    float confidence = 0.f;
    bool pinZone = false;
    int extraLevel = 0;
    float gpuShare = 0.f;
    float scriptShare = 0.f;
    float physicsShare = 0.f;
    float netShare = 0.f;
};

inline float gpuShareOf(const PhaseMeter& m) {
    return m.share(Phase::Present) + m.share(Phase::World)
         + m.share(Phase::Shadows) + m.share(Phase::PostFx);
}

inline bool looksLikeOneShot(const FrameClock& clock, float targetMs) {
    if (clock.count() < 6) return false;
    if (clock.stalls() + clock.heavies() > 2) return false;
    if (clock.stalls() + clock.heavies() < 1) return false;
    if (clock.goodTail(targetMs * 1.1f) < 4) return false;
    if (clock.latestMs() > targetMs * 1.4f) return false;
    return true;
}

inline bool overBudget(const FrameClock& clock, float frameMs, float targetMs) {
    if (targetMs < 1.f) targetMs = 16.7f;
    if (frameMs > targetMs * 1.25f) return true;
    if (clock.count() >= 8 && clock.percentile(0.95f) > targetMs * 1.35f) return true;
    return false;
}

inline CauseReport classify(const FrameClock& clock, const PhaseMeter& meter,
                            float frameMs, float targetMs) {
    CauseReport r;
    if (targetMs < 1.f) targetMs = 16.7f;
    r.gpuShare = gpuShareOf(meter);
    r.scriptShare = meter.share(Phase::Scripts);
    r.physicsShare = meter.share(Phase::Physics);
    r.netShare = meter.share(Phase::Network);

    if (looksLikeOneShot(clock, targetMs)) {
        r.cause = Cause::OneShot;
        r.confidence = 0.8f;
        r.pinZone = false;
        r.extraLevel = 0;
        return r;
    }
    if (!overBudget(clock, frameMs, targetMs) && clock.badFrames(targetMs * 1.2f) < 3) {
        r.cause = Cause::None;
        r.confidence = 0.6f;
        return r;
    }

    float top = r.gpuShare;
    Cause best = Cause::Gpu;
    if (r.scriptShare > top) { top = r.scriptShare; best = Cause::Script; }
    if (r.physicsShare > top) { top = r.physicsShare; best = Cause::Physics; }
    if (r.netShare > top) { top = r.netShare; best = Cause::Network; }

    int runners = 0;
    if (r.gpuShare > 0.22f) ++runners;
    if (r.scriptShare > 0.22f) ++runners;
    if (r.physicsShare > 0.22f) ++runners;
    if (r.netShare > 0.22f) ++runners;

    if (meter.sumMs() < 0.4f) {
        // Nessuna fase misurata: si sa solo che il frame e' lungo.
        r.cause = Cause::Mixed;
        r.confidence = 0.35f;
    } else if (runners >= 2 && top < 0.55f) {
        r.cause = Cause::Mixed;
        r.confidence = 0.55f;
    } else if (top < 0.28f) {
        r.cause = Cause::Mixed;
        r.confidence = 0.4f;
    } else {
        r.cause = best;
        r.confidence = clampf(top, 0.3f, 1.f);
    }

    r.pinZone = clock.badFrames(targetMs * 1.25f) >= 3 && r.cause != Cause::None;
    if (r.cause == Cause::Gpu && frameMs > targetMs * 2.f) r.extraLevel = 2;
    else if (r.cause != Cause::None && r.cause != Cause::OneShot) r.extraLevel = 1;
    if (clock.badTail(targetMs * 1.5f) >= 6) r.extraLevel = std::max(r.extraLevel, 2);
    if (r.extraLevel > 2) r.extraLevel = 2;
    return r;
}

// Taglio aggiuntivo, piccolo, solo per la causa. 1 = non toccare.
inline float causeScale(const CauseReport& r) {
    if (r.cause != Cause::Gpu && r.cause != Cause::Mixed) return 1.f;
    if (r.extraLevel >= 2) return 0.85f;
    if (r.extraLevel == 1) return 0.92f;
    return 1.f;
}

inline float causeScript(const CauseReport& r) {
    if (r.cause != Cause::Script && r.cause != Cause::Mixed) return 1.f;
    if (r.extraLevel >= 2) return 0.7f;
    if (r.extraLevel == 1) return 0.85f;
    return 1.f;
}

inline float causePhysics(const CauseReport& r) {
    if (r.cause != Cause::Physics && r.cause != Cause::Mixed) return 1.f;
    if (r.extraLevel >= 2) return 0.75f;
    return 0.9f;
}

inline void applyCause(FramePlan& plan, const CauseReport& r) {
    if (r.cause == Cause::None || r.cause == Cause::OneShot) return;
    float beforeScale = plan.renderScale;
    float beforeLua = (float)plan.luaBudgetUs;
    float beforeHz = plan.physicsHz;
    plan.renderScale = std::max(0.5f, plan.renderScale * causeScale(r));
    plan.luaBudgetUs = std::max(200, (int)(plan.luaBudgetUs * causeScript(r)));
    plan.physicsHz = std::max(20.f, plan.physicsHz * causePhysics(r));
    if (plan.renderScale > beforeScale) plan.renderScale = beforeScale;
    if (plan.luaBudgetUs > (int)beforeLua) plan.luaBudgetUs = (int)beforeLua;
    if (plan.physicsHz > beforeHz) plan.physicsHz = beforeHz;
    if (r.cause == Cause::Network && plan.netMinInterval < 0.04f)
        plan.netMinInterval = 0.04f;
}

}
