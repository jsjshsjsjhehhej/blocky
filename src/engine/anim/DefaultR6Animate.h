#pragma once

/**
 * Animazioni di base del personaggio R6, come lo script "Animate" di Roblox:
 * dallo stato del personaggio (fermo, cammina, salta, cade, seduto, oggetto in
 * mano, emote) sceglie le tracce e le dissolve una nell'altra.
 *
 * Le animazioni stanno in assets/animations/blocky_r6.blkanim (fatte in
 * Blender: tools/blender/blocky_r6_anims.py -> tools/make_r6_anims.cpp).
 * Girano sullo stesso rig che il motore costruisce per il player (Auto Rig R6
 * sulle parti di Humanoid::createR6Blocky), con un CharacterAnimator proprio:
 * cosi' valgono anche per gli altri giocatori, ognuno con la sua istanza.
 *
 * applyTo() scrive la posa nelle parti (RigPose anim + animWeight): BlockPlayer
 * la disegna al posto della camminata procedurale. Le AnimationTrack degli
 * script arrivano dopo e si mescolano sopra (LuaRuntime::applyCharacterAnimation).
 */

#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/Humanoid.h"
#include "engine/anim/AnimIO.h"
#include "engine/anim/CharacterAnimator.h"

namespace blk {
namespace anim {

struct R6AnimInput {
    bool grounded = true;
    bool walking = false;
    bool seated = false;
    bool toolEquipped = false;
    float velocityY = 0.f;
    float walkSpeed = 16.f;   // stud/s: la camminata va piu' svelta con la velocita'
    std::string emote;        // "", "wave", "dance"
};

/** Le Action del file, caricate una volta e condivise da tutti i personaggi. */
inline std::shared_ptr<const std::vector<Action>>& r6DefaultActions() {
    static std::shared_ptr<const std::vector<Action>> acts;
    return acts;
}

inline bool loadR6DefaultActions(const std::string& path, std::string& err) {
    Armature arm;
    std::vector<Action> acts;
    if (!loadFile(path, arm, acts, nullptr, err)) return false;
    if (acts.empty()) { err = "nessuna animazione nel file"; return false; }
    r6DefaultActions() = std::make_shared<const std::vector<Action>>(std::move(acts));
    return true;
}

class DefaultR6Animate {
public:
    /** Prepara il rig e le tracce. Falso se le animazioni non sono caricate. */
    bool setup() {
        if (ready_) return true;
        const auto acts = r6DefaultActions();
        if (!acts) return false;
        std::vector<PartBox> body;
        for (const auto& d : Humanoid::createR6Blocky().parts)
            if (d.name != "HumanoidRootPart") body.push_back({d.name, d.offset, d.size, glm::quat(1.f, 0.f, 0.f, 0.f)});
        if (!anim_.setupFromParts(body)) return false;
        for (const auto& a : *acts) ids_[a.name] = anim_.load(a);
        ready_ = ids_.count("Idle") && ids_.count("Walk");
        return ready_;
    }

    bool ready() const { return ready_; }
    const std::string& state() const { return state_; }

    void update(const R6AnimInput& in, float dt) {
        if (!ready_) return;
        airTime_ = in.grounded ? 0.f : airTime_ + dt;
        if (in.grounded) jumped_ = false;
        else if (airTime_ <= dt + 1e-4f) jumped_ = in.velocityY > 0.5f;   // appena staccato: salto o caduta?

        // Stato di movimento (una sola traccia alla volta, come Animate di Roblox).
        std::string want;
        float fade = 0.1f;
        if (in.seated) {
            want = "Sit";
        } else if (!in.grounded) {
            const float jumpLen = length("Jump");
            if (jumped_ && airTime_ < jumpLen && !(in.velocityY < 0.f && airTime_ > 0.3f)) want = "Jump";
            else if (jumped_ || airTime_ > 0.2f) { want = "Fall"; fade = 0.3f; }
            else want = state_.empty() ? "Idle" : state_;   // scalino: non cambiare per un attimo
        } else if (in.walking) {
            want = "Walk";
            fade = (state_ == "Fall" || state_ == "Jump") ? 0.15f : 0.1f;
        } else {
            want = (state_ == "Idle2") ? "Idle2" : "Idle";
            fade = (state_ == "Fall" || state_ == "Jump") ? 0.15f : 0.2f;
        }
        if (want != state_) switchTo(want, fade);
        if (state_ == "Walk") anim_.adjustSpeed(id("Walk"), std::clamp(in.walkSpeed / 16.f, 0.4f, 2.5f));

        // Oggetto in mano: braccio destro teso, sopra camminata e salto.
        setLooping("Tool", in.toolEquipped && !in.seated);

        // Emote: wave una volta, dance finche' non ci si muove.
        if (in.emote == "wave" && lastEmote_ != "wave") anim_.play(id("Wave"), 0.15f);
        const bool dance = in.emote == "dance" && !in.walking && in.grounded && !in.seated;
        setLooping("Dance", dance);
        lastEmote_ = in.emote;

        std::vector<TrackEvent> ev;
        anim_.update(dt, ev);
        // Idle: ogni tanto (1 volta su 10) la variante, poi si torna alla normale.
        for (const auto& e : ev) {
            if (e.type != TrackEventType::DidLoop) continue;
            if (e.track == id("Idle") && state_ == "Idle" && roll() < 0.1f) switchTo("Idle2", 0.2f);
            else if (e.track == id("Idle2") && state_ == "Idle2") switchTo("Idle", 0.2f);
        }
    }

    /** Posa nelle parti del corpo: animWeight 1 dove anima, 0 (e posa vuota) altrove. */
    void applyTo(Humanoid& h) const {
        if (!ready_) return;
        std::vector<float> boneW;
        const Pose p = anim_.pose(&boneW);
        const std::vector<Transform> parts = anim_.partTransforms(p);
        for (auto& part : h.parts) {
            part.animFromDefault = false;
            for (size_t i = 0; i < anim_.binds.size() && i < parts.size(); ++i) {
                const auto& b = anim_.binds[i];
                if (b.part != part.name) continue;
                const float w = b.bone >= 0 && b.bone < (int)boneW.size() ? boneW[(size_t)b.bone] : 0.f;
                if (w <= 1e-4f) { part.anim = RigPose{}; part.animWeight = 0.f; break; }
                glm::vec3 pos, eul;
                toBlockPartPose(parts[i], part.offset, part.pivot, pos, eul);
                part.anim.pos = pos;
                part.anim.rot = eul;
                part.anim.scale = glm::vec3(1.f);
                part.animWeight = std::min(1.f, w);
                part.animFromDefault = true;
                break;
            }
        }
        h.defaultAnimActive = true;
    }

    /** Toglie la posa (quando le animazioni di base non comandano: rig editor, clip custom...). */
    static void clearFrom(Humanoid& h) {
        for (auto& part : h.parts) {
            if (part.animFromDefault) { part.anim = RigPose{}; part.animWeight = 0.f; }
            part.animFromDefault = false;
        }
        h.defaultAnimActive = false;
    }

    /** Traccia che sta suonando (per i test e la Developer Console). */
    bool isPlaying(const std::string& name) const {
        const auto it = ids_.find(name);
        if (it == ids_.end()) return false;
        const CharTrack* t = anim_.track(it->second);
        return t && t->playing && !t->stopping;
    }

    CharacterAnimator& animator() { return anim_; }

private:
    CharacterAnimator anim_;
    std::map<std::string, int> ids_;
    std::string state_;
    std::string lastEmote_;
    bool ready_ = false;
    bool jumped_ = false;
    float airTime_ = 0.f;
    unsigned rng_ = 12345u;

    int id(const std::string& n) const {
        const auto it = ids_.find(n);
        return it == ids_.end() ? -1 : it->second;
    }

    float length(const std::string& n) const { return anim_.length(id(n)); }

    float roll() {
        rng_ = rng_ * 1103515245u + 12345u;
        return (float)((rng_ >> 8) & 0xffff) / 65535.f;
    }

    void switchTo(const std::string& next, float fade) {
        if (!state_.empty() && id(state_) >= 0) anim_.stop(id(state_), fade);
        if (id(next) >= 0) anim_.play(id(next), fade);
        state_ = next;
    }

    void setLooping(const std::string& n, bool on) {
        const int i = id(n);
        if (i < 0) return;
        const bool playing = isPlaying(n);
        if (on && !playing) anim_.play(i, 0.15f);
        else if (!on && playing) anim_.stop(i, 0.15f);
    }
};

} // namespace anim
} // namespace blk
