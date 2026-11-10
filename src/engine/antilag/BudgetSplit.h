#pragma once

// Il livello del governor e' un tetto uguale per tutti. Qui si stringe
// ancora, ma solo la fase che ha davvero mangiato il frame. Il risultato
// non supera mai il piano in ingresso (e quindi nemmeno la qualita' scelta
// dal giocatore).

#include "Governor.h"
#include "PhaseMeter.h"

namespace antilag {

struct SplitInput {
    FramePlan plan;
    Phase blame = Phase::Count;
    float frameMs = 0.f;
    float targetMs = 16.7f;
    float heat = 0.f;
};

inline float overAmount(float frameMs, float targetMs) {
    if (targetMs < 1.f) targetMs = 16.7f;
    if (frameMs <= targetMs * 1.05f) return 0.f;
    return clampf((frameMs / targetMs - 1.05f) / 1.6f, 0.f, 1.f);
}

// 0 = non toccare quella manopola. 1 = taglio pieno previsto per la fase.
struct PhaseCut {
    float scale = 0.f;
    float distance = 0.f;
    float parts = 0.f;
    float physics = 0.f;
    float lua = 0.f;
    int strideAdd = 0;
    float particles = 0.f;
    float lights = 0.f;
    float shadows = 0.f;
    bool dropPost = false;
    float netAdd = 0.f;
};

inline PhaseCut cutForPhase(Phase blame) {
    PhaseCut c;
    switch (blame) {
    case Phase::Present:
        // Lo scambio buffer e' la GPU che non ha finito. Meno pixel,
        // meno part, niente alone.
        c.scale = 0.22f;
        c.distance = 0.18f;
        c.parts = 0.35f;
        c.shadows = 1.f;
        c.lights = 0.40f;
        c.particles = 0.45f;
        c.dropPost = true;
        break;
    case Phase::World:
        c.scale = 0.10f;
        c.distance = 0.28f;
        c.parts = 0.45f;
        c.particles = 0.25f;
        c.dropPost = true;
        break;
    case Phase::Shadows:
        c.shadows = 1.f;
        c.lights = 0.25f;
        c.parts = 0.10f;
        break;
    case Phase::PostFx:
        c.dropPost = true;
        c.scale = 0.12f;
        c.particles = 0.20f;
        break;
    case Phase::Scripts:
        // Non si tocca il disegno: lo script e' in ritardo, il mondo no.
        c.lua = 0.55f;
        c.strideAdd = 1;
        break;
    case Phase::Physics:
        c.physics = 0.35f;
        c.parts = 0.08f;
        break;
    case Phase::Network:
        c.netAdd = 0.05f;
        break;
    case Phase::Ui:
        c.particles = 0.15f;
        break;
    case Phase::Input:
    case Phase::Count:
        break;
    }
    return c;
}

inline void applyPhaseCut(FramePlan& p, const PhaseCut& cut, float t, float heat) {
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    float ht = heat < 0.f ? 0.f : (heat > 1.f ? 1.f : heat);
    // Il posto caldo stringe vista e part anche se questo frame, da solo,
    // non avrebbe superato la soglia di molto.
    float geo = std::max(t, ht * 0.65f);

    auto down = [](float v, float amount, float tnow) {
        return v * (1.f - amount * tnow);
    };

    p.renderScale = clampf(down(p.renderScale, cut.scale, geo), 0.5f, p.renderScale);
    p.maxDrawDistance = std::max(24.f, down(p.maxDrawDistance, cut.distance, geo));
    p.maxVisibleParts = std::max(32, (int)down((float)p.maxVisibleParts, cut.parts, geo));
    p.physicsHz = std::max(20.f, down(p.physicsHz, cut.physics, t));
    p.luaBudgetUs = std::max(200, (int)down((float)p.luaBudgetUs, cut.lua, t));
    if (t > 0.35f && cut.strideAdd > 0) {
        p.heartbeatStride += cut.strideAdd;
        if (p.heartbeatStride > 4) p.heartbeatStride = 4;
    }
    p.maxParticles = std::max(8, (int)down((float)p.maxParticles, cut.particles, std::max(t, ht * 0.5f)));
    p.maxLights = std::max(1, (int)down((float)p.maxLights, cut.lights, geo));
    if (cut.shadows > 0.f && geo > 0.45f)
        p.shadowOccluders = 0;
    else if (cut.shadows > 0.f)
        p.shadowOccluders = std::max(0, (int)down((float)p.shadowOccluders, cut.shadows, geo));
    if (cut.dropPost && (t > 0.25f || ht > 0.7f))
        p.postFx = false;
    if (cut.netAdd > 0.f && t > 0.2f) {
        float add = cut.netAdd * t;
        if (p.netMinInterval < add) p.netMinInterval = add;
    }
}

inline FramePlan splitBudget(const SplitInput& in) {
    FramePlan p = in.plan;
    float t = overAmount(in.frameMs, in.targetMs);
    if (t <= 0.f && in.heat < 0.15f) return p;
    if (in.blame == Phase::Count && in.heat < 0.15f) return p;
    PhaseCut cut = cutForPhase(in.blame);
    if (in.blame == Phase::Count && in.heat >= 0.15f) {
        // Nessuna fase sporge, ma questo tratto di mappa ha gia' fatto
        // male: si accorcia solo la vista, non gli script.
        cut.distance = 0.22f;
        cut.parts = 0.20f;
    }
    applyPhaseCut(p, cut, t, in.heat);
    // applyPhaseCut non alza, ma il clamp della scala usa il valore gia'
    // ridotto come tetto. Si ricontrolla contro il piano del governor.
    if (p.renderScale > in.plan.renderScale) p.renderScale = in.plan.renderScale;
    if (p.maxDrawDistance > in.plan.maxDrawDistance) p.maxDrawDistance = in.plan.maxDrawDistance;
    if (p.maxVisibleParts > in.plan.maxVisibleParts) p.maxVisibleParts = in.plan.maxVisibleParts;
    if (p.physicsHz > in.plan.physicsHz) p.physicsHz = in.plan.physicsHz;
    if (p.luaBudgetUs > in.plan.luaBudgetUs) p.luaBudgetUs = in.plan.luaBudgetUs;
    if (p.maxParticles > in.plan.maxParticles) p.maxParticles = in.plan.maxParticles;
    if (p.maxLights > in.plan.maxLights) p.maxLights = in.plan.maxLights;
    if (p.shadowOccluders > in.plan.shadowOccluders) p.shadowOccluders = in.plan.shadowOccluders;
    if (p.heartbeatStride < in.plan.heartbeatStride) p.heartbeatStride = in.plan.heartbeatStride;
    if (in.plan.netMinInterval > p.netMinInterval) p.netMinInterval = in.plan.netMinInterval;
    return p;
}

}
