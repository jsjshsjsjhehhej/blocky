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
              const bool* hideSlot = nullptr, float rightArmHold = 0.f) const;

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

    static void drawSmileFace(const BlockDrawContext& ctx, DrawCubeFn fn,
                              glm::mat4 root, float headY) {
        const glm::vec3 black{0.05f, 0.05f, 0.05f};
        const float z = 0.64f;

        drawPart(ctx, fn, root, {-0.28f, headY + 0.12f, z}, {0.18f, 0.18f, 0.06f}, black);
        drawPart(ctx, fn, root, { 0.28f, headY + 0.12f, z}, {0.18f, 0.18f, 0.06f}, black);

        drawPart(ctx, fn, root, {-0.32f, headY - 0.22f, z}, {0.12f, 0.14f, 0.05f}, black);
        drawPart(ctx, fn, root, {-0.18f, headY - 0.32f, z}, {0.14f, 0.10f, 0.05f}, black);
        drawPart(ctx, fn, root, { 0.00f, headY - 0.36f, z}, {0.22f, 0.10f, 0.05f}, black);
        drawPart(ctx, fn, root, { 0.18f, headY - 0.32f, z}, {0.14f, 0.10f, 0.05f}, black);
        drawPart(ctx, fn, root, { 0.32f, headY - 0.22f, z}, {0.12f, 0.14f, 0.05f}, black);
    }
};

inline void BlockPlayer::draw(const BlockDrawContext& ctx, DrawCubeFn drawCube,
                              glm::vec3 worldPos, float yawDeg,
                              const bool* hideSlot, float rightArmHold) const {
    const float walkLeg = humanoidLegSwing(humanoid);
    const float walkArm = humanoidArmSwing(humanoid);
    float jL = 0.f, jR = 0.f, jl = 0.f, jr = 0.f;
    humanoidJumpPose(humanoid, jL, jR, jl, jr);
    const float jb = humanoid.jumpArmBlend;
    const float bob = humanoidBob(humanoid);

    glm::mat4 root = glm::rotate(glm::translate(glm::mat4(1.f), worldPos),
                                 glm::radians(yawDeg), glm::vec3(0, 1, 0));

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

        glm::vec3 c = p.offset;
        if (p.name == "Torso" || p.name == "Head") c.y += bob;

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

        drawPart(ctx, drawCube, root, c, p.size, p.color, p.pivot, rx, rz, p.anim);
        if (p.name == "Head")
            drawSmileFace(ctx, drawCube, root, headY + bob);
    }
}
