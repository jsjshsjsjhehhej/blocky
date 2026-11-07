#pragma once

/**
 * State machine delle animazioni (Animator Controller di Unity, Animate di
 * Roblox): stati come Idle, Walk, Run, Jump, Fall, Climb, Swim, Attack, e
 * transizioni con condizioni sui parametri.
 *
 *   parametri  numeri (Speed), vero/falso (Grounded), trigger (Attack: vale
 *              una volta e si spegne quando una transizione lo usa)
 *   transizione da uno stato (o da "qualsiasi") a un altro quando TUTTE le
 *              condizioni valgono, opzionalmente solo dopo una certa
 *              percentuale dell'animazione (exit time); passa con un
 *              cross fade di `duration` secondi
 *
 * update() avanza il tempo, cambia stato se serve, restituisce la posa e gli
 * eventi attraversati (Footstep, Land...) dell'Action dello stato attuale.
 */

#include <map>
#include <string>
#include <vector>

#include "engine/anim/AnimBlend.h"

namespace blk {
namespace anim {

struct AnimParams {
    std::map<std::string, float> numbers;
    std::map<std::string, bool> flags;
    std::map<std::string, bool> triggers;

    void setTrigger(const std::string& n) { triggers[n] = true; }
    float number(const std::string& n) const { auto it = numbers.find(n); return it == numbers.end() ? 0.f : it->second; }
    bool flag(const std::string& n) const { auto it = flags.find(n); return it != flags.end() && it->second; }
    bool trigger(const std::string& n) const { auto it = triggers.find(n); return it != triggers.end() && it->second; }
};

enum class CondOp { Greater, Less, IsTrue, IsFalse, Trigger };

struct Condition {
    std::string param;
    CondOp op = CondOp::Greater;
    float value = 0.f;

    bool test(const AnimParams& p) const {
        switch (op) {
        case CondOp::Greater: return p.number(param) > value;
        case CondOp::Less: return p.number(param) < value;
        case CondOp::IsTrue: return p.flag(param);
        case CondOp::IsFalse: return !p.flag(param);
        case CondOp::Trigger: return p.trigger(param);
        }
        return false;
    }
};

struct AnimState {
    std::string name;
    const Action* action = nullptr;
    float speed = 1.f;
    // Blend tree facoltativo (Locomotion): se ha voci, vince sull'Action.
    BlendTree1D tree;
    std::string treeParam;
    glm::vec2 editorPos{0.f}; // posizione del nodo nell'editor visuale
};

struct AnimTransition {
    int from = -1;          // -1 = da qualsiasi stato
    int to = 0;
    std::vector<Condition> conditions;
    float exitTime = -1.f;  // 0..1 dell'animazione; <0 = subito
    float duration = 0.2f;  // cross fade
};

class AnimStateMachine {
public:
    std::vector<AnimState> states;
    std::vector<AnimTransition> transitions;
    int current = 0;
    float stateTime = 0.f;   // secondi nello stato attuale

    int addState(const std::string& name, const Action* a, float speed = 1.f) {
        AnimState s;
        s.name = name;
        s.action = a;
        s.speed = speed;
        states.push_back(std::move(s));
        return (int)states.size() - 1;
    }

    int find(const std::string& name) const {
        for (size_t i = 0; i < states.size(); ++i)
            if (states[i].name == name) return (int)i;
        return -1;
    }

    AnimTransition& addTransition(int from, int to, std::vector<Condition> conds, float duration = 0.2f, float exitTime = -1.f) {
        AnimTransition t;
        t.from = from;
        t.to = to;
        t.conditions = std::move(conds);
        t.duration = duration;
        t.exitTime = exitTime;
        transitions.push_back(std::move(t));
        return transitions.back();
    }

    const char* currentName() const { return current >= 0 && current < (int)states.size() ? states[(size_t)current].name.c_str() : ""; }

    /** Avanza di dt; events riceve gli eventi attraversati in questo passo. */
    Pose update(const Armature& arm, float dt, AnimParams& params, std::vector<AnimEvent>* events = nullptr) {
        if (states.empty()) { Pose p; p.reset(arm.bones.size()); return p; }
        current = glm::clamp(current, 0, (int)states.size() - 1);

        // Prima la transizione: lo stato nuovo parte da zero in questo stesso frame.
        for (const auto& t : transitions) {
            if (t.to == current || (t.from >= 0 && t.from != current)) continue;
            if (t.to < 0 || t.to >= (int)states.size()) continue;
            if (t.exitTime >= 0.f && normalizedTime() < t.exitTime) continue;
            bool ok = true;
            for (const auto& c : t.conditions) if (!c.test(params)) { ok = false; break; }
            if (!ok) continue;
            for (const auto& c : t.conditions)
                if (c.op == CondOp::Trigger) params.triggers[c.param] = false;
            fade_.start(lastPose_, t.duration);
            current = t.to;
            stateTime = 0.f;
            break;
        }

        AnimState& s = states[(size_t)current];
        const float prevTime = stateTime;
        stateTime += dt * s.speed;
        Pose pose;
        if (!s.tree.entries.empty()) {
            pose = s.tree.sample(arm, params.number(s.treeParam), dt * s.speed);
        } else if (s.action) {
            pose = s.action->sample(arm, s.action->frameAt(stateTime));
            if (events) {
                const float f0 = s.action->frameAt(prevTime), f1 = s.action->frameAt(stateTime);
                const bool wrapped = s.action->loop && f1 < f0;
                const bool first = prevTime <= 0.f;
                for (const AnimEvent* e : s.action->eventsBetween(first ? s.action->frameStart - 1.f : f0, f1, wrapped))
                    events->push_back(*e);
            }
        } else {
            pose.reset(arm.bones.size());
        }
        pose = fade_.apply(pose, dt);
        lastPose_ = pose;
        return pose;
    }

    float normalizedTime() const {
        if (current < 0 || current >= (int)states.size() || !states[(size_t)current].action) return 1.f;
        const float d = states[(size_t)current].action->durationSeconds();
        return d > 1e-4f ? stateTime / d : 1.f;
    }

    bool fading() const { return fade_.active; }

private:
    CrossFade fade_;
    Pose lastPose_;
};

/**
 * La macchina del personaggio, come lo script Animate di Roblox. Parametri:
 * Speed (numero), Grounded, Climbing, Swimming (vero/falso), Attack (trigger).
 * Le Action possono mancare (nullptr): lo stato resta a riposo.
 */
inline AnimStateMachine humanoidStateMachine(const Action* idle, const Action* walk, const Action* run, const Action* jump,
                                             const Action* fall, const Action* climb, const Action* swim, const Action* attack) {
    AnimStateMachine m;
    const int sIdle = m.addState("Idle", idle);
    const int sWalk = m.addState("Walk", walk);
    const int sRun = m.addState("Run", run);
    const int sJump = m.addState("Jump", jump);
    const int sFall = m.addState("Fall", fall);
    const int sClimb = m.addState("Climb", climb);
    const int sSwim = m.addState("Swim", swim);
    const int sAttack = m.addState("Attack", attack);
    using C = Condition;
    // A terra: velocita'.
    m.addTransition(sIdle, sWalk, {C{"Speed", CondOp::Greater, 0.5f}, C{"Grounded", CondOp::IsTrue}});
    m.addTransition(sWalk, sIdle, {C{"Speed", CondOp::Less, 0.5f}});
    m.addTransition(sWalk, sRun, {C{"Speed", CondOp::Greater, 12.f}});
    m.addTransition(sRun, sWalk, {C{"Speed", CondOp::Less, 12.f}});
    // In aria.
    m.addTransition(-1, sClimb, {C{"Climbing", CondOp::IsTrue}}, 0.15f);
    m.addTransition(sClimb, sIdle, {C{"Climbing", CondOp::IsFalse}}, 0.15f);
    m.addTransition(-1, sSwim, {C{"Swimming", CondOp::IsTrue}}, 0.3f);
    m.addTransition(sSwim, sIdle, {C{"Swimming", CondOp::IsFalse}}, 0.3f);
    m.addTransition(-1, sJump, {C{"Grounded", CondOp::IsFalse}, C{"Climbing", CondOp::IsFalse}, C{"Swimming", CondOp::IsFalse}, C{"VerticalSpeed", CondOp::Greater, 0.f}}, 0.1f);
    m.addTransition(sJump, sFall, {C{"VerticalSpeed", CondOp::Less, 0.f}}, 0.2f);
    m.addTransition(sFall, sIdle, {C{"Grounded", CondOp::IsTrue}}, 0.1f);
    // Attacco: trigger, torna a Idle a fine animazione.
    m.addTransition(-1, sAttack, {C{"Attack", CondOp::Trigger}}, 0.08f);
    m.addTransition(sAttack, sIdle, {}, 0.15f, 1.f);
    m.current = sIdle;
    float x = 0.f;
    for (auto& s : m.states) { s.editorPos = {x, 0.f}; x += 160.f; }
    return m;
}

} // namespace anim
} // namespace blk
