#include "systems/Combo.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Combo ===\n");
    Combo c;
    c.reset();
    c.setTimeout(1.f);
    check(c.hit() == 1 && c.hit() == 2, "hit");
    c.update(0.5f);
    check(c.count() == 2, "tiene");
    c.update(1.f);
    check(c.count() == 0, "scade");
    check(c.best() == 2, "best");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
