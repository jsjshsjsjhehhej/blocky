#pragma once

// Salute: HP per attore. Danno, cura, morte. Clamp su [0, max].

#include <algorithm>
#include <string>
#include <vector>

class Salute {
public:
    struct Actor {
        std::string name;
        float hp = 100.f;
        float max = 100.f;
        bool dead = false;
    };

    static constexpr int kMax = 48;

    void reset() {
        actors_.clear();
        lastDead_.clear();
    }

    void update(float) {}

    bool add(const std::string& name, float maxHp) {
        if (name.empty() || name.size() > 24) return false;
        float m = std::clamp(maxHp, 1.f, 9999.f);
        for (Actor& a : actors_) {
            if (a.name == name) {
                a.max = m;
                a.hp = m;
                a.dead = false;
                return true;
            }
        }
        if ((int)actors_.size() >= kMax) return false;
        Actor a;
        a.name = name;
        a.max = m;
        a.hp = m;
        actors_.push_back(a);
        return true;
    }

    bool damage(const std::string& name, float amount) {
        Actor* a = find(name);
        if (!a || a->dead || amount <= 0.f) return false;
        a->hp = std::max(0.f, a->hp - amount);
        if (a->hp <= 0.f) {
            a->hp = 0.f;
            a->dead = true;
            lastDead_ = name;
        }
        return true;
    }

    bool heal(const std::string& name, float amount) {
        Actor* a = find(name);
        if (!a || a->dead || amount <= 0.f) return false;
        a->hp = std::min(a->max, a->hp + amount);
        return true;
    }

    bool revive(const std::string& name, float hp) {
        Actor* a = find(name);
        if (!a) return false;
        a->dead = false;
        a->hp = std::clamp(hp > 0.f ? hp : a->max, 1.f, a->max);
        return true;
    }

    float hp(const std::string& name) const {
        const Actor* a = findConst(name);
        return a ? a->hp : 0.f;
    }
    float maxHp(const std::string& name) const {
        const Actor* a = findConst(name);
        return a ? a->max : 0.f;
    }
    bool dead(const std::string& name) const {
        const Actor* a = findConst(name);
        return a ? a->dead : true;
    }
    float fraction(const std::string& name) const {
        const Actor* a = findConst(name);
        if (!a || a->max <= 1e-6f) return 0.f;
        return a->hp / a->max;
    }
    const std::string& lastDead() const { return lastDead_; }
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
    std::string lastDead_;
};
