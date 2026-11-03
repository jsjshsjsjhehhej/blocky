#include "systems/Fame.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Fame ===\n");
    Fame f;
    f.reset();
    check(f.hunger() == 20.f, "piena");
    check(f.isFood("bread") && f.foodValue("apple") == 4, "cibo");
    f.set(10.f);
    check(f.eat("bread") && f.hunger() > 10.f, "mangia pane");
    // Frame veri (update limita ogni passo a 0.1 s) e abbastanza tempo per
    // consumare prima la saturazione lasciata dal pane.
    f.set(0.f);
    float starveDmg = 0.f;
    for (int i = 0; i < 4000 && starveDmg <= 0.f; ++i) {
        f.update(0.05f);
        if (f.starving()) starveDmg += f.takeStarveDamage();
    }
    check(f.starving() && starveDmg > 0.f, "danno fame");
    check(!f.eat("cobble"), "non cibo");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
