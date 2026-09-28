// Test di Blocky Physics (src/engine/BlockyPhysics.h) senza finestra ne' GPU:
// le part vere (WorkspacePart) cadono, si posano, rispettano il budget LOD.
// Ogni controllo stampa il numero misurato, non solo OK/FALLITO.

#include "engine/BlockyPhysics.h"

#include <chrono>
#include <cstdio>

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

static WorkspacePart makePart(glm::vec3 pos, glm::vec3 size, bool anchored,
                              PartShape shape = PartShape::Block) {
    WorkspacePart p;
    p.position = pos;
    p.size = size;
    p.anchored = anchored;
    p.shape = shape;
    return p;
}

static std::vector<WorkspacePart> baseplateWorld() {
    std::vector<WorkspacePart> w;
    w.push_back(makePart({0, -1, 0}, {120, 2, 120}, true));
    return w;
}

// Stesso schema del loop di main.cpp: sync, passi fissi, writeBack.
static void simulate(bphys::World& phys, std::vector<WorkspacePart>& parts,
                     float seconds, float hz, glm::vec3 focus = {0, 0, 0}) {
    const float step = 1.f / hz;
    const float frame = 1.f / 60.f;
    float acc = 0.f;
    for (float t = 0.f; t < seconds; t += frame) {
        phys.sync(parts, focus, frame);
        acc += frame;
        while (acc >= step) {
            phys.step(step);
            acc -= step;
        }
        phys.writeBack(parts, acc / step);
    }
}

static void testEuler() {
    std::printf("\nRotazioni (Euler Studio <-> quaternione):\n");
    float worst = 0.f;
    const glm::vec3 samples[] = {{0, 0, 0}, {30, 45, 60}, {-80, 170, 10}, {89, -120, -45}, {15, 270, 5}};
    for (const glm::vec3& e : samples) {
        glm::vec3 back = bphys::quatToEulerDeg(bphys::eulerDegToQuat(e));
        glm::mat4 a = partRotationMatrix(e), b = partRotationMatrix(back);
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r)
                worst = std::max(worst, std::fabs(a[c][r] - b[c][r]));
    }
    std::printf("  errore massimo sulla matrice: %.6f\n", worst);
    check(worst < 1e-4f, "stessa orientazione dopo andata e ritorno");
}

static void testLanding(const QualityProfile& q) {
    std::printf("\nCaduta e appoggio (%s, %.0f Hz):\n", bphys::budgetFor(q).tier, q.physicsHz);
    bphys::World phys;
    phys.configure(q);
    auto parts = baseplateWorld();
    parts.push_back(makePart({0, 20, 0}, {4, 4, 4}, false));
    parts.push_back(makePart({10, 15, 0}, {2, 2, 2}, false, PartShape::Ball));
    parts.push_back(makePart({-10, 12, 0}, {2, 4, 2}, false, PartShape::Cylinder));
    parts.push_back(makePart({0, 12, 10}, {4, 2, 4}, false, PartShape::Wedge));
    parts.push_back(makePart({0, 8, -10}, {2, 1, 2}, true));   // ancorata: non deve muoversi

    simulate(phys, parts, 4.f, q.physicsHz);

    std::printf("  cassa y=%.3f  palla y=%.3f  cilindro y=%.3f  cuneo y=%.3f  ancorata y=%.3f\n",
                parts[1].position.y, parts[2].position.y, parts[3].position.y,
                parts[4].position.y, parts[5].position.y);
    check(parts[1].position.y > 1.85f && parts[1].position.y < 2.2f, "cassa 4x4x4 appoggiata (atteso ~2)");
    check(std::fabs(parts[2].position.x - 10.f) < 30.f && parts[2].position.y > 0.8f
          && parts[2].position.y < 1.2f, "palla appoggiata (atteso ~1)");
    check(parts[3].position.y > 0.8f && parts[3].position.y < 2.2f, "cilindro appoggiato (dritto ~2 o sdraiato ~1)");
    check(parts[4].position.y > 0.8f && parts[4].position.y < 1.4f, "cuneo appoggiato (atteso ~1)");
    check(parts[5].position.y == 8.f, "part ancorata ferma");

    // Teletrasporto da script: la part torna in aria e ricade.
    parts[1].position = {0, 30, 0};
    simulate(phys, parts, 0.1f, q.physicsHz);
    const float midAir = parts[1].position.y;
    simulate(phys, parts, 4.f, q.physicsHz);
    std::printf("  dopo teletrasporto a y=30: %.3f in aria, poi %.3f\n", midAir, parts[1].position.y);
    check(midAir > 20.f && parts[1].position.y > 1.85f && parts[1].position.y < 2.2f,
          "spostata da script: il corpo la segue e ricade");
}

static void testBudget() {
    QualityProfile weak = applyPreset(profileFor(TargetPlatform::MobileAndroid), QualityPreset::Low);
    const bphys::Budget b = bphys::budgetFor(weak);
    std::printf("\nBudget LOD (profilo %s: max %d attivi, raggio %.0f):\n", b.tier, b.maxActive, b.nearRadius);
    bphys::World phys;
    phys.configure(weak);
    auto parts = baseplateWorld();
    for (int i = 0; i < 60; ++i)
        parts.push_back(makePart({(i % 10) * 3.f - 15.f, 5.f + (i / 10) * 3.f, 0.f}, {2, 2, 2}, false));
    parts.push_back(makePart({1000, 5, 0}, {2, 2, 2}, false));   // lontanissima

    simulate(phys, parts, 0.5f, weak.physicsHz);
    const bphys::Stats& s = phys.stats();
    std::printf("  attivi %d, congelati %d, fuori dal broadphase %d, statici %d\n",
                s.active, s.frozen, s.culled, s.statics);
    check(s.active == b.maxActive, "mai piu' corpi attivi del budget");
    check(s.frozen == 60 - b.maxActive, "gli altri vicini sono congelati");
    check(s.culled == 1, "quella a 1000 stud e' fuori dal broadphase");
    check(parts.back().position.y == 5.f, "la part lontana non si e' mossa");
}

// Stesso schema di stepPlayerPhysicsWithCollision: l'input sposta i piedi in
// orizzontale, la gravita' la velocita' verticale, il personaggio risolve.
static glm::vec3 walk(bphys::World& phys, std::vector<WorkspacePart>& parts, glm::vec3 feet,
                      glm::vec3 dir, float speed, float seconds, float hz = 30.f) {
    const float dt = 1.f / hz;
    float velY = 0.f;
    for (float t = 0.f; t < seconds; t += dt) {
        phys.sync(parts, feet, dt);
        feet += dir * (speed * dt);
        velY -= bphys::kGravity * dt;
        bphys::CharacterStep r = phys.moveCharacter(feet, velY, dt);
        feet = r.feet;
        velY = r.velY;
        phys.step(dt);
        phys.writeBack(parts, 1.f);
    }
    return feet;
}

static void testCharacter() {
    std::printf("\nPersonaggio contro forme vere (profilo mobile, 30 Hz):\n");
    const QualityProfile q = profileFor(TargetPlatform::MobileAndroid);

    // La trave del bug: sottile, ruotata di 45 gradi. La sua scatola dritta
    // arriva fino a x,z = +-2.47, ma la trave vera passa solo lungo x = -z.
    auto plankWorld = [] {
        auto w = baseplateWorld();
        WorkspacePart p = makePart({0, 2, 0}, {6, 4, 1}, true);
        p.rotation = {0, 45, 0};
        w.push_back(p);
        return w;
    };
    {
        bphys::World phys;
        phys.configure(q);
        auto parts = plankWorld();
        glm::vec3 end = walk(phys, parts, {2.2f, 0.f, 6.f}, {0, 0, -1}, 16.f, 0.33f);
        std::printf("  vicino alla trave (nello spazio vuoto della sua scatola): arrivato a z=%.2f\n", end.z);
        check(end.z < 1.5f, "nessun muro invisibile accanto a una part ruotata");

        std::vector<uint8_t> touch;
        phys.touching(end, touch);
        check(touch.size() == parts.size() && touch[1] == 0, "Touched non scatta nello spazio vuoto");
    }
    {
        bphys::World phys;
        phys.configure(q);
        auto parts = plankWorld();
        glm::vec3 end = walk(phys, parts, {3.f, 0.f, 3.f}, glm::normalize(glm::vec3(-1, 0, -1)), 16.f, 1.f);
        const float side = (end.x + end.z) / std::sqrt(2.f);
        std::printf("  contro la trave: distanza dal centro della trave %.2f (raggio 1 + meta' spessore 0.5)\n", side);
        check(side > 1.4f && side < 1.7f, "la trave vera blocca, senza attraversarla");

        std::vector<uint8_t> touch;
        phys.touching(end, touch);
        check(touch.size() == parts.size() && touch[1] == 1, "Touched scatta contro la trave vera");
    }
    {
        bphys::World phys;
        phys.configure(q);
        auto parts = baseplateWorld();
        for (int i = 0; i < 5; ++i)   // la Scala della Toolbox
            parts.push_back(makePart({0.f, 0.25f + 0.5f * i, -2.f * i}, {4, 0.5f, 2}, true));
        glm::vec3 end = walk(phys, parts, {0.f, 0.f, 4.f}, {0, 0, -1}, 16.f, 0.75f);
        std::printf("  scala (gradini da 0.5): quota finale %.2f (cima 2.5)\n", end.y);
        check(end.y > 2.3f && end.y < 2.7f, "sale i gradini senza saltare");
    }
    {
        bphys::World phys;
        phys.configure(q);
        auto parts = baseplateWorld();
        parts.push_back(makePart({0, 2, 0}, {4, 4, 8}, true, PartShape::Wedge));
        glm::vec3 end = walk(phys, parts, {0.f, 0.f, -7.f}, {0, 0, 1}, 16.f, 0.6f);
        std::printf("  rampa (cuneo 4x4x8, 27 gradi): arrivato a z=%.2f y=%.2f\n", end.z, end.y);
        check(end.z > -1.f && end.y > 1.5f, "sale il cuneo come una rampa liscia");
    }
    {
        bphys::World phys;
        phys.configure(q);
        auto parts = baseplateWorld();
        parts.push_back(makePart({0, 1, -4}, {2, 2, 2}, false));
        simulate(phys, parts, 0.5f, 30.f);
        const float z0 = parts[1].position.z;
        walk(phys, parts, {0.f, 0.f, 0.f}, {0, 0, -1}, 16.f, 1.f);
        std::printf("  cassa 2x2x2 spinta: da z=%.2f a z=%.2f\n", z0, parts[1].position.z);
        check(parts[1].position.z < z0 - 3.f, "cammina contro una cassa e la spinge");
    }
}

static void testCost() {
    for (TargetPlatform tp : {TargetPlatform::MobileAndroid, TargetPlatform::PC}) {
        QualityProfile q = profileFor(tp);
        const bphys::Budget b = bphys::budgetFor(q);
        bphys::World phys;
        phys.configure(q);
        auto parts = baseplateWorld();
        for (int i = 0; i < 400; ++i)       // mappa con tanta roba ancorata
            parts.push_back(makePart({(i % 20) * 5.f - 50.f, 0.5f, (i / 20) * 5.f - 50.f}, {4, 1, 4}, true));
        const int n = b.maxActive;
        for (int i = 0; i < n; ++i)         // e il budget pieno di casse che cadono
            parts.push_back(makePart({(i % 6) * 2.5f - 6.f, 6.f + (i / 6) * 2.5f, 0.f}, {2, 2, 2}, false));

        const float step = 1.f / q.physicsHz;
        phys.sync(parts, {0, 0, 0}, step);
        double worst = 0.0, total = 0.0;
        const int steps = (int)(q.physicsHz * 3.f);
        for (int k = 0; k < steps; ++k) {
            auto t0 = std::chrono::steady_clock::now();
            phys.sync(parts, {0, 0, 0}, step);
            phys.step(step);
            phys.writeBack(parts, 1.f);
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            total += ms;
            worst = std::max(worst, ms);
        }
        std::printf("\nCosto su QUESTO PC, profilo %s (%d casse attive + 400 ancorate, %.0f Hz):\n",
                    b.tier, n, q.physicsHz);
        std::printf("  media %.3f ms/passo, peggiore %.3f ms\n", total / steps, worst);
    }
}

int main() {
    std::printf("=== Blocky Physics (Jolt + LOD) ===\n");
    testEuler();
    testLanding(profileFor(TargetPlatform::MobileAndroid));
    testLanding(profileFor(TargetPlatform::PC));
    testBudget();
    testCharacter();
    testCost();
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
