#pragma once

// Zone: scatole AABB. Lo script le registra e chiede se il player e' dentro,
// o legge l'ultima zona appena entrata/uscita.

#include <cmath>
#include <string>
#include <vector>

class Zone {
public:
    struct Box {
        std::string name;
        float x = 0, y = 0, z = 0;
        float hx = 2, hy = 2, hz = 2;
        bool inside = false;
    };

    static constexpr int kMax = 48;

    void reset() {
        boxes_.clear();
        entered_.clear();
        exited_.clear();
    }

    void update(float) {}

    bool add(const std::string& name, float x, float y, float z, float hx, float hy, float hz) {
        if (name.empty() || name.size() > 24 || (int)boxes_.size() >= kMax) return false;
        for (const Box& b : boxes_)
            if (b.name == name) return false;
        Box b;
        b.name = name;
        b.x = x; b.y = y; b.z = z;
        b.hx = hx > 0.1f ? hx : 0.1f;
        b.hy = hy > 0.1f ? hy : 0.1f;
        b.hz = hz > 0.1f ? hz : 0.1f;
        boxes_.push_back(b);
        return true;
    }

    bool remove(const std::string& name) {
        for (auto it = boxes_.begin(); it != boxes_.end(); ++it) {
            if (it->name == name) {
                boxes_.erase(it);
                return true;
            }
        }
        return false;
    }

    void scan(float px, float py, float pz) {
        entered_.clear();
        exited_.clear();
        for (Box& b : boxes_) {
            const bool now = std::fabs(px - b.x) <= b.hx
                          && std::fabs(py - b.y) <= b.hy
                          && std::fabs(pz - b.z) <= b.hz;
            if (now && !b.inside) entered_.push_back(b.name);
            if (!now && b.inside) exited_.push_back(b.name);
            b.inside = now;
        }
    }

    bool contains(const std::string& name) const {
        for (const Box& b : boxes_)
            if (b.name == name) return b.inside;
        return false;
    }

    const std::vector<std::string>& entered() const { return entered_; }
    const std::vector<std::string>& exited() const { return exited_; }
    const std::vector<Box>& all() const { return boxes_; }
    int count() const { return (int)boxes_.size(); }

private:
    std::vector<Box> boxes_;
    std::vector<std::string> entered_;
    std::vector<std::string> exited_;
};
