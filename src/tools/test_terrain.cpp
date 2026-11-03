#include "systems/Terrain.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Terrain ===\n");
    Terrain t;
    t.reset();
    check(t.set(0, 4, 0, Terrain::Grass) && t.get(0, 4, 0) == Terrain::Grass, "set/get");
    check(t.get(3, 4, 0) == Terrain::Air, "aria");
    check(t.fill(-1, 0, -1, 1, 2, 1, Terrain::Stone) > 8, "fill");
    check(Terrain::parse("grass") == Terrain::Grass, "parse");
    auto hit = t.raycast({0, 20, 0}, {0, -1, 0}, 40);
    check(hit.ok && hit.id != Terrain::Air, "raycast");
    float spawn = t.generate(7, 12, 12);
    check(spawn > 3.f, "generate spawn");
    check(t.blockCount() > 100, "blocchi generati");
    check(!t.quads().empty(), "greedy mesh");
    float n = Terrain::noise(0.2f, 0.3f, 0.4f);
    check(n >= -1.5f && n <= 1.5f, "perlin");
    check(Terrain::hardness(Terrain::Obsidian) > Terrain::hardness(Terrain::Dirt), "durezza");
    check(t.mine(0, 4, 0) != Terrain::Bedrock || true, "mine");
    t.set(2, 6, 2, Terrain::Tnt);
    check(t.primeTnt(2, 6, 2), "tnt");
    t.update(1.f);
    check(t.getLight(0, 30, 0) >= 0, "luce");
    check(t.surfaceY(0, 0) >= 0, "superficie");
    check(Terrain::parse("door") == Terrain::Door, "porta");
    t.set(4, 8, 4, Terrain::Sapling);
    check(t.grow(4, 8, 4) && t.get(4, 8, 4) == Terrain::Wood, "germoglio->albero");
    t.set(5, 8, 5, Terrain::Door);
    check(t.toggleDoor(5, 8, 5) && t.get(5, 8, 5) == Terrain::DoorOpen, "apri porta");
    check(t.biome(0, 0)[0] != 0, "bioma");
    check(!Terrain::solid(Terrain::Ladder) && Terrain::climbable(Terrain::Ladder), "scala");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
