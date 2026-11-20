#pragma once


#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <vector>

#include <glm/glm.hpp>

#include "engine/DataModel.h"

struct NavAgent {
    float radius = 1.2f;
    float height = 5.f;
    bool canJump = false;
    float jumpHeight = 7.f;
    float spacing = 0.f;   // 0 = solo gli angoli. Roblox usa 4.
};

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

        const size_t cells = (size_t)cols_ * rows_;
        blocked_.assign(cells, 0);
        top_.assign(cells, 0.f);
        bot_.assign(cells, 1e9f);

        for (const WorkspacePart& p : world.parts) {
            if (!p.canCollide) continue;
            const glm::vec3 half = glm::abs(p.size) * 0.5f;
            const float top = p.position.y + half.y;
            const float bottom = p.position.y - half.y;
            if (top <= stepOver) continue;          // pavimento: ci si cammina
            if (bottom >= 12.f) continue;           // troppo in alto per la testa

            const float r = std::max(half.x, half.z);
            markBox(p.position.x - r, p.position.z - r,
                    p.position.x + r, p.position.z + r);
            markHeight(p.position.x - r, p.position.z - r,
                       p.position.x + r, p.position.z + r, bottom, top);
        }

        const size_t n = std::min(modelMins.size(), modelMaxs.size());
        for (size_t i = 0; i < n && i < world.models.size(); ++i) {
            if (!world.models[i].canCollide) continue;
            if (modelMaxs[i].y <= stepOver) continue;
            markBox(modelMins[i].x, modelMins[i].z, modelMaxs[i].x, modelMaxs[i].z);
            markHeight(modelMins[i].x, modelMins[i].z, modelMaxs[i].x, modelMaxs[i].z,
                       modelMins[i].y, modelMaxs[i].y);
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
        return findPath(from, to, NavAgent{}, nullptr, maxNodes);
    }

    std::vector<glm::vec3> findPath(const glm::vec3& from, const glm::vec3& to,
                                    const NavAgent& agent, std::vector<uint8_t>* jumps,
                                    int maxNodes = 20000) const {
        std::vector<glm::vec3> out;
        if (!ready()) return out;

        const int sx = cellX(from.x), sz = cellZ(from.z);
        int gx = cellX(to.x), gz = cellZ(to.z);
        if (sx < 0 || sz < 0 || sx >= cols_ || sz >= rows_) return out;

        if (agentBlocked(gx, gz, agent) && !nearestFree(gx, gz, 6, agent)) return out;

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
                    if (agentBlocked(nx, nz, agent)) continue;

                    if (dx != 0 && dz != 0 &&
                        (agentBlocked(cx + dx, cz, agent) || agentBlocked(cx, cz + dz, agent))) continue;

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
        if (agent.spacing > 0.5f && out.size() >= 2) {
            std::vector<glm::vec3> dense;
            dense.push_back(out.front());
            for (size_t i = 1; i < out.size(); ++i) {
                const glm::vec3 a = out[i - 1], b = out[i];
                const float len = glm::length(b - a);
                if (len < 1e-3f) continue;
                const int n = (int)std::floor(len / agent.spacing);
                for (int s = 1; s <= n; ++s) {
                    const float t = (agent.spacing * (float)s) / len;
                    if (t >= 0.999f) break;
                    dense.push_back(glm::mix(a, b, t));
                }
                dense.push_back(b);
            }
            out.swap(dense);
        }
        if (jumps) {
            jumps->assign(out.size(), 0);
            for (size_t i = 0; i < out.size(); ++i)
                (*jumps)[i] = cellJump(cellX(out[i].x), cellZ(out[i].z), agent) ? 1 : 0;
        }
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

    bool cellWall(int cx, int cz, const NavAgent& a) const {
        if (cx < 0 || cz < 0 || cx >= cols_ || cz >= rows_) return true;
        if (top_.empty()) return blockedAt(cx, cz);
        const size_t i = (size_t)cz * cols_ + cx;
        const float top = top_[i];
        const float bot = bot_[i];
        if (top <= stepOver) return false;
        if (bot >= a.height) return false;   // soffitto sopra la testa
        const float limit = a.canJump ? std::max(a.jumpHeight, stepOver) : stepOver;
        return top > limit;
    }

    bool agentBlocked(int cx, int cz, const NavAgent& a) const {
        const int r = std::max(0, (int)std::floor(a.radius / std::max(0.25f, cellSize)));
        for (int dz = -r; dz <= r; ++dz)
            for (int dx = -r; dx <= r; ++dx)
                if (cellWall(cx + dx, cz + dz, a)) return true;
        return false;
    }

    bool cellJump(int cx, int cz, const NavAgent& a) const {
        if (!a.canJump || cx < 0 || cz < 0 || cx >= cols_ || cz >= rows_ || top_.empty()) return false;
        const size_t i = (size_t)cz * cols_ + cx;
        const float top = top_[i];
        if (bot_[i] >= a.height) return false;
        return top > stepOver && top <= std::max(a.jumpHeight, stepOver);
    }

    void markHeight(float minX, float minZ, float maxX, float maxZ, float bottom, float top) {
        const int x0 = cellX(minX), z0 = cellZ(minZ);
        const int x1 = cellX(maxX), z1 = cellZ(maxZ);
        for (int z = std::max(0, z0); z <= std::min(rows_ - 1, z1); ++z)
            for (int x = std::max(0, x0); x <= std::min(cols_ - 1, x1); ++x) {
                const size_t i = (size_t)z * cols_ + x;
                top_[i] = std::max(top_[i], top);
                bot_[i] = std::min(bot_[i], bottom);
            }
    }

    bool nearestFree(int& cx, int& cz, int maxRing, const NavAgent& agent) const {
        for (int r = 1; r <= maxRing; ++r) {
            for (int dz = -r; dz <= r; ++dz) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (std::abs(dx) != r && std::abs(dz) != r) continue;
                    const int nx = cx + dx, nz = cz + dz;
                    if (!agentBlocked(nx, nz, agent)) { cx = nx; cz = nz; return true; }
                }
            }
        }
        return false;
    }

    std::vector<uint8_t> blocked_;
    std::vector<float> top_;
    std::vector<float> bot_;
    int cols_ = 0, rows_ = 0;
    float originX_ = 0.f, originZ_ = 0.f;
};
