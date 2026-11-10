#pragma once

// Quali particelle tenere quando il tetto scende. Si buttano prima quelle
// gia' quasi morte e quelle che a schermo sono un puntino: un fumo grande
// vicino al giocatore resta, le scintille in fondo alla mappa no.

#include "Frustum.h"

#include <algorithm>
#include <vector>

namespace antilag {

struct PuffIn {
    float x = 0.f, y = 0.f, z = 0.f;
    float life = 0.f;
    float maxLife = 1.f;
    float size = 0.2f;
    int index = -1;
};

inline float puffScore(const PuffIn& p, const Camera& cam) {
    if (p.life <= 0.f || p.maxLife <= 0.01f) return -1.f;
    float frac = p.life / p.maxLife;
    if (frac < 0.f) frac = 0.f;
    if (frac > 1.f) frac = 1.f;
    float radius = p.size > 0.02f ? p.size : 0.02f;
    float px = screenRadiusPx(cam.eye, cam.fovyRad, cam.viewportH, v3(p.x, p.y, p.z), radius);
    if (px < 0.35f && frac < 0.9f) return 0.02f * frac;
    return px * (0.35f + 0.65f * frac);
}

// Scrive in out gli indici da TENERE, i migliori prima. Ritorna quanti.
inline int rankParticles(const PuffIn* puffs, int count, const Camera& cam,
                         int keep, int* out, int outCap) {
    if (!out || outCap <= 0 || keep <= 0 || !puffs || count <= 0) return 0;
    if (keep > outCap) keep = outCap;
    struct Scored { int index; float score; };
    std::vector<Scored> all;
    all.reserve((size_t)count);
    for (int i = 0; i < count; ++i) {
        float s = puffScore(puffs[i], cam);
        int id = puffs[i].index >= 0 ? puffs[i].index : i;
        if (s < 0.f) continue;
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
