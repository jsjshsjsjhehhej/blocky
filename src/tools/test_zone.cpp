#include "systems/Zone.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Zone ===\n");
    Zone z;
    z.reset();
    check(z.add("casa", 0, 0, 0, 2, 2, 2), "add");
    z.scan(0, 0, 0);
    check(z.contains("casa") && z.entered().size() == 1, "entra");
    z.scan(20, 0, 0);
    check(!z.contains("casa") && z.exited().size() == 1, "esce");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
