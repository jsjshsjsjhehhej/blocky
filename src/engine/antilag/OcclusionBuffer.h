#pragma once

// Occlusione a bassa risoluzione, sulla CPU, prima delle draw call.
// Non sostituisce lo z-buffer: toglie le part che stanno INTERAMENTE
// dietro a un muro gia' grande a schermo. Una griglia 64x36 e' abbastanza
// fitta da beccare un palazzo e abbastanza piccola da costare meno di
// una draw call.

#include "Frustum.h"

#include <algorithm>
#include <vector>

namespace antilag {

class OcclusionBuffer {
public:
    static constexpr int kW = 64;
    static constexpr int kH = 36;

    void clear() {
        for (float& d : depth_) d = 1e9f;
        drawn_ = 0;
        tested_ = 0;
        hidden_ = 0;
    }

    int drawn() const { return drawn_; }
    int tested() const { return tested_; }
    int hidden() const { return hidden_; }

    struct Projected {
        bool ok = false;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        float depth = 0.f;
        int bins = 0;
    };

    Projected project(const Camera& cam, V3 center, float radius) const {
        Projected r;
        if (radius < 0.05f) radius = 0.05f;
        V4 clip = transformPoint(cam.viewProj, center);
        if (clip.w < 0.08f) return r;
        float nx = clip.x / clip.w;
        float ny = clip.y / clip.w;
        float dist = len(sub(center, cam.eye));
        if (dist < 0.05f) return r;
        float vh = cam.viewportH > 2.f ? cam.viewportH : 720.f;
        float vw = vh * (16.f / 9.f);
        float px = screenRadiusPx(cam.eye, cam.fovyRad, vh, center, radius);
        if (px < 0.5f) return r;
        float cx = (nx * 0.5f + 0.5f) * (float)kW;
        float cy = (1.f - (ny * 0.5f + 0.5f)) * (float)kH;
        float rx = (px / vw) * (float)kW;
        float ry = (px / vh) * (float)kH;
        if (rx < 0.5f) rx = 0.5f;
        if (ry < 0.5f) ry = 0.5f;
        r.x0 = (int)std::floor(cx - rx);
        r.y0 = (int)std::floor(cy - ry);
        r.x1 = (int)std::ceil(cx + rx);
        r.y1 = (int)std::ceil(cy + ry);
        if (r.x0 < 0) r.x0 = 0;
        if (r.y0 < 0) r.y0 = 0;
        if (r.x1 > kW) r.x1 = kW;
        if (r.y1 > kH) r.y1 = kH;
        if (r.x0 >= r.x1 || r.y0 >= r.y1) return r;
        r.bins = (r.x1 - r.x0) * (r.y1 - r.y0);
        r.depth = dist;
        r.ok = true;
        return r;
    }

    // Scrive la profondita' solo se questo volume e' piu' vicino di cio'
    // che c'era. I volume piccoli non fanno da muro: un cartello non deve
    // nascondere un palazzo dietro per un errore di un bin.
    void drawOccluder(const Camera& cam, V3 center, float radius, float minPx) {
        Projected p = project(cam, center, radius);
        if (!p.ok) return;
        float vh = cam.viewportH > 2.f ? cam.viewportH : 720.f;
        float px = screenRadiusPx(cam.eye, cam.fovyRad, vh, center, radius);
        if (px < minPx) return;
        for (int y = p.y0; y < p.y1; ++y) {
            for (int x = p.x0; x < p.x1; ++x) {
                float& slot = depth_[(size_t)(y * kW + x)];
                if (p.depth < slot) slot = p.depth;
            }
        }
        ++drawn_;
    }

    // Frazione di bin gia' coperti da qualcosa di chiaramente piu' vicino.
    // 0 = niente davanti. 1 = tutto il rettangolo e' dietro un muro.
    float coverage(const Camera& cam, V3 center, float radius) {
        Projected p = project(cam, center, radius);
        ++tested_;
        if (!p.ok || p.bins <= 0) return 0.f;
        int covered = 0;
        for (int y = p.y0; y < p.y1; ++y) {
            for (int x = p.x0; x < p.x1; ++x) {
                float d = depth_[(size_t)(y * kW + x)];
                if (d + radius * 0.35f < p.depth) ++covered;
            }
        }
        return (float)covered / (float)p.bins;
    }

    bool occluded(const Camera& cam, V3 center, float radius, float threshold = 0.82f) {
        bool hide = coverage(cam, center, radius) >= threshold;
        if (hide) ++hidden_;
        return hide;
    }

private:
    float depth_[kW * kH];
    int drawn_ = 0;
    int tested_ = 0;
    int hidden_ = 0;
};

struct OccludeIn {
    float x = 0.f, y = 0.f, z = 0.f;
    float radius = 1.f;
    float screenPx = 0.f;
    bool opaque = true;
    bool important = false;
    int index = -1;
};

// Prima i muri grandi e vicini, poi si chiede agli altri se sono coperti.
// Gli importanti (luci, script, parti non ancorate) non si nascondono:
// spegnerli fa comparire un buco nel gameplay, non un risparmio.
inline int markOccluded(const Camera& cam, const OccludeIn* items, int count,
                        char* hidden, int hiddenCap, float minOccluderPx = 28.f) {
    if (!items || count <= 0 || !hidden || hiddenCap <= 0) return 0;
    int n = count < hiddenCap ? count : hiddenCap;
    for (int i = 0; i < n; ++i) hidden[i] = false;

    struct Order { int i; float depth; };
    std::vector<Order> order;
    order.reserve((size_t)n);
    for (int i = 0; i < n; ++i) {
        float dx = items[i].x - cam.eye.x;
        float dy = items[i].y - cam.eye.y;
        float dz = items[i].z - cam.eye.z;
        order.push_back({i, dx * dx + dy * dy + dz * dz});
    }
    std::sort(order.begin(), order.end(),
        [](const Order& a, const Order& b) { return a.depth < b.depth; });

    OcclusionBuffer buf;
    buf.clear();
    int hid = 0;
    int occluders = 0;
    for (const Order& o : order) {
        const OccludeIn& it = items[o.i];
        V3 c = v3(it.x, it.y, it.z);
        // Si testa prima di disegnare, cosi' un muro non nasconde se stesso.
        bool canHide = !it.important && it.screenPx < 90.f;
        if (canHide && buf.occluded(cam, c, it.radius)) {
            hidden[o.i] = true;
            ++hid;
            continue;
        }
        if (it.opaque && it.screenPx >= minOccluderPx && occluders < 16) {
            buf.drawOccluder(cam, c, it.radius, minOccluderPx);
            ++occluders;
        }
    }
    return hid;
}

}
