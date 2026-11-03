#pragma once

// Veicoli: auto guidabili. Generato da tools/genera.py il 2026-09-30.
//
// Un'auto e' una part NON ancorata con VehicleSeat = true (vedi
// PartProps.def): la fisica (motore, cambio, sospensioni, 4 ruote) la fa
// Blocky Physics con il controller per veicoli di Jolt. Qui c'e' il resto:
// salire/scendere, guidare con l'input grezzo (tastiera, pad e joystick
// touch arrivano tutti come avanti/destra), e le 4 ruote visibili, che sono
// part cilindro create durante il gioco e mosse ogni frame.

#include "engine/BlockyPhysics.h"

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <vector>

class Veicoli {
public:
    // Entrambi i limiti vengono dall'anti-cheat di rete (LanNet: oltre ~80
    // stud/s, o un salto di oltre 12 stud in un pacchetto, l'host scarta il
    // movimento e alla sesta volta espelle il giocatore). Salendo si va dal
    // punto in cui si e' fino al tetto in un colpo: entro 5 stud dal bordo il
    // salto resta sotto gli 11.5 stud anche per un'auto lunga 10.
    static constexpr float kEnterDistance = 5.f;   // stud dal bordo dell'auto
    static constexpr float kMaxSpeed = 60.f;       // stud/s, sotto il tetto di rete
    static constexpr const char* kWheelName = "Ruota";

    void attach(bphys::World* w) { phys_ = w; }

    void reset() {
        toggleRequested_ = false;
        driving_ = -1;
        near_ = -1;
        prevForward_ = 0.f;
        speed_ = 0.f;
        wheels_.clear();
    }

    void update(float) {}

    // Il bottone a schermo (o F) chiede di salire/scendere; main.cpp esegue,
    // perche' serve spostare il giocatore.
    void requestToggle() { toggleRequested_ = true; }
    bool takeToggleRequest() { const bool r = toggleRequested_; toggleRequested_ = false; return r; }

    bool seated() const { return driving_ >= 0; }
    int driving() const { return driving_; }
    int nearVehicle() const { return near_; }
    float speed() const { return speed_; }

    static bool isCar(const WorkspacePart& p) { return p.vehicleSeat && !p.anchored && p.canCollide; }

    // Auto guidabile piu' vicina al giocatore (per il bottone "Guida").
    void scan(const std::vector<WorkspacePart>& parts, const glm::vec3& feet) {
        near_ = -1;
        if (seated() || !phys_) return;
        float best = 1e9f;
        for (int i = 0; i < (int)parts.size(); ++i) {
            const WorkspacePart& p = parts[(size_t)i];
            if (!isCar(p) || !phys_->isVehicle(i)) continue;
            const float reach = 0.5f * std::max(p.size.x, p.size.z) + kEnterDistance;
            const float d = glm::length(p.position - feet);
            if (d < reach && d < best) { best = d; near_ = i; }
        }
    }

    bool enter(int part) {
        if (!phys_ || !phys_->isVehicle(part)) return false;
        driving_ = part;
        prevForward_ = 0.f;
        return true;
    }

    // Scende di lato, a sinistra dell'auto, e la lascia frenata.
    glm::vec3 exit(const std::vector<WorkspacePart>& parts) {
        glm::vec3 out(0.f);
        if (driving_ >= 0 && driving_ < (int)parts.size()) {
            const WorkspacePart& p = parts[(size_t)driving_];
            const glm::mat3 r(partRotationMatrix(p.rotation));
            glm::vec3 left = r * glm::vec3(1, 0, 0);
            left.y = 0.f;
            left = glm::dot(left, left) > 1e-4f ? glm::normalize(left) : glm::vec3(1, 0, 0);
            out = p.position + left * (p.size.x * 0.5f + 2.5f);
            if (phys_) phys_->setVehicleInput(driving_, 0.f, 0.f, 1.f, 1.f);
        }
        driving_ = -1;
        speed_ = 0.f;
        return out;
    }

    // forward/right tra -1 e 1. Passare da avanti a indietro (o viceversa)
    // mentre l'auto si muove frena prima di ingranare la marcia opposta.
    void drive(float forward, float right) {
        if (!seated() || !phys_) return;
        forward = std::clamp(forward, -1.f, 1.f);
        right = std::clamp(right, -1.f, 1.f);
        speed_ = phys_->vehicleForwardSpeed(driving_);
        float brake = 0.f;
        if (prevForward_ * forward < 0.f) {
            if ((forward > 0.f && speed_ < -0.5f) || (forward < 0.f && speed_ > 0.5f)) {
                forward = 0.f;
                brake = 1.f;
            } else {
                prevForward_ = forward;
            }
        } else if (forward != 0.f) {
            prevForward_ = forward;
        }
        // Limitatore: oltre la massima l'acceleratore non spinge piu'.
        if ((forward > 0.f && speed_ > kMaxSpeed) || (forward < 0.f && speed_ < -kMaxSpeed))
            forward = 0.f;
        phys_->setVehicleInput(driving_, forward, right, brake, 0.f);
    }

    // Dove stanno i piedi del guidatore: sul tetto, un po' indietro.
    glm::vec3 seatPosition(const std::vector<WorkspacePart>& parts) const {
        if (driving_ < 0 || driving_ >= (int)parts.size()) return glm::vec3(0.f);
        const WorkspacePart& p = parts[(size_t)driving_];
        const glm::mat3 r(partRotationMatrix(p.rotation));
        return p.position + r * glm::vec3(0.f, p.size.y * 0.5f, -p.size.z * 0.15f);
    }

    // Direzione del muso sul piano (per girare il personaggio e la camera).
    glm::vec3 forwardFlat(const std::vector<WorkspacePart>& parts) const {
        if (driving_ < 0 || driving_ >= (int)parts.size()) return {0, 0, 1};
        glm::vec3 f = glm::mat3(partRotationMatrix(parts[(size_t)driving_].rotation)) * glm::vec3(0, 0, 1);
        f.y = 0.f;
        return glm::dot(f, f) > 1e-4f ? glm::normalize(f) : glm::vec3(0, 0, 1);
    }

    // Crea (la prima volta) e muove le 4 ruote visibili di ogni auto.
    void syncWheels(std::vector<WorkspacePart>& parts) {
        if (!phys_) return;
        const size_t count = parts.size();
        for (int i = 0; i < (int)count; ++i) {
            if (!isCar(parts[(size_t)i]) || !phys_->isVehicle(i)) continue;
            auto& ids = wheels_[i];
            const float r = phys_->wheelRadius(i), w = phys_->wheelWidth(i);
            for (int k = 0; k < bphys::World::kWheels; ++k) {
                int& id = ids[(size_t)k];
                if (id <= 0 || id >= (int)parts.size() || parts[(size_t)id].name != kWheelName) {
                    WorkspacePart wheel;
                    wheel.name = kWheelName;
                    wheel.anchored = true;
                    wheel.canCollide = false;
                    wheel.shape = PartShape::Cylinder;
                    wheel.size = {r * 2.f, w, r * 2.f};
                    wheel.color = {0.12f, 0.12f, 0.13f};
                    wheel.useStud = false;
                    id = (int)parts.size();
                    parts.push_back(wheel);
                }
                glm::vec3 lp;
                glm::quat lr;
                if (!phys_->wheelLocal(i, k, lp, lr)) continue;
                const WorkspacePart& car = parts[(size_t)i];
                const glm::quat cr = bphys::eulerDegToQuat(car.rotation);
                WorkspacePart& wheel = parts[(size_t)id];
                wheel.position = car.position + cr * lp;
                wheel.rotation = bphys::quatToEulerDeg(cr * lr);
            }
        }
    }

private:
    bphys::World* phys_ = nullptr;
    int driving_ = -1;
    int near_ = -1;
    float prevForward_ = 0.f;
    float speed_ = 0.f;
    bool toggleRequested_ = false;
    std::map<int, std::array<int, bphys::World::kWheels>> wheels_;
};
