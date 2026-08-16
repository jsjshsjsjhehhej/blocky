#pragma once
/**
 * Math utilities — Roblox-2015 style engine (OpenGL ES 2.0)
 * Minimal vec3 / mat4 helpers. Column-major matrices (OpenGL convention).
 */

#include <cmath>

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

inline float length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vec3 normalize(const Vec3& v) {
    float len = length(v);
    if (len < 1e-8f) return {0, 0, 0};
    return v * (1.f / len);
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/** 4x4 matrix, column-major (m[col * 4 + row]). */
struct Mat4 {
    float m[16];

    Mat4() {
        for (int i = 0; i < 16; ++i) m[i] = 0.f;
        m[0] = m[5] = m[10] = m[15] = 1.f; // identity
    }

    static Mat4 identity() { return Mat4(); }

    const float* data() const { return m; }
};

inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return r;
}

inline Mat4 translate(const Vec3& t) {
    Mat4 r;
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

/** Rotate around axis (degrees). Axis should be normalized. */
inline Mat4 rotate(float degrees, const Vec3& axis) {
    float rad = degrees * 0.01745329251f; // pi/180
    float c = std::cos(rad);
    float s = std::sin(rad);
    float t = 1.f - c;
    Vec3 a = normalize(axis);

    Mat4 r;
    r.m[0]  = t * a.x * a.x + c;
    r.m[1]  = t * a.x * a.y + s * a.z;
    r.m[2]  = t * a.x * a.z - s * a.y;
    r.m[4]  = t * a.x * a.y - s * a.z;
    r.m[5]  = t * a.y * a.y + c;
    r.m[6]  = t * a.y * a.z + s * a.x;
    r.m[8]  = t * a.x * a.z + s * a.y;
    r.m[9]  = t * a.y * a.z - s * a.x;
    r.m[10] = t * a.z * a.z + c;
    return r;
}

inline Mat4 scale(const Vec3& s) {
    Mat4 r;
    r.m[0]  = s.x;
    r.m[5]  = s.y;
    r.m[10] = s.z;
    return r;
}

/** Perspective projection (fovY in degrees). */
inline Mat4 perspective(float fovYDeg, float aspect, float nearZ, float farZ) {
    float f = 1.f / std::tan(fovYDeg * 0.01745329251f * 0.5f);
    Mat4 r;
    for (int i = 0; i < 16; ++i) r.m[i] = 0.f;
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (farZ + nearZ) / (nearZ - farZ);
    r.m[11] = -1.f;
    r.m[14] = (2.f * farZ * nearZ) / (nearZ - farZ);
    return r;
}

/** Camera look-at matrix (column-major, OpenGL style).
 *
 *  eye    = camera position
 *  center = point the camera looks at
 *  up     = world/camera up (usually 0,1,0) — will be re-orthonormalized
 *
 *  Builds the view matrix that transforms world → camera space.
 */
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& upHint) {
    // Forward (from eye toward target), then right, then true up
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, upHint));
    Vec3 u = normalize(cross(s, f)); // already orthogonal; normalize for safety

    Mat4 r;
    // Columns of rotation part
    r.m[0]  =  s.x; r.m[4]  =  s.y; r.m[8]  =  s.z; r.m[12] = -dot(s, eye);
    r.m[1]  =  u.x; r.m[5]  =  u.y; r.m[9]  =  u.z; r.m[13] = -dot(u, eye);
    r.m[2]  = -f.x; r.m[6]  = -f.y; r.m[10] = -f.z; r.m[14] =  dot(f, eye);
    r.m[3]  =  0.f; r.m[7]  =  0.f; r.m[11] =  0.f; r.m[15] =  1.f;
    return r;
}
