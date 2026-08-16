#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

/**
 * Humanoid R6 Blocky — 6 cubi, zero skinning.
 * Animazioni = solo rotazione arti (codice minimo, ottimo su PC/mobile/console).
 */
struct HumanoidPartDef {
    std::string name;
    glm::vec3 offset{0};
    glm::vec3 size{1};
    glm::vec3 color{1};
    glm::vec3 pivot{0}; // pivot relativo al centro della part (spalla/anca)
    bool isLimb = false;
    float rotX = 0, rotZ = 0;
    int parent = -1;
};

struct Humanoid {
    float health = 100.f;
    float maxHealth = 100.f;
    bool  dead = false;
    float deathTimer = 0.f; // secondi da morto (respawn ~3s)
    float walkSpeed = 16.f; // default Roblox-like
    float jumpPower = 52.f;
    float walkPhase = 0.f;
    bool rigEnabled = true;
    bool userRigged = false; // true dopo conferma "Vuoi riggare il Player?"

    float velocityY = 0.f;
    bool  grounded  = true;
    float jumpArmBlend = 0.f;
    /**
     * True se chi ha scritto le rotazioni questo frame ha applicato una posa
     * di salto. Serve al disegno per sapere se deve metterla lui: in aria la
     * posa di salto deve vedersi comunque, anche se a monte qualcosa ha
     * scritto una camminata.
     */
    bool poseIsJump = false;
    /** Tempo dall'inizio salto (secondi) — guida i keyframe braccia/gambe. */
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

    /** Avatar R6 Blocky — torso blu, pantaloni verdi, testa/braccia bianchi. */
    static Humanoid createR6Blocky() {
        Humanoid h;
        const glm::vec3 white{0.95f, 0.95f, 0.97f};
        const glm::vec3 blue{0.18f, 0.45f, 0.92f};
        const glm::vec3 green{0.22f, 0.72f, 0.32f};

        // Offset = centro part in spazio personaggio (piedi a y≈0)
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

    /** Alias legacy */
    static Humanoid createR6Noob() { return createR6Blocky(); }
    static Humanoid createR6Bacon() { return createR6Blocky(); }

    void update(float dt, bool walking) {
        // ciclo ~0.56s fluido (2π / 11.2 ≈ 0.56)
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

// --- Animazioni R6 (fluido, stile Roblox) ---
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
/**
 * Jump R6: crouch → braccia SU, gambe ferme.
 */
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
    constexpr int n = 10;
    float t = std::max(0.f, h.jumpAnimTime);
    float la = k[n - 1][1], ra = k[n - 1][2], ll = k[n - 1][3], rl = k[n - 1][4];
    if (t <= k[0][0]) {
        la = k[0][1]; ra = k[0][2]; ll = k[0][3]; rl = k[0][4];
    } else {
        for (int i = 0; i + 1 < n; ++i) {
            if (t > k[i + 1][0]) continue;
            float span = k[i + 1][0] - k[i][0];
            float u = span > 1e-5f ? (t - k[i][0]) / span : 0.f;
            u = u * u * (3.f - 2.f * u);
            la = k[i][1] + (k[i + 1][1] - k[i][1]) * u;
            ra = k[i][2] + (k[i + 1][2] - k[i][2]) * u;
            ll = k[i][3] + (k[i + 1][3] - k[i][3]) * u;
            rl = k[i][4] + (k[i + 1][4] - k[i][4]) * u;
            break;
        }
    }
    const float w = std::clamp(h.jumpArmBlend, 0.f, 1.f);
    armL = la * w;
    armR = ra * w;
    legL = ll * w;
    legR = rl * w;
}

/** Compat: media braccia (accessori / fallback). */
inline float humanoidJumpArmRaise(const Humanoid& h) {
    float aL = 0.f, aR = 0.f, lL = 0.f, lR = 0.f;
    humanoidJumpPose(h, aL, aR, lL, lR);
    return 0.5f * (aL + aR);
}
