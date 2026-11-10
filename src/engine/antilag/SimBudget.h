#pragma once

// Quanta simulazione entra in questo frame, oltre al disegno. Il tetto dei
// passi fisici e' quello che impedisce a uno scatto di 50 ms di provare a
// recuperare quattro decimi di fisica nello stesso frame in cui si e' gia'
// in ritardo: il recupero si spalma, il frame no.

#include "Governor.h"

namespace antilag {

struct SimCaps {
    int physicsSteps = 8;
    int particleEmits = 16;
    int bonesPerMesh = 32;
    int voices = 16;
    int navRepairs = 4;
    float netInterval = 0.f;
    int maxPeers = 16;
    bool skinFar = true;
    bool extraCollision = true;
};

inline int physicsStepCap(int level) {
    static const int k[8] = { 8, 6, 5, 4, 4, 3, 2, 2 };
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    return k[level];
}

inline int stepsForFrame(const FramePlan& plan, float dt) {
    if (dt < 0.f) dt = 0.f;
    if (dt > 0.05f) dt = 0.05f;
    float hz = plan.physicsHz < 20.f ? 20.f : plan.physicsHz;
    int need = (int)std::ceil(dt * hz - 1e-4f);
    if (need < 1) need = 1;
    int cap = physicsStepCap(plan.level);
    if (need > cap) need = cap;
    return need;
}

inline int particleEmitsFor(const FramePlan& plan, int requested, int alive) {
    if (requested < 0) requested = 0;
    int room = plan.maxParticles - alive;
    if (room < 0) room = 0;
    static const float kAdmit[8] = { 1.f, 0.85f, 0.7f, 0.5f, 0.35f, 0.22f, 0.12f, 0.08f };
    int level = plan.level;
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    int admit = (int)std::lround(requested * kAdmit[level]);
    if (requested > 0 && admit < 1 && level < 6) admit = 1;
    if (admit > room) admit = room;
    if (admit > 16) admit = 16;
    return admit;
}

inline int bonesFor(const FramePlan& plan, float screenPx, int available) {
    if (available < 1) return 0;
    if (screenPx >= 22.f) return available;
    if (plan.level <= 1 && screenPx >= 8.f) return available;
    if (screenPx < 3.f || plan.level >= 6) return std::min(available, 6);
    if (screenPx < 8.f || plan.level >= 4) return std::min(available, 12);
    return std::min(available, 20);
}

inline int voiceCap(const FramePlan& plan) {
    static const int k[8] = { 16, 14, 12, 8, 6, 4, 3, 2 };
    int level = plan.level;
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    return k[level];
}

inline bool voiceAudible(const FramePlan& plan, float distance, float maxDist) {
    if (maxDist < 1.f) maxDist = 1.f;
    if (distance < 0.f) distance = 0.f;
    float keep = 1.f;
    if (plan.level >= 6) keep = 0.35f;
    else if (plan.level >= 4) keep = 0.55f;
    else if (plan.level >= 2) keep = 0.75f;
    return distance <= maxDist * keep;
}

inline int navRepairsFor(const FramePlan& plan) {
    if (plan.level >= 6) return 0;
    if (plan.level >= 4) return 1;
    if (plan.level >= 2) return 2;
    return 4;
}

inline float netIntervalFor(const FramePlan& plan, int peers, float heat) {
    float iv = plan.netMinInterval;
    if (iv < 0.f) iv = 0.f;
    if (peers > 10) iv = std::max(iv, 0.033f);
    else if (peers > 6 && plan.level >= 2) iv = std::max(iv, 0.02f);
    if (heat > 0.65f && plan.level >= 2) iv = std::max(iv, 0.05f);
    if (plan.level >= 6) iv = std::max(iv, 0.1f);
    if (iv > 0.25f) iv = 0.25f;
    return iv;
}

inline int peerCap(const FramePlan& plan, int peers) {
    int cap = 32;
    if (plan.level >= 6) cap = 8;
    else if (plan.level >= 4) cap = 12;
    else if (plan.level >= 2) cap = 20;
    if (peers < cap) cap = peers;
    if (cap < 0) cap = 0;
    return cap;
}

inline SimCaps capsFor(const FramePlan& plan, float dt, int particlesAlive,
                       int peers, float heat) {
    SimCaps c;
    c.physicsSteps = stepsForFrame(plan, dt);
    c.particleEmits = particleEmitsFor(plan, 16, particlesAlive);
    c.bonesPerMesh = bonesFor(plan, 40.f, 32);
    c.voices = voiceCap(plan);
    c.navRepairs = navRepairsFor(plan);
    c.netInterval = netIntervalFor(plan, peers, heat);
    c.maxPeers = peerCap(plan, peers > 0 ? peers : 32);
    c.skinFar = plan.level < 4;
    c.extraCollision = plan.level < 5;
    return c;
}

}
