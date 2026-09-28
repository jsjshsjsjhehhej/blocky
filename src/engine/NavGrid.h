#pragma once


#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <vector>

#include <glm/glm.hpp>

#include "engine/DataModel.h"

class NavGrid {
public:
    float cellSize = 2.f;
    float clearance = 1.2f;
    float stepOver = 1.5f;

    bool ready() const { return cols_ > 0 && rows_ > 0; }
    int cols() const { return cols_; }
    int rows() const { return rows_; }

    void build(const DataModel& world,
               const std::vector<glm::vec3>& modelMins,
               const std::vector<glm::vec3>& modelMaxs,
               float bounds = 120.f) {
        originX_ = -bounds;
        originZ_ = -bounds;
        cols_ = std::max(1, (int)std::ceil((bounds * 2.f) / cellSize));
        rows_ = cols_;

        blocked_.assign((size_t)cols_ * rows_, 0);

        for (const WorkspacePart& p : world.parts) {
            if (!p.canCollide) continue;
            const glm::vec3 half = glm::abs(p.size) * 0.5f;
            if (p.position.y + half.y <= stepOver) continue;

            const float r = std::max(half.x, half.z);
            markBox(p.position.x - r, p.position.z - r,
                    p.position.x + r, p.position.z + r);
        }

        const size_t n = std::min(modelMins.size(), modelMaxs.size());
        for (size_t i = 0; i < n && i < world.models.size(); ++i) {
            if (!world.models[i].canCollide) continue;
            if (modelMaxs[i].y <= stepOver) continue;
            markBox(modelMins[i].x, modelMins[i].z, modelMaxs[i].x, modelMaxs[i].z);
        }
    }

    bool blockedAt(int cx, int cz) const {
        if (cx < 0 || cz < 0 || cx >= cols_ || cz >= rows_) return true;
        return blocked_[(size_t)cz * cols_ + cx] != 0;
    }

    bool worldBlocked(float x, float z) const {
        return blockedAt(cellX(x), cellZ(z));
    }

    std::vector<glm::vec3> findPath(const glm::vec3& from, const glm::vec3& to,
                                    int maxNodes = 20000) const {
        std::vector<glm::vec3> out;
        if (!ready()) return out;

        const int sx = cellX(from.x), sz = cellZ(from.z);
        int gx = cellX(to.x), gz = cellZ(to.z);
        if (sx < 0 || sz < 0 || sx >= cols_ || sz >= rows_) return out;

        if (blockedAt(gx, gz) && !nearestFree(gx, gz, 6)) return out;

        const size_t total = (size_t)cols_ * rows_;
        std::vector<float> gScore(total, kInf);
        std::vector<int> parent(total, -1);
        std::vector<uint8_t> closed(total, 0);

        auto idx = [&](int x, int z) { return (size_t)z * cols_ + x; };

        using Node = std::pair<float, int>;
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

        gScore[idx(sx, sz)] = 0.f;
        open.push({heuristic(sx, sz, gx, gz), (int)idx(sx, sz)});

        int expanded = 0;
        int goalIndex = -1;

        while (!open.empty() && expanded < maxNodes) {
            const int current = open.top().second;
            open.pop();
            if (closed[(size_t)current]) continue;
            closed[(size_t)current] = 1;
            ++expanded;

            const int cx = current % cols_;
            const int cz = current / cols_;
            if (cx == gx && cz == gz) { goalIndex = current; break; }

            for (int dz = -1; dz <= 1; ++dz) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dz == 0) continue;
                    const int nx = cx + dx, nz = cz + dz;
                    if (blockedAt(nx, nz)) continue;

                    if (dx != 0 && dz != 0 &&
                        (blockedAt(cx + dx, cz) || blockedAt(cx, cz + dz))) continue;

                    const size_t ni = idx(nx, nz);
                    if (closed[ni]) continue;

                    const float stepCost = (dx != 0 && dz != 0) ? 1.41421356f : 1.f;
                    const float tentative = gScore[(size_t)current] + stepCost;
                    if (tentative < gScore[ni]) {
                        gScore[ni] = tentative;
                        parent[ni] = current;
                        open.push({tentative + heuristic(nx, nz, gx, gz), (int)ni});
                    }
                }
            }
        }

        if (goalIndex < 0) return out;

        std::vector<int> chain;
        for (int i = goalIndex; i >= 0; i = parent[(size_t)i]) chain.push_back(i);
        std::reverse(chain.begin(), chain.end());

        int lastDx = 999, lastDz = 999;
        for (size_t k = 1; k < chain.size(); ++k) {
            const int px = chain[k - 1] % cols_, pz = chain[k - 1] / cols_;
            const int cx = chain[k] % cols_,     cz = chain[k] / cols_;
            const int dx = cx - px, dz = cz - pz;
            if (dx != lastDx || dz != lastDz) {
                out.push_back(worldOf(px, pz, from.y));
                lastDx = dx;
                lastDz = dz;
            }
        }
        out.push_back(worldOf(chain.back() % cols_, chain.back() / cols_, from.y));

        if (!out.empty()) out.back() = glm::vec3(to.x, from.y, to.z);
        return out;
    }

private:
    static constexpr float kInf = 1e30f;

    int cellX(float x) const { return (int)std::floor((x - originX_) / cellSize); }
    int cellZ(float z) const { return (int)std::floor((z - originZ_) / cellSize); }

    glm::vec3 worldOf(int cx, int cz, float y) const {
        return {originX_ + (cx + 0.5f) * cellSize, y, originZ_ + (cz + 0.5f) * cellSize};
    }

    static float heuristic(int ax, int az, int bx, int bz) {
        const float dx = (float)std::abs(ax - bx);
        const float dz = (float)std::abs(az - bz);
        return (dx + dz) + (1.41421356f - 2.f) * std::min(dx, dz);
    }

    void markBox(float minX, float minZ, float maxX, float maxZ) {
        const int x0 = cellX(minX - clearance);
        const int z0 = cellZ(minZ - clearance);
        const int x1 = cellX(maxX + clearance);
        const int z1 = cellZ(maxZ + clearance);

        for (int z = std::max(0, z0); z <= std::min(rows_ - 1, z1); ++z)
            for (int x = std::max(0, x0); x <= std::min(cols_ - 1, x1); ++x)
                blocked_[(size_t)z * cols_ + x] = 1;
    }

    bool nearestFree(int& cx, int& cz, int maxRing) const {
        for (int r = 1; r <= maxRing; ++r) {
            for (int dz = -r; dz <= r; ++dz) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::abs(dx) != r && std::abs(dz) != r) continue;
                    const int nx = cx + dx, nz = cz + dz;
                    if (!blockedAt(nx, nz)) { cx = nx; cz = nz; return true; }
                }
            }
        }
        return false;
    }

    std::vector<uint8_t> blocked_;
    int cols_ = 0, rows_ = 0;
    float originX_ = 0.f, originZ_ = 0.f;
};
