#pragma once

// Mappa dei punti in cui il gioco ha gia' scattato. Un vicolo di una
// mappa puo' costare 40 ms e il resto del mondo 12: abbassare la
// qualita' ovunque per quel vicolo e' uno spreco, ma quando il giocatore
// ci torna il piano deve essere gia' stretto, non scoprirlo un frame dopo.

#include "Math.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace antilag {

class HeatGrid {
public:
    void reset(float cellSize) {
        cell_ = cellSize < 4.f ? 4.f : cellSize;
        inv_ = 1.f / cell_;
        cells_.clear();
    }

    void decay(float dt) {
        if (dt <= 0.f) return;
        if (dt > 0.5f) dt = 0.5f;
        float keep = std::exp(-dt * 0.45f);
        for (auto it = cells_.begin(); it != cells_.end();) {
            it->second.heat *= keep;
            if (it->second.heat < 0.02f) it = cells_.erase(it);
            else ++it;
        }
    }

    // severity 0..1. Piu' scatti nello stesso posto, piu' la cella resta calda.
    void add(float x, float y, float z, float severity) {
        if (severity <= 0.f) return;
        if (severity > 1.f) severity = 1.f;
        Cell& c = cells_[keyOf(x, y, z)];
        c.heat = std::min(1.f, c.heat + severity * (1.f - c.heat * 0.35f));
        c.hits += 1;
    }

    float at(float x, float y, float z) const {
        auto it = cells_.find(keyOf(x, y, z));
        if (it == cells_.end()) return 0.f;
        return it->second.heat;
    }

    // Il calore conta anche le celle vicine: il giocatore non e' fermo
    // sul pixel dello scatto, e' nel tratto di mappa.
    float around(float x, float y, float z) const {
        float h = 0.f;
        int cx = (int)std::floor(x * inv_);
        int cy = (int)std::floor(y * inv_);
        int cz = (int)std::floor(z * inv_);
        for (int dz = -1; dz <= 1; ++dz)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    auto it = cells_.find(key(cx + dx, cy + dy, cz + dz));
                    if (it == cells_.end()) continue;
                    float w = (dx == 0 && dy == 0 && dz == 0) ? 1.f : 0.45f;
                    h = std::max(h, it->second.heat * w);
                }
        return h;
    }

    int hotCount(float threshold) const {
        int n = 0;
        for (const auto& kv : cells_)
            if (kv.second.heat >= threshold) ++n;
        return n;
    }

    int cellCount() const { return (int)cells_.size(); }

private:
    struct Cell {
        float heat = 0.f;
        int hits = 0;
    };

    uint64_t keyOf(float x, float y, float z) const {
        int cx = (int)std::floor(x * inv_);
        int cy = (int)std::floor(y * inv_);
        int cz = (int)std::floor(z * inv_);
        return key(cx, cy, cz);
    }

    static uint64_t key(int x, int y, int z) {
        auto pack = [](int v) -> uint64_t {
            return (uint64_t)(uint32_t)(v + 1048576) & 0x1fffffu;
        };
        return pack(x) | (pack(y) << 21) | (pack(z) << 42);
    }

    float cell_ = 32.f;
    float inv_ = 1.f / 32.f;
    std::unordered_map<uint64_t, Cell> cells_;
};

}
