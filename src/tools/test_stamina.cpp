#include "systems/Stamina.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Stamina ===\n");
    Stamina s;
    s.reset();
    check(s.add("p", 100.f), "add");
    check(s.use("p", 40.f) && s.value("p") == 60.f, "use");
    check(!s.use("p", 70.f), "non basta");
    s.update(0.3f);
    check(s.value("p") == 60.f, "delay");
    s.update(2.f);
    check(s.value("p") > 60.f, "regen");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
