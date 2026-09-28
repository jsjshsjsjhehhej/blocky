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
#include <Jolt/Physics/Body/BodyCreationSettings.h>

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
    if (q.maxVisibleParts <= 800)
        return {12, 40.f, 120.f, 4, 1, 512 * 1024, 0.25f, "debole"};
    if (q.mobile)
        return {24, 60.f, std::max(160.f, q.maxDrawDistance), 6, 1, 1024 * 1024, 0.3f, "mobile"};
    return {160, 200.f, std::max(400.f, q.maxDrawDistance), 10, 2, 4 * 1024 * 1024, 0.5f, "pc"};
}

// Il personaggio: capsula alta quanto il vecchio box (5,1 stud), origine ai
// piedi. Sale da solo gradini fino a kStepUp; oltre serve saltare.
constexpr float kCharRadius = 1.0f;
constexpr float kCharHeight = 5.1f;
constexpr float kStepUp = 2.0f;
constexpr float kMaxSlopeDeg = 50.f;
// Plastica Roblox: 0.7 per stud cubo. Con la densita' di default di Jolt
// (1000) una cassa 2x2x2 peserebbe 8 tonnellate e non si spingerebbe mai.
constexpr float kPartDensity = 0.7f;

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
        if (a == layers::Dynamic || b == layers::Dynamic) return true;
        return false;
    }
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
    int active = 0, frozen = 0, culled = 0, statics = 0, sleeping = 0;
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

    bool simulating(int partIndex) const {
        return partIndex >= 0 && partIndex < (int)slots_.size()
            && slots_[partIndex].tier == Tier::Active;
    }

    void reset() {
        character_ = nullptr;   // prima del sistema: distrugge anche il suo corpo interno
        if (sys_) {
            JPH::BodyInterface& bi = sys_->GetBodyInterface();
            for (Slot& s : slots_) destroyBody(bi, s);
        }
        slots_.clear();
        sys_.reset();
        temp_.reset();
        jobs_.reset();
        capacity_ = 0;
        lodClock_ = 0.f;
        stats_ = Stats{};
        stats_.tier = budget_.tier;
    }

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

        for (size_t i = 0; i < parts.size(); ++i) {
            WorkspacePart& p = parts[i];
            Slot& s = slots_[i];
            if (s.sig != signature(p)) {
                destroyBody(bi, s);
                createBody(bi, p, s, i);
                continue;
            }
            if (s.id.IsInvalid() || s.fallen) continue;
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

        lodClock_ -= dt;
        if (lodClock_ <= 0.f) {
            lodClock_ = 0.25f;
            updateLod(bi, focus);
        }
    }

    bool ready() const { return sys_ != nullptr; }

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
        character_->ExtendedUpdate(dt, JPH::Vec3(0.f, -kGravity, 0.f), ext,
                                   JPH::BroadPhaseLayerFilter{}, charFilter_,
                                   JPH::BodyFilter{}, JPH::ShapeFilter{}, *temp_);

        out.feet = toG(JPH::Vec3(character_->GetPosition()));
        out.grounded = character_->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
        const float movedY = out.feet.y - from.y;
        if (out.grounded && velY <= 0.f)
            out.velY = 0.f;
        else if (velY > 0.f && movedY < velY * dt * 0.5f)
            out.velY = 0.f;   // testa contro un soffitto
        return out;
    }

    // Per ogni part: 1 se tocca DAVVERO il personaggio (forma esatta, con
    // 5 centesimi di stud di tolleranza), 0 altrimenti. false = non
    // disponibile (fisica spenta): chi chiama deve usare il suo ripiego.
    bool touching(const glm::vec3& feet, std::vector<uint8_t>& out) const {
        out.assign(slots_.size(), 0);
        if (!sys_) return false;
        JPH::CapsuleShape probe(0.5f * kCharHeight - kCharRadius, kCharRadius);
        probe.SetEmbedded();
        JPH::CollideShapeSettings cs;
        cs.mMaxSeparationDistance = 0.05f;
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
        const JPH::RMat44 xf = JPH::RMat44::sTranslation(
            JPH::RVec3(feet.x, feet.y + 0.5f * kCharHeight, feet.z));
        sys_->GetNarrowPhaseQuery().CollideShape(&probe, JPH::Vec3::sReplicate(1.f), xf, cs,
                                                 JPH::RVec3::sZero(), hits, {}, touchFilter_);
        const JPH::BodyInterface& bi = sys_->GetBodyInterfaceNoLock();
        for (const JPH::CollideShapeResult& h : hits.mHits) {
            uint64_t idx = bi.GetUserData(h.mBodyID2);
            if (idx < out.size()) out[(size_t)idx] = 1;
        }
        return true;
    }

    void step(float dt) {
        if (!sys_) return;
        for (Slot& s : slots_) {
            s.prevPos = s.curPos;
            s.prevRot = s.curRot;
        }
        sys_->Update(dt, 1, temp_.get(), jobs_.get());

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
    };

    static uint32_t signature(const WorkspacePart& p) {
        uint32_t h = 2166136261u;
        auto mix = [&](const void* d, size_t n) {
            const unsigned char* c = (const unsigned char*)d;
            for (size_t i = 0; i < n; ++i) { h ^= c[i]; h *= 16777619u; }
        };
        uint8_t flags = (uint8_t)((p.anchored ? 1 : 0) | (p.canCollide ? 2 : 0));
        mix(&flags, 1);
        mix(&p.shape, sizeof(p.shape));
        mix(&p.size, sizeof(p.size));
        return h | 1u;   // mai 0: 0 vuol dire "slot mai costruito"
    }

    static JPH::RefConst<JPH::Shape> makeShape(const WorkspacePart& p) {
        glm::vec3 sz = glm::max(p.size, glm::vec3(0.05f));
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
        sys_->SetGravity(JPH::Vec3(0.f, -kGravity, 0.f));

        JPH::PhysicsSettings ps;
        ps.mNumVelocitySteps = budget_.velocitySteps;
        ps.mNumPositionSteps = budget_.positionSteps;
        ps.mTimeBeforeSleep = budget_.timeBeforeSleep;
        // Unita' = stud, non metri: la soglia di default (0.03) terrebbe
        // sveglie a lungo casse che a occhio sono gia' ferme.
        ps.mPointVelocitySleepThreshold = 0.08f;
        sys_->SetPhysicsSettings(ps);

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
        cs->mMaxStrength = 2500.f;   // spinge una cassa 4x4x4 di plastica, non un muro 10x10x10
        character_ = new JPH::CharacterVirtual(cs, JPH::RVec3(feet.x, feet.y, feet.z),
                                               JPH::Quat::sIdentity(), sys_.get());
    }

    void createBody(JPH::BodyInterface& bi, const WorkspacePart& p, Slot& s, size_t index) {
        s = Slot{};
        s.sig = signature(p);
        s.writtenPos = s.prevPos = s.curPos = p.position;
        s.writtenRot = p.rotation;
        s.prevRot = s.curRot = eulerDegToQuat(p.rotation);

        s.dynamic = !p.anchored;
        JPH::ObjectLayer layer = p.anchored ? (p.canCollide ? layers::Static : layers::StaticGhost)
                               : (p.canCollide ? layers::Dynamic : layers::Ghost);
        JPH::BodyCreationSettings bcs(makeShape(p),
            JPH::RVec3(p.position.x, p.position.y, p.position.z),
            JPH::Quat(s.curRot.x, s.curRot.y, s.curRot.z, s.curRot.w),
            p.anchored ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic, layer);
        bcs.mUserData = index;
        if (s.dynamic) {
            glm::vec3 sz = glm::max(p.size, glm::vec3(0.05f));
            bcs.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            bcs.mMassPropertiesOverride.mMass = std::max(0.05f, sz.x * sz.y * sz.z * kPartDensity);
            // A 30 Hz una cassa che cade veloce attraversa una part sottile in
            // un solo passo. LinearCast si attiva solo quando serve (movimento
            // oltre ~meta' del corpo per passo), quindi da fermi non costa.
            bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;
            bcs.mFriction = 0.5f;
            bcs.mRestitution = 0.1f;
        }
        JPH::Body* body = bi.CreateBody(bcs);
        if (!body) return;
        s.id = body->GetID();
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

    void destroyBody(JPH::BodyInterface& bi, Slot& s) {
        if (s.id.IsInvalid()) return;
        if (s.inWorld) bi.RemoveBody(s.id);
        bi.DestroyBody(s.id);
        s.id = JPH::BodyID();
        s.inWorld = false;
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
            if (d2 > cull2) {
                if (s.tier == Tier::Active) freeze(bi, s);
                if (s.inWorld) { bi.RemoveBody(s.id); s.inWorld = false; }
                s.tier = Tier::Culled;
                ++st.culled;
                continue;
            }
            if (!s.inWorld) {
                bi.AddBody(s.id, JPH::EActivation::DontActivate);
                s.inWorld = true;
                s.tier = Tier::Frozen;
            }
            if (d2 <= (s.tier == Tier::Active ? keep2 : near2))
                cands.push_back({d2, i});
            else if (s.tier == Tier::Active) {
                freeze(bi, s);
                s.tier = Tier::Frozen;
            }
        }

        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
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

        for (const Slot& s : slots_) {
            if (s.id.IsInvalid() || !s.dynamic || s.fallen) continue;
            if (s.tier == Tier::Active) {
                ++st.active;
                if (!bi.IsActive(s.id)) ++st.sleeping;
            } else if (s.tier == Tier::Frozen) {
                ++st.frozen;
            }
        }
        stats_ = st;
    }

    Budget budget_ = budgetFor(QualityProfile{});
    Stats stats_;
    BpLayers bpLayers_;
    ObjVsBp objVsBp_;
    LayerPairs layerPairs_;
    std::unique_ptr<JPH::TempAllocator> temp_;
    std::unique_ptr<JPH::JobSystem> jobs_;
    std::unique_ptr<JPH::PhysicsSystem> sys_;
    std::vector<Slot> slots_;
    JPH::Ref<JPH::CharacterVirtual> character_;
    CharacterSolidFilter charFilter_;
    TouchFilter touchFilter_;
    size_t capacity_ = 0;
    float lodClock_ = 0.f;
};

} // namespace bphys
