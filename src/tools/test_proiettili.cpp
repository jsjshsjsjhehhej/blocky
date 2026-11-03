#include "systems/Proiettili.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Proiettili ===\n");
    Proiettili p;
    p.reset();
    check(p.spawn("shot", 0, 0, 0, 10, 0, 0, 2.f, 0.3f), "spawn");
    p.update(0.5f);
    float x = 0, y = 0, z = 0;
    check(p.pos("shot", x, y, z) && x > 4.9f && x < 5.1f, "volo");
    check(!p.hits(5.f, 0, 0, 0.5f).empty(), "hit vicino");
    check(p.hits(50.f, 0, 0, 0.5f).empty(), "niente lontano");
    p.update(2.f);
    check(p.count() == 0, "scade");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
