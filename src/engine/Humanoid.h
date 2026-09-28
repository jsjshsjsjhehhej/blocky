#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

/**
 * Trasformazione di animazione di una singola parte del corpo.
 *
 * E AGGIUNTIVA rispetto alla posa di riposo: pos si somma all offset, rot si
 * somma alle rotazioni gia esistenti, scale moltiplica la dimensione. Cosi le
 * animazioni che c erano gia (camminata, salto, oggetto in mano) continuano a
 * funzionare identiche, e una clip di keyframe si sovrappone senza doverle
 * riscrivere.
 *
 * rot e in GRADI, non radianti: e quello che l utente digita nell editor, e
 * convertirlo una volta sola al momento di disegnare evita numeri illeggibili
 * nell interfaccia e negli file salvati.
 */
struct RigPose {
    glm::vec3 pos{0.f};
    glm::vec3 rot{0.f};
    glm::vec3 scale{1.f};

    bool isIdentity() const {
        return pos == glm::vec3(0.f) && rot == glm::vec3(0.f) && scale == glm::vec3(1.f);
    }
};

inline RigPose rigPoseLerp(const RigPose& a, const RigPose& b, float t) {
    RigPose r;
    r.pos   = a.pos   + (b.pos   - a.pos)   * t;
    r.rot   = a.rot   + (b.rot   - a.rot)   * t;
    r.scale = a.scale + (b.scale - a.scale) * t;
    return r;
}

struct HumanoidPartDef {
    std::string name;
    glm::vec3 offset{0};
    glm::vec3 size{1};
    glm::vec3 color{1};
    glm::vec3 pivot{0};
    bool isLimb = false;
    float rotX = 0, rotZ = 0;
    // Posa dei keyframe, sovrapposta a rotX/rotZ invece che al posto loro.
    RigPose anim;
    int parent = -1;
};

struct Humanoid {
    float health = 100.f;
    float maxHealth = 100.f;
    bool  dead = false;
    float deathTimer = 0.f;
    float walkSpeed = 16.f;
    float jumpPower = 52.f;
    float walkPhase = 0.f;
    bool rigEnabled = true;
    bool userRigged = false;

    float velocityY = 0.f;
    bool  grounded  = true;
    float jumpArmBlend = 0.f;
    bool poseIsJump = false;
    float jumpAnimTime = 0.f;

    float healthPct() const {
        if (maxHealth <= 1e-4f) return 0.f;
        return std::clamp(health / maxHealth, 0.f, 1.f);
    }

    void takeDamage(float dmg) {
        if (dead || dmg <= 0.f) return;
        health = std::max(0.f, health - dmg);
        if (health <= 0.f) {
            health = 0.f;
            dead = true;
            deathTimer = 0.f;
        }
    }

    void heal(float amount) {
        if (dead || amount <= 0.f) return;
        health = std::min(maxHealth, health + amount);
    }

    void tickDeath(float dt) {
        if (dead) deathTimer += dt;
    }

    bool shouldRespawn() const { return dead && deathTimer >= 3.f; }

    void respawnFull() {
        dead = false;
        deathTimer = 0.f;
        health = maxHealth;
        velocityY = 0.f;
        grounded = true;
    }

    std::vector<HumanoidPartDef> parts;

    static Humanoid createR6Blocky() {
        Humanoid h;
        const glm::vec3 white{0.95f, 0.95f, 0.97f};
        const glm::vec3 blue{0.18f, 0.45f, 0.92f};
        const glm::vec3 green{0.22f, 0.72f, 0.32f};

        h.parts = {
            {"HumanoidRootPart", {0, 2.5f, 0}, {0.2f, 0.2f, 0.2f}, white, {0, 0, 0}, false},
            {"Torso",     {0, 3.0f, 0}, {2.0f, 2.0f, 1.0f}, blue,  {0, 0, 0}, false},
            {"Head",      {0, 4.7f, 0}, {1.25f, 1.25f, 1.25f}, white, {0, 0, 0}, false},
            {"Left Arm",  {-1.5f, 3.0f, 0}, {1.0f, 2.0f, 1.0f}, white, {0, 1.0f, 0}, true},
            {"Right Arm", { 1.5f, 3.0f, 0}, {1.0f, 2.0f, 1.0f}, white, {0, 1.0f, 0}, true},
            {"Left Leg",  {-0.5f, 1.0f, 0}, {1.0f, 2.0f, 1.0f}, green, {0, 1.0f, 0}, true},
            {"Right Leg", { 0.5f, 1.0f, 0}, {1.0f, 2.0f, 1.0f}, green, {0, 1.0f, 0}, true},
        };
        return h;
    }

    static Humanoid createR6Noob() { return createR6Blocky(); }
    static Humanoid createR6Bacon() { return createR6Blocky(); }

    void update(float dt, bool walking) {
        if (walking && grounded) walkPhase += dt * 11.2f;
        else walkPhase = 0.f;

        if (!grounded) {
            jumpAnimTime += dt;
            if (velocityY < -2.f && jumpAnimTime < 0.48f)
                jumpAnimTime = glm::mix(jumpAnimTime, 0.55f, std::min(1.f, 10.f * dt));
            jumpArmBlend += (1.f - jumpArmBlend) * std::min(1.f, 18.f * dt);
        } else {
            jumpArmBlend += (0.f - jumpArmBlend) * std::min(1.f, 12.f * dt);
            if (jumpArmBlend < 0.02f) {
                jumpArmBlend = 0.f;
                jumpAnimTime = 0.f;
            }
        }
    }

    bool tryJump() {
        if (dead || !grounded) return false;
        velocityY = jumpPower * 0.55f;
        grounded = false;
        jumpAnimTime = 0.f;
        jumpArmBlend = std::max(jumpArmBlend, 0.35f);
        return true;
    }

    void stepPhysics(float dt, float& posY, float groundY = 0.f) {
        const float gravity = 80.f;
        if (!grounded || velocityY > 0.f) {
            velocityY -= gravity * dt;
            posY += velocityY * dt;
        }
        if (posY <= groundY) {
            posY = groundY;
            velocityY = 0.f;
            grounded = true;
        } else {
            grounded = false;
        }
    }

    std::vector<std::string> partNames() const {
        std::vector<std::string> n;
        n.reserve(parts.size() + 1);
        n.push_back("Humanoid");
        for (const auto& p : parts) n.push_back(p.name);
        return n;
    }
};

inline float humanoidLegSwing(const Humanoid& h) {
    if (!h.grounded) return 0.f;
    float s = std::sin(h.walkPhase);
    float soft = std::copysign(std::pow(std::fabs(s), 0.94f), s);
    return soft * 48.f;
}
inline float humanoidArmSwing(const Humanoid& h) {
    if (!h.grounded) return 0.f;
    float s = std::sin(h.walkPhase + 0.18f);
    float soft = std::copysign(std::pow(std::fabs(s), 0.94f), s);
    return -soft * 68.f;
}
inline float humanoidBob(const Humanoid& h) {
    if (!h.grounded) return 0.f;
    return std::fabs(std::cos(h.walkPhase)) * 0.11f;
}
inline void humanoidJumpPose(const Humanoid& h,
                             float& armL, float& armR,
                             float& legL, float& legR) {
    static const float k[][5] = {
        { 0.00f,   -8.f,  -12.f,    6.f,   -4.f },
        { 0.05f,  -55.f,  -70.f,    4.f,   -2.f },
        { 0.11f, -125.f, -145.f,    2.f,   -2.f },
        { 0.18f, -168.f, -178.f,    0.f,    0.f },
        { 0.26f, -180.f, -180.f,    0.f,    0.f },
        { 0.36f, -165.f, -160.f,    0.f,    0.f },
        { 0.48f, -105.f,  -98.f,    2.f,   -2.f },
        { 0.60f,  -52.f,  -45.f,    2.f,   -2.f },
        { 0.75f,  -22.f,  -18.f,    2.f,   -2.f },
        { 0.92f,   -8.f,   -8.f,    0.f,    0.f },
    };
    (void)k;

    const float v = h.velocityY;
    const float rise = std::min(1.f, std::max(0.f,  v / 26.f));
    const float fall = std::min(1.f, std::max(0.f, -v / 30.f));

    const float arms = -32.f - 128.f * rise - 46.f * fall;
    armL = arms;
    armR = arms * 1.05f;

    legL =  12.f + 34.f * rise - 24.f * fall;
    legR = -14.f + 20.f * rise - 16.f * fall;
}

inline float humanoidJumpArmRaise(const Humanoid& h) {
    float aL = 0.f, aR = 0.f, lL = 0.f, lR = 0.f;
    humanoidJumpPose(h, aL, aR, lL, lR);
    return 0.5f * (aL + aR);
}
