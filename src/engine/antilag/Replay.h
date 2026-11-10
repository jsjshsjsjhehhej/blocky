#pragma once

// Ripete una sequenza di frame su un Engine locale. Serve a controllare
// che una traccia "GPU piena" stringa il disegno e una traccia "script
// pieno" stringa il budget Lua, senza aprire una finestra.

#include "AntiLag.h"

namespace antilag {

struct TraceFrame {
    float dt = 0.016f;
    float phaseMs[PhaseMeter::kCount] = {};
};

struct TraceTotals {
    int frames = 0;
    int maxLevel = 0;
    int finalLevel = 0;
    float minScale = 1.f;
    int minParts = 0;
    int minLuaUs = 0;
    int scriptStrides = 0;
    int postOffFrames = 0;
    int journalled = 0;
};

inline void playTrace(Engine& eng, const TraceFrame* frames, int count, TraceTotals& out) {
    out = {};
    out.minScale = 99.f;
    out.minParts = 1 << 30;
    out.minLuaUs = 1 << 30;
    if (!frames || count <= 0) return;
    for (int i = 0; i < count; ++i) {
        const TraceFrame& f = frames[i];
        eng.notePhases(f.phaseMs, PhaseMeter::kCount);
        float dt = f.dt;
        if (dt < 0.f) dt = 0.f;
        eng.observe(dt);
        const FramePlan& p = eng.plan();
        if (p.level > out.maxLevel) out.maxLevel = p.level;
        if (p.renderScale < out.minScale) out.minScale = p.renderScale;
        if (p.maxVisibleParts < out.minParts) out.minParts = p.maxVisibleParts;
        if (p.luaBudgetUs < out.minLuaUs) out.minLuaUs = p.luaBudgetUs;
        if (p.heartbeatStride > 1) ++out.scriptStrides;
        if (!p.postFx) ++out.postOffFrames;
        ++out.frames;
    }
    out.finalLevel = eng.level();
    out.journalled = eng.journal().size();
}

inline TraceFrame gpuFrame(float dt, float presentMs) {
    TraceFrame f;
    f.dt = dt;
    f.phaseMs[(int)Phase::Input] = 0.2f;
    f.phaseMs[(int)Phase::Scripts] = 0.4f;
    f.phaseMs[(int)Phase::Physics] = 0.6f;
    f.phaseMs[(int)Phase::World] = 1.5f;
    f.phaseMs[(int)Phase::Present] = presentMs;
    return f;
}

inline TraceFrame scriptFrame(float dt, float scriptMs) {
    TraceFrame f;
    f.dt = dt;
    f.phaseMs[(int)Phase::Input] = 0.2f;
    f.phaseMs[(int)Phase::Scripts] = scriptMs;
    f.phaseMs[(int)Phase::Physics] = 0.5f;
    f.phaseMs[(int)Phase::World] = 0.8f;
    f.phaseMs[(int)Phase::Present] = 0.4f;
    return f;
}

inline TraceFrame calmFrame() {
    return gpuFrame(0.012f, 1.2f);
}

}
