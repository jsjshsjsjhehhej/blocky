// Test di Veicoli (src/systems/Veicoli.h) con la fisica vera, senza finestra:
// un'auto si posa sulle ruote, accelera, sterza, fa retromarcia, frena.

#include "systems/Veicoli.h"

#include <cmath>
#include <cstdio>

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

static WorkspacePart part(glm::vec3 pos, glm::vec3 size, bool anchored) {
    WorkspacePart p;
    p.position = pos;
    p.size = size;
    p.anchored = anchored;
    return p;
}

struct Scene {
    bphys::World phys;
    Veicoli car;
    std::vector<WorkspacePart> parts;
    const float dt = 1.f / 30.f;   // passo di Android: il caso peggiore

    explicit Scene(const QualityProfile& q) {
        phys.configure(q);
        car.attach(&phys);
        car.reset();
        parts.push_back(part({0, -1, 0}, {400, 2, 400}, true));
        WorkspacePart c = part({0, 4, 0}, {6, 2, 10}, false);
        c.vehicleSeat = true;
        parts.push_back(c);
    }

    void run(float seconds, float forward = 0.f, float right = 0.f) {
        for (float t = 0.f; t < seconds; t += dt) {
            phys.sync(parts, parts[1].position, dt);
            car.drive(forward, right);
            phys.step(dt);
            phys.writeBack(parts, 1.f);
            car.syncWheels(parts);
        }
    }

    float upY() const { return (glm::mat3(partRotationMatrix(parts[1].rotation)) * glm::vec3(0, 1, 0)).y; }
    float headingDeg() const {
        glm::vec3 f = glm::mat3(partRotationMatrix(parts[1].rotation)) * glm::vec3(0, 0, 1);
        return glm::degrees(std::atan2(f.x, f.z));
    }
};

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // se crasha, l'output fin li' resta
    std::printf("=== Veicoli (profilo mobile, 30 Hz) ===\n");
    Scene s(profileFor(TargetPlatform::MobileAndroid));

    s.run(1.5f);
    std::printf("  a riposo: telaio a y=%.2f, dritta %.3f\n", s.parts[1].position.y, s.upY());
    check(s.phys.isVehicle(1), "la part con VehicleSeat e' un'auto");
    check(s.parts[1].position.y > 1.5f && s.parts[1].position.y < 4.f && s.upY() > 0.98f,
          "si posa sulle ruote, dritta, senza toccare terra");

    int wheels = 0;
    for (const auto& p : s.parts) wheels += p.name == Veicoli::kWheelName ? 1 : 0;
    const glm::vec3 w0 = s.parts[2].position;
    std::printf("  ruote create: %d, prima ruota a (%.2f, %.2f, %.2f)\n", wheels, w0.x, w0.y, w0.z);
    check(wheels == 4 && w0.y < s.parts[1].position.y && std::fabs(w0.z) > 2.f,
          "4 ruote visibili agli angoli, sotto il telaio");

    s.car.scan(s.parts, {0, 0, 9});
    check(s.car.nearVehicle() == 1, "vicino all'auto: si puo' salire");
    {
        // Il salto da dove si e' fino al sedile non deve superare il margine
        // dell'host (12 stud su un pacchetto).
        // Il punto piu' lontano da cui si puo' ancora salire (la distanza e'
        // in 3D: il telaio sta ~2 stud sopra i piedi).
        const glm::vec3 seatFrom = glm::vec3(0, 0, 9.7f);
        s.car.scan(s.parts, seatFrom);
        Veicoli probe = s.car;
        probe.enter(1);
        const glm::vec3 seat = probe.seatPosition(s.parts);
        const float jump = glm::length(glm::vec2(seat.x - seatFrom.x, seat.z - seatFrom.z));
        std::printf("  salto massimo salendo: %.1f stud (margine dell'host: 12)\n", jump);
        check(s.car.nearVehicle() == 1 && jump < 12.f, "salire non sembra un teletrasporto all'host");
    }
    s.car.scan(s.parts, {0, 0, 60});
    check(s.car.nearVehicle() == -1, "lontano: niente bottone");

    check(s.car.enter(1), "sale in auto");
    const float z0 = s.parts[1].position.z;
    s.run(3.f, 1.f, 0.f);
    const float dz = s.parts[1].position.z - z0;
    std::printf("  3 s di acceleratore: %.1f stud avanti, velocita' %.1f stud/s, dritta %.3f\n",
                dz, s.car.speed(), s.upY());
    check(dz > 45.f && s.car.speed() > 35.f && s.upY() > 0.95f,
          "accelera in avanti senza impennarsi (ruote che non pattinano a 30 Hz)");

    // Limitatore: anche con l'acceleratore a fondo resta sotto il tetto
    // dell'anti-cheat di rete (~80 stud/s), se no il guidatore verrebbe espulso.
    {
        Scene fast(profileFor(TargetPlatform::MobileAndroid));
        fast.run(1.5f);
        fast.car.enter(1);
        float top = 0.f;
        // Solo finche' e' sulla piattaforma: cadendo dal bordo col muso in
        // giu' la "velocita' in avanti" diventa quella della caduta.
        for (int i = 0; i < 12 * 30; ++i) {
            fast.run(1.f / 30.f, 1.f, 0.f);
            const glm::vec3 p = fast.parts[1].position;
            if (std::fabs(p.x) < 190.f && std::fabs(p.z) < 190.f && p.y > 0.5f)
                top = std::max(top, fast.phys.vehicleForwardSpeed(1));
        }
        std::printf("  12 s a tavoletta: velocita' massima %.1f stud/s\n", top);
        check(top > 50.f && top < 70.f, "limitatore a 60 stud/s (sotto il controllo anti-cheat dell'host)");

        // Giu' dal bordo del mondo: niente crash, e l'auto smette di essere
        // guidabile (main.cpp fa scendere il guidatore).
        fast.run(15.f, 1.f, 0.f);
        std::printf("  dopo il bordo del mondo: y=%.0f, guidabile=%d\n",
                    fast.parts[1].position.y, (int)fast.phys.isVehicle(1));
        check(!fast.phys.isVehicle(1), "caduta fuori dal mondo: niente crash, auto non piu' guidabile");
    }

    // Destra = verso -X quando il muso guarda +Z (stessa destra della camera:
    // cross(avanti, su)), quindi girando a destra l'angolo atan2(x, z) scende.
    const float h0 = s.headingDeg();
    s.run(0.5f, 0.6f, 1.f);
    const float early = s.headingDeg() - h0;
    s.run(1.0f, 0.6f, 1.f);
    const float turned = std::fabs(s.headingDeg() - h0);
    std::printf("  sterzata a destra: prima %.0f gradi, in tutto %.0f, dritta %.3f\n", early, turned, s.upY());
    check(turned > 25.f && s.upY() > 0.9f, "sterza senza ribaltarsi");
    check(early < -5.f, "D gira verso la destra della camera");

    const glm::vec3 exitPos = s.car.exit(s.parts);
    check(!s.car.seated() && glm::length(exitPos - s.parts[1].position) > 3.f, "scende di lato");
    s.run(4.f);
    std::printf("  4 s dopo essere scesi: velocita' %.2f stud/s\n", std::fabs(s.phys.vehicleForwardSpeed(1)));
    check(std::fabs(s.phys.vehicleForwardSpeed(1)) < 1.f, "lasciata frenata, si ferma");

    s.car.enter(1);
    const glm::vec3 startBack = s.parts[1].position;
    const glm::vec3 fwdBack = s.car.forwardFlat(s.parts);
    s.run(3.f, -1.f, 0.f);
    const float along = glm::dot(s.parts[1].position - startBack, fwdBack);
    std::printf("  retromarcia: %.1f stud lungo il muso (negativo = indietro), velocita' %.1f\n",
                along, s.car.speed());
    check(along < -3.f && s.car.speed() < -2.f, "S fa retromarcia");

    // Budget pieno di casse: l'auto resta comunque tra quelle che simulano.
    {
        Scene b(applyPreset(profileFor(TargetPlatform::MobileAndroid), QualityPreset::Low));
        for (int i = 0; i < 30; ++i)
            b.parts.push_back(part({(float)(i % 6) * 3.f - 8.f, 2.f, 8.f + (float)(i / 6) * 3.f}, {2, 2, 2}, false));
        b.run(0.5f);
        check(b.phys.simulating(1), "con il budget pieno l'auto ha la precedenza sulle casse");
    }

    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
