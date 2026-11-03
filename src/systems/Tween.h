#pragma once

// Tween: interpola numeri e Vector3 nel tempo, con easing.
// Usato dagli script (Tween.Number / Tween.Vector) per porte, HUD, camera.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

class Tween {
public:
    enum class Ease : uint8_t {
        Linear = 0,
        QuadIn, QuadOut, QuadInOut,
        CubicIn, CubicOut, CubicInOut,
        SineIn, SineOut, SineInOut,
        ExpoIn, ExpoOut, ExpoInOut,
        BackIn, BackOut, BackInOut,
        BounceIn, BounceOut,
        ElasticOut,
        Count
    };

    struct Job {
        int id = 0;
        Ease ease = Ease::Linear;
        float t = 0.f;
        float dur = 1.f;
        bool vec = false;
        bool done = false;
        float a = 0.f, b = 0.f;
        float ax = 0.f, ay = 0.f, az = 0.f;
        float bx = 0.f, by = 0.f, bz = 0.f;
    };

    static constexpr int kMaxJobs = 128;

    void reset() {
        jobs_.clear();
        nextId_ = 1;
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        if (dt > 1.f) dt = 1.f;
        for (Job& j : jobs_) {
            if (j.done || j.dur <= 1e-6f) {
                j.done = true;
                j.t = j.dur;
                continue;
            }
            j.t += dt;
            if (j.t >= j.dur) {
                j.t = j.dur;
                j.done = true;
            }
        }
        if ((int)jobs_.size() > kMaxJobs) {
            jobs_.erase(std::remove_if(jobs_.begin(), jobs_.end(),
                                       [](const Job& j) { return j.done; }),
                        jobs_.end());
        }
    }

    static Ease parseEase(const std::string& name) {
        if (name == "quadin") return Ease::QuadIn;
        if (name == "quadout") return Ease::QuadOut;
        if (name == "quadinout") return Ease::QuadInOut;
        if (name == "cubicin") return Ease::CubicIn;
        if (name == "cubicout") return Ease::CubicOut;
        if (name == "cubicinout") return Ease::CubicInOut;
        if (name == "sinein") return Ease::SineIn;
        if (name == "sineout") return Ease::SineOut;
        if (name == "sineinout") return Ease::SineInOut;
        if (name == "expoin") return Ease::ExpoIn;
        if (name == "expoout") return Ease::ExpoOut;
        if (name == "expoinout") return Ease::ExpoInOut;
        if (name == "backin") return Ease::BackIn;
        if (name == "backout") return Ease::BackOut;
        if (name == "backinout") return Ease::BackInOut;
        if (name == "bouncein") return Ease::BounceIn;
        if (name == "bounceout") return Ease::BounceOut;
        if (name == "elastic") return Ease::ElasticOut;
        return Ease::Linear;
    }

    static float ease(Ease e, float x) {
        x = std::clamp(x, 0.f, 1.f);
        const float pi = 3.14159265f;
        switch (e) {
        case Ease::QuadIn: return x * x;
        case Ease::QuadOut: return 1.f - (1.f - x) * (1.f - x);
        case Ease::QuadInOut: return x < 0.5f ? 2.f * x * x : 1.f - std::pow(-2.f * x + 2.f, 2.f) / 2.f;
        case Ease::CubicIn: return x * x * x;
        case Ease::CubicOut: { float t = 1.f - x; return 1.f - t * t * t; }
        case Ease::CubicInOut: return x < 0.5f ? 4.f * x * x * x : 1.f - std::pow(-2.f * x + 2.f, 3.f) / 2.f;
        case Ease::SineIn: return 1.f - std::cos((x * pi) / 2.f);
        case Ease::SineOut: return std::sin((x * pi) / 2.f);
        case Ease::SineInOut: return -(std::cos(pi * x) - 1.f) / 2.f;
        case Ease::ExpoIn: return x <= 0.f ? 0.f : std::pow(2.f, 10.f * x - 10.f);
        case Ease::ExpoOut: return x >= 1.f ? 1.f : 1.f - std::pow(2.f, -10.f * x);
        case Ease::ExpoInOut:
            if (x <= 0.f) return 0.f;
            if (x >= 1.f) return 1.f;
            return x < 0.5f ? std::pow(2.f, 20.f * x - 10.f) / 2.f
                            : (2.f - std::pow(2.f, -20.f * x + 10.f)) / 2.f;
        case Ease::BackIn: {
            const float c1 = 1.70158f, c3 = c1 + 1.f;
            return c3 * x * x * x - c1 * x * x;
        }
        case Ease::BackOut: {
            const float c1 = 1.70158f, c3 = c1 + 1.f;
            float t = x - 1.f;
            return 1.f + c3 * t * t * t + c1 * t * t;
        }
        case Ease::BackInOut: {
            const float c1 = 1.70158f, c2 = c1 * 1.525f;
            if (x < 0.5f)
                return (std::pow(2.f * x, 2.f) * ((c2 + 1.f) * 2.f * x - c2)) / 2.f;
            return (std::pow(2.f * x - 2.f, 2.f) * ((c2 + 1.f) * (x * 2.f - 2.f) + c2) + 2.f) / 2.f;
        }
        case Ease::BounceIn: return 1.f - bounceOut(1.f - x);
        case Ease::BounceOut: return bounceOut(x);
        case Ease::ElasticOut: {
            if (x <= 0.f) return 0.f;
            if (x >= 1.f) return 1.f;
            const float c4 = (2.f * pi) / 3.f;
            return std::pow(2.f, -10.f * x) * std::sin((x * 10.f - 0.75f) * c4) + 1.f;
        }
        default: return x;
        }
    }

    int number(float from, float to, float dur, Ease e) {
        Job j;
        j.id = nextId_++;
        j.ease = e;
        j.dur = dur > 0.01f ? dur : 0.01f;
        j.a = from;
        j.b = to;
        jobs_.push_back(j);
        return j.id;
    }

    int vector(float ax, float ay, float az, float bx, float by, float bz, float dur, Ease e) {
        Job j;
        j.id = nextId_++;
        j.ease = e;
        j.dur = dur > 0.01f ? dur : 0.01f;
        j.vec = true;
        j.ax = ax; j.ay = ay; j.az = az;
        j.bx = bx; j.by = by; j.bz = bz;
        jobs_.push_back(j);
        return j.id;
    }

    const Job* find(int id) const {
        for (const Job& j : jobs_)
            if (j.id == id) return &j;
        return nullptr;
    }

    float value(int id) const {
        const Job* j = find(id);
        if (!j) return 0.f;
        float u = ease(j->ease, j->dur > 0.f ? j->t / j->dur : 1.f);
        return j->a + (j->b - j->a) * u;
    }

    void value3(int id, float& x, float& y, float& z) const {
        const Job* j = find(id);
        if (!j) { x = y = z = 0.f; return; }
        float u = ease(j->ease, j->dur > 0.f ? j->t / j->dur : 1.f);
        x = j->ax + (j->bx - j->ax) * u;
        y = j->ay + (j->by - j->ay) * u;
        z = j->az + (j->bz - j->az) * u;
    }

    bool isDone(int id) const {
        const Job* j = find(id);
        return j ? j->done : true;
    }

    bool cancel(int id) {
        for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
            if (it->id == id) {
                jobs_.erase(it);
                return true;
            }
        }
        return false;
    }

    int activeCount() const {
        int n = 0;
        for (const Job& j : jobs_)
            if (!j.done) ++n;
        return n;
    }

    int jobCount() const { return (int)jobs_.size(); }

private:
    static float bounceOut(float x) {
        const float n1 = 7.5625f, d1 = 2.75f;
        if (x < 1.f / d1) return n1 * x * x;
        if (x < 2.f / d1) { x -= 1.5f / d1; return n1 * x * x + 0.75f; }
        if (x < 2.5f / d1) { x -= 2.25f / d1; return n1 * x * x + 0.9375f; }
        x -= 2.625f / d1;
        return n1 * x * x + 0.984375f;
    }

    std::vector<Job> jobs_;
    int nextId_ = 1;
};
