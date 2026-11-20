// Test dei vincoli (engine/PartConstraints.h + BlockyPhysics::syncJoints) con
// la fisica vera, senza finestra. Ogni vincolo e' provato dove fa la
// differenza: senza, la part cadrebbe o si allontanerebbe.

#include "engine/BlockyPhysics.h"
#include "engine/GameHub.h"
#include "engine/PartConstraints.h"
#include "engine/PlaceExporter.h"

#include <cmath>
#include <cstdio>

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

static WorkspacePart part(const char* name, glm::vec3 pos, glm::vec3 size, bool anchored) {
    WorkspacePart p;
    p.name = name;
    p.position = pos;
    p.size = size;
    p.anchored = anchored;
    return p;
}

struct Scene {
    bphys::World phys;
    DataModel dm;
    explicit Scene() {
        phys.configure(profileFor(TargetPlatform::MobileAndroid));
        dm.parts.push_back(part("Base", {0, -1, 0}, {200, 2, 200}, true));
    }
    int follow = -1;   // part seguita dal "giocatore" (centro del LOD); -1 = fermo
    void run(float seconds) {
        const float dt = 1.f / 30.f;
        for (float t = 0.f; t < seconds; t += dt) {
            const glm::vec3 focus = follow >= 0 ? dm.parts[(size_t)follow].position : glm::vec3(0, 10, 0);
            phys.sync(dm.parts, focus, dt);
            phys.syncJoints(dm.welds);
            phys.step(dt);
            phys.writeBack(dm.parts, 1.f);
        }
    }
    float dist(int a, int b) const { return glm::length(dm.parts[(size_t)a].position - dm.parts[(size_t)b].position); }
};

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== Vincoli (Weld, Hinge, Rope, Spring, Rod) — profilo mobile, 30 Hz ===\n");

    {
        DataModel dm;
        dm.parts.push_back(part("A", {0, 0, 0}, {2, 2, 2}, true));
        dm.parts.push_back(part("B", {4, 0, 0}, {2, 2, 2}, false));
        dm.parts.push_back(part("C", {8, 0, 0}, {2, 2, 2}, false));
        const int r1 = partconstraints::create(dm, ConstraintKind::Rope, 0, 1);
        const int r2 = partconstraints::create(dm, ConstraintKind::Rope, 0, 2);
        const int h = partconstraints::create(dm, ConstraintKind::Hinge, 0, 1);
        check(dm.welds[(size_t)r1].name != dm.welds[(size_t)r2].name, "nomi dei vincoli unici");
        check(std::fabs(dm.welds[(size_t)r2].length - 8.f) < 1e-4f, "corda lunga quanto la distanza attuale");
        check(glm::length(dm.welds[(size_t)h].pivot - glm::vec3(2, 0, 0)) < 1e-4f,
              "cerniera a meta' strada (spazio locale di Part0)");

        dm.parts.erase(dm.parts.begin() + 1);
        partconstraints::onPartErased(dm, 1);
        check(dm.welds.size() == 1 && dm.welds[0].part1 == 1 && dm.parts[1].name == "C",
              "cancellare una part toglie i suoi vincoli e riallinea gli altri");
    }

    {
        // Salva e ricarica un .blocky vero, con un vincolo rovinato in mezzo.
        DataModel dm;
        for (int i = 0; i < 3; ++i) dm.addPart("P" + std::to_string(i), {(float)i * 3, 1, 0}, {2, 2, 2}, {1, 1, 1});
        const int s = partconstraints::create(dm, ConstraintKind::Spring, 0, 1);
        dm.welds[(size_t)s].stiffness = 7.5f;
        dm.welds[(size_t)s].damping = 0.8f;
        const int h = partconstraints::create(dm, ConstraintKind::Hinge, 1, 2);
        dm.welds[(size_t)h].axis = {1, 0, 0};
        dm.welds[(size_t)h].enabled = false;
        dm.addWeld(0, 9);   // part inesistente: deve sparire al caricamento
        Humanoid hum;
        const std::string path = "test_constraints.blocky";
        { std::ofstream f(path, std::ios::binary); f << PlaceExporter::exportPlace(dm, hum, {0, 0, 0}, 0.f); }
        DataModel back;
        const bool ok = loadPlaceFromJson(path, back);
        std::remove(path.c_str());
        const bool spring = back.welds.size() == 2 && back.welds[0].kind == ConstraintKind::Spring
                         && std::fabs(back.welds[0].stiffness - 7.5f) < 1e-4f && back.welds[0].damping > 0.79f;
        const bool hinge = back.welds.size() == 2 && back.welds[1].kind == ConstraintKind::Hinge
                        && !back.welds[1].enabled && glm::length(back.welds[1].axis - glm::vec3(1, 0, 0)) < 1e-4f;
        check(ok && spring && hinge, "salva e ricarica: tipo, parametri, abilitato (prima si perdevano)");
        check(back.welds.size() == 2, "vincolo verso una part inesistente scartato");
    }

    {
        Scene s;
        s.dm.parts.push_back(part("Gancio", {0, 10, 0}, {2, 2, 2}, true));
        s.dm.parts.push_back(part("Libera", {2.5f, 10, 0}, {2, 2, 2}, false));
        s.dm.parts.push_back(part("Sciolta", {-2.5f, 10, 0}, {2, 2, 2}, false));
        partconstraints::create(s.dm, ConstraintKind::Weld, 1, 2);
        s.run(2.f);
        std::printf("  saldata y=%.2f, non saldata y=%.2f\n", s.dm.parts[2].position.y, s.dm.parts[3].position.y);
        check(s.dm.parts[2].position.y > 9.5f && s.dm.parts[3].position.y < 2.f,
              "Weld: la part saldata resta attaccata, quella senza cade");
        check(s.phys.jointCount() == 1, "il vincolo e' nella fisica");
    }

    {
        Scene s;
        s.dm.parts.push_back(part("Perno", {0, 20, 0}, {1, 1, 1}, true));
        s.dm.parts.push_back(part("Pendolo", {4, 20, 0}, {1, 1, 1}, false));
        const int h = partconstraints::create(s.dm, ConstraintKind::Hinge, 1, 2);
        s.dm.welds[(size_t)h].pivot = {0, 0, 0};       // gira attorno al perno
        s.dm.welds[(size_t)h].axis = {0, 0, 1};        // in un piano verticale
        float lowest = 20.f, rMin = 99.f, rMax = 0.f;
        for (int i = 0; i < 60; ++i) {
            s.run(1.f / 30.f);
            lowest = std::min(lowest, s.dm.parts[2].position.y);
            rMin = std::min(rMin, s.dist(1, 2));
            rMax = std::max(rMax, s.dist(1, 2));
        }
        std::printf("  pendolo: scende fino a y=%.2f, raggio %.2f..%.2f (atteso 4)\n", lowest, rMin, rMax);
        check(lowest < 17.f && rMin > 3.8f && rMax < 4.2f, "Hinge: oscilla restando sul suo cerchio");
        check(std::fabs(s.dm.parts[2].position.z) < 0.05f, "Hinge: niente uscite dal piano della cerniera");
    }

    {
        Scene s;
        s.dm.parts.push_back(part("Soffitto", {0, 30, 0}, {2, 1, 2}, true));
        s.dm.parts.push_back(part("Lampadario", {0, 28, 0}, {1, 1, 1}, false));
        const int r = partconstraints::create(s.dm, ConstraintKind::Rope, 1, 2);
        s.dm.welds[(size_t)r].length = 8.f;
        s.run(3.f);
        std::printf("  corda da 8: distanza %.2f, y=%.2f\n", s.dist(1, 2), s.dm.parts[2].position.y);
        check(s.dist(1, 2) < 8.15f && s.dm.parts[2].position.y > 21.5f, "Rope: cade fino a lunghezza di corda e si ferma");
    }

    {
        Scene s;
        s.dm.parts.push_back(part("Fisso", {0, 30, 0}, {1, 1, 1}, true));
        s.dm.parts.push_back(part("Asta", {5, 30, 0}, {1, 1, 1}, false));
        partconstraints::create(s.dm, ConstraintKind::Rod, 1, 2);
        float dMin = 99.f, dMax = 0.f;
        for (int i = 0; i < 60; ++i) {
            s.run(1.f / 30.f);
            dMin = std::min(dMin, s.dist(1, 2));
            dMax = std::max(dMax, s.dist(1, 2));
        }
        std::printf("  asta da 5: distanza %.2f..%.2f\n", dMin, dMax);
        check(dMin > 4.85f && dMax < 5.15f, "Rod: distanza fissa anche mentre oscilla");
    }

    {
        Scene s;
        s.dm.parts.push_back(part("Fisso", {0, 30, 0}, {1, 1, 1}, true));
        s.dm.parts.push_back(part("Peso", {0, 25, 0}, {1, 1, 1}, false));
        const int k = partconstraints::create(s.dm, ConstraintKind::Spring, 1, 2);
        s.dm.welds[(size_t)k].stiffness = 1.f;    // 1 Hz: morbida
        s.dm.welds[(size_t)k].damping = 0.2f;
        float lowest = 30.f;
        for (int i = 0; i < 45; ++i) { s.run(1.f / 30.f); lowest = std::min(lowest, s.dm.parts[2].position.y); }
        s.run(6.f);
        const glm::vec3 b = s.dm.parts[2].position;
        s.run(0.2f);
        const float settle = glm::length(s.dm.parts[2].position - b);
        // Una molla a f Hz sotto gravita' g si allunga di g / (2*pi*f)^2.
        const float expected = bphys::kGravity / ((6.2831853f * 1.f) * (6.2831853f * 1.f));
        const float stretch = 25.f - b.y;
        std::printf("  molla 1 Hz: allungamento %.2f (teoria %.2f), punto piu' basso y=%.2f, moto residuo %.3f\n",
                    stretch, expected, lowest, settle);
        check(std::fabs(stretch - expected) < expected * 0.15f, "Spring: si allunga col peso quanto dice la fisica");
        check(lowest < b.y - 0.3f && settle < 0.1f, "Spring: prima oscilla oltre l'equilibrio, poi si assesta");
    }

    {
        // Una part vincolata cade fuori dal mondo: niente crash, vincolo tolto.
        Scene s;
        s.dm.parts[0].position.x = 1000.f;   // niente pavimento sotto
        s.dm.parts.push_back(part("Fisso", {0, 30, 0}, {1, 1, 1}, true));
        s.dm.parts.push_back(part("Cade", {0, 25, 0}, {1, 1, 1}, false));
        const int r = partconstraints::create(s.dm, ConstraintKind::Rope, 1, 2);
        s.dm.welds[(size_t)r].length = 5000.f;
        s.follow = 2;   // il giocatore la segue: lontana dal giocatore il LOD la congelerebbe
        s.run(5.f);
        check(s.dm.parts[2].position.y < -150.f && s.phys.jointCount() == 0,
              "caduta fuori dal mondo con un vincolo: niente crash, vincolo tolto");
    }

    {
        // Part vincolata ricostruita in partita (cambia misura): corpo nuovo,
        // vincolo rifatto sul corpo nuovo, niente crash.
        Scene s;
        s.dm.parts.push_back(part("Gancio", {0, 10, 0}, {2, 2, 2}, true));
        s.dm.parts.push_back(part("Libera", {2.5f, 10, 0}, {2, 2, 2}, false));
        partconstraints::create(s.dm, ConstraintKind::Weld, 1, 2);
        s.run(0.5f);
        s.dm.parts[2].size = {3, 2, 2};
        s.run(1.f);
        check(s.phys.jointCount() == 1 && s.dm.parts[2].position.y > 9.f,
              "part vincolata cambiata in partita: vincolo rifatto, niente crash");
        s.dm.welds.clear();
        s.run(1.f);
        check(s.phys.jointCount() == 0 && s.dm.parts[2].position.y < 3.f, "vincolo tolto da script: la part cade");
    }

    {
        // Saldature fra part ancorate: seguono lo script, con la rotazione vera.
        DataModel dm;
        dm.parts.push_back(part("Piattaforma", {0, 5, 0}, {4, 1, 4}, true));
        dm.parts.push_back(part("Ringhiera", {3, 6, 0}, {1, 1, 1}, true));
        partconstraints::create(dm, ConstraintKind::Weld, 0, 1);
        partconstraints::AnchoredWeldFollow f;
        f.apply(dm);   // prende l'offset
        dm.parts[0].position = {10, 5, 0};
        dm.parts[0].rotation = {0, 90, 0};
        f.apply(dm);
        // +X ruotato di 90 gradi attorno a Y diventa -Z.
        check(glm::length(dm.parts[1].position - glm::vec3(10, 6, -3)) < 1e-3f
              && std::fabs(dm.parts[1].rotation.y - 90.f) < 1e-2f,
              "saldatura su part ancorate: segue lo script, anche ruotando");
    }

    {
        // Attachment0 / Attachment1 (strumento vincoli: due punti cliccati sulle superfici).
        DataModel dm;
        dm.addPart("Soffitto", {0, 10, 0}, {4, 1, 4}, {1, 1, 1});
        dm.addPart("Lampada", {0, 4, 0}, {1, 1, 1}, {1, 1, 1});
        const glm::vec3 top{0, 9.5f, 0}, lamp{0, 4.5f, 0};   // sotto il soffitto, sopra la lampada
        const int r = partconstraints::createAt(dm, ConstraintKind::Rope, 0, 1, top, lamp, {0, -1, 0});
        const WeldConstraint& c = dm.welds[(size_t)r];
        check(r >= 0 && glm::length(c.attach0 - glm::vec3(0, -0.5f, 0)) < 1e-4f && glm::length(c.attach1 - glm::vec3(0, 0.5f, 0)) < 1e-4f,
              "gli agganci sono nei punti cliccati (spazio locale di ogni part)");
        check(std::fabs(c.length - 5.f) < 1e-4f, "la corda e' lunga quanto la distanza fra i due agganci (5), non fra i centri (6)");
        const int h = partconstraints::createAt(dm, ConstraintKind::Hinge, 0, 1, top, lamp, {0, -1, 0});
        check(glm::length(dm.welds[(size_t)h].axis - glm::vec3(0, -1, 0)) < 1e-4f, "la cerniera gira attorno alla normale della superficie cliccata");
        Humanoid hum;
        const std::string path = "test_constraints_attach.blocky";
        { std::ofstream fo(path, std::ios::binary); fo << PlaceExporter::exportPlace(dm, hum, {0, 0, 0}, 0.f); }
        DataModel back;
        const bool ok = loadPlaceFromJson(path, back);
        std::remove(path.c_str());
        check(ok && back.welds.size() == 2 && glm::length(back.welds[0].attach0 - c.attach0) < 1e-4f
                  && glm::length(back.welds[0].attach1 - c.attach1) < 1e-4f,
              "salva e ricarica: gli agganci restano");
    }

    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
