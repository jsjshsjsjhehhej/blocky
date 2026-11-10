#pragma once

// Griglia uniforme delle part. La query "chi sta nel raggio" e "quali
// celle toccano il frustum" non deve scorrere il mondo intero quando
// la mappa ha migliaia di blocchi. Le celle vuote non esistono: si
// tiene solo la mappa delle celle occupate.

#include "Frustum.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace antilag {

struct GridItem {
    int index = -1;
    float x = 0.f, y = 0.f, z = 0.f;
    float radius = 1.f;
};

class SpatialGrid {
public:
    void reset(float cellSize) {
        cell_ = cellSize < 1.f ? 1.f : cellSize;
        inv_ = 1.f / cell_;
        cells_.clear();
        items_.clear();
    }

    void reserve(int n) { items_.reserve((size_t)n); }

    void insert(int index, float x, float y, float z, float radius) {
        GridItem it;
        it.index = index;
        it.x = x; it.y = y; it.z = z;
        it.radius = radius < 0.05f ? 0.05f : radius;
        items_.push_back(it);
        int cx, cy, cz;
        cellOf(x, y, z, cx, cy, cz);
        // Una part grande copre piu' celle, altrimenti una query sul
        // bordo la perde. Il raggio in celle e' piccolo (i blocchi di
        // Blocky sono di qualche stud, la cella e' 16).
        int reach = (int)std::ceil(it.radius * inv_);
        if (reach < 0) reach = 0;
        if (reach > 4) reach = 4;
        for (int dz = -reach; dz <= reach; ++dz)
            for (int dy = -reach; dy <= reach; ++dy)
                for (int dx = -reach; dx <= reach; ++dx)
                    cells_[key(cx + dx, cy + dy, cz + dz)].push_back((int)items_.size() - 1);
    }

    int itemCount() const { return (int)items_.size(); }
    int cellCount() const { return (int)cells_.size(); }
    float cellSize() const { return cell_; }

    const GridItem* item(int index) const {
        if (index < 0 || index >= (int)items_.size()) return nullptr;
        return &items_[(size_t)index];
    }

    void querySphere(float x, float y, float z, float radius, std::vector<int>& out) const {
        out.clear();
        if (items_.empty()) return;
        beginQuery();
        float r = radius < 0.f ? 0.f : radius;
        int cx, cy, cz;
        cellOf(x, y, z, cx, cy, cz);
        int reach = (int)std::ceil(r * inv_) + 1;
        if (reach < 1) reach = 1;
        if (reach > 48) reach = 48;
        for (int dz = -reach; dz <= reach; ++dz) {
            for (int dy = -reach; dy <= reach; ++dy) {
                for (int dx = -reach; dx <= reach; ++dx) {
                    auto it = cells_.find(key(cx + dx, cy + dy, cz + dz));
                    if (it == cells_.end()) continue;
                    for (int itemIndex : it->second) {
                        const GridItem& g = items_[(size_t)itemIndex];
                        float ddx = g.x - x, ddy = g.y - y, ddz = g.z - z;
                        float lim = r + g.radius;
                        if (ddx * ddx + ddy * ddy + ddz * ddz <= lim * lim && claim(g.index))
                            out.push_back(g.index);
                    }
                }
            }
        }
    }

    void queryCentersInSphere(float x, float y, float z, float radius, std::vector<int>& out) const {
        out.clear();
        float r = radius < 0.f ? 0.f : radius;
        int cx, cy, cz;
        cellOf(x, y, z, cx, cy, cz);
        int reach = (int)std::ceil(r * inv_) + 1;
        if (reach > 48) reach = 48;
        beginQuery();
        float r2 = r * r;
        for (int dz = -reach; dz <= reach; ++dz)
            for (int dy = -reach; dy <= reach; ++dy)
                for (int dx = -reach; dx <= reach; ++dx) {
                    auto it = cells_.find(key(cx + dx, cy + dy, cz + dz));
                    if (it == cells_.end()) continue;
                    for (int itemIndex : it->second) {
                        const GridItem& g = items_[(size_t)itemIndex];
                        float ddx = g.x - x, ddy = g.y - y, ddz = g.z - z;
                        if (ddx * ddx + ddy * ddy + ddz * ddz <= r2 && claim(g.index))
                            out.push_back(g.index);
                    }
                }
    }

    // Celle occupate il cui AABB non e' fuori dal frustum. Poi il
    // chiamante testa la sfera della part. Cosi' una mappa da 8000
    // blocchi non paga 8000 test di piano se la camera ne vede 400.
    void queryFrustum(const Frustum& frustum, std::vector<int>& out) const {
        out.clear();
        out.reserve(items_.size() / 4 + 8);
        beginQuery();
        for (const auto& kv : cells_) {
            int cx, cy, cz;
            unpack(kv.first, cx, cy, cz);
            float minx = (float)cx * cell_;
            float miny = (float)cy * cell_;
            float minz = (float)cz * cell_;
            float maxx = minx + cell_;
            float maxy = miny + cell_;
            float maxz = minz + cell_;
            if (frustum.aabbOutside(minx, miny, minz, maxx, maxy, maxz))
                continue;
            for (int itemIndex : kv.second) {
                const GridItem& g = items_[(size_t)itemIndex];
                if (!frustum.sphereOutside(g.x, g.y, g.z, g.radius) && claim(g.index))
                    out.push_back(g.index);
            }
        }
    }

private:
    void cellOf(float x, float y, float z, int& cx, int& cy, int& cz) const {
        cx = (int)std::floor(x * inv_);
        cy = (int)std::floor(y * inv_);
        cz = (int)std::floor(z * inv_);
    }

    static uint64_t key(int x, int y, int z) {
        auto pack = [](int v) -> uint64_t {
            return (uint64_t)(uint32_t)(v + 1048576) & 0x1fffffu;
        };
        return pack(x) | (pack(y) << 21) | (pack(z) << 42);
    }

    static void unpack(uint64_t k, int& x, int& y, int& z) {
        auto unpack1 = [](uint64_t v) -> int {
            return (int)(v & 0x1fffffu) - 1048576;
        };
        x = unpack1(k);
        y = unpack1(k >> 21);
        z = unpack1(k >> 42);
    }

    void beginQuery() const {
        ++stampGen_;
        if (stampGen_ == 0) {
            std::fill(stamp_.begin(), stamp_.end(), 0);
            stampGen_ = 1;
        }
    }

    bool claim(int index) const {
        if (index < 0) return false;
        if ((int)stamp_.size() <= index)
            stamp_.resize((size_t)index + 1, 0);
        if (stamp_[(size_t)index] == stampGen_) return false;
        stamp_[(size_t)index] = stampGen_;
        return true;
    }

    float cell_ = 16.f;
    float inv_ = 1.f / 16.f;
    std::vector<GridItem> items_;
    std::unordered_map<uint64_t, std::vector<int>> cells_;
    mutable std::vector<int> stamp_;
    mutable int stampGen_ = 1;
};

}
