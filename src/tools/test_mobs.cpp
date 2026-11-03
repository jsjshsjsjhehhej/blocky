#include "systems/Terrain.h"
#include "systems/Mobs.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Mobs ===\n");
    Terrain t;
    t.reset();
    t.set(0, 0, 0, Terrain::Stone);
    Mobs m;
    m.reset();
    m.attach(&t);
    check(m.spawn("pig", 0, 4, 0), "spawn pig");
    check(m.count() == 1, "count");
    m.setPlayer(0, 4, 8, true);
    m.update(0.05f);
    check(m.count() >= 1, "vive");
    check(m.hurt(m.all()[0].name, 99.f), "hurt");
    m.update(0.05f);
    check(m.count() >= 1, "drop item");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
