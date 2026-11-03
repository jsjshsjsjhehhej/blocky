#include "systems/Dialoghi.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Dialoghi ===\n");
    Dialoghi d;
    d.reset();
    check(d.addNode("a", "NPC", "Ciao") && d.addNode("b", "NPC", "Ciao 2"), "nodi");
    check(d.link("a", "b") && d.start("a", "Intro"), "start");
    check(d.next() && d.current() && d.current()->id == "b", "next");
    check(d.next() && !d.open(), "fine chiude");
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
