#include "systems/Debris.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Debris ===\n");
    Debris d;
    d.reset();
    check(d.add("Palla", 0.2f) && d.pending() == 1, "add");
    d.update(0.1f);
    check(d.takeExpired().empty(), "non ancora");
    d.update(0.2f);
    check(d.takeExpired() == "Palla" && d.pending() == 0, "scade");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
