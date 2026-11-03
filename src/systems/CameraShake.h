#pragma once

// CameraShake: trauma [0,1] che decade. Offset XY da sommare alla camera.

#include <algorithm>
#include <cmath>

class CameraShake {
public:
    void reset() {
        trauma_ = 0.f;
        t_ = 0.f;
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        t_ += dt;
        trauma_ = std::max(0.f, trauma_ - dt * 1.6f);
    }

    void add(float amount) {
        trauma_ = std::clamp(trauma_ + std::max(0.f, amount), 0.f, 1.f);
    }

    float trauma() const { return trauma_; }
    float shake() const { return trauma_ * trauma_; }

    float offsetX() const {
        return shake() * 0.35f * std::sin(t_ * 47.f);
    }
    float offsetY() const {
        return shake() * 0.28f * std::sin(t_ * 53.f + 1.7f);
    }
    float roll() const {
        return shake() * 4.f * std::sin(t_ * 31.f);
    }

private:
    float trauma_ = 0.f;
    float t_ = 0.f;
};
