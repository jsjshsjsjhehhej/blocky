#pragma once

// Luci e ombre condividono lo stesso problema: il renderer ne regge poche,
// e "le piu' vicine" non e' sempre la scelta giusta. Una torcia piccola
// addosso al giocatore vale piu' di un neon enorme a 80 stud, e un muro
// che copre meta' schermo vale piu' di un cubetto vicino per l'ombra.

#include "Frustum.h"

#include <algorithm>
#include <vector>

namespace antilag {

struct CasterIn {
    float x = 0.f, y = 0.f, z = 0.f;
    float hx = 1.f, hy = 1.f, hz = 1.f;
    float brightness = 1.f;
    int index = -1;
    bool light = false;
    bool transparent = false;
};

inline float casterRadius(const CasterIn& c) {
    float r = 0.5f * std::sqrt(c.hx * c.hx + c.hy * c.hy + c.hz * c.hz);
    return r < 0.05f ? 0.05f : r;
}

inline float casterScore(const CasterIn& c, const Camera& cam, bool wantLights) {
    if (wantLights != c.light) return -1.f;
    if (!wantLights && c.transparent) return -1.f;
    float radius = casterRadius(c);
    float px = screenRadiusPx(cam.eye, cam.fovyRad, cam.viewportH, v3(c.x, c.y, c.z), radius);
    if (cam.frustum.sphereOutside(c.x, c.y, c.z, radius) && px < 8.f) return -1.f;
    float bright = c.brightness;
    if (bright < 0.05f) bright = 0.05f;
    if (bright > 4.f) bright = 4.f;
    if (wantLights) return px * bright + (px > 6.f ? 4.f : 0.f);
    return px * (0.6f + 0.4f * std::min(bright, 1.5f));
}

inline int rankCasters(const CasterIn* casters, int count, const Camera& cam,
                       bool wantLights, int keep, int* out, int outCap) {
    if (!out || outCap <= 0 || keep <= 0 || !casters || count <= 0) return 0;
    if (keep > outCap) keep = outCap;
    struct Scored { int index; float score; };
    std::vector<Scored> all;
    all.reserve((size_t)count);
    for (int i = 0; i < count; ++i) {
        float s = casterScore(casters[i], cam, wantLights);
        if (s < 0.f) continue;
        int id = casters[i].index >= 0 ? casters[i].index : i;
        all.push_back({id, s});
    }
    if ((int)all.size() > keep) {
        std::nth_element(all.begin(), all.begin() + keep, all.end(),
            [](const Scored& a, const Scored& b) { return a.score > b.score; });
        all.resize((size_t)keep);
    }
    std::sort(all.begin(), all.end(),
        [](const Scored& a, const Scored& b) { return a.score > b.score; });
    int n = 0;
    for (const Scored& s : all) {
        if (n >= outCap) break;
        out[n++] = s.index;
    }
    return n;
}

}
