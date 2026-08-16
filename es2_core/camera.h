#pragma once
/**
 * FPS Camera — OpenGL ES 2.0 compatible (no glm).
 *
 * Angles are in DEGREES.
 * Call updateVectors() after any rotate/move that changes orientation,
 * or rely on move* which calls it when needed.
 */

#include "math.h"

struct Camera {
    Vec3 position{ 0.f, 8.f, 20.f };

    // Orientation (degrees). yaw first, then pitch.
    // yaw   =  -90 looks toward -Z (OpenGL default forward)
    // pitch =    0 looks horizontal
    float yaw   = -90.f;
    float pitch = -15.f;

    float moveSpeed = 12.f;
    float lookSens  = 0.12f; // degrees per pixel (for rotate)
    float fov       = 70.f;

    // Recomputed every updateVectors() — always normalized
    Vec3 front{ 0.f, 0.f, -1.f };
    Vec3 right{ 1.f, 0.f,  0.f };
    Vec3 up   { 0.f, 1.f,  0.f };

    /** Rebuild front/right/up from yaw + pitch. Call after rotate(). */
    void updateVectors();

    /**
     * Mouse (or stick) look.
     * dx, dy in pixels (or scaled units).
     * Positive dx → turn right; positive dy → look down (FPS style).
     */
    void rotate(float dx, float dy);

    /** W — move along front (full 3D fly). */
    void moveForward(float dt);
    /** S */
    void moveBackward(float dt);
    /** D */
    void moveRight(float dt);
    /** A */
    void moveLeft(float dt);

    /**
     * Optional: combine WASD into one move and normalize
     * so diagonal is not faster.
     * w/a/s/d = pressed flags.
     */
    void moveWASD(float dt, bool w, bool a, bool s, bool d);

    /** View matrix for shaders (uses lookAt). */
    Mat4 viewMatrix() const;
};
