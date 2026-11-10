#pragma once

// Diario corto degli scatti. Il pannello non puo' mostrare la finestra
// intera del governor: mostra gli ultimi, con il posto e la fase.

#include "Governor.h"
#include "PhaseMeter.h"

#include <array>

namespace antilag {

struct HitchEvent {
    float ms = 0.f;
    int level = 0;
    StepReason reason = StepReason::None;
    Phase phase = Phase::Count;
    float x = 0.f, y = 0.f, z = 0.f;
};

class HitchLog {
public:
    static constexpr int kCap = 32;

    void clear() {
        head_ = 0;
        count_ = 0;
    }

    void push(const HitchEvent& e) {
        events_[head_] = e;
        head_ = (head_ + 1) % kCap;
        if (count_ < kCap) ++count_;
    }

    int size() const { return count_; }

    // 0 = il piu' recente.
    HitchEvent at(int newestFirst) const {
        if (newestFirst < 0 || newestFirst >= count_) return {};
        int i = head_ - 1 - newestFirst;
        while (i < 0) i += kCap;
        return events_[i];
    }

    int countReason(StepReason r) const {
        int n = 0;
        for (int i = 0; i < count_; ++i)
            if (at(i).reason == r) ++n;
        return n;
    }

    float worstMs() const {
        float m = 0.f;
        for (int i = 0; i < count_; ++i)
            m = std::max(m, at(i).ms);
        return m;
    }

private:
    std::array<HitchEvent, kCap> events_{};
    int head_ = 0;
    int count_ = 0;
};

}
