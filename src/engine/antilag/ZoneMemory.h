#pragma once

// Il livello che e' servito l'ultima volta in questa cella. Quando ci
// torni, il primo frame e' gia' a quel pavimento: non si aspetta un altro
// scatto per ricordarselo. Le celle si raffreddano, cosi' un lag di un
// minuto fa non tiene la qualita' bassa per sempre.

#include "Math.h"

#include <cstdint>
#include <unordered_map>

namespace antilag {

class ZoneMemory {
public:
    void reset(float cellSize) {
        cell_ = cellSize < 8.f ? 8.f : cellSize;
        inv_ = 1.f / cell_;
        cells_.clear();
    }

    void remember(float x, float y, float z, int level) {
        if (level < 1) return;
        if (level > 7) level = 7;
        Cell& c = cells_[keyOf(x, y, z)];
        if (level > c.level) c.level = level;
        c.age = 0.f;
        c.visits += 1;
    }

    int recall(float x, float y, float z) const {
        auto it = cells_.find(keyOf(x, y, z));
        if (it == cells_.end()) return 0;
        if (it->second.age > kRememberSec) return 0;
        return it->second.level;
    }

    void tick(float dt) {
        if (dt <= 0.f) return;
        if (dt > 0.5f) dt = 0.5f;
        for (auto it = cells_.begin(); it != cells_.end();) {
            it->second.age += dt;
            if (it->second.age > kRememberSec) it = cells_.erase(it);
            else ++it;
        }
    }

    int zones() const { return (int)cells_.size(); }

    static constexpr float kRememberSec = 20.f;

private:
    struct Cell {
        int level = 0;
        float age = 0.f;
        int visits = 0;
    };

    uint64_t keyOf(float x, float y, float z) const {
        int cx = (int)std::floor(x * inv_);
        int cy = (int)std::floor(y * inv_);
        int cz = (int)std::floor(z * inv_);
        auto pack = [](int v) -> uint64_t {
            return (uint64_t)(uint32_t)(v + 1048576) & 0x1fffffu;
        };
        return pack(cx) | (pack(cy) << 21) | (pack(cz) << 42);
    }

    float cell_ = 48.f;
    float inv_ = 1.f / 48.f;
    std::unordered_map<uint64_t, Cell> cells_;
};

}
