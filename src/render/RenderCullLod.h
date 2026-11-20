#pragma once

#include "../platform/QualityProfile.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

/** Statistiche culling/LOD del frame (solo diagnostica). */
struct RenderCullStats {
    int partsTotal = 0;
    int partsDistCulled = 0;
    int partsFrustumCulled = 0;
    int partsOcclusionCulled = 0;
    int partsSmallCulled = 0;
    int partsBudgetCulled = 0;
    int partsDrawn = 0;
    int partsStudLod = 0;
    int partsShapeLod = 0;
    int occlusionOccluders = 0;
    int modelsTotal = 0;
    int modelsCulled = 0;
    int modelsDrawn = 0;

    void reset() { *this = {}; }
};

inline RenderCullStats g_renderCullStats;

struct FrustumPlanes { glm::vec4 p[6]; };

inline FrustumPlanes extractFrustum(const glm::mat4& viewProj) {
    FrustumPlanes f;
    glm::vec4 r0{viewProj[0][0], viewProj[1][0], viewProj[2][0], viewProj[3][0]};
    glm::vec4 r1{viewProj[0][1], viewProj[1][1], viewProj[2][1], viewProj[3][1]};
    glm::vec4 r2{viewProj[0][2], viewProj[1][2], viewProj[2][2], viewProj[3][2]};
    glm::vec4 r3{viewProj[0][3], viewProj[1][3], viewProj[2][3], viewProj[3][3]};
    f.p[0] = r3 + r0;
    f.p[1] = r3 - r0;
    f.p[2] = r3 + r1;
    f.p[3] = r3 - r1;
    f.p[4] = r3 + r2;
    f.p[5] = r3 - r2;
    for (auto& pl : f.p) {
        float len = glm::length(glm::vec3(pl));
        if (len > 1e-6f) pl /= len;
    }
    return f;
}

inline bool sphereOutsideFrustum(const FrustumPlanes& f, glm::vec3 center, float radius) {
    for (const auto& pl : f.p)
        if (glm::dot(glm::vec3(pl), center) + pl.w < -radius) return true;
    return false;
}

enum class PartMeshLod : uint8_t { High, Low };

struct RenderLodDistances {
    float studOff = 1e9f;    // oltre: niente texture stud
    float shapeLow = 1e9f;   // oltre: mesh sfera/cilindro low-poly
    float smallCull = 48.f;  // oltre: part molto piccole nascoste
    float smallMaxSize = 0.f; // 0 = disattivo
};

inline bool lodAutoEnabled(const QualityProfile& q) {
    return q.autoLod || q.partLod;
}

inline RenderLodDistances lodDistancesFor(const QualityProfile& q) {
    RenderLodDistances d;
    const float md = std::max(32.f, q.maxDrawDistance);
    if (lodAutoEnabled(q)) {
        d.studOff = md * (q.mobile ? 0.34f : 0.44f);
        if (q.studLowDetail) d.studOff *= 0.5f;   // Low: stud semplici e solo da vicino
        d.shapeLow = md * (q.mobile ? 0.50f : 0.65f);
    }
    if (q.autoLod || q.cullSmallParts) {
        d.smallMaxSize = 0.85f;
        d.smallCull = std::max(24.f, md * 0.16f);
    }
    return d;
}

inline float partBoundRadius(const glm::vec3& size) {
    return glm::length(size) * 0.5f;
}

inline float modelBoundRadius(const glm::vec3& scale) {
    return std::max(2.f, glm::length(scale) * 2.5f);
}

/** Punteggio per budget cull: alto = da tenere (vicino e/o grande). */
inline float partKeepScore(float d2, const glm::vec3& size) {
    const float vol = std::max(0.05f, size.x * size.y * size.z);
    const float extent = std::max(size.x, std::max(size.y, size.z));
    return (vol * 0.15f + extent * extent) / (d2 + 4.f);
}

inline float modelKeepScore(float d2, const glm::vec3& scale) {
    const float r = modelBoundRadius(scale);
    return (r * r) / (d2 + 4.f);
}

/**
 * Soft Z a bassa risoluzione (CPU): i blocchi opachi grandi fanno da occluder,
 * le part dietro spariscono. Pesa poco e gira anche su mobile senza query GL.
 */
struct SoftOcclusionBuffer {
    static constexpr int kW = 72;
    static constexpr int kH = 40;
    float z[kW * kH]{};

    void clear() {
        std::fill(z, z + kW * kH, 1.f); // NDC far
    }

    static bool projectAabb(const glm::mat4& viewProj, const glm::vec3& center,
                            const glm::vec3& half, int& outMinX, int& outMinY,
                            int& outMaxX, int& outMaxY, float& outNearZ) {
        float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
        float nearZ = 1.f;
        int front = 0;
        for (int ix = 0; ix < 2; ++ix)
            for (int iy = 0; iy < 2; ++iy)
                for (int iz = 0; iz < 2; ++iz) {
                    glm::vec3 corner = center + glm::vec3(
                        ix ? half.x : -half.x,
                        iy ? half.y : -half.y,
                        iz ? half.z : -half.z);
                    glm::vec4 clip = viewProj * glm::vec4(corner, 1.f);
                    if (clip.w <= 1e-4f) continue;
                    glm::vec3 ndc = glm::vec3(clip) / clip.w;
                    if (ndc.z < -1.f || ndc.z > 1.f) continue;
                    ++front;
                    float sx = (ndc.x * 0.5f + 0.5f) * (float)kW;
                    float sy = (1.f - (ndc.y * 0.5f + 0.5f)) * (float)kH;
                    minX = std::min(minX, sx);
                    maxX = std::max(maxX, sx);
                    minY = std::min(minY, sy);
                    maxY = std::max(maxY, sy);
                    nearZ = std::min(nearZ, ndc.z);
                }
        if (front < 2) return false;
        outMinX = std::max(0, (int)std::floor(minX));
        outMinY = std::max(0, (int)std::floor(minY));
        outMaxX = std::min(kW - 1, (int)std::ceil(maxX));
        outMaxY = std::min(kH - 1, (int)std::ceil(maxY));
        if (outMinX > outMaxX || outMinY > outMaxY) return false;
        outNearZ = nearZ;
        return true;
    }

    void rasterizeOccluder(const glm::mat4& viewProj, const glm::vec3& center,
                           const glm::vec3& half) {
        int x0, y0, x1, y1;
        float nearZ;
        if (!projectAabb(viewProj, center, half, x0, y0, x1, y1, nearZ))
            return;
        // Conservativo: un filo piu' lontano cosi' non "mangia" i bordi.
        const float writeZ = std::min(1.f, nearZ + 0.02f);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                float& d = z[y * kW + x];
                if (writeZ < d) d = writeZ;
            }
    }

    /** true = nascosta dietro occluder (tutta la footprint e' occlusa). */
    bool isFullyOccluded(const glm::mat4& viewProj, const glm::vec3& center,
                         const glm::vec3& half) const {
        int x0, y0, x1, y1;
        float nearZ;
        if (!projectAabb(viewProj, center, half, x0, y0, x1, y1, nearZ))
            return false;
        // Oggetti molto vicini: non occludere (evita pop sullo schermo).
        if (nearZ < -0.92f) return false;
        const int span = (x1 - x0 + 1) * (y1 - y0 + 1);
        if (span <= 1) return false;
        // Sample a passo 1 o 2 se rettangolo grande.
        const int step = (span > 48) ? 2 : 1;
        int tested = 0;
        for (int y = y0; y <= y1; y += step)
            for (int x = x0; x <= x1; x += step) {
                ++tested;
                if (z[y * kW + x] >= nearZ - 0.008f)
                    return false; // almeno un pixel non coperto / non davanti
            }
        return tested > 0;
    }
};

inline SoftOcclusionBuffer& softOcclusionBuffer() {
    static SoftOcclusionBuffer buf;
    return buf;
}

/**
 * Costruisce soft-Z dagli occluder (part opache grosse) e segna in outMask
 * le candidate occluse. outMask[i]=1 → cull. Ritorna quanti occluder usati.
 */
template<typename PartVec, typename CandVec>
inline int buildAndTestOcclusion(const PartVec& parts, const CandVec& cands,
                                 const glm::mat4& viewProj,
                                 float minOccluderSize, int maxOccluders,
                                 std::vector<uint8_t>& outMask) {
    outMask.assign(cands.size(), 0);
    SoftOcclusionBuffer& buf = softOcclusionBuffer();
    buf.clear();

    struct Occ { int candIdx; float d2; float maxS; };
    static std::vector<Occ> occs;
    occs.clear();
    for (int ci = 0; ci < (int)cands.size(); ++ci) {
        const auto& c = cands[ci];
        if (c.transparent) continue;
        const auto& p = parts[c.i];
        float maxS = std::max(p.size.x, std::max(p.size.y, p.size.z));
        if (maxS < minOccluderSize) continue;
        // Solo blocchi "pieni": vetro/ice/forcefield non occludono.
        if (p.isTransparent()) continue;
        occs.push_back({ci, c.d2, maxS});
    }
    std::sort(occs.begin(), occs.end(), [](const Occ& a, const Occ& b) {
        if (a.d2 != b.d2) return a.d2 < b.d2;
        return a.maxS > b.maxS;
    });
    if ((int)occs.size() > maxOccluders)
        occs.resize(maxOccluders);

    static std::vector<uint8_t> isOcc;
    isOcc.assign(cands.size(), 0);
    for (const Occ& o : occs) {
        isOcc[(size_t)o.candIdx] = 1;
        const auto& p = parts[cands[o.candIdx].i];
        buf.rasterizeOccluder(viewProj, p.position, p.size * 0.5f);
    }

    int culled = 0;
    for (int ci = 0; ci < (int)cands.size(); ++ci) {
        const auto& c = cands[ci];
        if (c.transparent || isOcc[(size_t)ci]) continue;

        const auto& p = parts[c.i];
        if (buf.isFullyOccluded(viewProj, p.position, p.size * 0.5f)) {
            outMask[ci] = 1;
            ++culled;
        }
    }
    g_renderCullStats.occlusionOccluders = (int)occs.size();
    return culled;
}

/** Sceglie fino a maxN modelli visibili (priorita' distanza×scala + frustum). */
template<typename ModelVec>
inline void selectVisibleModels(const ModelVec& models, glm::vec3 origin,
                                const glm::mat4& view, const glm::mat4& proj,
                                const QualityProfile& quality,
                                std::vector<int>& outIndices,
                                int selectedModel = -1) {
    outIndices.clear();
    const float maxD = quality.maxDrawDistance;
    const float maxD2 = maxD * maxD;
    const int maxN = std::max(1, quality.maxVisibleModels);
    const bool useFrustum = quality.autoLod || quality.frustumCull;
    const FrustumPlanes frustum = extractFrustum(proj * view);
    // Modelli minuscoli lontanissimi: skip prima del budget (overload-friendly).
    const float tinyFar2 = (maxD * (quality.mobile ? 0.42f : 0.55f));
    const float tinyFar2sq = tinyFar2 * tinyFar2;

    struct Cand { int i; float d2; float score; };
    static std::vector<Cand> cands;
    cands.clear();
    cands.reserve(models.size());

    for (int i = 0; i < (int)models.size(); ++i) {
        const auto& m = models[i];
        glm::vec3 d = m.position - origin;
        float d2 = glm::dot(d, d);
        if (d2 > maxD2) continue;
        float r = modelBoundRadius(m.scale);
        if (i != selectedModel && r < 1.25f && d2 > tinyFar2sq) continue;
        if (useFrustum && sphereOutsideFrustum(frustum, m.position, r)) continue;
        float score = modelKeepScore(d2, m.scale);
        if (i == selectedModel) score *= 1.0e6f;
        cands.push_back({i, d2, score});
    }

    g_renderCullStats.modelsTotal = (int)models.size();

    if ((int)cands.size() > maxN) {
        std::nth_element(cands.begin(), cands.begin() + maxN, cands.end(),
            [](const Cand& a, const Cand& b) { return a.score > b.score; });
        cands.resize(maxN);
    }
    // Vicini prima: cosi' il tetto vertici in draw conserva i modelli importanti.
    std::sort(cands.begin(), cands.end(),
              [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });

    outIndices.reserve(cands.size());
    for (const auto& c : cands)
        outIndices.push_back(c.i);
    g_renderCullStats.modelsDrawn = (int)outIndices.size();
    g_renderCullStats.modelsCulled = g_renderCullStats.modelsTotal - g_renderCullStats.modelsDrawn;
}
