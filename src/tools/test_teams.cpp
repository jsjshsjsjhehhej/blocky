#include "systems/Teams.h"
#include <cstdio>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    std::printf("=== Teams ===\n");
    Teams t;
    t.reset();
    check(t.create("Rosso", 1, 0.2f, 0.2f), "create");
    check(t.join("Rosso") && t.mine() == "Rosso", "join");
    t.leave();
    check(t.mine().empty(), "leave");
    check(!t.create("Rosso", 1, 0, 0), "no duplicati");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
