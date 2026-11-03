#include "systems/Inventario.h"
#include <cstdio>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    std::printf("=== Inventario ===\n");
    Inventario inv;
    inv.reset();
    check(inv.add("mela", "Mela", 3) == 3, "add 3 mele");
    check(inv.count("mela") == 3 && inv.has("mela", 2), "count/has");
    check(inv.remove("mela", 1) == 1 && inv.count("mela") == 2, "remove 1");
    check(!inv.validId("") && !inv.validId("spazio no"), "id invalidi");
    check(inv.add("mela", "Mela", 200) <= Inventario::kMaxStack * Inventario::kMaxSlots, "cap stack");
    inv.select(0);
    check(inv.selected() == 0 && inv.selectedId()[0] != 0, "hotbar");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
