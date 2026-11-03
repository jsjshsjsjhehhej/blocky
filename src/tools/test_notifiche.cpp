#include "systems/Notifiche.h"
#include <cstdio>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    std::printf("=== Notifiche ===\n");
    Notifiche n;
    n.reset();
    n.push("ciao", 1.f);
    check(n.items().size() == 1, "push");
    n.update(2.f);
    check(n.items().empty(), "scade");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
