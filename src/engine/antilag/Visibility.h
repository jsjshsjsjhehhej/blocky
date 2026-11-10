#pragma once

// Decisione per ogni part: si disegna, si disegna povera (niente stud),
// oppure si salta. La fisica resta al solver (ha gia' il suo LOD). Qui
// si decide solo il costo di disegno e di luci.

#include "OcclusionBuffer.h"
#include "SpatialGrid.h"

#include <algorithm>
#include <vector>

namespace antilag {

enum class DrawKind : uint8_t {
    Full = 0,
    Cheap,
    Skip
};

struct PartIn {
    float x = 0.f, y = 0.f, z = 0.f;
    float hx = 1.f, hy = 1.f, hz = 1.f;
    bool light = false;
    bool script = false;
    bool transparent = false;
    bool anchored = true;
};

struct PartOut {
    DrawKind draw = DrawKind::Skip;
    float distance = 0.f;
    float screenPx = 0.f;
    int score = 0;
};

struct VisibilityLimits {
    float maxDistance = 400.f;
    int maxFull = 2000;
    int maxCheap = 2000;
    float minScreenPx = 0.f;
    bool allowStuds = true;
    bool useOcclusion = false;
};

class Visibility {
public:
    void rebuild(const Camera& cam, const PartIn* parts, int count, const VisibilityLimits& lim) {
        outs_.assign((size_t)std::max(0, count), PartOut{});
        full_.clear();
        cheap_.clear();
        if (!parts || count <= 0) return;

        ranks_.clear();
        ranks_.reserve((size_t)count);

        float maxD = lim.maxDistance < 1.f ? 1.f : lim.maxDistance;
        float maxD2 = maxD * maxD;

        for (int i = 0; i < count; ++i) {
            const PartIn& p = parts[i];
            float dx = p.x - cam.eye.x;
            float dy = p.y - cam.eye.y;
            float dz = p.z - cam.eye.z;
            float d2 = dx * dx + dy * dy + dz * dz;
            PartOut& o = outs_[(size_t)i];
            o.distance = std::sqrt(d2);
            float radius = 0.5f * std::sqrt(p.hx * p.hx + p.hy * p.hy + p.hz * p.hz);
            if (radius < 0.05f) radius = 0.05f;
            o.screenPx = screenRadiusPx(cam.eye, cam.fovyRad, cam.viewportH, v3(p.x, p.y, p.z), radius);
            o.score = scoreOf(p, o.distance, o.screenPx, maxD);
            o.draw = DrawKind::Skip;
            if (d2 > maxD2 && !p.light) continue;
            float predR = predictedRadius(radius, cam, 0.08f);
            if (cam.frustum.sphereOutside(p.x, p.y, p.z, predR) && !p.script) continue;
            if (o.screenPx < lim.minScreenPx && !p.light && !p.script && d2 > 16.f) continue;
            Rank r;
            r.index = i;
            r.d2 = d2;
            r.px = o.screenPx;
            r.score = o.score;
            r.transparent = p.transparent;
            r.important = p.light || p.script || !p.anchored;
            ranks_.push_back(r);
        }

        if (lim.useOcclusion && (int)ranks_.size() > 4)
            dropOccluded(cam, parts);

        // Le importanti passano davanti, poi le vicine. nth_element sul
        // budget, non un sort completo del mondo.
        auto better = [](const Rank& a, const Rank& b) {
            if (a.important != b.important) return a.important && !b.important;
            return a.d2 < b.d2;
        };
        int cap = lim.maxFull + lim.maxCheap;
        if (cap < 0) cap = 0;
        if ((int)ranks_.size() > cap && cap > 0) {
            std::nth_element(ranks_.begin(), ranks_.begin() + cap, ranks_.end(), better);
            ranks_.resize((size_t)cap);
        } else if (cap == 0) {
            ranks_.clear();
        }

        int fullBudget = lim.maxFull;
        int cheapBudget = lim.maxCheap;
        for (const Rank& r : ranks_) {
            PartOut& o = outs_[(size_t)r.index];
            bool wantFull = lim.allowStuds && r.px >= 8.f && fullBudget > 0;
            if (wantFull) {
                o.draw = DrawKind::Full;
                --fullBudget;
                full_.push_back(r.index);
            } else if (cheapBudget > 0) {
                o.draw = DrawKind::Cheap;
                --cheapBudget;
                cheap_.push_back(r.index);
            }
        }

        // Opachi lontani prima, trasparenti dopo e dal piu' lontano: lo
        // stesso ordine che il renderer gia' usa, cosi' il blend non
        // cambia solo perche' l'anti-lag ha filtrato la lista.
        auto drawOrder = [&](int a, int b) {
            const PartOut& pa = outs_[(size_t)a];
            const PartOut& pb = outs_[(size_t)b];
            bool ta = parts[a].transparent;
            bool tb = parts[b].transparent;
            if (ta != tb) return !ta && tb;
            if (ta) return pa.distance < pb.distance;
            return pa.distance > pb.distance;
        };
        std::sort(full_.begin(), full_.end(), drawOrder);
        std::sort(cheap_.begin(), cheap_.end(), drawOrder);
        (void)better;
    }

    const PartOut* get(int index) const {
        if (index < 0 || index >= (int)outs_.size()) return nullptr;
        return &outs_[(size_t)index];
    }

    const std::vector<int>& full() const { return full_; }
    const std::vector<int>& cheap() const { return cheap_; }
    int drawn() const { return (int)full_.size() + (int)cheap_.size(); }
    int considered() const { return (int)outs_.size(); }

    static int scoreOf(const PartIn& p, float distance, float screenPx, float maxD) {
        int s = 0;
        if (p.light) s += 80;
        if (p.script) s += 40;
        if (!p.anchored) s += 25;
        if (screenPx >= 24.f) s += 30;
        else if (screenPx >= 8.f) s += 15;
        else if (screenPx < 2.f) s -= 20;
        float norm = maxD > 1.f ? distance / maxD : 1.f;
        if (norm < 0.15f) s += 20;
        else if (norm > 0.75f) s -= 15;
        return s;
    }

private:
    void dropOccluded(const Camera& cam, const PartIn* parts) {
        std::vector<OccludeIn> items(ranks_.size());
        std::vector<char> hidden(ranks_.size(), 0);
        for (size_t i = 0; i < ranks_.size(); ++i) {
            const PartIn& p = parts[ranks_[i].index];
            OccludeIn& it = items[i];
            it.x = p.x; it.y = p.y; it.z = p.z;
            it.radius = 0.5f * std::sqrt(p.hx * p.hx + p.hy * p.hy + p.hz * p.hz);
            it.screenPx = outs_[(size_t)ranks_[i].index].screenPx;
            it.opaque = !p.transparent;
            it.important = p.light || p.script || !p.anchored;
            it.index = ranks_[i].index;
        }
        markOccluded(cam, items.data(), (int)items.size(), hidden.data(), (int)hidden.size());
        size_t w = 0;
        for (size_t i = 0; i < ranks_.size(); ++i) {
            if (hidden[i]) {
                outs_[(size_t)ranks_[i].index].draw = DrawKind::Skip;
                continue;
            }
            ranks_[w++] = ranks_[i];
        }
        ranks_.resize(w);
    }

    struct Rank {
        int index;
        float d2;
        float px;
        int score;
        bool transparent;
        bool important;
    };

    std::vector<PartOut> outs_;
    std::vector<int> full_;
    std::vector<int> cheap_;
    std::vector<Rank> ranks_;
};

}
