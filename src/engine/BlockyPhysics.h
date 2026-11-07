#pragma once

// Blocky Physics: il solver e' Jolt, le regole su COSA simulare sono nostre.
//
// Su un telefono debole il costo della fisica non dipende da quanti oggetti
// esistono ma da quanti sono svegli nello stesso istante. Quindi:
//   - le part ancorate diventano collider statici (costo ~zero a runtime);
//   - tra le part non ancorate solo le N piu' vicine al giocatore simulano
//     davvero (N dal QualityProfile: pochi su Android 7, molti su PC);
//   - quelle sveglie oltre il budget vengono congelate (kinematic, fuori dal
//     solver) e riprendono con la loro velocita' quando rientrano;
//   - quelle molto lontane escono proprio dal broadphase;
//   - il passo fisso e' basso (30 Hz su mobile) ma il disegno interpola tra
//     due passi, quindi a 45/60 fps il movimento resta fluido.

#include "DataModel.h"
#include "../platform/QualityProfile.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/BodyLock.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace bphys {

// Stessa gravita' del giocatore (ScriptsAndPhysics.h): un oggetto e un
// personaggio che cadono insieme devono arrivare a terra insieme.
constexpr float kGravity = 80.f;
// Sotto questa quota una part non ancorata smette di esistere per la fisica.
constexpr float kFallenY = -200.f;

struct Budget {
    int   maxActive;      // corpi dinamici svegli contemporaneamente
    float nearRadius;     // oltre: non puo' essere tra gli attivi
    float cullRadius;     // oltre: fuori dal broadphase
    int   velocitySteps;
    int   positionSteps;
    int   tempBytes;
    float timeBeforeSleep;
    const char* tier;
};

inline Budget budgetFor(const QualityProfile& q) {
    // "Debolissimo" = preset Low o fallback (Android 7, GPU vecchie): pochi
    // corpi e un solver con meno iterazioni, che resta stabile con box e
    // sfere ma costa circa la meta'.
    if (q.maxVisibleParts <= 900)
        return {16, 48.f, 140.f, 4, 1, 768 * 1024, 0.25f, "debole"};
    if (q.mobile)
        return {48, 80.f, std::max(200.f, q.maxDrawDistance), 6, 1, 1536 * 1024, 0.3f, "mobile"};
    // PC: tanti dinamici svegli — place densi / veicoli / debris senza freeze.
    return {320, 280.f, std::max(600.f, q.maxDrawDistance), 10, 2, 8 * 1024 * 1024, 0.5f, "pc"};
}

// Il personaggio: capsula alta quanto il vecchio box (5,1 stud), origine ai
// piedi. Sale da solo gradini fino a kStepUp; oltre serve saltare.
constexpr float kCharRadius = 1.0f;
constexpr float kCharHeight = 5.1f;
constexpr float kStepUp = 2.0f;
constexpr float kMaxSlopeDeg = 50.f;
constexpr float kVehicleMass = 1500.f;

namespace layers {
constexpr JPH::ObjectLayer Static      = 0;
constexpr JPH::ObjectLayer Dynamic     = 1;
constexpr JPH::ObjectLayer Player      = 2;   // corpo interno del personaggio
constexpr JPH::ObjectLayer Ghost       = 3;   // non ancorata, CanCollide=false
constexpr JPH::ObjectLayer StaticGhost = 4;   // ancorata, CanCollide=false: solo per Touched
constexpr JPH::ObjectLayer Count       = 5;
}

inline bool isGhost(JPH::ObjectLayer l) { return l == layers::Ghost || l == layers::StaticGhost; }

namespace bp {
constexpr JPH::BroadPhaseLayer NonMoving(0);
constexpr JPH::BroadPhaseLayer Moving(1);
constexpr unsigned Count = 2;
}

class LayerPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        if (isGhost(a) || isGhost(b)) return false;
        // Static–Static: no (anchored scenery). Dynamic–* and Player–solid: yes.
        if (a == layers::Player || b == layers::Player) {
            JPH::ObjectLayer o = (a == layers::Player) ? b : a;
            return o == layers::Static || o == layers::Dynamic;
        }
        if (a == layers::Dynamic || b == layers::Dynamic) return true;
        return false;
    }
};

// Roblox-like PhysicsService collision groups (0..31).
constexpr int kCollisionGroups = 32;

struct TouchPair {
    int a = -1;
    int b = -1;
};

// Accumula contatti part↔part + rifiuta coppie di gruppi non collidibili.
class ContactSink final : public JPH::ContactListener {
public:
    const bool* groupMatrix = nullptr; // [kCollisionGroups * kCollisionGroups]
    const int*  partGroup = nullptr;   // per-part collision group
    int         partCount = 0;

    void ResetFrame() { pairs_.clear(); }
    const std::vector<TouchPair>& pairs() const { return pairs_; }

    bool groupCollide(int ga, int gb) const {
        if (!groupMatrix) return true;
        ga = std::clamp(ga, 0, kCollisionGroups - 1);
        gb = std::clamp(gb, 0, kCollisionGroups - 1);
        return groupMatrix[ga * kCollisionGroups + gb];
    }

    JPH::ValidateResult OnContactValidate(
        const JPH::Body& b1, const JPH::Body& b2, JPH::RVec3Arg,
        const JPH::CollideShapeResult&) override {
        if (b1.GetObjectLayer() == layers::Player || b2.GetObjectLayer() == layers::Player)
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        if (!partGroup || partCount <= 0) return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        const int i1 = (int)b1.GetUserData();
        const int i2 = (int)b2.GetUserData();
        if (i1 < 0 || i2 < 0 || i1 >= partCount || i2 >= partCount)
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        if (!groupCollide(partGroup[i1], partGroup[i2]))
            return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body& b1, const JPH::Body& b2,
                        const JPH::ContactManifold&, JPH::ContactSettings&) override {
        const uint64_t u1 = b1.GetUserData();
        const uint64_t u2 = b2.GetUserData();
        if (b1.GetObjectLayer() == layers::Player || b2.GetObjectLayer() == layers::Player)
            return;
        if (u1 == u2) return;
        TouchPair p{(int)u1, (int)u2};
        if (p.a > p.b) std::swap(p.a, p.b);
        for (const TouchPair& e : pairs_)
            if (e.a == p.a && e.b == p.b) return;
        if (pairs_.size() < 512) pairs_.push_back(p);
    }

private:
    std::vector<TouchPair> pairs_;
};

class BpLayers final : public JPH::BroadPhaseLayerInterface {
public:
    unsigned GetNumBroadPhaseLayers() const override { return bp::Count; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override {
        return (l == layers::Static || l == layers::StaticGhost) ? bp::NonMoving : bp::Moving;
    }
};

class ObjVsBp final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer l, JPH::BroadPhaseLayer b) const override {
        if (isGhost(l)) return false;
        if (l == layers::Dynamic) return true;
        return b == bp::Moving;
    }
};

// Contro cosa sbatte il personaggio: part solide, ancorate o no.
class CharacterSolidFilter final : public JPH::ObjectLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer l) const override {
        return l == layers::Static || l == layers::Dynamic;
    }
};

// Cosa puo' "toccare" (evento Touched): tutto tranne il personaggio stesso.
class TouchFilter final : public JPH::ObjectLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer l) const override { return l != layers::Player; }
};

struct CharacterStep {
    glm::vec3 feet{0};
    float velY = 0.f;
    bool grounded = false;
};

inline JPH::Vec3 toJ(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
inline glm::vec3 toG(JPH::Vec3Arg v) { return {v.GetX(), v.GetY(), v.GetZ()}; }

// Stesso ordine di partRotationMatrix(): Ry * Rx * Rz, angoli in gradi.
inline glm::quat eulerDegToQuat(const glm::vec3& deg) {
    return glm::quat_cast(glm::mat3(partRotationMatrix(deg)));
}

inline glm::vec3 quatToEulerDeg(const glm::quat& q) {
    float y = 0.f, x = 0.f, z = 0.f;
    glm::extractEulerAngleYXZ(glm::mat4_cast(q), y, x, z);
    return glm::degrees(glm::vec3(x, y, z));
}

inline void ensureJoltRuntime() {
    static bool done = false;
    if (done) return;
    done = true;
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

struct Stats {
    int active = 0, frozen = 0, culled = 0, statics = 0, sleeping = 0, vehicles = 0;
    int bodies = 0;
    const char* tier = "";
};

class World {
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    ~World() { reset(); }

    void configure(const QualityProfile& q) {
        Budget b = budgetFor(q);
        if (sys_ && b.maxActive == budget_.maxActive && b.tempBytes == budget_.tempBytes) return;
        reset();
        budget_ = b;
        stats_.tier = b.tier;
    }

    const Budget& budget() const { return budget_; }
    const Stats& stats() const { return stats_; }

    // Altri punti attorno a cui la fisica resta sveglia, oltre a quello di
    // sync(): sul server dedicato ogni giocatore e' un centro di attivita'.
    void setExtraFocus(const std::vector<glm::vec3>& pts) { extraFocus_ = pts; }

    // workspace.Gravity (stud/s^2): part e personaggio. Default kGravity.
    void setGravity(float g) {
        g = std::clamp(g, 0.f, 1000.f);
        if (g == gravity_) return;
        gravity_ = g;
        if (sys_) sys_->SetGravity(JPH::Vec3(0.f, -gravity_, 0.f));
    }
    float gravity() const { return gravity_; }

    bool simulating(int partIndex) const {
        return partIndex >= 0 && partIndex < (int)slots_.size()
            && slots_[partIndex].tier == Tier::Active;
    }

    // ---- Veicoli (part non ancorate con VehicleSeat) ---------------------

    static constexpr int kWheels = 4;

    // Auto guidabile adesso: esiste ed e' nella fisica (non caduta, non lontana).
    bool isVehicle(int partIndex) const {
        return partIndex >= 0 && partIndex < (int)slots_.size()
            && slots_[partIndex].vehicle && slots_[partIndex].vehicleLive;
    }

    // forward/right in [-1,1], brake/handBrake in [0,1].
    void setVehicleInput(int partIndex, float forward, float right, float brake, float handBrake) {
        if (!isVehicle(partIndex)) return;
        Slot& s = slots_[partIndex];
        auto* c = static_cast<JPH::WheeledVehicleController*>(s.vehicle->GetController());
        c->SetDriverInput(forward, right, brake, handBrake);
        if (forward != 0.f || right != 0.f || brake != 0.f || handBrake != 0.f)
            sys_->GetBodyInterface().ActivateBody(s.id);
    }

    // Velocita' lungo il muso dell'auto (stud/s, negativa in retromarcia).
    float vehicleForwardSpeed(int partIndex) const {
        if (!isVehicle(partIndex)) return 0.f;
        const JPH::BodyInterface& bi = sys_->GetBodyInterfaceNoLock();
        const JPH::Quat r = bi.GetRotation(slots_[partIndex].id);
        return (r.Conjugated() * bi.GetLinearVelocity(slots_[partIndex].id)).GetZ();
    }

    // Ruota i rispetto al centro dell'auto (non interpolata: chi disegna la
    // applica alla posizione interpolata della part, cosi' non trema).
    bool wheelLocal(int partIndex, int wheel, glm::vec3& pos, glm::quat& rot) const {
        if (!isVehicle(partIndex) || wheel < 0 || wheel >= kWheels) return false;
        const Slot& s = slots_[partIndex];
        const JPH::RMat44 wheelW = s.vehicle->GetWheelWorldTransform((JPH::uint)wheel,
                                                                    JPH::Vec3::sAxisY(), JPH::Vec3::sAxisX());
        const JPH::RMat44 bodyW = sys_->GetBodyInterfaceNoLock().GetWorldTransform(s.id);
        const JPH::Mat44 local = JPH::Mat44(bodyW.InversedRotationTranslation() * wheelW);
        pos = toG(local.GetTranslation());
        const JPH::Quat q = local.GetQuaternion();
        rot = glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
        return true;
    }

    float wheelRadius(int partIndex) const {
        return isVehicle(partIndex) ? slots_[partIndex].vehicle->GetWheel(0)->GetSettings()->mRadius : 0.f;
    }
    float wheelWidth(int partIndex) const {
        return isVehicle(partIndex) ? slots_[partIndex].vehicle->GetWheel(0)->GetSettings()->mWidth : 0.f;
    }

    float vehicleRpm(int partIndex) const {
        if (!isVehicle(partIndex)) return 0.f;
        return static_cast<const JPH::WheeledVehicleController*>(slots_[partIndex].vehicle->GetController())
            ->GetEngine().GetCurrentRPM();
    }
    int vehicleGear(int partIndex) const {
        if (!isVehicle(partIndex)) return 0;
        return static_cast<const JPH::WheeledVehicleController*>(slots_[partIndex].vehicle->GetController())
            ->GetTransmission().GetCurrentGear();
    }

    // Seduto in un'auto: il corpo del personaggio sparisce dalla fisica (se
    // no l'auto sbatterebbe contro il guidatore); scendendo ricompare li'.
    void setCharacterSeated(bool seated, const glm::vec3& feet) {
        if (!sys_ || !character_) return;
        JPH::BodyInterface& bi = sys_->GetBodyInterface();
        const JPH::BodyID inner = character_->GetInnerBodyID();
        if (seated && !charSeated_) {
            if (!inner.IsInvalid()) bi.RemoveBody(inner);
        } else if (!seated && charSeated_) {
            character_->SetPosition(JPH::RVec3(feet.x, feet.y, feet.z));
            if (!inner.IsInvalid()) bi.AddBody(inner, JPH::EActivation::Activate);
        }
        charSeated_ = seated;
    }

    void reset() {
        if (character_ && charSeated_ && sys_) {
            // Il distruttore del personaggio toglie il corpo interno: deve esserci.
            const JPH::BodyID inner = character_->GetInnerBodyID();
            if (!inner.IsInvalid()) sys_->GetBodyInterface().AddBody(inner, JPH::EActivation::DontActivate);
        }
        charSeated_ = false;
        character_ = nullptr;   // prima del sistema: distrugge anche il suo corpo interno
        if (sys_) {
            destroyJoints();    // i vincoli prima dei corpi che collegano
            JPH::BodyInterface& bi = sys_->GetBodyInterface();
            for (Slot& s : slots_) destroyBody(bi, s);
        }
        slots_.clear();
        if (sys_) sys_->SetContactListener(nullptr);
        sys_.reset();
        temp_.reset();
        jobs_.reset();
        partGroupCache_.clear();
        contacts_.partGroup = nullptr;
        contacts_.partCount = 0;
        capacity_ = 0;
        lodClock_ = 0.f;
        activeVehicles_ = 0;
        stats_ = Stats{};
        stats_.tier = budget_.tier;
    }

    // ---- Vincoli fra part (WeldConstraint di DataModel) --------------------
    //
    // Si ricreano tutti quando cambia l'elenco o quando cambia un corpo (una
    // part ricostruita ha un corpo nuovo): sono pochi, rifarli costa meno che
    // tenere aggiornata ogni coppia. Chiamare ogni frame dopo sync().
    void syncJoints(const std::vector<WeldConstraint>& cs) {
        if (!sys_) return;
        const uint64_t h = jointsSignature(cs);
        if (h == jointsHash_) return;
        destroyJoints();
        jointsHash_ = h;

        JPH::BodyInterface& bi = sys_->GetBodyInterface();
        for (const WeldConstraint& c : cs) {
            if (!c.enabled || c.part0 == c.part1) continue;
            if (c.part0 < 0 || c.part1 < 0 || c.part0 >= (int)slots_.size() || c.part1 >= (int)slots_.size())
                continue;
            Slot& a = slots_[(size_t)c.part0];
            Slot& b = slots_[(size_t)c.part1];
            if (a.id.IsInvalid() || b.id.IsInvalid() || a.fallen || b.fallen) continue;
            if (!a.dynamic && !b.dynamic) continue;   // due part ancorate: niente da simulare
            if (!a.inWorld) { bi.AddBody(a.id, JPH::EActivation::DontActivate); a.inWorld = true; }
            if (!b.inWorld) { bi.AddBody(b.id, JPH::EActivation::DontActivate); b.inWorld = true; }

            JPH::Ref<JPH::TwoBodyConstraintSettings> settings = jointSettings(c, bi, a.id, b.id);
            if (!settings) continue;
            // Il risolutore leggero dei telefoni (6 iterazioni) lascia allungare
            // un'asta del 6% quando oscilla: solo i corpi vincolati ne fanno di
            // piu' (Jolt usa il massimo dell'isola, le casse restano leggere).
            settings->mNumVelocityStepsOverride = 12;
            settings->mNumPositionStepsOverride = 4;
            JPH::TwoBodyConstraint* j = bi.CreateConstraint(settings, a.id, b.id);
            if (!j) continue;
            sys_->AddConstraint(j);
            joints_.push_back(j);
            jointParts_.push_back({c.part0, c.part1});
            a.jointed = b.jointed = true;
            if (a.dynamic && a.tier == Tier::Active) bi.ActivateBody(a.id);
            if (b.dynamic && b.tier == Tier::Active) bi.ActivateBody(b.id);
        }
        lodClock_ = 0.f;   // i corpi collegati vanno svegliati insieme subito
    }

    int jointCount() const { return (int)joints_.size(); }

    // Prima dei passi di questo frame: allinea i corpi alle part (aggiunte,
    // spostate da script/Studio, cambiate di forma) e ricalcola il LOD.
    void sync(std::vector<WorkspacePart>& parts, const glm::vec3& focus, float dt) {
        if (!sys_ || parts.size() + 8 > capacity_ || parts.size() < slots_.size())
            rebuild(parts);
        JPH::BodyInterface& bi = sys_->GetBodyInterface();

        while (slots_.size() < parts.size()) {
            slots_.emplace_back();
            createBody(bi, parts[slots_.size() - 1], slots_.back(), slots_.size() - 1);
        }

        if (partGroupCache_.size() != parts.size())
            partGroupCache_.assign(parts.size(), 0);
        for (size_t i = 0; i < parts.size(); ++i) {
            WorkspacePart& p = parts[i];
            Slot& s = slots_[i];
            partGroupCache_[i] = std::clamp((int)std::lround(p.collisionGroup), 0, kCollisionGroups - 1);
            s.collisionGroup = partGroupCache_[i];
            if (s.sig != signature(p)) {
                destroyBody(bi, s);
                createBody(bi, p, s, i);
                continue;
            }
            if (s.id.IsInvalid() || s.fallen) continue;
            // Live material updates without full rebuild (friction / bounce / drag).
            if (s.dynamic && s.tier == Tier::Active) {
                bi.SetFriction(s.id, p.friction);
                bi.SetRestitution(s.id, p.elasticity);
                JPH::BodyLockWrite lock(sys_->GetBodyLockInterface(), s.id);
                if (lock.Succeeded() && lock.GetBody().IsDynamic()) {
                    auto* mp = lock.GetBody().GetMotionProperties();
                    mp->SetLinearDamping(std::clamp(p.linearDamping, 0.f, 2.f));
                    mp->SetAngularDamping(std::clamp(p.angularDamping, 0.f, 2.f));
                }
            }
            if (p.position != s.writtenPos || p.rotation != s.writtenRot) {
                // Qualcuno (Lua, gizmo) ha spostato la part: il corpo la segue.
                glm::quat r = eulerDegToQuat(p.rotation);
                bi.SetPositionAndRotation(s.id, JPH::RVec3(p.position.x, p.position.y, p.position.z),
                                          JPH::Quat(r.x, r.y, r.z, r.w),
                                          s.tier == Tier::Active ? JPH::EActivation::Activate
                                                                 : JPH::EActivation::DontActivate);
                s.prevPos = s.curPos = s.writtenPos = p.position;
                s.prevRot = s.curRot = r;
                s.writtenRot = p.rotation;
            }
        }

        contacts_.groupMatrix = &groupMatrix_[0][0];
        contacts_.partGroup = partGroupCache_.empty() ? nullptr : partGroupCache_.data();
        contacts_.partCount = (int)partGroupCache_.size();

        lodClock_ -= dt;
        if (lodClock_ <= 0.f) {
            lodClock_ = 0.25f;
            updateLod(bi, focus);
        }
    }

    bool ready() const { return sys_ != nullptr; }

    // --- API gameplay stile Roblox (impulse / velocity / mass / groups) ---

    bool applyImpulse(int partIndex, const glm::vec3& impulse) {
        if (!sys_ || partIndex < 0 || partIndex >= (int)slots_.size()) return false;
        Slot& s = slots_[(size_t)partIndex];
        if (s.id.IsInvalid() || !s.dynamic || s.fallen) return false;
        JPH::BodyInterface& bi = sys_->GetBodyInterface();
        if (s.tier != Tier::Active) {
            bi.SetMotionType(s.id, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
            bi.SetLinearAndAngularVelocity(s.id, s.savedLin, s.savedAng);
            s.savedLin = s.savedAng = JPH::Vec3::sZero();
            s.tier = Tier::Active;
        }
        bi.AddImpulse(s.id, toJ(impulse));
        bi.ActivateBody(s.id);
        return true;
    }

    bool setLinearVelocity(int partIndex, const glm::vec3& v) {
        if (!sys_ || partIndex < 0 || partIndex >= (int)slots_.size()) return false;
        Slot& s = slots_[(size_t)partIndex];
        if (s.id.IsInvalid() || !s.dynamic || s.fallen) return false;
        JPH::BodyInterface& bi = sys_->GetBodyInterface();
        if (s.tier != Tier::Active) {
            bi.SetMotionType(s.id, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
            s.tier = Tier::Active;
        }
        bi.SetLinearVelocity(s.id, toJ(v));
        bi.ActivateBody(s.id);
        return true;
    }

    bool getLinearVelocity(int partIndex, glm::vec3& out) const {
        out = {0, 0, 0};
        if (!sys_ || partIndex < 0 || partIndex >= (int)slots_.size()) return false;
        const Slot& s = slots_[(size_t)partIndex];
        if (s.id.IsInvalid() || !s.dynamic) return false;
        out = toG(sys_->GetBodyInterface().GetLinearVelocity(s.id));
        return true;
    }

    bool setAngularVelocity(int partIndex, const glm::vec3& w) {
        if (!sys_ || partIndex < 0 || partIndex >= (int)slots_.size()) return false;
        Slot& s = slots_[(size_t)partIndex];
        if (s.id.IsInvalid() || !s.dynamic || s.fallen) return false;
        JPH::BodyInterface& bi = sys_->GetBodyInterface();
        if (s.tier != Tier::Active) {
            bi.SetMotionType(s.id, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
            s.tier = Tier::Active;
        }
        bi.SetAngularVelocity(s.id, toJ(w));
        bi.ActivateBody(s.id);
        return true;
    }

    float getMass(int partIndex) const {
        if (!sys_ || partIndex < 0 || partIndex >= (int)slots_.size()) return 0.f;
        const Slot& s = slots_[(size_t)partIndex];
        if (s.id.IsInvalid()) return 0.f;
        JPH::BodyLockRead lock(sys_->GetBodyLockInterface(), s.id);
        if (!lock.Succeeded()) return 0.f;
        const JPH::Body& body = lock.GetBody();
        if (!body.IsDynamic()) return 0.f;
        return 1.f / std::max(1e-6f, body.GetMotionProperties()->GetInverseMass());
    }

    void setGroupsCollidable(int a, int b, bool collidable) {
        a = std::clamp(a, 0, kCollisionGroups - 1);
        b = std::clamp(b, 0, kCollisionGroups - 1);
        groupMatrix_[a][b] = groupMatrix_[b][a] = collidable;
    }

    bool groupsCollidable(int a, int b) const {
        a = std::clamp(a, 0, kCollisionGroups - 1);
        b = std::clamp(b, 0, kCollisionGroups - 1);
        return groupMatrix_[a][b];
    }

    // Contatti part↔part dell'ultimo step (per Touched tra oggetti).
    const std::vector<TouchPair>& lastContactPairs() const { return contacts_.pairs(); }

    // Spinta radiale stile Explosion (casse volano via).
    void applyRadialImpulse(const glm::vec3& center, float radius, float force) {
        if (!sys_ || radius <= 0.f || force <= 0.f) return;
        JPH::BodyInterface& bi = sys_->GetBodyInterface();
        const float r2 = radius * radius;
        for (int i = 0; i < (int)slots_.size(); ++i) {
            Slot& s = slots_[(size_t)i];
            if (s.id.IsInvalid() || !s.dynamic || s.fallen) continue;
            glm::vec3 d = s.curPos - center;
            float d2 = glm::dot(d, d);
            if (d2 > r2 || d2 < 1e-6f) continue;
            float dist = std::sqrt(d2);
            float falloff = 1.f - dist / radius;
            glm::vec3 dir = d / dist;
            dir.y += 0.35f; // alza un po' come le esplosioni Roblox
            dir = glm::normalize(dir);
            applyImpulse(i, dir * (force * falloff));
            (void)bi;
        }
    }

    // Muove il personaggio contro la forma VERA di ogni part (ruotata,
    // rimpicciolita, cuneo, palla, cilindro), non contro scatole dritte.
    //   wantedFeet: dove l'input lo ha gia' portato in orizzontale
    //   velY:       velocita' verticale con la gravita' di questo passo gia' applicata
    // Scivola lungo i muri, sale gradini fino a kStepUp e rampe fino a
    // kMaxSlopeDeg, resta attaccato al pavimento in discesa, spinge le casse.
    CharacterStep moveCharacter(const glm::vec3& wantedFeet, float velY, float dt) {
        CharacterStep out{wantedFeet, velY, false};
        if (!sys_ || dt <= 0.f) return out;
        if (!character_) createCharacter(wantedFeet);

        glm::vec3 from = toG(JPH::Vec3(character_->GetPosition()));
        glm::vec3 delta = wantedFeet - from;
        // Respawn, teletrasporto da script: non e' un movimento da far
        // strisciare contro il mondo, e' un salto di posizione.
        if (glm::dot(delta, delta) > 8.f * 8.f) {
            character_->SetPosition(JPH::RVec3(wantedFeet.x, wantedFeet.y, wantedFeet.z));
            from = wantedFeet;
            delta = glm::vec3(0.f);
        }

        const bool onGround = character_->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
        JPH::Vec3 v(delta.x / dt, velY + delta.y / dt, delta.z / dt);
        if (onGround) v += character_->GetGroundVelocity();
        character_->SetLinearVelocity(v);

        JPH::CharacterVirtual::ExtendedUpdateSettings ext;
        ext.mWalkStairsStepUp = JPH::Vec3(0.f, kStepUp, 0.f);
        ext.mStickToFloorStepDown = JPH::Vec3(0.f, -1.0f, 0.f);
        ext.mWalkStairsStepForwardTest = 0.5f;
        character_->ExtendedUpdate(dt, JPH::Vec3(0.f, -gravity_, 0.f), ext,
                                   JPH::BroadPhaseLayerFilter{}, charFilter_,
                                   JPH::BodyFilter{}, JPH::ShapeFilter{}, *temp_);

        out.feet = toG(JPH::Vec3(character_->GetPosition()));
        out.grounded = character_->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
        const float movedY = out.feet.y - from.y;
        if (out.grounded && velY <= 0.f) {
            // Trampolino / Bounce: Elasticity alta sul pavimento rimbalza il player
            // (stile Bounce pad Roblox), altrimenti attacca a terra.
            float bounce = 0.f;
            const JPH::BodyID groundId = character_->GetGroundBodyID();
            if (!groundId.IsInvalid() && velY < -4.f) {
                bounce = sys_->GetBodyInterface().GetRestitution(groundId);
                if (bounce > 0.25f)
                    out.velY = -velY * std::min(1.35f, bounce * 1.25f);
                else
                    out.velY = 0.f;
            } else {
                out.velY = 0.f;
            }
            (void)bounce;
        } else if (velY > 0.f && movedY < velY * dt * 0.5f) {
            out.velY = 0.f;   // testa contro un soffitto
        }
        return out;
    }

    // Per ogni part: 1 se tocca DAVVERO il personaggio (forma esatta, con
    // 5 centesimi di stud di tolleranza), 0 altrimenti. false = non
    // disponibile (fisica spenta): chi chiama deve usare il suo ripiego.
    // Rispetta CanTouch (decorazioni ghost al touch).
    bool touching(const glm::vec3& feet, const std::vector<WorkspacePart>& parts,
                  std::vector<uint8_t>& out) const {
        out.assign(slots_.size(), 0);
        if (!sys_) return false;
        JPH::CapsuleShape probe(0.5f * kCharHeight - kCharRadius, kCharRadius);
        probe.SetEmbedded();
        JPH::CollideShapeSettings cs;
        cs.mMaxSeparationDistance = 0.08f; // un filo piu' generoso = pad/kill brick affidabili
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
        const JPH::RMat44 xf = JPH::RMat44::sTranslation(
            JPH::RVec3(feet.x, feet.y + 0.5f * kCharHeight, feet.z));
        sys_->GetNarrowPhaseQuery().CollideShape(&probe, JPH::Vec3::sReplicate(1.f), xf, cs,
                                                 JPH::RVec3::sZero(), hits, {}, touchFilter_);
        const JPH::BodyInterface& bi = sys_->GetBodyInterfaceNoLock();
        for (const JPH::CollideShapeResult& h : hits.mHits) {
            uint64_t idx = bi.GetUserData(h.mBodyID2);
            if (idx >= out.size()) continue;
            if (idx < parts.size() && !parts[idx].canTouch) continue;
            out[(size_t)idx] = 1;
        }
        return true;
    }

    // Compat: senza lista parts (non filtra CanTouch).
    bool touching(const glm::vec3& feet, std::vector<uint8_t>& out) const {
        static const std::vector<WorkspacePart> kEmpty;
        return touching(feet, kEmpty, out);
    }

    void step(float dt) {
        if (!sys_) return;
        contacts_.ResetFrame();
        for (Slot& s : slots_) {
            s.prevPos = s.curPos;
            s.prevRot = s.curRot;
        }
        // Auto / tanti dinamici: piu' substep = meno tunneling, stacking piu' solido.
        int substeps = 1;
        if (activeVehicles_ > 0 && dt > 1.f / 50.f) substeps = 2;
        if (stats_.active >= 24 && dt > 1.f / 55.f) substeps = std::max(substeps, 2);
        if (stats_.active >= 80) substeps = std::max(substeps, 3);
        sys_->Update(dt, substeps, temp_.get(), jobs_.get());

        JPH::BodyInterface& bi = sys_->GetBodyInterface();
        for (Slot& s : slots_) {
            if (s.id.IsInvalid() || s.fallen || s.tier != Tier::Active) continue;
            if (!bi.IsActive(s.id)) continue;
            JPH::RVec3 pos;
            JPH::Quat rot;
            bi.GetPositionAndRotation(s.id, pos, rot);
            s.curPos = {(float)pos.GetX(), (float)pos.GetY(), (float)pos.GetZ()};
            s.curRot = glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
            if (s.curPos.y < kFallenY) {
                detachVehicle(s);
                // Un vincolo non deve restare attaccato a un corpo fuori dalla
                // fisica: via tutti, il prossimo syncJoints rifa quelli validi.
                if (s.jointed) destroyJoints();
                bi.RemoveBody(s.id);
                s.inWorld = false;
                s.fallen = true;
                s.tier = Tier::Culled;
            }
        }
    }

    // alpha = frazione di passo gia' trascorsa: interpola tra gli ultimi due
    // passi invece di mostrare la fisica a scatti a 30 Hz.
    void writeBack(std::vector<WorkspacePart>& parts, float alpha) {
        if (!sys_ || parts.size() != slots_.size()) return;
        alpha = std::clamp(alpha, 0.f, 1.f);
        for (size_t i = 0; i < parts.size(); ++i) {
            Slot& s = slots_[i];
            if (s.id.IsInvalid() || !s.dynamic) continue;
            if (s.prevPos == s.curPos && s.prevRot == s.curRot && parts[i].position == s.writtenPos)
                continue;
            glm::vec3 pos = glm::mix(s.prevPos, s.curPos, alpha);
            glm::quat rot = glm::slerp(s.prevRot, s.curRot, alpha);
            parts[i].position = pos;
            parts[i].rotation = quatToEulerDeg(rot);
            s.writtenPos = parts[i].position;
            s.writtenRot = parts[i].rotation;
        }
    }

    /** Velocita' infinite o assurde → clamp (anti "esplosione" fisica). */
    int sanityClampActiveBodies(float maxSpeedStud = 140.f) {
        if (!sys_) return 0;
        int fixes = 0;
        JPH::BodyInterface& bi = sys_->GetBodyInterface();
        for (Slot& s : slots_) {
            if (s.id.IsInvalid() || s.tier != Tier::Active) continue;
            if (!std::isfinite(s.curPos.x) || !std::isfinite(s.curPos.y) || !std::isfinite(s.curPos.z)) {
                bi.SetPositionAndRotation(s.id, JPH::RVec3(0, 12, 0), JPH::Quat::sIdentity(),
                                          JPH::EActivation::Activate);
                bi.SetLinearAndAngularVelocity(s.id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
                s.curPos = {0.f, 12.f, 0.f};
                ++fixes;
                continue;
            }
            JPH::Vec3 v = bi.GetLinearVelocity(s.id);
            const float spd = v.Length();
            if (!std::isfinite(spd)) {
                bi.SetLinearVelocity(s.id, JPH::Vec3::sZero());
                ++fixes;
            } else if (spd > maxSpeedStud) {
                bi.SetLinearVelocity(s.id, v * (maxSpeedStud / spd));
                ++fixes;
            }
            JPH::Vec3 w = bi.GetAngularVelocity(s.id);
            const float spin = w.Length();
            if (!std::isfinite(spin)) {
                bi.SetAngularVelocity(s.id, JPH::Vec3::sZero());
                ++fixes;
            } else if (spin > 40.f) {
                bi.SetAngularVelocity(s.id, w * (40.f / spin));
                ++fixes;
            }
        }
        return fixes;
    }

private:
    enum class Tier : uint8_t { None, Static, Active, Frozen, Culled };

    struct Slot {
        JPH::BodyID id;
        uint32_t sig = 0;
        Tier tier = Tier::None;
        bool dynamic = false;
        bool inWorld = false;
        bool fallen = false;
        glm::vec3 prevPos{0}, curPos{0}, writtenPos{0}, writtenRot{0};
        glm::quat prevRot{1, 0, 0, 0}, curRot{1, 0, 0, 0};
        JPH::Vec3 savedLin = JPH::Vec3::sZero();
        JPH::Vec3 savedAng = JPH::Vec3::sZero();
        JPH::Ref<JPH::VehicleConstraint> vehicle;
        bool vehicleLive = false;   // controller registrato nel sistema fisico
        bool jointed = false;       // collegata ad altre part da un vincolo
        int collisionGroup = 0;
    };

    static uint32_t signature(const WorkspacePart& p) {
        uint32_t h = 2166136261u;
        auto mix = [&](const void* d, size_t n) {
            const unsigned char* c = (const unsigned char*)d;
            for (size_t i = 0; i < n; ++i) { h ^= c[i]; h *= 16777619u; }
        };
        uint8_t flags = (uint8_t)((p.anchored ? 1 : 0) | (p.canCollide ? 2 : 0) | (p.netDriven ? 4 : 0));
        mix(&flags, 1);
        mix(&p.shape, sizeof(p.shape));
        mix(&p.size, sizeof(p.size));
        mix(&p.hitboxScaleX, sizeof(p.hitboxScaleX));
        mix(&p.hitboxScaleY, sizeof(p.hitboxScaleY));
        mix(&p.hitboxScaleZ, sizeof(p.hitboxScaleZ));
        mix(&p.density, sizeof(p.density));
        mix(&p.friction, sizeof(p.friction));
        mix(&p.elasticity, sizeof(p.elasticity));
        mix(&p.linearDamping, sizeof(p.linearDamping));
        mix(&p.angularDamping, sizeof(p.angularDamping));
        mix(&p.collisionGroup, sizeof(p.collisionGroup));
        const uint8_t car = p.vehicleSeat ? 1 : 0;
        mix(&car, 1);
        return h | 1u;   // mai 0: 0 vuol dire "slot mai costruito"
    }

    static JPH::RefConst<JPH::Shape> makeShape(const WorkspacePart& p) {
        glm::vec3 sz = partHitboxSize(p);
        glm::vec3 h = sz * 0.5f;
        float minHalf = std::min(h.x, std::min(h.y, h.z));
        float convex = std::min(0.05f, minHalf * 0.5f);
        switch (p.shape) {
        case PartShape::Ball:
            return new JPH::SphereShape(minHalf);
        case PartShape::Cylinder: {
            float r = std::min(h.x, h.z);
            return new JPH::CylinderShape(h.y, r, std::min(convex, r * 0.5f));
        }
        case PartShape::Wedge: {
            // Stessi vertici di PartMeshes::buildWedge(): base piena, spigolo
            // alto sul lato +Z.
            JPH::Array<JPH::Vec3> pts = {
                {-h.x, -h.y, -h.z}, {h.x, -h.y, -h.z}, {h.x, -h.y, h.z}, {-h.x, -h.y, h.z},
                {-h.x, h.y, h.z}, {h.x, h.y, h.z},
            };
            JPH::ConvexHullShapeSettings hs(pts, convex);
            hs.SetEmbedded();
            JPH::ShapeSettings::ShapeResult r = hs.Create();
            if (r.IsValid()) return r.Get();
            break;
        }
        case PartShape::Block:
        case PartShape::Count:
            break;
        }
        return new JPH::BoxShape(toJ(h), convex);
    }

    void rebuild(std::vector<WorkspacePart>& parts) {
        Budget keep = budget_;
        reset();
        budget_ = keep;
        stats_.tier = budget_.tier;
        ensureJoltRuntime();

        capacity_ = std::max<size_t>(256, parts.size() * 2 + 16);
        const unsigned pairs = budget_.maxActive <= 24 ? 2048u : 8192u;
        const unsigned contacts = budget_.maxActive <= 24 ? 1024u : 4096u;

        temp_ = std::make_unique<JPH::TempAllocatorImplWithMallocFallback>(budget_.tempBytes);
        jobs_ = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
        sys_ = std::make_unique<JPH::PhysicsSystem>();
        sys_->Init((JPH::uint)capacity_, 0, pairs, contacts, bpLayers_, objVsBp_, layerPairs_);
        sys_->SetGravity(JPH::Vec3(0.f, -gravity_, 0.f));

        JPH::PhysicsSettings ps;
        ps.mNumVelocitySteps = budget_.velocitySteps;
        ps.mNumPositionSteps = budget_.positionSteps;
        ps.mTimeBeforeSleep = budget_.timeBeforeSleep;
        // Unita' = stud, non metri: la soglia di default (0.03) terrebbe
        // sveglie a lungo casse che a occhio sono gia' ferme.
        ps.mPointVelocitySleepThreshold = 0.08f;
        sys_->SetPhysicsSettings(ps);
        // Friction geometrica (stile Roblox CustomPhysicalProperties mix).
        sys_->SetCombineFriction([](const JPH::Body& b1, const JPH::SubShapeID&,
                                   const JPH::Body& b2, const JPH::SubShapeID&) {
            return std::sqrt(std::max(0.f, b1.GetFriction() * b2.GetFriction()));
        });
        // Restitution = max: un bounce pad elastico batte un pavimento plastica.
        sys_->SetCombineRestitution([](const JPH::Body& b1, const JPH::SubShapeID&,
                                      const JPH::Body& b2, const JPH::SubShapeID&) {
            return std::max(b1.GetRestitution(), b2.GetRestitution());
        });

        for (int i = 0; i < kCollisionGroups; ++i)
            for (int j = 0; j < kCollisionGroups; ++j)
                groupMatrix_[i][j] = true;
        partGroupCache_.assign(parts.size(), 0);
        for (size_t i = 0; i < parts.size(); ++i)
            partGroupCache_[i] = std::clamp((int)std::lround(parts[i].collisionGroup), 0, kCollisionGroups - 1);
        contacts_.groupMatrix = &groupMatrix_[0][0];
        contacts_.partGroup = partGroupCache_.empty() ? nullptr : partGroupCache_.data();
        contacts_.partCount = (int)partGroupCache_.size();
        sys_->SetContactListener(&contacts_);

        JPH::BodyInterface& bi = sys_->GetBodyInterface();
        slots_.resize(parts.size());
        for (size_t i = 0; i < parts.size(); ++i)
            createBody(bi, parts[i], slots_[i], i);
        sys_->OptimizeBroadPhase();
    }

    void createCharacter(const glm::vec3& feet) {
        JPH::Ref<JPH::CharacterVirtualSettings> cs = new JPH::CharacterVirtualSettings();
        JPH::RefConst<JPH::Shape> capsule = new JPH::RotatedTranslatedShape(
            JPH::Vec3(0.f, 0.5f * kCharHeight, 0.f), JPH::Quat::sIdentity(),
            new JPH::CapsuleShape(0.5f * kCharHeight - kCharRadius, kCharRadius));
        cs->mShape = capsule;
        // Le casse che cadono sbattono su questo corpo (non passano attraverso
        // il giocatore); il personaggio stesso lo ignora grazie al filtro.
        cs->mInnerBodyShape = capsule;
        cs->mInnerBodyLayer = layers::Player;
        cs->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -kCharRadius);
        cs->mMaxSlopeAngle = JPH::DegreesToRadians(kMaxSlopeDeg);
        cs->mMass = 70.f;
        // Spinta stile Roblox: casse medie si muovono, muri pesanti no.
        cs->mMaxStrength = 4500.f;
        cs->mPredictiveContactDistance = 0.12f;
        character_ = new JPH::CharacterVirtual(cs, JPH::RVec3(feet.x, feet.y, feet.z),
                                               JPH::Quat::sIdentity(), sys_.get());
    }

    void createBody(JPH::BodyInterface& bi, const WorkspacePart& p, Slot& s, size_t index) {
        s = Slot{};
        s.sig = signature(p);
        s.writtenPos = s.prevPos = s.curPos = p.position;
        s.writtenRot = p.rotation;
        s.prevRot = s.curRot = eulerDegToQuat(p.rotation);

        // netDriven (client): la muove il server, qui e' cinematica — spinge
        // le casse locali e ci si sbatte contro, ma non la simula due volte.
        s.dynamic = !p.anchored && !p.netDriven;
        JPH::ObjectLayer layer = p.anchored ? (p.canCollide ? layers::Static : layers::StaticGhost)
                               : (p.canCollide ? layers::Dynamic : layers::Ghost);
        JPH::RefConst<JPH::Shape> shape = makeShape(p);
        JPH::BodyCreationSettings bcs(shape,
            JPH::RVec3(p.position.x, p.position.y, p.position.z),
            JPH::Quat(s.curRot.x, s.curRot.y, s.curRot.z, s.curRot.w),
            p.anchored ? JPH::EMotionType::Static
                       : (p.netDriven ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic),
            layer);
        bcs.mUserData = index;
        // Anche le ancorate: l'attrito di un contatto combina quello di
        // entrambe le part (un pavimento di ghiaccio fa scivolare le casse).
        bcs.mFriction = p.friction;
        bcs.mRestitution = p.elasticity;
        bcs.mLinearDamping = std::clamp(p.linearDamping, 0.f, 2.f);
        bcs.mAngularDamping = std::clamp(p.angularDamping, 0.f, 2.f);
        s.collisionGroup = std::clamp((int)std::lround(p.collisionGroup), 0, kCollisionGroups - 1);
        if (s.dynamic) {
            // Volume della forma vera (una palla pesa meno del cubo che la contiene).
            bcs.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            bcs.mMassPropertiesOverride.mMass = std::max(0.05f, shape->GetVolume() * p.density);
            // A 30 Hz una cassa che cade veloce attraversa una part sottile in
            // un solo passo. LinearCast si attiva solo quando serve (movimento
            // oltre ~meta' del corpo per passo), quindi da fermi non costa.
            bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;
            // Motore, freni e molle sono tarati su questa massa: con quella da
            // densita' un'auto di plastica leggera si impennerebbe a ogni accelerata.
            if (p.vehicleSeat && p.canCollide) bcs.mMassPropertiesOverride.mMass = kVehicleMass;
        }
        JPH::Body* body = bi.CreateBody(bcs);
        if (!body) return;
        s.id = body->GetID();
        ++bodyEpoch_;
        if (s.dynamic && p.vehicleSeat && p.canCollide) createVehicle(*body, p, s);
        // Nasce congelata: sara' updateLod a decidere se entra nel budget.
        if (s.dynamic) {
            bi.SetMotionType(s.id, JPH::EMotionType::Kinematic, JPH::EActivation::DontActivate);
            s.tier = Tier::Frozen;
        } else {
            s.tier = Tier::Static;
        }
        bi.AddBody(s.id, JPH::EActivation::DontActivate);
        s.inWorld = true;
        lodClock_ = 0.f;
    }

    // Auto a 4 ruote agli angoli della part, trazione integrale. Muso = +Z
    // locale della part. Le misure delle ruote seguono le misure della part.
    void createVehicle(JPH::Body& body, const WorkspacePart& p, Slot& s) {
        const glm::vec3 h = glm::max(p.size, glm::vec3(0.2f)) * 0.5f;
        const float radius = std::clamp(h.y * 0.9f, 0.5f, 2.f);
        const float width = std::clamp(h.x * 0.3f, 0.4f, 1.2f);
        const float x = std::max(0.1f, h.x - width * 0.5f);
        const float z = std::max(0.1f, h.z - radius);
        const float y = -h.y * 0.6f;

        JPH::VehicleConstraintSettings vs;
        vs.mUp = JPH::Vec3::sAxisY();
        vs.mForward = JPH::Vec3::sAxisZ();
        vs.mMaxPitchRollAngle = JPH::DegreesToRadians(60.f);   // non si ribalta
        const JPH::Vec3 pos[kWheels] = {{x, y, z}, {-x, y, z}, {x, y, -z}, {-x, y, -z}};
        for (int i = 0; i < kWheels; ++i) {
            JPH::WheelSettingsWV* w = new JPH::WheelSettingsWV;
            w->mPosition = pos[i];
            w->mRadius = radius;
            w->mWidth = width;
            w->mSuspensionMinLength = 0.15f;
            w->mSuspensionMaxLength = 0.7f;
            // Gravita' da 80 stud/s2 (non 9.8): molle piu' dure, se no l'auto
            // toccherebbe terra da ferma.
            w->mSuspensionSpring = JPH::SpringSettings(JPH::ESpringMode::FrequencyAndDamping, 3.f, 0.6f);
            const bool front = i < 2;
            w->mMaxSteerAngle = front ? JPH::DegreesToRadians(32.f) : 0.f;
            w->mMaxHandBrakeTorque = front ? 0.f : 6000.f;
            w->mMaxBrakeTorque = 6000.f;   // proporzionati al motore (default 1500)
            vs.mWheels.push_back(w);
        }

        JPH::WheeledVehicleControllerSettings* ctrl = new JPH::WheeledVehicleControllerSettings;
        // Ruote grandi (raggio ~1 stud, 3 volte quelle di esempio di Jolt)
        // spingono meno a parita' di coppia: 3000 da' ~15 stud/s2 di spunto.
        ctrl->mEngine.mMaxTorque = 3000.f;
        // Con la frizione di default la coppia arriva alle ruote solo in parte.
        ctrl->mTransmission.mClutchStrength = 40.f;
        ctrl->mDifferentials.resize(2);
        ctrl->mDifferentials[0].mLeftWheel = 0;
        ctrl->mDifferentials[0].mRightWheel = 1;
        ctrl->mDifferentials[0].mEngineTorqueRatio = 0.5f;
        ctrl->mDifferentials[1].mLeftWheel = 2;
        ctrl->mDifferentials[1].mRightWheel = 3;
        ctrl->mDifferentials[1].mEngineTorqueRatio = 0.5f;
        vs.mController = ctrl;

        s.vehicle = new JPH::VehicleConstraint(body, vs);
        s.vehicle->SetVehicleCollisionTester(new JPH::VehicleCollisionTesterCastCylinder(layers::Dynamic));
        attachVehicle(s);
    }

    // Il controller dell'auto lavora sul corpo a ogni passo: se il corpo
    // esce dalla fisica (caduto fuori dal mondo, o troppo lontano) deve uscire
    // anche lui, altrimenti lavorerebbe su un corpo che non c'e' piu' (crash).
    void attachVehicle(Slot& s) {
        if (!s.vehicle || s.vehicleLive) return;
        sys_->AddConstraint(s.vehicle);
        sys_->AddStepListener(s.vehicle);
        s.vehicleLive = true;
    }

    void detachVehicle(Slot& s) {
        if (!s.vehicle || !s.vehicleLive) return;
        sys_->RemoveStepListener(s.vehicle);
        sys_->RemoveConstraint(s.vehicle);
        s.vehicleLive = false;
    }

    void destroyBody(JPH::BodyInterface& bi, Slot& s) {
        if (s.id.IsInvalid()) return;
        // Un vincolo non deve sopravvivere al corpo che collega (crash):
        // si tolgono tutti, syncJoints li rifa sul corpo nuovo.
        if (s.jointed) destroyJoints();
        detachVehicle(s);
        s.vehicle = nullptr;
        if (s.inWorld) bi.RemoveBody(s.id);
        bi.DestroyBody(s.id);
        s.id = JPH::BodyID();
        s.inWorld = false;
        ++bodyEpoch_;
    }

    void freeze(JPH::BodyInterface& bi, Slot& s) {
        if (s.tier == Tier::Active) {
            s.savedLin = bi.GetLinearVelocity(s.id);
            s.savedAng = bi.GetAngularVelocity(s.id);
        }
        bi.SetMotionType(s.id, JPH::EMotionType::Kinematic, JPH::EActivation::DontActivate);
        bi.SetLinearAndAngularVelocity(s.id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
        s.prevPos = s.curPos;
        s.prevRot = s.curRot;
    }

    void updateLod(JPH::BodyInterface& bi, const glm::vec3& focus) {
        struct Cand { float d2; int idx; };
        std::vector<Cand> cands;
        cands.reserve(slots_.size());
        const float near2 = budget_.nearRadius * budget_.nearRadius;
        const float keep2 = near2 * 1.3f;       // isteresi: chi e' attivo esce un po' piu' in la'
        const float cull2 = budget_.cullRadius * budget_.cullRadius;

        Stats st;
        st.tier = budget_.tier;
        for (int i = 0; i < (int)slots_.size(); ++i) {
            Slot& s = slots_[i];
            if (s.id.IsInvalid()) continue;
            ++st.bodies;
            if (!s.dynamic) { ++st.statics; continue; }
            if (s.fallen) { ++st.culled; continue; }
            glm::vec3 d = s.curPos - focus;
            float d2 = glm::dot(d, d);
            for (const glm::vec3& f : extraFocus_) {
                const glm::vec3 e = s.curPos - f;
                d2 = std::min(d2, glm::dot(e, e));
            }
            // Una part vincolata resta nel broadphase anche lontana: il vincolo
            // non puo' puntare a un corpo che non c'e'. Al massimo si congela.
            if (d2 > cull2 && !s.jointed) {
                if (s.tier == Tier::Active) freeze(bi, s);
                if (s.inWorld) {
                    detachVehicle(s);
                    bi.RemoveBody(s.id);
                    s.inWorld = false;
                }
                s.tier = Tier::Culled;
                ++st.culled;
                continue;
            }
            if (!s.inWorld) {
                bi.AddBody(s.id, JPH::EActivation::DontActivate);
                s.inWorld = true;
                s.tier = Tier::Frozen;
                attachVehicle(s);
            }
            if (d2 <= (s.tier == Tier::Active ? keep2 : near2))
                cands.push_back({d2, i});
            else if (s.tier == Tier::Active) {
                freeze(bi, s);
                s.tier = Tier::Frozen;
            }
        }

        // Le auto prima delle casse: con il budget pieno, quella su cui sei
        // non deve mai essere congelata a favore di una cassa vicina.
        std::sort(cands.begin(), cands.end(), [this](const Cand& a, const Cand& b) {
            const bool va = (bool)slots_[a.idx].vehicle, vb = (bool)slots_[b.idx].vehicle;
            if (va != vb) return va;
            return a.d2 < b.d2;
        });
        for (size_t k = 0; k < cands.size(); ++k) {
            Slot& s = slots_[cands[k].idx];
            const bool wantActive = (int)k < budget_.maxActive;
            if (wantActive && s.tier != Tier::Active) {
                bi.SetMotionType(s.id, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
                bi.SetLinearAndAngularVelocity(s.id, s.savedLin, s.savedAng);
                s.savedLin = s.savedAng = JPH::Vec3::sZero();
                s.tier = Tier::Active;
            } else if (!wantActive && s.tier == Tier::Active) {
                freeze(bi, s);
                s.tier = Tier::Frozen;
            }
        }

        // Le part vincolate si svegliano insieme: una sveglia legata a una
        // congelata si comporterebbe come attaccata a un muro immobile.
        auto wake = [&](Slot& s, const Slot& other) {
            if (other.tier != Tier::Active || !s.dynamic || s.fallen || !s.inWorld || s.tier != Tier::Frozen)
                return false;
            bi.SetMotionType(s.id, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
            bi.SetLinearAndAngularVelocity(s.id, s.savedLin, s.savedAng);
            s.savedLin = s.savedAng = JPH::Vec3::sZero();
            s.tier = Tier::Active;
            return true;
        };
        for (bool changed = true; changed;) {
            changed = false;
            for (const auto& jp : jointParts_) {
                Slot& a = slots_[(size_t)jp.first];
                Slot& b = slots_[(size_t)jp.second];
                changed |= wake(a, b);
                changed |= wake(b, a);
            }
        }

        int activeVehicles = 0;
        for (const Slot& s : slots_) {
            if (s.id.IsInvalid() || !s.dynamic || s.fallen) continue;
            if (s.vehicle) {
                ++st.vehicles;
                if (s.tier == Tier::Active) ++activeVehicles;
            }
            if (s.tier == Tier::Active) {
                ++st.active;
                if (!bi.IsActive(s.id)) ++st.sleeping;
            } else if (s.tier == Tier::Frozen) {
                ++st.frozen;
            }
        }
        stats_ = st;
        activeVehicles_ = activeVehicles;
    }

    Budget budget_ = budgetFor(QualityProfile{});
    Stats stats_;
    std::vector<glm::vec3> extraFocus_;
    float gravity_ = kGravity;
    BpLayers bpLayers_;
    ObjVsBp objVsBp_;
    LayerPairs layerPairs_;
    ContactSink contacts_;
    bool groupMatrix_[kCollisionGroups][kCollisionGroups]{};
    std::vector<int> partGroupCache_;
    std::unique_ptr<JPH::TempAllocator> temp_;
    std::unique_ptr<JPH::JobSystem> jobs_;
    std::unique_ptr<JPH::PhysicsSystem> sys_;
    std::vector<Slot> slots_;
    JPH::Ref<JPH::CharacterVirtual> character_;
    bool charSeated_ = false;
    CharacterSolidFilter charFilter_;
    TouchFilter touchFilter_;
    size_t capacity_ = 0;
    float lodClock_ = 0.f;
    int activeVehicles_ = 0;
    std::vector<JPH::Ref<JPH::TwoBodyConstraint>> joints_;
    std::vector<std::pair<int, int>> jointParts_;
    uint64_t jointsHash_ = 0;
    uint32_t bodyEpoch_ = 1;   // cambia quando un corpo nasce o muore: i vincoli vanno rifatti

    void destroyJoints() {
        if (sys_) {
            JPH::BodyInterface& bi = sys_->GetBodyInterface();
            for (auto& j : joints_) sys_->RemoveConstraint(j);
            // Una part ferma appesa a un vincolo "dorme": togliendo il vincolo
            // va svegliata, se no resta sospesa nel vuoto.
            for (const auto& jp : jointParts_)
                for (int idx : {jp.first, jp.second}) {
                    const Slot& s = slots_[(size_t)idx];
                    if (s.dynamic && s.inWorld && !s.fallen && s.tier == Tier::Active && !s.id.IsInvalid())
                        bi.ActivateBody(s.id);
                }
        }
        joints_.clear();
        jointParts_.clear();
        for (Slot& s : slots_) s.jointed = false;
        jointsHash_ = 0;
    }

    uint64_t jointsSignature(const std::vector<WeldConstraint>& cs) const {
        uint64_t h = 1469598103934665603ull ^ bodyEpoch_;
        auto mix = [&](const void* d, size_t n) {
            const unsigned char* c = (const unsigned char*)d;
            for (size_t i = 0; i < n; ++i) { h ^= c[i]; h *= 1099511628211ull; }
        };
        for (const WeldConstraint& c : cs) {
            const int head[4] = {(int)c.kind, c.part0, c.part1, c.enabled ? 1 : 0};
            const float tail[9] = {c.length, c.stiffness, c.damping, c.pivot.x, c.pivot.y, c.pivot.z,
                                   c.axis.x, c.axis.y, c.axis.z};
            mix(head, sizeof(head));
            mix(tail, sizeof(tail));
        }
        return h | 1u;   // mai 0: 0 vuol dire "da rifare"
    }

    // Impostazioni Jolt del vincolo, in coordinate del mondo calcolate dalle
    // posizioni attuali dei due corpi.
    JPH::Ref<JPH::TwoBodyConstraintSettings> jointSettings(const WeldConstraint& c, JPH::BodyInterface& bi,
                                                          JPH::BodyID a, JPH::BodyID b) {
        const JPH::RMat44 ta = bi.GetWorldTransform(a);
        const JPH::RVec3 pa = bi.GetPosition(a), pb = bi.GetPosition(b);
        const float current = (float)(pb - pa).Length();
        const float len = c.length > 0.f ? c.length : current;
        switch (c.kind) {
        case ConstraintKind::Weld: {
            auto* s = new JPH::FixedConstraintSettings;
            s->mAutoDetectPoint = true;   // fisse nella posizione relativa di adesso
            return s;
        }
        case ConstraintKind::Hinge: {
            JPH::Vec3 axis = ta.Multiply3x3(toJ(c.axis));
            if (axis.LengthSq() < 1e-8f) axis = JPH::Vec3::sAxisY();
            axis = axis.Normalized();
            auto* s = new JPH::HingeConstraintSettings;
            s->mPoint1 = s->mPoint2 = ta * JPH::Vec3(c.pivot.x, c.pivot.y, c.pivot.z);
            s->mHingeAxis1 = s->mHingeAxis2 = axis;
            s->mNormalAxis1 = s->mNormalAxis2 = axis.GetNormalizedPerpendicular();
            return s;
        }
        case ConstraintKind::Rope:
        case ConstraintKind::Rod:
        case ConstraintKind::Spring: {
            auto* s = new JPH::DistanceConstraintSettings;
            s->mPoint1 = pa;
            s->mPoint2 = pb;
            s->mMinDistance = c.kind == ConstraintKind::Rope ? 0.f : len;
            s->mMaxDistance = len;
            if (c.kind == ConstraintKind::Spring)
                s->mLimitsSpringSettings = JPH::SpringSettings(JPH::ESpringMode::FrequencyAndDamping,
                                                               std::max(0.05f, c.stiffness),
                                                               std::clamp(c.damping, 0.f, 1.f));
            return s;
        }
        case ConstraintKind::Count:
            break;
        }
        return nullptr;
    }
};

} // namespace bphys
