#include "systems/Tag.h"
#include <cstdio>

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

int main() {
    std::printf("=== Tag ===\n");
    Tag t;
    t.reset();
    check(t.add("porta", 3) && t.has("porta", 3), "add/has");
    check(t.get("porta").size() == 1, "get");
    check(t.remove("porta", 3) && !t.has("porta", 3), "remove");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
