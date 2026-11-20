#pragma once

/**
 * Griglia spaziale delle Part per le mappe grandi (open world).
 *
 * Senza, ogni frame il disegno passava TUTTE le part del mondo (distanza,
 * inquadratura...): con 50.000 part sono 50.000 controlli anche se ne vedi 800.
 * Qui le part ancorate stanno in celle quadrate sul piano XZ; si guardano solo
 * le celle attorno alla camera. Le part libere (si muovono ogni frame) e quelle
 * enormi (coprono tante celle) restano in una lista a parte, sempre controllata.
 *
 * Nessuna dipendenza grafica: la usa WorldRenderPipeline.h e la prova
 * tools/test_spatial_grid.cpp.
 */

#include "engine/DataModel.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace blk {

class PartSpatialGrid {
public:
    float cell = 32.f;   // lato della cella, in stud

    /** Rimette ogni part nella sua cella (dalla posizione di adesso). */
    void build(const std::vector<WorkspacePart>& parts) {
        cells_.clear();
        always_.clear();
        const float big = cell * 4.f;
        for (int i = 0; i < (int)parts.size(); ++i) {
            const WorkspacePart& p = parts[(size_t)i];
            const float maxS = std::max(p.size.x, std::max(p.size.y, p.size.z));
            if (!p.anchored || p.netDriven || maxS > big) {
                always_.push_back(i);   // si muove, o e' troppo grande per una cella sola
                continue;
            }
            cells_[key(coord(p.position.x), coord(p.position.z))].push_back(i);
        }
        builtCount_ = parts.size();
    }

    size_t builtCount() const { return builtCount_; }
    size_t cellCount() const { return cells_.size(); }
    size_t alwaysCount() const { return always_.size(); }

    /**
     * Chiama fn(i) per le part che POSSONO stare entro `radius` da `center`
     * (sul piano XZ): quelle delle celle vicine piu' la lista "sempre".
     * Il controllo preciso (distanza, inquadratura) lo fa chi chiama.
     */
    template <class F>
    void query(const glm::vec3& center, float radius, F&& fn) const {
        const int x0 = coord(center.x - radius - cell), x1 = coord(center.x + radius + cell);
        const int z0 = coord(center.z - radius - cell), z1 = coord(center.z + radius + cell);
        // Raggio enorme rispetto alle celle: conviene passare le celle che ci sono.
        if ((int64_t)(x1 - x0 + 1) * (z1 - z0 + 1) > (int64_t)cells_.size() * 2) {
            for (const auto& kv : cells_)
                for (int i : kv.second) fn(i);
        } else {
            for (int x = x0; x <= x1; ++x)
                for (int z = z0; z <= z1; ++z) {
                    auto it = cells_.find(key(x, z));
                    if (it == cells_.end()) continue;
                    for (int i : it->second) fn(i);
                }
        }
        for (int i : always_) fn(i);
    }

private:
    std::unordered_map<int64_t, std::vector<int>> cells_;
    std::vector<int> always_;
    size_t builtCount_ = 0;

    int coord(float v) const { return (int)std::floor(v / cell); }
    static int64_t key(int x, int z) { return ((int64_t)x << 32) ^ (int64_t)(uint32_t)z; }
};

} // namespace blk
