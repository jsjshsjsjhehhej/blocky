#include "systems/Badge.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w); if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Badge ===\n");
    Badge b; b.reset();
    check(b.award("primo") && b.has("primo") && !b.award("primo"), "award una volta");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
