#include "systems/Crafting.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Crafting ===\n");
    Crafting c;
    Inventario inv;
    inv.reset();
    c.reset();
    c.attach(&inv);
    check(c.count() >= 20, "ricette alpha");
    check(c.get("woodpick") && c.get("planks"), "piccone e tavole");
    check(inv.add("wood", "Wood", 2) == 2, "legno");
    check(c.tryCraft("planks") && inv.count("planks") == 4, "craft tavole");
    check(c.addRecipe("pane", "pane", "Pane", 1), "recipe extra");
    check(c.addIngredient("pane", "farina", 2) && c.addIngredient("pane", "acqua", 1), "ing");
    check(c.get("pane") && c.get("pane")->inCount == 2, "2 ingredienti");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
