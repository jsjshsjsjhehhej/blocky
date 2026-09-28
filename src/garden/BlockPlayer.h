#pragma once


#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

struct BlockDrawContext {
    GLuint cubeVAO;
    GLuint partProg;
    const glm::mat4& view;
    const glm::mat4& proj;
    const glm::vec3& sunDir;
};

class BlockPlayer {
public:
    float walkPhase = 0.0f;

    void update(float dt, bool walking) {
        if (walking) walkPhase += dt * 11.0f;
        else walkPhase *= 0.85f;
    }

    void draw(const BlockDrawContext& ctx,
              void (*drawCube)(GLuint, GLuint, const glm::mat4&, const glm::mat4&, const glm::mat4&,
                               const glm::vec3&, const glm::vec3&),
              glm::vec3 worldPos, float yawDeg) const
    {
        const float legSwing  = std::sin(walkPhase) * 38.0f;
        const float armSwing  = -legSwing;
        const float bob       = std::fabs(std::cos(walkPhase)) * 0.12f;
        const float hairSway  = std::sin(walkPhase * 0.5f) * 6.0f;

        glm::mat4 root = glm::translate(glm::mat4(1.0f), worldPos);
        root = glm::rotate(root, glm::radians(yawDeg), glm::vec3(0, 1, 0));

        const glm::vec3 yellow { 0.96f, 0.80f, 0.24f };
        const glm::vec3 blue   { 0.05f, 0.45f, 0.72f };
        const glm::vec3 green  { 0.18f, 0.55f, 0.24f };
        const glm::vec3 bacon  { 0.48f, 0.28f, 0.12f };
        const glm::vec3 white  { 0.95f, 0.95f, 0.95f };
        const glm::vec3 black  { 0.08f, 0.08f, 0.08f };

        drawLimb(ctx, drawCube, root, glm::vec3(-0.5f, 1.0f, 0.0f), glm::vec3(1, 2, 1), green,
                 glm::vec3(0, 1.0f, 0), legSwing, 0);
        drawLimb(ctx, drawCube, root, glm::vec3( 0.5f, 1.0f, 0.0f), glm::vec3(1, 2, 1), green,
                 glm::vec3(0, 1.0f, 0), -legSwing, 0);

        drawPart(ctx, drawCube, root,
                 glm::vec3(0.0f, 3.0f + bob, 0.0f), glm::vec3(2, 2, 1), blue);

        drawPart(ctx, drawCube, root,
                 glm::vec3(0.0f, 3.0f + bob, 0.52f), glm::vec3(1.4f, 0.35f, 0.08f),
                 glm::vec3(0.15f, 0.65f, 0.88f));

        drawLimb(ctx, drawCube, root, glm::vec3(-1.5f, 3.0f + bob, 0.0f), glm::vec3(1, 2, 1), yellow,
                 glm::vec3(0, 1.0f, 0), armSwing, 0);
        drawLimb(ctx, drawCube, root, glm::vec3( 1.5f, 3.0f + bob, 0.0f), glm::vec3(1, 2, 1), yellow,
                 glm::vec3(0, 1.0f, 0), -armSwing, 0);

        const float headY = 4.75f + bob;
        drawPart(ctx, drawCube, root, glm::vec3(0.0f, headY, 0.0f), glm::vec3(1.6f, 1.5f, 1.5f), yellow);

        drawPart(ctx, drawCube, root, glm::vec3(-0.35f, headY + 0.15f, 0.78f), glm::vec3(0.28f, 0.28f, 0.08f), white);
        drawPart(ctx, drawCube, root, glm::vec3( 0.35f, headY + 0.15f, 0.78f), glm::vec3(0.28f, 0.28f, 0.08f), white);
        drawPart(ctx, drawCube, root, glm::vec3(-0.35f, headY + 0.15f, 0.84f), glm::vec3(0.14f, 0.14f, 0.05f), black);
        drawPart(ctx, drawCube, root, glm::vec3( 0.35f, headY + 0.15f, 0.84f), glm::vec3(0.14f, 0.14f, 0.05f), black);
        drawPart(ctx, drawCube, root, glm::vec3(0.0f, headY - 0.25f, 0.80f), glm::vec3(0.55f, 0.12f, 0.06f), black);

        const float top = headY + 0.75f;
        for (int i = 0; i < 5; ++i) {
            float off = -0.55f + i * 0.28f;
            float sway = (i - 2) * 1.5f + hairSway;
            drawLimb(ctx, drawCube, root,
                     glm::vec3(off, top - 0.05f, 0.30f),
                     glm::vec3(0.32f, 0.32f, 1.35f), bacon,
                     glm::vec3(0, 0.15f, -0.5f), 32.0f + sway, 0);
        }
        drawLimb(ctx, drawCube, root,
                 glm::vec3(0.0f, top + 0.1f, 0.15f),
                 glm::vec3(0.38f, 0.38f, 1.6f), bacon,
                 glm::vec3(0, 0, -0.6f), 18.0f + hairSway * 0.5f, 0);
    }

private:
    static glm::mat4 partTransform(glm::mat4 root, glm::vec3 center, glm::vec3 size,
                                   glm::vec3 pivot, float rotXDeg, float rotZDeg) {
        glm::mat4 m = root;
        m = glm::translate(m, center + pivot);
        if (rotXDeg != 0.0f) m = glm::rotate(m, glm::radians(rotXDeg), glm::vec3(1, 0, 0));
        if (rotZDeg != 0.0f) m = glm::rotate(m, glm::radians(rotZDeg), glm::vec3(0, 0, 1));
        m = glm::translate(m, -pivot);
        m = glm::scale(m, size);
        return m;
    }

    static void drawPart(const BlockDrawContext& ctx,
                         void (*drawCube)(GLuint, GLuint, const glm::mat4&, const glm::mat4&, const glm::mat4&,
                                          const glm::vec3&, const glm::vec3&),
                         glm::mat4 root, glm::vec3 center, glm::vec3 size, glm::vec3 color) {
        glm::mat4 model = partTransform(root, center, size, glm::vec3(0), 0, 0);
        drawCube(ctx.cubeVAO, ctx.partProg, model, ctx.view, ctx.proj, color, ctx.sunDir);
    }

    static void drawLimb(const BlockDrawContext& ctx,
                         void (*drawCube)(GLuint, GLuint, const glm::mat4&, const glm::mat4&, const glm::mat4&,
                                          const glm::vec3&, const glm::vec3&),
                         glm::mat4 root, glm::vec3 center, glm::vec3 size, glm::vec3 color,
                         glm::vec3 pivot, float rotXDeg, float rotZDeg) {
        glm::mat4 model = partTransform(root, center, size, pivot, rotXDeg, rotZDeg);
        drawCube(ctx.cubeVAO, ctx.partProg, model, ctx.view, ctx.proj, color, ctx.sunDir);
    }
};
