#pragma once

// Quanto del frame se lo e' preso ogni fase. Il governor sa solo che il
// frame e' durato troppo: se la colpa e' uno script, tagliare le ombre
// non fa tornare i 60 fps, e se la colpa e' lo scambio buffer (la GPU)
// staccare i Heartbeat non fa disegnare prima.

#include "Math.h"

namespace antilag {

// Stesso ordine di plat::prof::Phase, cosi' main copia i lastMs in fila
// senza una tabella di traduzione.
enum class Phase : uint8_t {
    Input = 0,
    Network,
    Scripts,
    Physics,
    Shadows,
    World,
    PostFx,
    Ui,
    Present,
    Count
};

inline const char* phaseName(Phase p) {
    switch (p) {
    case Phase::Input:    return "input";
    case Phase::Network:  return "rete";
    case Phase::Scripts:  return "script";
    case Phase::Physics:  return "fisica";
    case Phase::Shadows:  return "ombre";
    case Phase::World:    return "mondo";
    case Phase::PostFx:   return "post";
    case Phase::Ui:       return "ui";
    case Phase::Present:  return "gpu";
    case Phase::Count:    return "nessuna";
    }
    return "?";
}

inline bool phaseIsGpu(Phase p) {
    return p == Phase::Present || p == Phase::World || p == Phase::Shadows || p == Phase::PostFx;
}

inline bool phaseIsSim(Phase p) {
    return p == Phase::Scripts || p == Phase::Physics;
}

class PhaseMeter {
public:
    static constexpr int kCount = (int)Phase::Count;

    void reset() {
        for (int i = 0; i < kCount; ++i) {
            last_[i] = 0.f;
            ema_[i] = 0.f;
        }
        sum_ = 0.f;
        frames_ = 0;
        blame_ = Phase::Count;
    }

    void begin() {
        for (float& v : last_) v = 0.f;
        sum_ = 0.f;
    }

    void add(Phase p, float ms) {
        int i = (int)p;
        if (i < 0 || i >= kCount) return;
        if (ms < 0.f) ms = 0.f;
        if (ms > 2000.f) ms = 2000.f;
        last_[i] += ms;
        sum_ += ms;
    }

    // Chiude il frame. L'EMA e' lenta di proposito: un singolo scatto non
    // deve far cambiare "colpevole" per i dieci frame dopo, ma last_ si
    // usa per la reazione di QUESTO frame.
    void commit() {
        ++frames_;
        for (int i = 0; i < kCount; ++i)
            ema_[i] = ema_[i] * 0.85f + last_[i] * 0.15f;
        blame_ = pick(last_);
    }

    int frames() const { return frames_; }
    float sumMs() const { return sum_; }
    float lastMs(Phase p) const { return at(last_, p); }
    float emaMs(Phase p) const { return at(ema_, p); }
    Phase blame() const { return blame_; }

    // La fase che pesa di piu' sopra un pavimento. Sotto il pavimento
    // non c'e' un colpevole: e' solo il costo fisso del frame.
    Phase blameAbove(float floorMs) const {
        Phase best = Phase::Count;
        float bestMs = floorMs;
        for (int i = 0; i < kCount; ++i) {
            if (last_[i] > bestMs) {
                bestMs = last_[i];
                best = (Phase)i;
            }
        }
        return best;
    }

    float share(Phase p) const {
        if (sum_ <= 0.01f) return 0.f;
        return at(last_, p) / sum_;
    }

private:
    static float at(const float* a, Phase p) {
        int i = (int)p;
        if (i < 0 || i >= kCount) return 0.f;
        return a[i];
    }

    static Phase pick(const float* ms) {
        int best = -1;
        float bestMs = 0.15f;
        for (int i = 0; i < kCount; ++i) {
            if (ms[i] > bestMs) {
                bestMs = ms[i];
                best = i;
            }
        }
        return best < 0 ? Phase::Count : (Phase)best;
    }

    float last_[kCount] = {};
    float ema_[kCount] = {};
    float sum_ = 0.f;
    int frames_ = 0;
    Phase blame_ = Phase::Count;
};

}
