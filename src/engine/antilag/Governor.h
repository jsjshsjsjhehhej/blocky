#pragma once

// Il governor non disegna e non simula. Guarda quanto e' durato il frame
// vero e sceglie un livello. Ogni livello e' un tetto: non alza mai la
// qualita' che l'utente ha gia' scelto, la puo' solo abbassare.
//
// Sale in fretta (pochi frame brutti bastano) e scende piano (decine di
// frame buoni). Altrimenti la risoluzione lampeggia e il "fix" si vede
// piu' del lag.

#include "FrameClock.h"

#include <cmath>

namespace antilag {

enum class Mode : uint8_t {
    Off = 0,
    Auto,
    Strong
};

inline const char* modeName(Mode m) {
    switch (m) {
    case Mode::Off:    return "Off";
    case Mode::Auto:   return "Auto";
    case Mode::Strong: return "Strong";
    }
    return "?";
}

inline Mode modeFromName(const char* s) {
    if (!s || !s[0]) return Mode::Auto;
    char a = s[0];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (a == 'o') return Mode::Off;
    if (a == 's' || a == 'f') return Mode::Strong; // strong / forte
    return Mode::Auto;
}

enum class StepReason : uint8_t {
    None = 0,
    Stall,
    HeavyRun,
    MildRun,
    Sustained,
    Recover,
    Manual,
    Floor
};

inline const char* reasonName(StepReason r) {
    switch (r) {
    case StepReason::None:      return "fermo";
    case StepReason::Stall:     return "stallo";
    case StepReason::HeavyRun:  return "scatti";
    case StepReason::MildRun:   return "sopra target";
    case StepReason::Sustained: return "media alta";
    case StepReason::Recover:   return "recupero";
    case StepReason::Manual:    return "manuale";
    case StepReason::Floor:     return "pavimento";
    }
    return "?";
}

struct BaseQuality {
    float renderScale = 1.f;
    float maxDrawDistance = 400.f;
    int maxVisibleParts = 4000;
    float physicsHz = 60.f;
    int luaBudgetUs = 3000;
    int maxParticles = 512;
    int maxLights = 8;
    int shadowOccluders = 8;
    bool studTextures = true;
    bool postFx = true;
};

struct FramePlan {
    int level = 0;
    StepReason reason = StepReason::None;
    float renderScale = 1.f;
    float maxDrawDistance = 400.f;
    int maxVisibleParts = 4000;
    float minScreenPx = 0.f;
    float physicsHz = 60.f;
    int luaBudgetUs = 3000;
    int heartbeatStride = 1;
    int maxParticles = 512;
    int maxLights = 8;
    int shadowOccluders = 8;
    bool studTextures = true;
    bool postFx = true;
    float netMinInterval = 0.f;
    float targetMs = 16.7f;
    float averageMs = 0.f;
    float p95Ms = 0.f;
};

struct LevelCut {
    float scale = 1.f;
    float distance = 1.f;
    float parts = 1.f;
    float minPx = 0.f;
    float physics = 1.f;
    float lua = 1.f;
    float particles = 1.f;
    float lights = 1.f;
    float shadows = 1.f;
    int stride = 1;
    bool studs = true;
    bool post = true;
    float netHz = 0.f; // 0 = ogni frame
};

// Otto gradini. Lo zero e' "non toccare". Il sette tiene in vita una
// mappa troppo pesante: pochi blocchi vicini, fisica a 20 Hz, script
// a turno, niente ombre e niente stud.
inline LevelCut cutFor(int level) {
    static const LevelCut k[8] = {
        { 1.00f, 1.00f, 1.00f, 0.0f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1, true,  true,  0.f },
        { 0.92f, 0.90f, 0.80f, 0.8f, 0.85f, 0.80f, 0.80f, 1.00f, 0.75f, 1, true,  true,  0.f },
        { 0.85f, 0.75f, 0.60f, 1.2f, 0.75f, 0.60f, 0.60f, 0.75f, 0.50f, 1, true,  false, 0.f },
        { 0.75f, 0.60f, 0.45f, 1.6f, 0.60f, 0.45f, 0.40f, 0.50f, 0.00f, 2, true,  false, 30.f },
        { 0.65f, 0.45f, 0.30f, 2.0f, 0.45f, 0.30f, 0.25f, 0.50f, 0.00f, 2, false, false, 20.f },
        { 0.55f, 0.35f, 0.18f, 2.6f, 0.40f, 0.22f, 0.15f, 0.25f, 0.00f, 3, false, false, 20.f },
        { 0.50f, 0.25f, 0.12f, 3.2f, 0.33f, 0.16f, 0.08f, 0.25f, 0.00f, 4, false, false, 15.f },
        { 0.50f, 0.18f, 0.08f, 4.0f, 0.28f, 0.10f, 0.04f, 0.15f, 0.00f, 4, false, false, 10.f },
    };
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    return k[level];
}

inline FramePlan planFrom(const BaseQuality& base, int level, StepReason why, float targetMs) {
    LevelCut c = cutFor(level);
    FramePlan p;
    p.level = level;
    p.reason = why;
    p.targetMs = targetMs;
    p.renderScale = clampf(c.scale, 0.5f, 1.f);
    p.maxDrawDistance = std::max(24.f, base.maxDrawDistance * c.distance);
    p.maxVisibleParts = std::max(32, (int)(base.maxVisibleParts * c.parts));
    p.minScreenPx = c.minPx;
    p.physicsHz = std::max(20.f, base.physicsHz * c.physics);
    p.luaBudgetUs = std::max(200, (int)(base.luaBudgetUs * c.lua));
    p.heartbeatStride = c.stride < 1 ? 1 : c.stride;
    p.maxParticles = std::max(8, (int)(base.maxParticles * c.particles));
    p.maxLights = std::max(1, (int)std::lround(base.maxLights * c.lights));
    if (p.maxLights > base.maxLights) p.maxLights = base.maxLights;
    p.shadowOccluders = c.shadows <= 0.f ? 0 : std::max(0, (int)std::lround(base.shadowOccluders * c.shadows));
    p.studTextures = base.studTextures && c.studs;
    p.postFx = base.postFx && c.post;
    p.netMinInterval = c.netHz <= 1.f ? 0.f : 1.f / c.netHz;
    return p;
}

class Governor {
public:
    static constexpr int kMaxLevel = 7;

    void setMode(Mode m) { mode_ = m; }
    Mode mode() const { return mode_; }

    void setTargetFps(float fps) {
        if (fps < 20.f) fps = 20.f;
        if (fps > 240.f) fps = 240.f;
        targetFps_ = fps;
    }
    float targetFps() const { return targetFps_; }
    float targetMs() const { return 1000.f / targetFps_; }

    int level() const { return level_; }
    StepReason lastReason() const { return reason_; }

    // Pavimento temporaneo: la zona in cui ti trovi ha gia' avuto bisogno
    // di questo livello. Non sale da solo e Off lo ignora.
    void setFloor(int level) {
        if (level < 0) level = 0;
        if (level > kMaxLevel) level = kMaxLevel;
        floor_ = level;
    }
    int floor() const { return floor_; }

    FramePlan evaluate(const FrameClock& clock, const BaseQuality& base) {
        float target = targetMs();
        if (mode_ == Mode::Strong) target *= 0.9f;

        StepReason why = StepReason::None;
        if (mode_ == Mode::Off) {
            level_ = 0;
            why = StepReason::Manual;
        } else {
            why = step(clock, target);
            if (mode_ == Mode::Strong && level_ < 2 && clock.count() > 8) {
                // Strong parte gia' un gradino sotto: il primo scatto non
                // deve aspettare che il giocatore lo veda.
                if (clock.percentile(0.5f) > target * 0.95f) {
                    level_ = 2;
                    why = StepReason::Floor;
                }
            }
            if (level_ < floor_) {
                level_ = floor_;
                why = StepReason::Floor;
            }
        }

        FramePlan plan = planFrom(base, level_, why, target);
        plan.averageMs = clock.averageMs();
        plan.p95Ms = clock.percentile(0.95f);
        reason_ = why;
        return plan;
    }

private:
    StepReason step(const FrameClock& clock, float target) {
        if (clock.count() < 4) return StepReason::None;

        float mild = target * (mode_ == Mode::Strong ? 1.08f : 1.20f);
        float heavy = target * (mode_ == Mode::Strong ? 1.35f : 1.60f);
        int needBad = mode_ == Mode::Strong ? 4 : 6;
        int needGood = mode_ == Mode::Strong ? 30 : 45;

        if (clock.latestMs() >= 80.f || clock.latestMs() >= target * 4.f) {
            bump(2);
            cooldown_ = 10;
            return StepReason::Stall;
        }
        if (clock.badTail(heavy) >= 3) {
            bump(1);
            cooldown_ = 8;
            return StepReason::HeavyRun;
        }
        if (clock.badTail(mild) >= needBad) {
            bump(1);
            cooldown_ = 6;
            return StepReason::MildRun;
        }
        if (clock.count() >= 30 && clock.averageMs() > mild && clock.percentile(0.95f) > heavy) {
            bump(1);
            cooldown_ = 8;
            return StepReason::Sustained;
        }
        if (cooldown_ > 0) {
            --cooldown_;
            return StepReason::None;
        }
        if (clock.goodTail(target * 0.85f) >= needGood && level_ > 0) {
            --level_;
            // Senza questa attesa, dopo 45 frame buoni il livello
            // crollerebbe di uno a ogni frame. La salita resta immediata
            // (i controlli sopra ignorano il cooldown); la discesa no.
            cooldown_ = needGood;
            return StepReason::Recover;
        }
        return StepReason::None;
    }

    void bump(int n) {
        level_ += n;
        if (level_ > kMaxLevel) level_ = kMaxLevel;
    }

    Mode mode_ = Mode::Auto;
    float targetFps_ = 60.f;
    int level_ = 0;
    int floor_ = 0;
    int cooldown_ = 0;
    StepReason reason_ = StepReason::None;
};

}
