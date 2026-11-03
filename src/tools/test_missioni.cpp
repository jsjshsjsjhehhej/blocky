#include "systems/Missioni.h"
#include <cstdio>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    std::printf("=== Missioni ===\n");
    Missioni m;
    m.reset();
    check(m.start("intro", "Benvenuto", 10), "start");
    check(m.addStep("intro", "parla", "Parla al NPC"), "add step");
    check(m.advance("intro", "parla") && m.get("intro")->completed, "un passo completa");
    check(!m.start("intro", "x", 0), "id duplicato");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
