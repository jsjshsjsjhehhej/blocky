#pragma once

// Proiettili: spawn, volo lineare, vita, raggio per overlap puntuale.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

class Proiettili {
public:
    struct Shot {
        std::string name;
        float x = 0, y = 0, z = 0;
        float vx = 0, vy = 0, vz = 0;
        float life = 2.f;
        float radius = 0.2f;
        bool dead = false;
    };

    static constexpr int kMax = 64;

    void reset() {
        shots_.clear();
        nextId_ = 1;
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        std::vector<Shot> keep;
        keep.reserve(shots_.size());
        for (Shot& s : shots_) {
            s.life -= dt;
            if (s.life <= 0.f || s.dead) continue;
            s.x += s.vx * dt;
            s.y += s.vy * dt;
            s.z += s.vz * dt;
            keep.push_back(s);
        }
        shots_.swap(keep);
    }

    bool spawn(const std::string& name, float x, float y, float z,
               float vx, float vy, float vz, float life, float radius) {
        if ((int)shots_.size() >= kMax) return false;
        Shot s;
        s.name = name.empty() ? ("p" + std::to_string(nextId_++)) : name.substr(0, 24);
        s.x = x; s.y = y; s.z = z;
        s.vx = vx; s.vy = vy; s.vz = vz;
        s.life = life > 0.05f ? life : 0.05f;
        s.radius = std::clamp(radius, 0.05f, 4.f);
        for (auto it = shots_.begin(); it != shots_.end(); ++it) {
            if (it->name == s.name) { shots_.erase(it); break; }
        }
        shots_.push_back(s);
        return true;
    }

    bool kill(const std::string& name) {
        for (Shot& s : shots_) {
            if (s.name == name) {
                s.dead = true;
                return true;
            }
        }
        return false;
    }

    bool pos(const std::string& name, float& x, float& y, float& z) const {
        for (const Shot& s : shots_) {
            if (s.name == name && !s.dead) {
                x = s.x; y = s.y; z = s.z;
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> hits(float x, float y, float z, float r) const {
        std::vector<std::string> out;
        r = std::max(0.f, r);
        for (const Shot& s : shots_) {
            if (s.dead) continue;
            float dx = s.x - x, dy = s.y - y, dz = s.z - z;
            float rr = r + s.radius;
            if (dx * dx + dy * dy + dz * dz <= rr * rr)
                out.push_back(s.name);
        }
        return out;
    }

    int count() const { return (int)shots_.size(); }
    const std::vector<Shot>& all() const { return shots_; }

private:
    std::vector<Shot> shots_;
    int nextId_ = 1;
};
