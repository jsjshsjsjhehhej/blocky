#include "systems/Cooldown.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Cooldown ===\n");
    Cooldown c;
    c.reset();
    check(c.ready("dash"), "pronto di default");
    check(c.start("dash", 1.f) && !c.ready("dash"), "start");
    c.update(0.4f);
    check(c.left("dash") > 0.5f, "ancora in cooldown");
    c.update(0.7f);
    check(c.ready("dash"), "scade");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
