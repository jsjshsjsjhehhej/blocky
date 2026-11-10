#pragma once

// Facciata. Il frame chiama observe() col dt vero (prima del clamp a
// 50 ms), poi legge plan() per fisica, script, particelle e risoluzione.
// rebuildVisibility() prepara la lista di part da disegnare.

#include "Cause.h"
#include "PlanTrace.h"
#include "Predictor.h"
#include "SimBudget.h"
#include "Workload.h"
#include "ZoneMemory.h"
#include "Visibility.h"

#include <cstdio>
#include <string>

namespace antilag {

class Engine {
public:
    void setMode(Mode m) { gov_.setMode(m); }
    Mode mode() const { return gov_.mode(); }

    bool setModeName(const char* name) {
        if (!name) return false;
        gov_.setMode(modeFromName(name));
        return true;
    }

    void setTargetFps(float fps) { gov_.setTargetFps(fps); }
    float targetFps() const { return gov_.targetFps(); }

    bool enabled() const { return gov_.mode() != Mode::Off; }

    void setBase(const BaseQuality& b) { base_ = b; }
    const BaseQuality& base() const { return base_; }

    void notePhases(const float* ms, int n) { work_.notePhases(ms, n); }

    // seconds e' il dt non clampato. Aggiorna il piano per QUESTO frame,
    // in base a quanto e' durato quello appena chiuso.
    void observe(float seconds) {
        if (seconds < 0.f) seconds = 0.f;
        clock_.push(seconds, gov_.targetMs());
        float frameMs = clock_.latestMs();
        pred_.push(frameMs);
        int floor = zones_.recall(cam_.eye.x, cam_.eye.y, cam_.eye.z);
        int early = pred_.earlySteps(gov_.targetMs());
        if (early > floor) floor = early;
        gov_.setFloor(gov_.mode() == Mode::Off ? 0 : floor);
        plan_ = gov_.evaluate(clock_, base_);
        lastCause_ = classify(clock_, work_.meter(), frameMs, gov_.targetMs());
        work_.setCause(lastCause_);
        if (gov_.mode() != Mode::Off)
            plan_ = work_.settle(plan_, base_, cam_, frameMs, gov_.targetMs());
        trace_.push(plan_);
        if (frameMs >= 80.f || frameMs >= gov_.targetMs() * 1.8f) {
            HitchEvent e;
            e.ms = frameMs;
            e.level = plan_.level;
            e.reason = plan_.reason;
            e.phase = work_.blame();
            e.x = cam_.eye.x;
            e.y = cam_.eye.y;
            e.z = cam_.eye.z;
            work_.journal().push(e);
            float target = gov_.targetMs();
            float sev = target > 1.f ? (frameMs / target - 1.f) / 4.f : 1.f;
            work_.heat().add(e.x, e.y, e.z, clampf(sev, 0.05f, 1.f));
        }
        float here = work_.heat().around(cam_.eye.x, cam_.eye.y, cam_.eye.z);
        // Un solo stallo (alt-tab, shader) non inchioda la zona: servono
        // piu' scatti nello stesso posto, altrimenti la qualita' resta
        // bassa venti secondi per un frame solo.
        if (gov_.mode() != Mode::Off && here > 0.35f && plan_.level >= 2
            && lastCause_.pinZone && work_.journal().size() >= 3) {
            int pin = plan_.level > 4 ? 4 : plan_.level;
            zones_.remember(cam_.eye.x, cam_.eye.y, cam_.eye.z, pin);
        }
        work_.heat().decay(seconds);
        zones_.tick(seconds);
    }

    const FramePlan& plan() const { return plan_; }
    const FrameClock& clock() const { return clock_; }
    int level() const { return plan_.level; }

    void setCamera(const Camera& cam) { cam_ = cam; }
    const Camera& camera() const { return cam_; }

    ScriptRoster& scripts() { return work_.scripts(); }
    const ScriptRoster& scripts() const { return work_.scripts(); }
    const HitchLog& journal() const { return work_.journal(); }
    const HeatGrid& heat() const { return work_.heat(); }
    Phase blame() const { return work_.blame(); }
    const ZoneMemory& zones() const { return zones_; }
    const Predictor& predictor() const { return pred_; }
    const CauseReport& cause() const { return lastCause_; }
    const PlanTrace& trace() const { return trace_; }
    void beginPartDistances() { work_.beginDistances(); }
    void notePartDistance(float d) { work_.noteDistance(d); }

    SimCaps sim(float dt, int particlesAlive, int peers) const {
        return capsFor(plan_, dt, particlesAlive, peers, work_.heat().around(cam_.eye.x, cam_.eye.y, cam_.eye.z));
    }

    void rebuildVisibility(const PartIn* parts, int count) {
        VisibilityLimits lim;
        lim.maxDistance = plan_.maxDrawDistance;
        int fullCap = plan_.studTextures ? plan_.maxVisibleParts / 2 : 0;
        if (fullCap < 0) fullCap = 0;
        lim.maxFull = fullCap;
        lim.maxCheap = std::max(0, plan_.maxVisibleParts - fullCap);
        lim.minScreenPx = plan_.minScreenPx;
        lim.allowStuds = plan_.studTextures;
        lim.useOcclusion = plan_.level >= 4;
        vis_.rebuild(cam_, parts, count, lim);
    }

    const Visibility& visibility() const { return vis_; }

    void rebuildGrid(const PartIn* parts, int count, float cell) {
        grid_.reset(cell);
        grid_.reserve(count);
        for (int i = 0; i < count; ++i) {
            const PartIn& p = parts[i];
            float r = 0.5f * std::sqrt(p.hx * p.hx + p.hy * p.hy + p.hz * p.hz);
            grid_.insert(i, p.x, p.y, p.z, r);
        }
    }

    const SpatialGrid& grid() const { return grid_; }

    std::string summary() const {
        char buf[320];
        std::snprintf(buf, sizeof(buf),
            "antilag %s L%d %s  %.1f ms (p95 %.1f)  %s  scale %.2f  parti %d  fisica %.0f Hz",
            modeName(gov_.mode()), plan_.level, reasonName(plan_.reason),
            plan_.averageMs, plan_.p95Ms, phaseName(work_.blame()),
            plan_.renderScale, plan_.maxVisibleParts, plan_.physicsHz);
        return buf;
    }

private:
    FrameClock clock_;
    Governor gov_;
    FramePlan plan_{};
    BaseQuality base_{};
    Camera cam_{};
    Visibility vis_;
    SpatialGrid grid_;
    Workload work_;
    Predictor pred_;
    ZoneMemory zones_;
    CauseReport lastCause_{};
    PlanTrace trace_;
};

inline Engine& engine() {
    static Engine e;
    return e;
}

}
