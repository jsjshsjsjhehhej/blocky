#include "systems/Salute.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Salute ===\n");
    Salute s;
    s.reset();
    check(s.add("player", 100.f), "add");
    check(s.hp("player") == 100.f, "full");
    check(s.damage("player", 40.f) && s.hp("player") == 60.f, "danno");
    check(s.heal("player", 10.f) && s.hp("player") == 70.f, "cura");
    check(s.damage("player", 999.f) && s.dead("player"), "morte");
    check(!s.heal("player", 10.f), "niente cura da morto");
    check(s.revive("player", 50.f) && !s.dead("player") && s.hp("player") == 50.f, "revive");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
