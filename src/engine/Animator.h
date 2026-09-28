#pragma once


#include "Humanoid.h"
#include "R6WalkAnim.h"
#include "AnimLibrary.h"
#include "ScriptControls.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

enum class AnimClip {
    Idle = 0,
    Walk,
    Wave,
    Dance,
    Jump,
    Custom,
    Count
};

inline const char* animClipName(AnimClip c) {
    switch (c) {
    case AnimClip::Idle: return "Idle";
    case AnimClip::Walk: return "Walk";
    case AnimClip::Wave: return "Wave";
    case AnimClip::Dance: return "Dance";
    case AnimClip::Jump: return "Jump";
    case AnimClip::Custom: return "Custom";
    default: return "Idle";
    }
}

inline AnimClip animClipFromName(const std::string& n) {
    std::string s = n;
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (s == "walk" || s == "run") return AnimClip::Walk;
    if (s == "wave" || s == "hi" || s == "hello") return AnimClip::Wave;
    if (s == "dance" || s == "disco") return AnimClip::Dance;
    if (s == "jump" || s == "fall") return AnimClip::Jump;
    if (s == "idle" || s == "stop" || s == "none") return AnimClip::Idle;
    return AnimClip::Custom;
}

class Animator {
public:
    AnimClip clip = AnimClip::Idle;
    float time = 0.f;
    float weight = 1.f;
    bool playing = true;
    std::string lastPlayed = "Idle";

    R6WalkClip walkOverride;
    bool hasWalkOverride = false;
    float walkPhase = 0.f;

    R6WalkClip jumpOverride;
    bool hasJumpOverride = false;

    R6WalkClip customClip;
    bool hasCustomClip = false;
    AnimClip lastActive = AnimClip::Idle;
    float customPhase = 0.f;
    bool customFinished = false;

    void setWalkOverride(const R6WalkClip& c) {
        walkOverride = c;
        hasWalkOverride = c.valid();
        walkPhase = 0.f;
    }

    void clearWalkOverride() {
        walkOverride = R6WalkClip{};
        hasWalkOverride = false;
        walkPhase = 0.f;
    }

    /**
     * Rifiuta una clip di camminata: e' il bug capitato stasera. setWalkAnimId
     * gia' rifiuta una clip di salto (riga sotto, c->kind == R6AnimKind::Jump)
     * ma il lato opposto non c'era — assegnare "Jump" a una clip Walk dal
     * catalogo (Studio) passava senza controlli, forzava kind=Jump sull'
     * ETICHETTA ma i dati restavano quelli della camminata: l'animatore
     * diceva "Jump" ovunque (F3, log) mentre disegnava il ciclo di passi.
     */
    void setJumpOverride(const R6WalkClip& c) {
        if (c.kind == R6AnimKind::Walk) {
            clearJumpOverride();
            return;
        }
        jumpOverride = c;
        hasJumpOverride = c.valid();
        if (hasJumpOverride) {
            jumpOverride.kind = R6AnimKind::Jump;
            jumpOverride.timeInSeconds = true;
            jumpOverride.looped = false;
        }
    }

    void clearJumpOverride() {
        jumpOverride = R6WalkClip{};
        hasJumpOverride = false;
    }

    static bool looksLikeToolAnim(const R6WalkClip& c) {
        if (c.kind == R6AnimKind::Tool) return true;
        if (c.kind == R6AnimKind::Jump) return false;
        auto low = [](std::string s) {
            for (char& ch : s)
                if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
            return s;
        };
        std::string id = low(c.id), dn = low(c.displayName), cn = low(c.clipName);
        auto hit = [&](const std::string& s) {
            return s.find("pistol") != std::string::npos || s.find("pistola") != std::string::npos
                || s.find("shoot") != std::string::npos || s.find("gun") != std::string::npos
                || s.find("fire") != std::string::npos || s.find("aim") != std::string::npos
                || s.find("reload") != std::string::npos || s.find("sword") != std::string::npos
                || s.find("tool") != std::string::npos || s.find("armatura") != std::string::npos
                || s.find("hold") != std::string::npos;
        };
        if (hit(id) || hit(dn) || hit(cn)) return true;
        if (!c.looped) {
            if (dn.find("walk") == std::string::npos && cn.find("walk") == std::string::npos
                && id.find("walk") == std::string::npos && dn.find("run") == std::string::npos
                && id.find("run") == std::string::npos
                && dn.find("jump") == std::string::npos && id.find("jump") == std::string::npos)
                return true;
        }
        return false;
    }

    void play(const std::string& name) {
        AnimClip built = animClipFromName(name);
        if (built != AnimClip::Custom) {
            clip = built;
            lastPlayed = animClipName(built);
            hasCustomClip = false;
            time = 0.f;
            playing = true;
            weight = 1.f;
            return;
        }
        const R6WalkClip* c = animLibrary().get(name);
        if (!c) c = animLibrary().resolve(name);
        if (!c) c = animLibrary().get(sanitizeGameId(name));
        if (!c) {
            clip = AnimClip::Idle;
            lastPlayed = name;
            hasCustomClip = false;
            return;
        }
        customClip = *c;
        hasCustomClip = customClip.valid();
        customPhase = 0.f;
        customFinished = false;
        clip = AnimClip::Custom;
        lastPlayed = name;
        time = 0.f;
        playing = true;
        weight = 1.f;
    }

    void setWalkAnimId(const std::string& animId) {
        const R6WalkClip* c = animLibrary().get(animId);
        if (!c) c = animLibrary().get(sanitizeGameId(animId));
        if (!c) c = animLibrary().resolve(animId);
        if (c && (looksLikeToolAnim(*c) || c->kind == R6AnimKind::Jump)) {
            clearWalkOverride();
            return;
        }
        animLibrary().setDefaultWalk(animId);
        if (c) setWalkOverride(*c);
        else clearWalkOverride();
    }

    void setJumpAnimId(const std::string& animId) {
        const R6WalkClip* c = animLibrary().get(animId);
        if (!c) c = animLibrary().get(sanitizeGameId(animId));
        if (!c) c = animLibrary().resolve(animId);
        if (c && looksLikeToolAnim(*c) && c->kind != R6AnimKind::Jump) {
            clearJumpOverride();
            return;
        }
        animLibrary().setDefaultJump(animId);
        if (c) setJumpOverride(*c);
        else clearJumpOverride();
    }

    void stop() {
        clip = AnimClip::Idle;
        lastPlayed = "Idle";
        playing = true;
        time = 0.f;
        hasCustomClip = false;
        customFinished = false;
    }

    void apply(Humanoid& h, float dt, bool isWalking) {
        if (!playing) { h.poseIsJump = false; return; }
        time += dt;

        if (!hasWalkOverride && !animLibrary().defaultWalkId.empty()) {
            if (const R6WalkClip* w = animLibrary().get(animLibrary().defaultWalkId)) {
                if (!looksLikeToolAnim(*w) && w->kind != R6AnimKind::Jump)
                    setWalkOverride(*w);
            }
        }
        if (hasWalkOverride && (looksLikeToolAnim(walkOverride) || walkOverride.kind == R6AnimKind::Jump))
            clearWalkOverride();
        // Simmetrico al controllo sopra: una jumpOverride rimasta di tipo Walk
        // (es. da un anim_library.json salvato prima di questo controllo)
        // va scartata qui, non solo bloccata all'assegnazione futura.
        if (hasJumpOverride && jumpOverride.kind == R6AnimKind::Walk)
            clearJumpOverride();

        if (!hasJumpOverride && !animLibrary().defaultJumpId.empty()) {
            if (const R6WalkClip* j = animLibrary().get(animLibrary().defaultJumpId))
                setJumpOverride(*j);
        }

        auto& sc = scriptControls();
        if (sc.walkSpeedOverride >= 0.f) h.walkSpeed = sc.walkSpeedOverride;
        if (sc.jumpPowerOverride >= 0.f) h.jumpPower = sc.jumpPowerOverride;

        AnimClip active = clip;
        if (clip == AnimClip::Idle && isWalking && h.grounded && sc.walkEnabled)
            active = AnimClip::Walk;
        const bool customOneShot = hasCustomClip && !customFinished && !customClip.looped;
        if (!h.grounded && !customOneShot)
            active = AnimClip::Jump;

        if (hasCustomClip && clip == AnimClip::Custom && !customFinished
            && active != AnimClip::Jump) {
            float dur = std::max(0.05f, customClip.duration);
            customPhase += dt / dur;
            if (customClip.looped) {
                float t = customPhase - std::floor(customPhase);
                float sampleT = customClip.timeInSeconds ? t * dur : t;
                R6WalkKey k = r6SampleClip(customClip, sampleT);
                r6ApplyKeyToHumanoid(h, k, std::clamp(weight, 0.f, 1.f));
                return;
            }
            if (customPhase < 1.f) {
                float sampleT = customClip.timeInSeconds ? customPhase * dur : customPhase;
                R6WalkKey k = r6SampleClip(customClip, sampleT);
                r6ApplyKeyToHumanoid(h, k, std::clamp(weight, 0.f, 1.f));
                return;
            }
            customFinished = true;
            hasCustomClip = false;
            clip = AnimClip::Idle;
            for (auto& p : h.parts) {
                if (p.isLimb) { p.rotX = 0.f; p.rotZ = 0.f; }
            }
            if (isWalking && h.grounded && sc.walkEnabled)
                active = AnimClip::Walk;
        }

        lastActive = active;
        h.poseIsJump = (active == AnimClip::Jump);
        float w = std::clamp(weight, 0.f, 1.f);

        if (active == AnimClip::Walk && hasWalkOverride && walkOverride.valid()
            && walkOverride.looped && !looksLikeToolAnim(walkOverride)
            && walkOverride.kind != R6AnimKind::Jump) {
            float dur = std::max(0.05f, walkOverride.duration);
            float speedScale = std::clamp(h.walkSpeed / 16.f, 0.4f, 2.5f);
            walkPhase += (dt / dur) * speedScale;
            walkPhase -= std::floor(walkPhase);
            float sampleT = walkOverride.timeInSeconds ? walkPhase * dur : walkPhase;
            R6WalkKey k = r6SampleClip(walkOverride, sampleT);
            r6ApplyKeyToHumanoid(h, k, w);
            return;
        }

        if (active != AnimClip::Walk)
            walkPhase = 0.f;

        float leg = 0.f, armL = 0.f, armR = 0.f, armZ = 0.f;

        switch (active) {
        case AnimClip::Walk: {
            float ph = h.walkPhase;
            float sLeg = std::sin(ph);
            float sArm = std::sin(ph + 0.18f);
            auto soft = [](float s) {
                return std::copysign(std::pow(std::fabs(s), 0.94f), s);
            };
            leg = soft(sLeg) * 48.f;
            armL = -soft(sArm) * 68.f;
            armR = soft(sArm) * 68.f * 1.03f;
            break;
        }
        case AnimClip::Wave: {
            float ph = time * 10.f;
            armR = -20.f + std::sin(ph) * 55.f;
            armZ = std::sin(ph * 0.5f) * 15.f;
            break;
        }
        case AnimClip::Dance: {
            float ph = time * 8.f;
            leg = std::sin(ph) * 25.f;
            armL = std::sin(ph + 1.f) * 70.f - 40.f;
            armR = std::sin(ph + 2.f) * 70.f - 40.f;
            armZ = std::cos(ph) * 20.f;
            break;
        }
        case AnimClip::Jump: {
            float ease = std::clamp(h.jumpArmBlend, 0.f, 1.f);
            if (!h.grounded) ease = std::max(ease, 0.75f);
            const float blend = ease * w;
            if (hasJumpOverride && jumpOverride.valid()) {
                R6WalkKey k = r6SampleClip(jumpOverride, h.jumpAnimTime);
                r6ApplyKeyToHumanoid(h, k, blend);
                return;
            }
            float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
            humanoidJumpPose(h, jL, jR, jl, jr);
            for (auto& p : h.parts) {
                if (p.name == "Left Leg") p.rotX = jl * blend;
                else if (p.name == "Right Leg") p.rotX = jr * blend;
                else if (p.name == "Left Arm") {
                    p.rotX = jL * blend;
                    p.rotZ = 0.f;
                } else if (p.name == "Right Arm") {
                    p.rotX = jR * blend;
                    p.rotZ = 0.f;
                }
            }
            return;
        }
        case AnimClip::Idle:
        case AnimClip::Custom:
        default: {
            float ph = time * 1.5f;
            armL = std::sin(ph) * 3.f;
            armR = -std::sin(ph) * 3.f;
            break;
        }
        }

        for (auto& p : h.parts) {
            if (p.name == "Left Leg")
                p.rotX = leg * w;
            else if (p.name == "Right Leg")
                p.rotX = -leg * w;
            else if (p.name == "Left Arm") {
                p.rotX = armL * w;
                p.rotZ = -armZ * w;
            } else if (p.name == "Right Arm") {
                p.rotX = armR * w;
                p.rotZ = armZ * w;
            }
        }
    }
};
