#pragma once

// Stamina: valore 0..max, Use() se basta, regen dopo un delay.

#include <algorithm>
#include <string>
#include <vector>

class Stamina {
public:
    struct Actor {
        std::string name;
        float value = 100.f;
        float max = 100.f;
        float regen = 18.f;
        float delay = 0.6f;
        float wait = 0.f;
    };

    static constexpr int kMax = 32;

    void reset() { actors_.clear(); }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        for (Actor& a : actors_) {
            float left = dt;
            if (a.wait > 0.f) {
                if (left <= a.wait) {
                    a.wait -= left;
                    continue;
                }
                left -= a.wait;
                a.wait = 0.f;
            }
            if (a.value < a.max)
                a.value = std::min(a.max, a.value + a.regen * left);
        }
    }

    bool add(const std::string& name, float maxVal) {
        if (name.empty() || name.size() > 24) return false;
        float m = std::clamp(maxVal, 1.f, 999.f);
        for (Actor& a : actors_) {
            if (a.name == name) {
                a.max = m;
                a.value = m;
                a.wait = 0.f;
                return true;
            }
        }
        if ((int)actors_.size() >= kMax) return false;
        Actor a;
        a.name = name;
        a.max = m;
        a.value = m;
        actors_.push_back(a);
        return true;
    }

    bool use(const std::string& name, float amount) {
        Actor* a = find(name);
        if (!a || amount <= 0.f || a->value < amount) return false;
        a->value -= amount;
        a->wait = a->delay;
        return true;
    }

    float value(const std::string& name) const {
        const Actor* a = findConst(name);
        return a ? a->value : 0.f;
    }
    float fraction(const std::string& name) const {
        const Actor* a = findConst(name);
        if (!a || a->max <= 1e-6f) return 0.f;
        return a->value / a->max;
    }
    bool ready(const std::string& name, float amount) const {
        const Actor* a = findConst(name);
        return a && a->value >= amount;
    }
    const std::vector<Actor>& all() const { return actors_; }

private:
    Actor* find(const std::string& name) {
        for (Actor& a : actors_)
            if (a.name == name) return &a;
        return nullptr;
    }
    const Actor* findConst(const std::string& name) const {
        for (const Actor& a : actors_)
            if (a.name == name) return &a;
        return nullptr;
    }

    std::vector<Actor> actors_;
};
