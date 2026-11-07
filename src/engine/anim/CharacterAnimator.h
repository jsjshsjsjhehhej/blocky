#pragma once

/**
 * Animazioni sul personaggio, come Animator / AnimationTrack di Roblox.
 *
 * Il rig del personaggio e' quello dell'Auto Rig R6 sulle misure del player
 * (le stesse del manichino "Inserisci Rig R6" dell'Animation Studio): le
 * animazioni fatte li' girano qui osso per osso, per nome.
 *
 * Ogni traccia ha peso (con dissolvenza), velocita', loop e priorita'. Come in
 * Roblox, una traccia di priorita' piu' alta comanda SOLO sulle ossa che anima:
 * un saluto (Action) sulle braccia lascia le gambe alla camminata (Movement).
 *
 * update() avanza il tempo e raccoglie gli eventi (marker, DidLoop, Stopped,
 * Ended) per gli script; pose() da' la posa mescolata; humanoidPartPose()
 * la converte nello spazio delle parti del corpo disegnate da BlockPlayer.
 */

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/anim/AnimCurve.h"
#include "engine/anim/AutoRig.h"

namespace blk {
namespace anim {

enum class TrackEventType { MarkerReached, DidLoop, Stopped, Ended };

struct TrackEvent {
    int track = -1;
    TrackEventType type = TrackEventType::Stopped;
    std::string name;  // marker
    std::string param;
};

struct CharTrack {
    int id = 0;
    std::shared_ptr<const Action> action;
    std::vector<char> animatesBone;  // l'Action ha curve per quell'osso?
    float time = 0.f;                // secondi
    float speed = 1.f;
    float weight = 0.f;              // attuale
    float targetWeight = 1.f;
    float fadeRate = 0.f;            // peso al secondo verso target
    int priority = 0;                // Core 0, Idle 1, Movement 2, Action 3..6
    bool looped = true;
    bool playing = false;            // IsPlaying (anche durante la dissolvenza in uscita)
    bool stopping = false;
    bool wantRemove = false;         // fine dissolvenza dopo Stop: evento Ended
};

class CharacterAnimator {
public:
    Armature arm;
    std::vector<PartBinding> binds;

    /** Rig R6 dalle parti del corpo (spazio del personaggio). Falso se non sono R6. */
    bool setupFromParts(const std::vector<PartBox>& parts) {
        AutoRigResult r = autoRig(parts);
        if (r.kind == RigKind::None) return false;
        arm = std::move(r.armature);
        binds = std::move(r.bindings);
        rest_.clear();
        for (const auto& b : binds)
            for (const auto& p : parts)
                if (p.name == b.part) { Transform t; t.pos = p.pos; t.rot = p.rot; rest_.push_back(t); }
        return true;
    }

    bool ready() const { return !arm.bones.empty(); }

    int load(const Action& a) {
        CharTrack t;
        t.id = nextId_++;
        t.action = std::make_shared<const Action>(a);
        t.looped = a.loop;
        t.priority = (int)a.priority;
        t.animatesBone.assign(arm.bones.size(), 0);
        for (const auto& c : a.curves) {
            const int b = arm.find(c.bone);
            if (b >= 0 && !c.mute && !c.keys.empty()) t.animatesBone[(size_t)b] = 1;
        }
        tracks_.push_back(std::move(t));
        return tracks_.back().id;
    }

    CharTrack* track(int id) {
        for (auto& t : tracks_) if (t.id == id) return &t;
        return nullptr;
    }
    const CharTrack* track(int id) const {
        for (const auto& t : tracks_) if (t.id == id) return &t;
        return nullptr;
    }

    float length(int id) const {
        const CharTrack* t = track(id);
        return t ? t->action->durationSeconds() : 0.f;
    }

    /** Play(fadeTime, weight, speed) di Roblox: riparte dall'inizio. */
    void play(int id, float fade = 0.1f, float weight = 1.f, float speed = 1.f) {
        CharTrack* t = track(id);
        if (!t) return;
        t->time = 0.f;
        t->speed = speed;
        t->targetWeight = std::max(0.f, weight);
        t->playing = true;
        t->stopping = false;
        t->wantRemove = false;
        if (fade <= 1e-4f) { t->weight = t->targetWeight; t->fadeRate = 0.f; }
        else t->fadeRate = std::fabs(t->targetWeight - t->weight) / fade;
        lastFrame_[id] = -1e9f;
    }

    void stop(int id, float fade = 0.1f) {
        CharTrack* t = track(id);
        if (!t || !t->playing) return;
        t->stopping = true;
        t->targetWeight = 0.f;
        if (fade <= 1e-4f) { t->weight = 0.f; t->fadeRate = 0.f; }
        else t->fadeRate = t->weight / fade;
        pendingStopped_.push_back(id);
    }

    void adjustWeight(int id, float w, float fade = 0.1f) {
        CharTrack* t = track(id);
        if (!t) return;
        t->targetWeight = std::max(0.f, w);
        if (fade <= 1e-4f) { t->weight = t->targetWeight; t->fadeRate = 0.f; }
        else t->fadeRate = std::fabs(t->targetWeight - t->weight) / fade;
    }

    void adjustSpeed(int id, float s) { if (CharTrack* t = track(id)) t->speed = s; }

    void setTimePosition(int id, float sec) {
        if (CharTrack* t = track(id)) {
            t->time = std::clamp(sec, 0.f, std::max(0.f, t->action->durationSeconds()));
            lastFrame_[id] = t->action->frameStart + t->time * t->action->fps - 0.001f;
        }
    }

    std::vector<int> playingTracks() const {
        std::vector<int> out;
        for (const auto& t : tracks_) if (t.playing) out.push_back(t.id);
        return out;
    }

    /** Avanza tutte le tracce; events riceve marker, DidLoop, Stopped, Ended. */
    void update(float dt, std::vector<TrackEvent>& events) {
        for (int id : pendingStopped_) events.push_back({id, TrackEventType::Stopped, "", ""});
        pendingStopped_.clear();
        for (auto& t : tracks_) {
            if (!t.playing) continue;
            // peso verso il bersaglio
            if (t.weight < t.targetWeight) t.weight = std::min(t.targetWeight, t.weight + t.fadeRate * dt);
            else if (t.weight > t.targetWeight) t.weight = std::max(t.targetWeight, t.weight - t.fadeRate * dt);
            if (t.fadeRate <= 0.f) t.weight = t.targetWeight;

            const Action& a = *t.action;
            const float len = a.durationSeconds();
            const float prevFrame = lastFrame_.count(t.id) ? lastFrame_[t.id] : -1e9f;
            t.time += dt * t.speed;
            bool wrapped = false;
            if (len > 1e-5f && t.time >= len) {
                if (t.looped) {
                    t.time = std::fmod(t.time, len);
                    wrapped = true;
                    events.push_back({t.id, TrackEventType::DidLoop, "", ""});
                } else {
                    t.time = len;
                    if (!t.stopping) {
                        // Fine di una traccia non in loop: Stopped, poi si spegne.
                        t.stopping = true;
                        t.targetWeight = 0.f;
                        t.fadeRate = t.weight / 0.1f;
                        events.push_back({t.id, TrackEventType::Stopped, "", ""});
                    }
                }
            }
            const float frame = a.frameStart + t.time * a.fps;
            // Marker ed eventi attraversati in questo passo.
            const float from = prevFrame < -1e8f ? a.frameStart - 0.001f : prevFrame;
            for (const AnimEvent* e : a.eventsBetween(from, frame, wrapped))
                events.push_back({t.id, TrackEventType::MarkerReached, e->name, e->param});
            for (const auto& m : a.markers) {
                const bool hit = wrapped ? (m.frame > from || m.frame <= frame) : (m.frame > from && m.frame <= frame);
                if (hit) events.push_back({t.id, TrackEventType::MarkerReached, m.name, ""});
            }
            lastFrame_[t.id] = frame;

            if (t.stopping && t.weight <= 1e-4f) {
                t.playing = false;
                t.stopping = false;
                events.push_back({t.id, TrackEventType::Ended, "", ""});
            }
        }
    }

    /**
     * Posa mescolata: per ogni osso si parte dal riposo e si applicano le
     * tracce dalla priorita' piu' bassa alla piu' alta, ognuna col suo peso,
     * solo se anima quell'osso. boneWeight = quanto le animazioni comandano
     * ogni osso (0 = nessuna: resta l'animazione procedurale del motore).
     */
    Pose pose(std::vector<float>* boneWeight = nullptr) const {
        Pose out;
        out.reset(arm.bones.size());
        if (boneWeight) boneWeight->assign(arm.bones.size(), 0.f);
        std::vector<const CharTrack*> order;
        for (const auto& t : tracks_) if (t.playing && t.weight > 1e-4f) order.push_back(&t);
        std::stable_sort(order.begin(), order.end(), [](const CharTrack* a, const CharTrack* b) { return a->priority < b->priority; });
        for (const CharTrack* t : order) {
            const Pose p = t->action->sample(arm, t->action->frameStart + t->time * t->action->fps);
            const float w = std::min(1.f, t->weight);
            for (size_t b = 0; b < arm.bones.size(); ++b) {
                if (b >= t->animatesBone.size() || !t->animatesBone[b]) continue;
                out.local[b] = lerp(out.local[b], p.get((int)b), w);
                if (boneWeight) (*boneWeight)[b] = (*boneWeight)[b] + (1.f - (*boneWeight)[b]) * w;
            }
        }
        return out;
    }

    /** Parte del corpo nella posa, nello spazio del personaggio (come le parti di setupFromParts). */
    std::vector<Transform> partTransforms(const Pose& p) const { return partWorlds(arm, p, binds); }
    const std::vector<Transform>& partRest() const { return rest_; }

    void clear() {
        tracks_.clear();
        lastFrame_.clear();
        pendingStopped_.clear();
    }

private:
    std::vector<CharTrack> tracks_;
    std::vector<Transform> rest_;
    std::map<int, float> lastFrame_;
    std::vector<int> pendingStopped_;
    int nextId_ = 1;
};

/**
 * Da trasformazione di una parte (centro + rotazione, spazio personaggio) ai
 * valori che BlockPlayer applica: posizione aggiunta e gradi X, Y, Z ruotando
 * attorno al perno (Rx * Ry * Rz, vedi partTransform in BlockPlayer.h).
 *   centro = c + pivot + pos + R * (-pivot)   =>   pos = centro - c - pivot + R * pivot
 */
inline void toBlockPartPose(const Transform& want, const glm::vec3& restCenter, const glm::vec3& pivot,
                            glm::vec3& outPos, glm::vec3& outEulerDeg) {
    const glm::mat3 R = glm::mat3_cast(glm::normalize(want.rot));
    // R[col][row]: R02 = R[2][0], R12 = R[2][1], R22 = R[2][2], R01 = R[1][0], R00 = R[0][0]
    const float r02 = glm::clamp(R[2][0], -1.f, 1.f);
    float rx, ry, rz;
    ry = std::asin(r02);
    if (std::fabs(r02) < 0.99999f) {
        rx = std::atan2(-R[2][1], R[2][2]);
        rz = std::atan2(-R[1][0], R[0][0]);
    } else {
        rx = std::atan2(R[1][2], R[1][1]);
        rz = 0.f;
    }
    outEulerDeg = glm::degrees(glm::vec3(rx, ry, rz));
    outPos = want.pos - restCenter - pivot + R * pivot;
}

} // namespace anim
} // namespace blk
