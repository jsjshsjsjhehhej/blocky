#pragma once

// Difese automatiche contro:
// 1) bug "giganteschi" (NaN nel mondo, fisica esplosa, script Lua che saturano)
// 2) lag intenso (degradazione qualita' a piu' livelli, poi recupero graduale)
//
// Non sostituisce TestSessionGuard / CrashHandler: li completa.

#include "../engine/DataModel.h"
#include "../engine/SceneIntegrity.h"
#include "QualityProfile.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace blk {
namespace resilience {

struct State {
    int emergencyLevel = 0;
    float badMsAcc = 0.f;
    float goodMsAcc = 0.f;
    QualityProfile qualityBaseline{};
    bool baselineValid = false;
    bool qualityDirty = false;

    int worldFixCursor = 0;
    int lastWorldFixes = 0;
    int lastPhysicsFixes = 0;
    int totalWorldFixes = 0;

    int scriptBudgetStrikes = 0;
    int scriptKillCount = 0;
};

inline State& state() { static State s; return s; }

inline void resetSession() {
    State& st = state();
    st.emergencyLevel = 0;
    st.badMsAcc = st.goodMsAcc = 0.f;
    st.baselineValid = false;
    st.qualityDirty = false;
    st.worldFixCursor = 0;
    st.lastWorldFixes = st.lastPhysicsFixes = 0;
    st.scriptBudgetStrikes = 0;
}

inline void syncQualityBaseline(const QualityProfile& q) {
    State& st = state();
    if (st.emergencyLevel == 0)
        st.qualityBaseline = q;
    st.baselineValid = true;
}

inline float clampSimulationDt(float dt) {
    if (!std::isfinite(dt) || dt <= 0.f)
        return 1.f / 60.f;
    return std::min(dt, 0.05f);
}

inline void applyEmergencyLevel(QualityProfile& q, int level) {
    State& st = state();
    if (!st.baselineValid)
        st.qualityBaseline = q;
    q = st.qualityBaseline;
    // Degradazione soft: toglie costo (stud/ombre/scala) prima di massacrare
    // i budget — cosi' un place "forte" resta giocabile sotto overload.
    switch (level) {
    case 0:
        break;
    case 1:
        q.maxVisibleParts = std::max(q.mobile ? 900 : 8000,
                                     (int)(q.maxVisibleParts * 0.82f));
        q.maxVisibleModels = std::max(q.mobile ? 28 : 120,
                                      (int)(q.maxVisibleModels * 0.75f));
        q.maxModelVerts = std::max(q.mobile ? 400'000 : 2'000'000,
                                   (int)(q.maxModelVerts * 0.70f));
        q.autoLod = true;
        q.partLod = true;
        q.frustumCull = true;
        q.occlusionCull = true;
        q.cullSmallParts = true;
        q.priorityPartCull = true;
        break;
    case 2:
        q.renderScale = std::min(q.renderScale, q.mobile ? 0.68f : 0.88f);
        q.studTextures = false;
        q.shadowOccluders = 0;
        q.maxVisibleParts = std::max(q.mobile ? 700 : 5000,
                                     (int)(q.maxVisibleParts * 0.62f));
        q.maxVisibleModels = std::max(q.mobile ? 20 : 80,
                                      (int)(q.maxVisibleModels * 0.55f));
        q.maxModelVerts = std::max(q.mobile ? 250'000 : 1'200'000,
                                   (int)(q.maxModelVerts * 0.45f));
        q.maxDrawDistance = std::max(q.mobile ? 100.f : 220.f,
                                     q.maxDrawDistance * 0.82f);
        q.autoLod = true;
        q.partLod = true;
        q.frustumCull = true;
        q.occlusionCull = true;
        q.priorityPartCull = true;
        break;
    case 3:
        q.renderScale = std::min(q.renderScale, q.mobile ? 0.58f : 0.75f);
        q.maxVisibleParts = std::max(q.mobile ? 500 : 2800,
                                     std::min(q.maxVisibleParts,
                                              q.mobile ? 900 : 6000));
        q.maxDrawDistance = std::max(q.mobile ? 80.f : 160.f,
                                     q.maxDrawDistance * 0.68f);
        q.physicsHz = std::min(q.physicsHz, q.mobile ? 30.f : 45.f);
        q.postFx = false;
        q.maxVisibleModels = std::max(q.mobile ? 12 : 48,
                                      std::min(q.maxVisibleModels,
                                               q.mobile ? 24 : 96));
        q.maxModelVerts = std::min(q.maxModelVerts,
                                   q.mobile ? 200'000 : 800'000);
        q.studTextures = false;
        q.shadowOccluders = 0;
        q.priorityPartCull = true;
        break;
    default:
        break;
    }
    st.qualityDirty = true;
}

/** Ritorna true se la qualita' e' cambiata (serve aggiornare render scale). */
inline bool tickLagDefense(float frameMs, QualityProfile& q, bool enabled) {
    if (!enabled) {
        state().qualityDirty = false;
        return false;
    }
    State& st = state();
    if (st.emergencyLevel == 0)
        st.qualityBaseline = q;

    const float targetMs = q.targetFps > 0 ? (1000.f / (float)q.targetFps) : 16.67f;
    // Soglie piu' tolleranti: spike brevi non buttano giu' la qualita'.
    const float lagMul = (q.mobile && q.targetFps >= 55) ? 3.8f : 2.6f;
    const float badThreshold = std::max(38.f, targetMs * lagMul);

    if (frameMs > badThreshold) {
        st.badMsAcc += frameMs;
        st.goodMsAcc = 0.f;
        if (st.badMsAcc > 220.f && st.emergencyLevel < 3) {
            st.badMsAcc = 0.f;
            ++st.emergencyLevel;
            applyEmergencyLevel(q, st.emergencyLevel);
            std::fprintf(stderr, "[Resilience] Lag intenso → emergenza %d/3\n",
                         st.emergencyLevel);
            return true;
        }
    } else if (frameMs < targetMs * 1.35f) {
        st.goodMsAcc += frameMs;
        st.badMsAcc *= 0.55f;
        if (st.goodMsAcc > 1600.f && st.emergencyLevel > 0) {
            st.emergencyLevel--;
            st.goodMsAcc = 0.f;
            applyEmergencyLevel(q, st.emergencyLevel);
            std::fprintf(stderr, "[Resilience] FPS stabili → emergenza %d/3\n",
                         st.emergencyLevel);
            return true;
        }
    }
    const bool dirty = st.qualityDirty;
    st.qualityDirty = false;
    return dirty;
}

inline int sanitizeWorldSlice(DataModel& world, glm::vec3& playerPos) {
    State& st = state();
    int fixed = 0;

    if (!SceneIntegrity::finite3(playerPos) || !SceneIntegrity::saneCoord(playerPos)) {
        playerPos = world.spawn.position;
        if (playerPos.y < 1.f)
            playerPos.y = 3.f;
        ++fixed;
    }

    const int n = (int)world.parts.size();
    if (n == 0) {
        st.lastWorldFixes = fixed;
        return fixed;
    }

    constexpr int kBatch = 48;
    for (int b = 0; b < kBatch && b < n; ++b) {
        const int i = (st.worldFixCursor + b) % n;
        WorkspacePart& p = world.parts[i];

        if (!SceneIntegrity::finite3(p.position) || !SceneIntegrity::saneCoord(p.position)) {
            p.position = world.spawn.position + glm::vec3(0.f, 2.f, 0.f);
            ++fixed;
        }
        if (!SceneIntegrity::finite3(p.size) || p.size.x <= 0.f || p.size.y <= 0.f || p.size.z <= 0.f) {
            p.size = {4.f, 1.f, 2.f};
            ++fixed;
        } else {
            // Solo NaN/inf o valori assurdi (>1e5): niente tetto a 80 stud.
            const float maxDim = std::max(p.size.x, std::max(p.size.y, p.size.z));
            if (maxDim > 1.0e5f) {
                p.size = glm::min(p.size, glm::vec3(1.0e5f));
                ++fixed;
            }
        }
        if (!SceneIntegrity::finite3(p.rotation))
            p.rotation = {0.f, 0.f, 0.f};
        if (!SceneIntegrity::finite3(p.color))
            p.color = {0.64f, 0.64f, 0.64f};
    }
    st.worldFixCursor = (st.worldFixCursor + kBatch) % n;
    st.lastWorldFixes = fixed;
    st.totalWorldFixes += fixed;
    return fixed;
}

inline void onLuaBudgetExceeded(bool& outKillHeartbeats) {
    outKillHeartbeats = false;
    State& st = state();
    ++st.scriptBudgetStrikes;
    if (st.scriptBudgetStrikes >= 4) {
        st.scriptBudgetStrikes = 0;
        ++st.scriptKillCount;
        outKillHeartbeats = true;
        std::fprintf(stderr,
            "[Resilience] Script Lua: budget superato troppe volte — Heartbeat disattivati\n");
    }
}

inline void onLuaFrameOk() {
    State& st = state();
    if (st.scriptBudgetStrikes > 0)
        --st.scriptBudgetStrikes;
}

inline void recordPhysicsFixes(int n) {
    state().lastPhysicsFixes = n;
}

} // namespace resilience
} // namespace blk
