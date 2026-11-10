#pragma once

// Previsione del frame prossimo. Il governor reagisce a quello che e' gia'
// successo; se la serie sta salendo (12, 18, 24, 30) il frame che sta per
// partire sara' peggio, e conviene stringere prima di vederlo. Holt: un
// livello e una tendenza, entrambi smussati, cosi' un solo scatto da disco
// non viene scambiato per una discesa infinita.

#include "Math.h"

namespace antilag {

class Predictor {
public:
    void reset() {
        level_ = 0.f;
        trend_ = 0.f;
        count_ = 0;
        rising_ = 0;
        falling_ = 0;
    }

    void push(float ms) {
        if (ms < 0.f) ms = 0.f;
        if (ms > 2000.f) ms = 2000.f;
        if (count_ == 0) {
            level_ = ms;
            trend_ = 0.f;
            count_ = 1;
            return;
        }
        float prev = level_;
        float forecast = level_ + trend_;
        level_ = 0.42f * ms + 0.58f * forecast;
        float delta = level_ - prev;
        trend_ = 0.28f * delta + 0.72f * trend_;
        if (trend_ > 0.35f) {
            ++rising_;
            falling_ = 0;
        } else if (trend_ < -0.35f) {
            ++falling_;
            rising_ = 0;
        }
        ++count_;
    }

    int count() const { return count_; }
    float level() const { return level_; }
    float trend() const { return trend_; }
    int risingRun() const { return rising_; }
    int fallingRun() const { return falling_; }

    float forecast(int steps = 1) const {
        if (count_ <= 0) return 0.f;
        if (steps < 1) steps = 1;
        if (steps > 4) steps = 4;
        return level_ + trend_ * (float)steps;
    }

    // Quanti gradini anticipare. 0 se la serie e' piatta o sta scendendo.
    int earlySteps(float targetMs) const {
        if (count_ < 6) return 0;
        if (targetMs < 1.f) targetMs = 16.7f;
        if (rising_ < 3) return 0;
        float next = forecast(1);
        if (next < targetMs * 1.35f) return 0;
        if (next >= targetMs * 2.4f || rising_ >= 6) return 2;
        return 1;
    }

private:
    float level_ = 0.f;
    float trend_ = 0.f;
    int count_ = 0;
    int rising_ = 0;
    int falling_ = 0;
};

}
