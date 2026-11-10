#pragma once

// Unisce misura, calore e taglio di fase. Il governor sceglie il livello;
// questo oggetto prepara il piano del frame che sta per cominciare.

#include "Cause.h"
#include "BudgetSplit.h"
#include "DistanceHist.h"
#include "Frustum.h"
#include "HeatGrid.h"
#include "HitchLog.h"
#include "PopGuard.h"
#include "RenderLadder.h"
#include "ScriptRoster.h"

namespace antilag {

class Workload {
public:
    void reset() {
        meter_.reset();
        scripts_.reset();
        journal_.clear();
        heat_.reset(32.f);
        blame_ = Phase::Count;
    }

    PhaseMeter& meter() { return meter_; }
    const PhaseMeter& meter() const { return meter_; }
    ScriptRoster& scripts() { return scripts_; }
    const ScriptRoster& scripts() const { return scripts_; }
    HitchLog& journal() { return journal_; }
    const HitchLog& journal() const { return journal_; }
    HeatGrid& heat() { return heat_; }
    const HeatGrid& heat() const { return heat_; }
    Phase blame() const { return blame_; }

    void notePhases(const float* ms, int n) {
        meter_.begin();
        if (!ms || n <= 0) {
            meter_.commit();
            blame_ = Phase::Count;
            return;
        }
        int lim = n < PhaseMeter::kCount ? n : PhaseMeter::kCount;
        for (int i = 0; i < lim; ++i)
            meter_.add((Phase)i, ms[i]);
        meter_.commit();
        blame_ = meter_.blame();
    }

    void beginDistances() { distances_.clear(); }
    void noteDistance(float d) { distances_.add(d); }
    const DistanceHist& distances() const { return distances_; }

    void setCause(const CauseReport& c) { cause_ = c; }

    FramePlan settle(const FramePlan& plan, const BaseQuality& base, const Camera& cam,
                     float frameMs, float targetMs) {
        SplitInput in;
        in.plan = plan;
        in.blame = blame_;
        in.frameMs = frameMs > meter_.sumMs() ? frameMs : meter_.sumMs();
        in.targetMs = targetMs;
        in.heat = heat_.around(cam.eye.x, cam.eye.y, cam.eye.z);
        FramePlan out = splitBudget(in);
        applyCause(out, cause_);
        out.renderScale = ladder_.follow(out.renderScale);
        if (out.renderScale > in.plan.renderScale) out.renderScale = in.plan.renderScale;
        out.maxDrawDistance = capDistanceToContent(out.maxDrawDistance, distances_, out.maxVisibleParts);
        float extended = popExtend(out.maxDrawDistance, cam.velocity, cam.forward, in.heat);
        out.maxDrawDistance = capExtend(extended, plan.maxDrawDistance, base.maxDrawDistance);
        if (out.maxDrawDistance > base.maxDrawDistance)
            out.maxDrawDistance = base.maxDrawDistance;
        return out;
    }

private:
    PhaseMeter meter_;
    ScriptRoster scripts_;
    HitchLog journal_;
    HeatGrid heat_;
    DistanceHist distances_;
    RenderLadder ladder_;
    CauseReport cause_{};
    Phase blame_ = Phase::Count;
};

}
