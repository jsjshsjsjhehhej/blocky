#pragma once

// Matematica piccola dell'anti-lag. Non usa GLM di proposito: il test
// gira con il solo compilatore, e main.cpp converte le matrici GLM in
// questo formato (column-major, lo stesso di glUniformMatrix4fv).

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace antilag {

struct V3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

struct V4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
};

struct M4 {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
};

inline V3 v3(float x, float y, float z) { return {x, y, z}; }

inline V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 mul(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float lenSq(V3 a) { return dot(a, a); }
inline float len(V3 a) { return std::sqrt(lenSq(a)); }

inline V3 cross(V3 a, V3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

inline V3 normalize(V3 a) {
    float L = len(a);
    if (L < 1e-8f) return {0, 0, 0};
    return mul(a, 1.f / L);
}

inline float clampf(float v, float a, float b) {
    return std::max(a, std::min(b, v));
}

inline V4 rowOf(const M4& m, int r) {
    return {m.m[r], m.m[4 + r], m.m[8 + r], m.m[12 + r]};
}

inline V4 add4(V4 a, V4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
inline V4 sub4(V4 a, V4 b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }

inline M4 mul(const M4& a, const M4& b) {
    M4 o;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float s = 0.f;
            for (int k = 0; k < 4; ++k)
                s += a.m[k * 4 + r] * b.m[c * 4 + k];
            o.m[c * 4 + r] = s;
        }
    }
    return o;
}

inline M4 perspective(float fovyRad, float aspect, float n, float f) {
    M4 o{};
    for (float& e : o.m) e = 0.f;
    float t = std::tan(fovyRad * 0.5f);
    if (t < 1e-8f) t = 1e-8f;
    if (aspect < 1e-8f) aspect = 1.f;
    o.m[0] = 1.f / (aspect * t);
    o.m[5] = 1.f / t;
    o.m[10] = (f + n) / (n - f);
    o.m[11] = -1.f;
    o.m[14] = (2.f * f * n) / (n - f);
    return o;
}

inline M4 lookAt(V3 eye, V3 center, V3 up) {
    V3 f = normalize(sub(center, eye));
    V3 s = normalize(cross(f, up));
    V3 u = cross(s, f);
    M4 o;
    o.m[0] = s.x;  o.m[4] = s.y;  o.m[8]  = s.z;  o.m[12] = -dot(s, eye);
    o.m[1] = u.x;  o.m[5] = u.y;  o.m[9]  = u.z;  o.m[13] = -dot(u, eye);
    o.m[2] = -f.x; o.m[6] = -f.y; o.m[10] = -f.z; o.m[14] = dot(f, eye);
    o.m[3] = 0;    o.m[7] = 0;    o.m[11] = 0;    o.m[15] = 1;
    return o;
}

inline V4 transformPoint(const M4& m, V3 p) {
    return {
        m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
        m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
        m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14],
        m.m[3] * p.x + m.m[7] * p.y + m.m[11] * p.z + m.m[15]
    };
}

inline M4 fromColumnMajor(const float* p) {
    M4 o;
    if (!p) return o;
    for (int i = 0; i < 16; ++i) o.m[i] = p[i];
    return o;
}

}
