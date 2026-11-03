#include "systems/Tween.h"
#include <cmath>
#include <cstdio>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    std::printf("=== Tween ===\n");
    Tween t;
    t.reset();
    int id = t.number(0.f, 10.f, 1.f, Tween::Ease::Linear);
    t.update(0.5f);
    check(std::fabs(t.value(id) - 5.f) < 0.05f, "linear a meta' e' 5");
    t.update(0.5f);
    check(t.isDone(id) && std::fabs(t.value(id) - 10.f) < 0.05f, "fine al valore target");
    check(Tween::ease(Tween::Ease::QuadIn, 0.5f) == 0.25f, "quad in 0.5 -> 0.25");
    int v = t.vector(0, 0, 0, 2, 4, 6, 1.f, Tween::Ease::Linear);
    t.update(0.5f);
    float x, y, z;
    t.value3(v, x, y, z);
    check(std::fabs(x - 1.f) < 0.1f && std::fabs(z - 3.f) < 0.1f, "vector interpola");
    check(t.cancel(id), "cancel toglie il job");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
