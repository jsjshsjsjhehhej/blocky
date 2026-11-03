#pragma once

// Knockback: impulso 3D per attore, decade con damping. Offset da sommare
// alla posizione ogni frame.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

class Knockback {
public:
    struct Body {
        std::string name;
        float vx = 0, vy = 0, vz = 0;
    };

    static constexpr int kMax = 48;

    void reset() { bodies_.clear(); }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        const float damp = std::pow(0.08f, dt);
        for (Body& b : bodies_) {
            b.vx *= damp;
            b.vy *= damp;
            b.vz *= damp;
            if (std::fabs(b.vx) < 0.01f) b.vx = 0.f;
            if (std::fabs(b.vy) < 0.01f) b.vy = 0.f;
            if (std::fabs(b.vz) < 0.01f) b.vz = 0.f;
        }
    }

    bool add(const std::string& name, float vx, float vy, float vz) {
        if (name.empty() || name.size() > 24) return false;
        Body* b = find(name);
        if (b) {
            b->vx += vx;
            b->vy += vy;
            b->vz += vz;
            return true;
        }
        if ((int)bodies_.size() >= kMax) return false;
        Body n;
        n.name = name;
        n.vx = vx;
        n.vy = vy;
        n.vz = vz;
        bodies_.push_back(n);
        return true;
    }

    bool fromHit(const std::string& name, float ox, float oy, float oz,
                 float hx, float hy, float hz, float force) {
        float dx = hx - ox, dy = hy - oy, dz = hz - oz;
        float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (len < 1e-5f) {
            dx = 0.f;
            dy = 1.f;
            dz = 0.f;
            len = 1.f;
        }
        force = std::clamp(force, 0.f, 80.f);
        return add(name, (dx / len) * force, (dy / len) * force + force * 0.15f, (dz / len) * force);
    }

    float vx(const std::string& name) const {
        const Body* b = findConst(name);
        return b ? b->vx : 0.f;
    }
    float vy(const std::string& name) const {
        const Body* b = findConst(name);
        return b ? b->vy : 0.f;
    }
    float vz(const std::string& name) const {
        const Body* b = findConst(name);
        return b ? b->vz : 0.f;
    }
    bool active(const std::string& name) const {
        const Body* b = findConst(name);
        if (!b) return false;
        return std::fabs(b->vx) + std::fabs(b->vy) + std::fabs(b->vz) > 0.05f;
    }

    const std::vector<Body>& all() const { return bodies_; }

private:
    Body* find(const std::string& name) {
        for (Body& b : bodies_)
            if (b.name == name) return &b;
        return nullptr;
    }
    const Body* findConst(const std::string& name) const {
        for (const Body& b : bodies_)
            if (b.name == name) return &b;
        return nullptr;
    }

    std::vector<Body> bodies_;
};
