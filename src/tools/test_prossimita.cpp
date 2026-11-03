#include "systems/Prossimita.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Prossimita ===\n");
    Prossimita p;
    p.reset();
    check(p.add("porta", "Apri", 0, 0, 0, 5), "add");
    p.scan(1, 0, 0);
    check(p.nearest() == "porta", "vicino");
    p.requestActivate();
    check(p.takeActivated() == "porta", "activate");
    p.scan(50, 0, 0);
    check(p.nearest().empty(), "lontano");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
