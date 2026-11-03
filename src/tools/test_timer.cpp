#include "systems/Timer.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w); if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Timer ===\n");
    Timer t; t.reset();
    check(t.start("boom", 0.5f, false), "start");
    t.update(0.2f);
    check(!t.done("boom"), "non ancora");
    t.update(0.4f);
    check(t.done("boom"), "scatta");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
