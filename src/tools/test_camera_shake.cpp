#include "systems/CameraShake.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== CameraShake ===\n");
    CameraShake c;
    c.reset();
    check(c.trauma() == 0.f, "zero");
    c.add(0.5f);
    check(c.trauma() > 0.49f && c.trauma() < 0.51f, "add");
    c.add(2.f);
    check(c.trauma() == 1.f, "clamp");
    c.update(0.2f);
    check(c.trauma() < 1.f, "decade");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
