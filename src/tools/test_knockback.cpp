#include "systems/Knockback.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Knockback ===\n");
    Knockback k;
    k.reset();
    check(k.add("p", 10.f, 0.f, 0.f), "add");
    check(k.vx("p") > 9.f, "impulso");
    k.update(0.5f);
    check(k.vx("p") < 10.f && k.vx("p") > 0.f, "decade");
    check(k.fromHit("e", 0, 0, 0, 1, 0, 0, 8.f) && k.vx("e") > 0.f, "fromHit");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
