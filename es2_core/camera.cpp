#include "camera.h"

#include <cmath>

static const float kDegToRad = 0.01745329251f;

void Camera::updateVectors() {
    // 1) Yaw first (around world Y), then pitch (around local X)
    //    Standard FPS spherical coords in degrees → radians.
    float yawR   = yaw   * kDegToRad;
    float pitchR = pitch * kDegToRad;

    // 2) Forward from yaw + pitch
    Vec3 f;
    f.x = std::cos(yawR) * std::cos(pitchR);
    f.y = std::sin(pitchR);
    f.z = std::sin(yawR) * std::cos(pitchR);
    front = normalize(f);

    // 3) Right = front × worldUp  (normalize)
    const Vec3 worldUp{ 0.f, 1.f, 0.f };
    right = normalize(cross(front, worldUp));

    // 4) Camera up = right × front  (normalize) — stays consistent when pitched
    up = normalize(cross(right, front));
}

void Camera::rotate(float dx, float dy) {
    // Apply yaw first conceptually (horizontal), then pitch (vertical)
    yaw   += dx * lookSens;
    pitch -= dy * lookSens; // invert Y like typical FPS mouse look

    // Clamp pitch so we never flip upside-down
    if (pitch >  89.f) pitch =  89.f;
    if (pitch < -89.f) pitch = -89.f;

    updateVectors();
}

void Camera::moveForward(float dt) {
    position = position + front * (moveSpeed * dt);
}

void Camera::moveBackward(float dt) {
    position = position - front * (moveSpeed * dt);
}

void Camera::moveRight(float dt) {
    position = position + right * (moveSpeed * dt);
}

void Camera::moveLeft(float dt) {
    position = position - right * (moveSpeed * dt);
}

void Camera::moveWASD(float dt, bool w, bool a, bool s, bool d) {
    // Build desired direction in camera space, then normalize
    // so W+A is not faster than W alone.
    Vec3 dir{ 0.f, 0.f, 0.f };
    if (w) dir = dir + front;
    if (s) dir = dir - front;
    if (d) dir = dir + right;
    if (a) dir = dir - right;

    float len = length(dir);
    if (len > 1e-6f) {
        dir = dir * (1.f / len); // normalize
        position = position + dir * (moveSpeed * dt);
    }
}

Mat4 Camera::viewMatrix() const {
    // lookAt(eye, center, up) — center = eye + front
    return lookAt(position, position + front, up);
}
