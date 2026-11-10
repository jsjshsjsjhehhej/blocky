#pragma once

// Gli ultimi piani, per vedere se la scala sta lampeggiando. Un su e giu'
// nello spazio di pochi frame e' peggio del lag: l'occhio legge il
// cambiamento di risoluzione come uno scatto anche quando i millisecondi
// sono a posto.

#include "Governor.h"

#include <array>

namespace antilag {

struct PlanSnap {
    int level = 0;
    float scale = 1.f;
    int parts = 0;
    float physicsHz = 60.f;
};

class PlanTrace {
public:
    static constexpr int kCap = 48;

    void clear() {
        head_ = 0;
        count_ = 0;
        reversals_ = 0;
    }

    void push(const FramePlan& plan) {
        PlanSnap s;
        s.level = plan.level;
        s.scale = plan.renderScale;
        s.parts = plan.maxVisibleParts;
        s.physicsHz = plan.physicsHz;
        if (count_ > 0) {
            const PlanSnap& prev = at(0);
            bool down = s.scale < prev.scale - 0.02f;
            bool up = s.scale > prev.scale + 0.02f;
            if (count_ > 1) {
                const PlanSnap& older = at(1);
                bool wasDown = prev.scale < older.scale - 0.02f;
                bool wasUp = prev.scale > older.scale + 0.02f;
                if ((wasDown && up) || (wasUp && down)) ++reversals_;
            }
            (void)down;
            (void)up;
        }
        snaps_[head_] = s;
        head_ = (head_ + 1) % kCap;
        if (count_ < kCap) ++count_;
    }

    int size() const { return count_; }
    int reversals() const { return reversals_; }

    PlanSnap at(int newestFirst) const {
        if (newestFirst < 0 || newestFirst >= count_) return {};
        int i = head_ - 1 - newestFirst;
        while (i < 0) i += kCap;
        return snaps_[i];
    }

    bool oscillating() const {
        if (count_ < 6) return false;
        int flips = 0;
        for (int i = 0; i < 5 && i + 1 < count_; ++i) {
            float a = at(i).scale;
            float b = at(i + 1).scale;
            if (a > b + 0.02f || b > a + 0.02f) ++flips;
        }
        return flips >= 3;
    }

    float minScale() const {
        float m = 1.f;
        for (int i = 0; i < count_; ++i)
            if (at(i).scale < m) m = at(i).scale;
        return m;
    }

private:
    std::array<PlanSnap, kCap> snaps_{};
    int head_ = 0;
    int count_ = 0;
    int reversals_ = 0;
};

}
