#include "systems/Mondo.h"
#include "systems/Terrain.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Mondo ===\n");
    Mondo m;
    Terrain t;
    m.reset();
    m.attach(&t);
    check(m.setName("alpha") && m.name() == "alpha", "nome");
    m.setSpawn(3, 12, -2);
    check(m.spawnX() == 3.f && m.spawnZ() == -2.f, "spawn");
    check(m.sleep() && m.day() == 2 && m.takeSleep(), "sonno");
    check(!m.takeSleep(), "sonno consumato");
    t.generate(9, 10, 10);
    check(m.seed() == 9, "seed dal terrain");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
