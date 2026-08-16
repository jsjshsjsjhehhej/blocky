#pragma once
/**
 * Minimal renderer — bind shader, set uniforms, draw mesh.
 * Not a full game loop: only the draw building blocks.
 */

#include "shader.h"
#include "mesh.h"
#include "math.h"

struct DrawItem {
    const Mesh* mesh = nullptr;
    Mat4        model;
    Vec3        color{ 0.64f, 0.64f, 0.64f }; // default Plastic
};

struct FrameUniforms {
    Mat4 view;
    Mat4 proj;
    Vec3 sunDir{ -0.4f, 1.0f, -0.3f }; // toward sun (will be normalized)
    Vec3 camPos{ 0, 0, 0 };
};

/** Clear color + depth (call once per frame before draws). */
void rendererBeginFrame();

/**
 * Draw one mesh with the plastic shader.
 * Shader must already be linked; this binds it and sets uniforms.
 */
void rendererDraw(const Shader& shader, const FrameUniforms& frame, const DrawItem& item);

/**
 * Minimal render-frame skeleton (NOT a full application loop).
 *
 * Typical usage pattern:
 *
 *   FrameUniforms f;
 *   f.view   = camera.viewMatrix();
 *   f.proj   = perspective(camera.fov, aspect, 0.1f, 500.f);
 *   f.camPos = camera.position;
 *   f.sunDir = normalize(Vec3{-0.4f, 1.f, -0.3f});
 *
 *   rendererBeginFrame();
 *   rendererDraw(plastic, f, baseplateItem);
 *   rendererDraw(plastic, f, brickItem);
 *   windowSwap(app);
 */
void rendererDrawSceneSkeleton(const Shader& plastic,
                               const FrameUniforms& frame,
                               const DrawItem* items,
                               int count);
