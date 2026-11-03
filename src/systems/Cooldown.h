#pragma once

// Cooldown: timer nominati. Ready("dash") dopo Start("dash", 1.5).

#include <algorithm>
#include <string>
#include <vector>

class Cooldown {
public:
    struct Slot {
        std::string name;
        float left = 0.f;
        float total = 1.f;
    };

    static constexpr int kMax = 32;

    void reset() { slots_.clear(); }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        for (Slot& s : slots_)
            if (s.left > 0.f) s.left = std::max(0.f, s.left - dt);
    }

    bool start(const std::string& name, float seconds) {
        if (name.empty() || name.size() > 24 || seconds <= 0.f) return false;
        for (Slot& s : slots_) {
            if (s.name == name) {
                s.left = seconds;
                s.total = seconds;
                return true;
            }
        }
        if ((int)slots_.size() >= kMax) return false;
        Slot s;
        s.name = name;
        s.left = seconds;
        s.total = seconds;
        slots_.push_back(s);
        return true;
    }

    bool ready(const std::string& name) const {
        for (const Slot& s : slots_)
            if (s.name == name) return s.left <= 0.f;
        return true;
    }

    float left(const std::string& name) const {
        for (const Slot& s : slots_)
            if (s.name == name) return s.left;
        return 0.f;
    }

    float fraction(const std::string& name) const {
        for (const Slot& s : slots_) {
            if (s.name != name) continue;
            if (s.total <= 1e-6f) return 1.f;
            return 1.f - (s.left / s.total);
        }
        return 1.f;
    }

    const std::vector<Slot>& all() const { return slots_; }

private:
    std::vector<Slot> slots_;
};
