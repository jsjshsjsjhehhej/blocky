#pragma once

// Combo: contatore colpi con timeout. Hit() incrementa, reset se scade.

#include <algorithm>

class Combo {
public:
    void reset() {
        count_ = 0;
        left_ = 0.f;
        timeout_ = 1.4f;
        best_ = 0;
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        if (count_ <= 0) return;
        left_ -= dt;
        if (left_ <= 0.f) {
            count_ = 0;
            left_ = 0.f;
        }
    }

    int hit() {
        count_ += 1;
        left_ = timeout_;
        if (count_ > best_) best_ = count_;
        return count_;
    }

    void setTimeout(float seconds) {
        timeout_ = std::clamp(seconds, 0.2f, 8.f);
    }

    int count() const { return count_; }
    int best() const { return best_; }
    float left() const { return left_; }

private:
    int count_ = 0;
    int best_ = 0;
    float left_ = 0.f;
    float timeout_ = 1.4f;
};
