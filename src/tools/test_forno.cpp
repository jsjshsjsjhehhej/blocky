#include "systems/Forno.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Forno ===\n");
    Forno f;
    f.reset();
    check(f.add("base"), "add");
    check(f.put("base", "wood", true), "fuel");
    check(f.put("base", "sand", false), "input");
    f.update(10.f);
    check(f.take("base") == "glass", "smelt sand->glass");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
