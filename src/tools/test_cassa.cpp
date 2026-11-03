#include "systems/Cassa.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Cassa ===\n");
    Cassa c;
    c.reset();
    check(c.open(1, 2, 3), "open");
    check(c.put(1, 2, 3, "dirt", "Dirt", 10) == 10, "put 10");
    check(c.count(1, 2, 3, "dirt") == 10, "count");
    check(c.take(1, 2, 3, "dirt", 4) == 4 && c.count(1, 2, 3, "dirt") == 6, "take 4");
    check(c.chestCount() == 1, "1 cassa");
    check(c.close(1, 2, 3) && c.chestCount() == 0, "close");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
