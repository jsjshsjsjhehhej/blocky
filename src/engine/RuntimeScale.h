#pragma once

/**
 * Scala del mondo per mappe con tante parti (il lab, le citta').
 *
 * - Streaming: fuori dal raggio le parti non si disegnano. Gli slot sono un
 *   pool: se gli slot finiscono, le piu' lontane restano fuori.
 * - Object pool: gli slot di streaming si riciclano, non si riallocano.
 * - Frame budget: i passi lunghi si fermano quando il frame ha gia' speso
 *   il tempo che gli spetta.
 * - Interest: la rete manda una part solo a chi le sta vicino.
 * - Il sonno della fisica e l'instancing stanno nei loro pass (BlockyPhysics,
 *   PartInstancedDraw, ShadowMap): qui ci sono i contatori.
 */

#include <glm/glm.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace blk::scale {

struct ScaleStats {
    int slept = 0;
    int streamedIn = 0;
    int streamedOut = 0;
    int poolLive = 0;
    int poolFree = 0;
    int interestSent = 0;
    int interestSkipped = 0;
    int asyncPhase = 0;   // 0 fermo, 1 in corso, 2 pronto, 3 fallito
};

inline ScaleStats& stats() {
    static ScaleStats s;
    return s;
}

struct FrameBudget {
    std::chrono::steady_clock::time_point deadline{};

    void begin(double milliseconds) {
        const auto ms = std::chrono::duration<double, std::milli>(std::max(0.5, milliseconds));
        deadline = std::chrono::steady_clock::now() +
                   std::chrono::duration_cast<std::chrono::steady_clock::duration>(ms);
        stats().slept = 0;
        stats().interestSent = 0;
        stats().interestSkipped = 0;
    }

    bool exhausted() const {
        return std::chrono::steady_clock::now() >= deadline;
    }
};

inline FrameBudget& budget() {
    static FrameBudget b;
    return b;
}

/** Pool di interi. acquire/release riciclano gli stessi id. */
class IndexPool {
public:
    void reset(int capacity) {
        capacity_ = -1;
        live_ = 0;
        free_.clear();
        configure(capacity);
    }

    void configure(int capacity) {
        if (capacity == capacity_ && (int)free_.size() + live_ == capacity_) return;
        capacity_ = std::max(0, capacity);
        live_ = 0;
        free_.clear();
        free_.reserve((size_t)capacity_);
        for (int i = capacity_ - 1; i >= 0; --i) free_.push_back(i);
    }

    bool acquire(int& out) {
        if (free_.empty()) return false;
        out = free_.back();
        free_.pop_back();
        ++live_;
        return true;
    }

    void release(int id) {
        if (id < 0) return;
        free_.push_back(id);
        if (live_ > 0) --live_;
    }

    int live() const { return live_; }
    int freeCount() const { return (int)free_.size(); }

private:
    std::vector<int> free_;
    int capacity_ = -1;
    int live_ = 0;
};

struct WorldStream {
    bool enabled = false;
    float radius = 120.f;
    float keepMul = 1.35f;   // isteresi: una volta dentro, esce piu' tardi
    std::vector<uint8_t> inside;
    std::vector<int> token;
    IndexPool pool;
    int cursor = 0;

    bool hides(int i) const {
        return enabled && i >= 0 && i < (int)inside.size() && inside[(size_t)i] == 0;
    }

    template <class Parts, class Foci>
    void tick(const Parts& parts, const Foci& foci, int scanBudget, int slotCap) {
        const int n = (int)parts.size();
        if (!enabled || n == 0 || foci.empty()) {
            if ((int)inside.size() != n) inside.assign((size_t)n, 1);
            else std::fill(inside.begin(), inside.end(), 1);
            token.assign((size_t)n, -1);
            pool.reset(std::max(slotCap, 1));
            stats().streamedIn = n;
            stats().streamedOut = 0;
            stats().poolLive = 0;
            stats().poolFree = pool.freeCount();
            return;
        }
        if ((int)inside.size() != n) {
            inside.assign((size_t)n, 1);
            token.assign((size_t)n, -1);
            pool.reset(std::max(1, slotCap));
            cursor = 0;
        }
        const float r2 = radius * radius;
        // Non "far": su Windows e' una macro di windows.h e non compila.
        const float keepR = radius * keepMul;
        const float far2 = keepR * keepR;
        const int scans = std::max(1, std::min(n, scanBudget));
        for (int s = 0; s < scans; ++s) {
            if (cursor >= n) cursor = 0;
            const int i = cursor++;
            float best = 1e30f;
            const glm::vec3 p = parts[(size_t)i].position;
            for (const glm::vec3& f : foci) {
                const glm::vec3 d = p - f;
                best = std::min(best, glm::dot(d, d));
            }
            if (best <= r2) {
                if (token[(size_t)i] < 0) {
                    int id = -1;
                    if (pool.acquire(id)) token[(size_t)i] = id;
                }
                inside[(size_t)i] = token[(size_t)i] >= 0 ? 1 : 0;
            } else if (best > far2) {
                if (token[(size_t)i] >= 0) {
                    pool.release(token[(size_t)i]);
                    token[(size_t)i] = -1;
                }
                inside[(size_t)i] = 0;
            }
        }
        int inn = 0;
        for (uint8_t v : inside) inn += v ? 1 : 0;
        stats().streamedIn = inn;
        stats().streamedOut = n - inn;
        stats().poolLive = pool.live();
        stats().poolFree = pool.freeCount();
    }
};

inline WorldStream& stream() {
    static WorldStream s;
    return s;
}

inline bool interestNear(const glm::vec3& partPos, const glm::vec3* listeners, int listenerCount,
                         float radius) {
    if (!listeners || listenerCount <= 0) return true;
    const float r2 = radius * radius;
    for (int i = 0; i < listenerCount; ++i) {
        const glm::vec3 d = partPos - listeners[i];
        if (glm::dot(d, d) <= r2) return true;
    }
    return false;
}

struct AsyncLoadState {
    std::atomic<int> phase{0};   // 0 fermo, 1 in corso, 2 pronto, 3 fallito
    std::string path;
    std::string message;
};

inline AsyncLoadState& asyncLoad() {
    static AsyncLoadState s;
    return s;
}

} // namespace blk::scale
