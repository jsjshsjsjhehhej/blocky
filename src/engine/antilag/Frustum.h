#pragma once

// Frustum estratto dalla matrice clip (proiezione * vista), lo stesso
// prodotto che il renderer usa per uProj * uView. Una sfera e' fuori se
// sta tutta dietro un piano. Il raggio e' meta' diagonale della part:
// meglio disegnare un angolo in piu' che tagliare un cubo a meta'.

#include "Math.h"

namespace antilag {

struct Plane {
    float a = 0.f, b = 0.f, c = 0.f, d = 0.f;

    void normalize() {
        float L = std::sqrt(a * a + b * b + c * c);
        if (L < 1e-8f) return;
        float inv = 1.f / L;
        a *= inv; b *= inv; c *= inv; d *= inv;
    }

    float distance(float x, float y, float z) const {
        return a * x + b * y + c * z + d;
    }
};

struct Frustum {
    Plane planes[6];

    bool sphereOutside(float x, float y, float z, float radius) const {
        for (const Plane& p : planes) {
            if (p.distance(x, y, z) < -radius) return true;
        }
        return false;
    }

    bool pointInside(float x, float y, float z) const {
        return !sphereOutside(x, y, z, 0.f);
    }

    bool aabbOutside(float minx, float miny, float minz,
                     float maxx, float maxy, float maxz) const {
        // Il vertice p e' l'angolo piu' dentro (lungo la normale). Se
        // persino quello e' fuori, la cella e' tutta fuori. Usare
        // l'angolo opposto buttava via celle che contenevano part in
        // vista solo perche' un angolo tagliava il piano.
        for (const Plane& p : planes) {
            float x = p.a >= 0.f ? maxx : minx;
            float y = p.b >= 0.f ? maxy : miny;
            float z = p.c >= 0.f ? maxz : minz;
            if (p.distance(x, y, z) < 0.f) return true;
        }
        return false;
    }
};

inline Plane planeFromRow(V4 r) {
    Plane p;
    p.a = r.x; p.b = r.y; p.c = r.z; p.d = r.w;
    p.normalize();
    return p;
}

inline Frustum frustumFromClip(const M4& clip) {
    V4 r0 = rowOf(clip, 0);
    V4 r1 = rowOf(clip, 1);
    V4 r2 = rowOf(clip, 2);
    V4 r3 = rowOf(clip, 3);
    Frustum f;
    f.planes[0] = planeFromRow(add4(r3, r0)); // left
    f.planes[1] = planeFromRow(sub4(r3, r0)); // right
    f.planes[2] = planeFromRow(add4(r3, r1)); // bottom
    f.planes[3] = planeFromRow(sub4(r3, r1)); // top
    f.planes[4] = planeFromRow(add4(r3, r2)); // near
    f.planes[5] = planeFromRow(sub4(r3, r2)); // far
    return f;
}

// Raggio in pixel di una sfera vista da eye. Sotto un paio di pixel la
// part e' un punto: disegnarla costa una draw call intera per niente.
inline float screenRadiusPx(V3 eye, float fovyRad, float viewportH,
                            V3 center, float radius) {
    if (viewportH < 1.f) return 0.f;
    if (radius < 0.f) radius = 0.f;
    V3 d = sub(center, eye);
    float dist = len(d);
    if (dist < 0.05f) return viewportH;
    float half = std::tan(fovyRad * 0.5f);
    if (half < 1e-6f) half = 1e-6f;
    float worldPerPx = (2.f * dist * half) / viewportH;
    return radius / worldPerPx;
}

struct Camera {
    V3 eye{};
    V3 forward{0, 0, -1};
    V3 velocity{};
    float fovyRad = 1.05f;
    float viewportH = 720.f;
    M4 viewProj{};
    Frustum frustum{};

    void setViewProj(const M4& vp) {
        viewProj = vp;
        frustum = frustumFromClip(vp);
    }
};

// Allarga la sfera in direzione del moto della camera: se stai correndo,
// le part che stanno per entrare in vista non devono comparire un frame
// dopo, quando il pop e' gia' visibile.
inline float predictedRadius(float radius, const Camera& cam, float lookaheadSec) {
    float speed = len(cam.velocity);
    return radius + speed * std::max(0.f, lookaheadSec);
}

}
