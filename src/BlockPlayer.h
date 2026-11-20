#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

#include "engine/Humanoid.h"

using DrawCubeFn = void(*)(GLuint, GLuint, const glm::mat4&, const glm::mat4&, const glm::mat4&,
                           const glm::vec3&, const glm::vec3&);

struct BlockDrawContext {
    GLuint cubeVAO;
    GLuint partProg;
    const glm::mat4& view;
    const glm::mat4& proj;
    const glm::vec3& sunDir;
};

class BlockPlayer {
public:
    Humanoid humanoid = Humanoid::createR6Blocky();

    void update(float dt, bool walking) { humanoid.update(dt, walking); }

    void draw(const BlockDrawContext& ctx, DrawCubeFn drawCube,
              glm::vec3 worldPos, float yawDeg,
              const bool* hideSlot = nullptr, float rightArmHold = 0.f,
              const std::vector<HoldPartPose>* holdParts = nullptr) const;

    void drawPreview(const BlockDrawContext& ctx, DrawCubeFn drawCube,
                     glm::vec3 worldPos, float yawDeg, float spinDeg,
                     const bool* hideSlot = nullptr) const {
        draw(ctx, drawCube, worldPos, yawDeg + spinDeg, hideSlot);
    }

private:
    static glm::mat4 partTransform(glm::mat4 root, glm::vec3 c, glm::vec3 s,
                                   glm::vec3 pivot, float rx, float rz,
                                   const RigPose& pose) {
        // L ordine conta: prima si va nel punto giusto, poi si ruota attorno al
        // perno, poi si torna indietro e si scala. Ruotare dopo aver scalato
        // storcerebbe la parte invece di girarla.
        glm::mat4 m = glm::translate(root, c + pivot + pose.pos);
        const float ax = rx + pose.rot.x;
        const float ay = pose.rot.y;
        const float az = rz + pose.rot.z;
        if (ax != 0) m = glm::rotate(m, glm::radians(ax), glm::vec3(1, 0, 0));
        if (ay != 0) m = glm::rotate(m, glm::radians(ay), glm::vec3(0, 1, 0));
        if (az != 0) m = glm::rotate(m, glm::radians(az), glm::vec3(0, 0, 1));
        m = glm::translate(m, -pivot);
        return glm::scale(m, s * pose.scale);
    }

    static void drawPart(const BlockDrawContext& ctx, DrawCubeFn fn, glm::mat4 root,
                         glm::vec3 c, glm::vec3 s, glm::vec3 col,
                         glm::vec3 pivot = {}, float rx = 0.f, float rz = 0.f,
                         const RigPose& pose = RigPose{}) {
        fn(ctx.cubeVAO, ctx.partProg, partTransform(root, c, s, pivot, rx, rz, pose),
           ctx.view, ctx.proj, col, ctx.sunDir);
    }

    /** Come partTransform ma senza la scala: porta i punti della testa (dal suo centro) nel mondo. */
    static glm::mat4 partRigid(glm::mat4 root, glm::vec3 c, glm::vec3 pivot, float rx, float rz, const RigPose& pose) {
        glm::mat4 m = glm::translate(root, c + pivot + pose.pos);
        const float ax = rx + pose.rot.x, ay = pose.rot.y, az = rz + pose.rot.z;
        if (ax != 0) m = glm::rotate(m, glm::radians(ax), glm::vec3(1, 0, 0));
        if (ay != 0) m = glm::rotate(m, glm::radians(ay), glm::vec3(0, 1, 0));
        if (az != 0) m = glm::rotate(m, glm::radians(az), glm::vec3(0, 0, 1));
        return glm::translate(m, -pivot);
    }

    // head = trasformazione della testa (centro della testa): la faccia la segue
    // quando la testa si gira o si alza (animazioni R6), invece di restare ferma.
    static void drawSmileFace(const BlockDrawContext& ctx, DrawCubeFn fn, const glm::mat4& head) {
        const glm::vec3 black{0.05f, 0.05f, 0.05f};
        const float z = 0.64f;

        drawPart(ctx, fn, head, {-0.28f, 0.12f, z}, {0.18f, 0.18f, 0.06f}, black);
        drawPart(ctx, fn, head, { 0.28f, 0.12f, z}, {0.18f, 0.18f, 0.06f}, black);

        drawPart(ctx, fn, head, {-0.32f, -0.22f, z}, {0.12f, 0.14f, 0.05f}, black);
        drawPart(ctx, fn, head, {-0.18f, -0.32f, z}, {0.14f, 0.10f, 0.05f}, black);
        drawPart(ctx, fn, head, { 0.00f, -0.36f, z}, {0.22f, 0.10f, 0.05f}, black);
        drawPart(ctx, fn, head, { 0.18f, -0.32f, z}, {0.14f, 0.10f, 0.05f}, black);
        drawPart(ctx, fn, head, { 0.32f, -0.22f, z}, {0.12f, 0.14f, 0.05f}, black);
    }
};

inline void BlockPlayer::draw(const BlockDrawContext& ctx, DrawCubeFn drawCube,
                              glm::vec3 worldPos, float yawDeg,
                              const bool* hideSlot, float rightArmHold,
                              const std::vector<HoldPartPose>* holdParts) const {
    const float walkLeg = humanoidLegSwing(humanoid);
    const float walkArm = humanoidArmSwing(humanoid);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(humanoid, jL, jR, jl, jr);
    const float jb = humanoid.jumpArmBlend;
    const float bob = humanoid.defaultAnimActive ? 0.f : humanoidBob(humanoid);

    glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), worldPos),
                                 glm::radians(yawDeg), glm::vec3(0, 1, 0));
    if (humanoid.dead) {
        root = glm::translate(glm::mat4(1.f), worldPos + glm::vec3(0.f, 0.85f, 0.f));
        root = glm::rotate(root, glm::radians(yawDeg), glm::vec3(0, 1, 0));
        root = glm::rotate(root, glm::radians(80.f), glm::vec3(1, 0, 0));
    }

    float headY = 4.7f;

    auto slotOf = [](const std::string& name) -> int {
        if (name == "Head") return 0;
        if (name == "Torso") return 1;
        if (name == "Left Arm") return 2;
        if (name == "Right Arm") return 3;
        if (name == "Left Leg") return 4;
        if (name == "Right Leg") return 5;
        return -1;
    };

    for (const auto& p : humanoid.parts) {
        if (p.name == "HumanoidRootPart") continue;
        int si = slotOf(p.name);
        if (hideSlot && si >= 0 && hideSlot[si])
            continue;

        glm::vec3 col = p.color;
        if (humanoid.shirtOn && (p.name == "Torso" || p.name == "Left Arm" || p.name == "Right Arm"))
            col = humanoid.shirtColor;
        if (humanoid.pantsOn && (p.name == "Left Leg" || p.name == "Right Leg"))
            col = humanoid.pantsColor;

        glm::vec3 c = p.offset;
        if (p.name == "Torso" || p.name == "Head") c.y += bob;

        // Posa custom dell'item: vince su camminata/salto per quella parte.
        if (const HoldPartPose* hp = holdParts ? findHoldPose(*holdParts, p.name) : nullptr) {
            // La posa dell'item sostituisce quella delle animazioni di base (non si sommano).
            RigPose pose = p.animFromDefault ? RigPose{} : p.anim;
            pose.pos += hp->pos;
            pose.rot.y += hp->rot.y;
            drawPart(ctx, drawCube, root, c, p.size, col, p.pivot,
                     hp->rot.x, hp->rot.z, pose);
            if (p.name == "Head")
                drawSmileFace(ctx, drawCube, partRigid(root, c, p.pivot, hp->rot.x, hp->rot.z, pose));
            continue;
        }

        float rx = p.rotX;
        float rz = p.rotZ;

        const bool jumping = !humanoid.grounded || jb > 0.05f;
        const bool animDriven = (std::fabs(p.rotX) + std::fabs(p.rotZ)) > 0.01f;

        if (jumping && !humanoid.poseIsJump) {
            if (p.name == "Left Leg") rx = jl;
            else if (p.name == "Right Leg") rx = jr;
            else if (p.name == "Left Arm") rx = jL;
            else if (p.name == "Right Arm") rx = jR;
            if (p.isLimb) rz = 0.f;
        } else if (!animDriven && !jumping) {
            if (p.name == "Left Leg") rx = walkLeg;
            else if (p.name == "Right Leg") rx = -walkLeg;
            else if (p.name == "Left Arm")
                rx = walkArm;
            else if (p.name == "Right Arm")
                rx = (rightArmHold != 0.f) ? rightArmHold : -walkArm;
        }

        // Un oggetto equipaggiato alza il braccio destro SEMPRE, anche mentre
        // una clip sta guidando le braccia. Prima la posa stava dentro il ramo
        // "nessuna animazione attiva", quindi con la R6 Walk caricata (che e la
        // camminata predefinita) equipaggiare non faceva scattare proprio
        // nulla: il braccio restava quello del ciclo di passo.
        if (rightArmHold != 0.f && p.name == "Right Arm" && !jumping)
            rx = rightArmHold;

        // Un'animazione degli script (AnimationTrack) su questa parte prende il
        // posto della camminata/salto procedurali, in proporzione al suo peso.
        if (p.animWeight > 0.f) {
            const float keep = 1.f - std::min(1.f, p.animWeight);
            rx *= keep;
            rz *= keep;
        }

        drawPart(ctx, drawCube, root, c, p.size, col, p.pivot, rx, rz, p.anim);
        if (p.name == "Head")
            drawSmileFace(ctx, drawCube, partRigid(root, c, p.pivot, rx, rz, p.anim));
    }
    if (humanoid.hatOn)
        drawPart(ctx, drawCube, root, {0.f, 5.55f, 0.f}, {1.35f, 0.42f, 1.35f}, humanoid.hatColor);
}
