// Prova della griglia spaziale per le mappe open world (render/PartSpatialGrid.h):
// la griglia deve trovare TUTTE le part vicine che trova il controllo una per una,
// e guardarne molte meno.

#include "render/PartSpatialGrid.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <set>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("[grid] %s %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    // Una mappa grande: 60.000 part ancorate su 4 km x 4 km, qualche cassa libera, un pavimento enorme.
    std::vector<WorkspacePart> parts;
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> pos(-2000.f, 2000.f), sz(1.f, 12.f);
    for (int i = 0; i < 60000; ++i) {
        WorkspacePart p;
        p.position = {pos(rng), 2.f, pos(rng)};
        p.size = {sz(rng), sz(rng), sz(rng)};
        p.anchored = true;
        parts.push_back(p);
    }
    for (int i = 0; i < 200; ++i) {
        WorkspacePart p;
        p.position = {pos(rng), 5.f, pos(rng)};
        p.size = {2, 2, 2};
        p.anchored = false;   // si muove: sempre controllata
        parts.push_back(p);
    }
    WorkspacePart floor;
    floor.position = {0, 0, 0};
    floor.size = {4000, 1, 4000};
    floor.anchored = true;   // enorme: sempre controllata
    parts.push_back(floor);

    blk::PartSpatialGrid grid;
    grid.build(parts);
    check(grid.alwaysCount() == 201, "le part libere e quelle enormi sono nella lista \"sempre\"");

    const glm::vec3 cam{150.f, 10.f, -320.f};
    const float radius = 200.f;
    std::set<int> brute, viaGrid;
    for (int i = 0; i < (int)parts.size(); ++i) {
        const glm::vec2 d(parts[(size_t)i].position.x - cam.x, parts[(size_t)i].position.z - cam.z);
        if (glm::length(d) <= radius) brute.insert(i);
    }
    size_t visited = 0;
    grid.query(cam, radius, [&](int i) {
        ++visited;
        const glm::vec2 d(parts[(size_t)i].position.x - cam.x, parts[(size_t)i].position.z - cam.z);
        if (glm::length(d) <= radius) viaGrid.insert(i);
    });
    bool same = true;
    for (int i : brute) same = same && viaGrid.count(i);
    check(same && viaGrid.size() == brute.size(), "la griglia trova esattamente le stesse part vicine del controllo una per una");
    std::printf("[grid]      vicine %zu, guardate %zu su %zu part (%.1f%%)\n", brute.size(), visited, parts.size(),
                100.0 * (double)visited / (double)parts.size());
    check(visited < parts.size() / 10, "guarda meno di un decimo delle part");

    // Tempo: 100 ricerche con la griglia contro 100 giri su tutte le part.
    auto t0 = std::chrono::steady_clock::now();
    size_t sink = 0;
    for (int r = 0; r < 100; ++r) grid.query(cam, radius, [&](int i) { sink += (size_t)i; });
    auto t1 = std::chrono::steady_clock::now();
    for (int r = 0; r < 100; ++r)
        for (int i = 0; i < (int)parts.size(); ++i) {
            const glm::vec3 d = parts[(size_t)i].position - cam;
            if (glm::dot(d, d) <= radius * radius) sink += (size_t)i;
        }
    auto t2 = std::chrono::steady_clock::now();
    const double gridMs = std::chrono::duration<double, std::milli>(t1 - t0).count() / 100.0;
    const double bruteMs = std::chrono::duration<double, std::milli>(t2 - t1).count() / 100.0;
    std::printf("[grid]      per frame: griglia %.3f ms, tutte le part %.3f ms (%zu)\n", gridMs, bruteMs, sink % 7);
    check(gridMs < bruteMs, "la griglia e' piu' veloce del giro su tutte le part");

    std::printf("[grid] %s\n", g_fail ? "CI SONO ERRORI" : "tutto OK");
    return g_fail ? 1 : 0;
}
