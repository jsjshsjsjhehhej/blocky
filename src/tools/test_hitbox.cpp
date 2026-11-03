#include "systems/Hitbox.h"
#include <cstdio>
static int g_fail = 0;
static void check(bool ok, const char* w) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", w);
    if (!ok) ++g_fail;
}
int main() {
    std::printf("=== Hitbox ===\n");
    Hitbox h;
    h.reset();
    check(h.addSphere("a", 0, 0, 0, 1, Hitbox::GHit), "sfera a");
    check(h.addSphere("b", 1.2f, 0, 0, 1, Hitbox::GHurt), "sfera b");
    h.setMask("a", Hitbox::GHurt);
    h.setMask("b", Hitbox::GHit);
    check(h.hits("a", "b"), "sfere si toccano");
    h.move("b", 10, 0, 0, 0);
    check(!h.hits("a", "b"), "sfere lontane");
    h.addBox("muro", 0, 0, 4, 2, 2, 0.4f, 0, Hitbox::GWorld);
    auto ray = h.ray(0, 0, 0, 0, 0, 1, 20, Hitbox::GWorld);
    check(ray.ok && ray.name == "muro", "raggio colpisce il box");
    h.addCapsule("p", 0, 2, 0, 0.8f, 2.f, 0, Hitbox::GPlayer);
    check(h.count() >= 3, "capsule registrata");

    Hitbox sat;
    sat.addBox("base", 0, 0, 0, 1, 1, 1, 0, Hitbox::GWorld);
    sat.addBox("lato", 1.4f, 0, 0, 0.5f, 1, 0.5f, 0, Hitbox::GWorld);
    check(sat.hits("base", "lato"), "box allineati si toccano");
    sat.move("lato", 2.6f, 0, 0, 0);
    check(!sat.hits("base", "lato"), "box allineati lontani");
    sat.addBox("rot", 0, 0, 0, 1, 1, 1, 45.f, Hitbox::GWorld);
    sat.addBox("corner", 1.55f, 0, 1.55f, 0.2f, 1, 0.2f, 0, Hitbox::GWorld);
    check(!sat.hits("rot", "corner"), "SAT: AABB gonfia, OBB no");

    Hitbox mask;
    mask.addSphere("atk", 0, 0, 0, 1, Hitbox::GHit);
    mask.addSphere("tgt", 0.5f, 0, 0, 1, Hitbox::GHurt);
    mask.setMask("atk", Hitbox::GWorld);
    check(!mask.hits("atk", "tgt"), "maschera blocca");
    mask.setMask("atk", Hitbox::GHurt);
    check(mask.hits("atk", "tgt"), "maschera passa");
    mask.setEnabled("tgt", false);
    check(!mask.hits("atk", "tgt"), "disable");
    mask.setEnabled("tgt", true);
    check(mask.hits("atk", "tgt"), "enable");

    mask.setLife("atk", 0.1f);
    mask.update(0.2f);
    check(mask.count() == 1, "vita scade");

    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
