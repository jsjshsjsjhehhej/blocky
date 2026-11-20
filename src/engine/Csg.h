#pragma once

/**
 * CSG (geometria solida costruttiva) come Unione / Negazione / Interseca di
 * Roblox Studio: unisce, sottrae o interseca solidi fatti di poligoni.
 *
 * E' l'algoritmo degli alberi BSP (lo stesso di csg.js di Evan Wallace):
 * ogni solido diventa un albero di piani; per unire A e B si tagliano i
 * poligoni di A che stanno dentro B e viceversa, poi si mettono insieme.
 * Sottrazione e intersezione sono la stessa idea con i solidi "rovesciati".
 *
 * Funziona con solidi chiusi (le Part lo sono). Nessuna dipendenza grafica:
 * lo usano lo Studio (main_ui/StudioCsg.h) e il test (tools/test_csg.cpp).
 */

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace csg {

constexpr float kEps = 1e-4f;   // tolleranza "sul piano" (stud)

struct Plane {
    glm::vec3 n{0.f, 1.f, 0.f};
    float w = 0.f;
    bool valid = false;

    static Plane from(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        Plane p;
        const glm::vec3 cr = glm::cross(b - a, c - a);
        const float len = glm::length(cr);
        if (len < 1e-9f) return p;
        p.n = cr / len;
        p.w = glm::dot(p.n, a);
        p.valid = true;
        return p;
    }
    void flip() { n = -n; w = -w; }
};

struct Polygon {
    std::vector<glm::vec3> v;
    Plane plane;
    int tag = 0;   // da quale solido viene (per i colori, se servono)

    Polygon() = default;
    Polygon(std::vector<glm::vec3> verts, int t = 0) : v(std::move(verts)), tag(t) { recompute(); }
    void recompute() {
        plane = Plane{};
        // il primo terzetto non allineato da' il piano
        for (size_t i = 2; i < v.size() && !plane.valid; ++i) plane = Plane::from(v[0], v[1], v[i]);
    }
    void flip() {
        std::reverse(v.begin(), v.end());
        plane.flip();
    }
};

/** Divide un poligono rispetto al piano: davanti, dietro, o complanare. */
inline void splitPolygon(const Plane& pl, const Polygon& poly, std::vector<Polygon>& coFront, std::vector<Polygon>& coBack,
                         std::vector<Polygon>& front, std::vector<Polygon>& back) {
    enum { COPLANAR = 0, FRONT = 1, BACK = 2, SPANNING = 3 };
    int polyType = 0;
    std::vector<int> types(poly.v.size());
    for (size_t i = 0; i < poly.v.size(); ++i) {
        const float t = glm::dot(pl.n, poly.v[i]) - pl.w;
        const int type = t < -kEps ? BACK : (t > kEps ? FRONT : COPLANAR);
        polyType |= type;
        types[i] = type;
    }
    switch (polyType) {
    case COPLANAR:
        (glm::dot(pl.n, poly.plane.n) > 0.f ? coFront : coBack).push_back(poly);
        break;
    case FRONT: front.push_back(poly); break;
    case BACK: back.push_back(poly); break;
    default: {
        std::vector<glm::vec3> f, b;
        const size_t n = poly.v.size();
        for (size_t i = 0; i < n; ++i) {
            const size_t j = (i + 1) % n;
            const int ti = types[i], tj = types[j];
            const glm::vec3& vi = poly.v[i];
            const glm::vec3& vj = poly.v[j];
            if (ti != BACK) f.push_back(vi);
            if (ti != FRONT) b.push_back(vi);
            if ((ti | tj) == SPANNING) {
                const float denom = glm::dot(pl.n, vj - vi);
                const float t = std::fabs(denom) > 1e-12f ? (pl.w - glm::dot(pl.n, vi)) / denom : 0.f;
                const glm::vec3 v = vi + (vj - vi) * t;
                f.push_back(v);
                b.push_back(v);
            }
        }
        if (f.size() >= 3) { Polygon p(f, poly.tag); p.plane = poly.plane; front.push_back(p); }
        if (b.size() >= 3) { Polygon p(b, poly.tag); p.plane = poly.plane; back.push_back(p); }
        break;
    }
    }
}

struct Node {
    Plane plane;
    std::unique_ptr<Node> front, back;
    std::vector<Polygon> polys;

    Node() = default;
    explicit Node(std::vector<Polygon> list) { build(std::move(list)); }

    /** Dentro <-> fuori. */
    void invert() {
        for (auto& p : polys) p.flip();
        plane.flip();
        if (front) front->invert();
        if (back) back->invert();
        std::swap(front, back);
    }

    /** Toglie dai poligoni dati le parti che stanno dentro questo solido. */
    std::vector<Polygon> clipPolygons(const std::vector<Polygon>& list) const {
        if (!plane.valid) return list;
        std::vector<Polygon> f, b;
        for (const auto& p : list) splitPolygon(plane, p, f, b, f, b);
        if (front) f = front->clipPolygons(f);
        b = back ? back->clipPolygons(b) : std::vector<Polygon>{};
        f.insert(f.end(), b.begin(), b.end());
        return f;
    }

    /** Toglie da questo albero tutto quello che sta dentro l'altro. */
    void clipTo(const Node& other) {
        polys = other.clipPolygons(polys);
        if (front) front->clipTo(other);
        if (back) back->clipTo(other);
    }

    void allPolygons(std::vector<Polygon>& out) const {
        out.insert(out.end(), polys.begin(), polys.end());
        if (front) front->allPolygons(out);
        if (back) back->allPolygons(out);
    }

    void build(std::vector<Polygon> list, int depth = 0) {
        if (list.empty()) return;
        if (!plane.valid) {
            for (const auto& p : list) if (p.plane.valid) { plane = p.plane; break; }
            if (!plane.valid) return;
        }
        if (depth > 256) {   // salvagente contro alberi patologici: i poligoni restano qui
            polys.insert(polys.end(), list.begin(), list.end());
            return;
        }
        std::vector<Polygon> f, b;
        for (const auto& p : list) splitPolygon(plane, p, polys, polys, f, b);
        if (!f.empty()) { if (!front) front = std::make_unique<Node>(); front->build(std::move(f), depth + 1); }
        if (!b.empty()) { if (!back) back = std::make_unique<Node>(); back->build(std::move(b), depth + 1); }
    }
};

inline std::vector<Polygon> polygonsOf(const Node& n) {
    std::vector<Polygon> out;
    n.allPolygons(out);
    return out;
}

/** A unito B. */
inline std::vector<Polygon> unite(const std::vector<Polygon>& A, const std::vector<Polygon>& B) {
    Node a(A), b(B);
    a.clipTo(b);
    b.clipTo(a);
    b.invert();
    b.clipTo(a);
    b.invert();
    a.build(polygonsOf(b));
    return polygonsOf(a);
}

/** A meno B. */
inline std::vector<Polygon> subtract(const std::vector<Polygon>& A, const std::vector<Polygon>& B) {
    Node a(A), b(B);
    a.invert();
    a.clipTo(b);
    b.clipTo(a);
    b.invert();
    b.clipTo(a);
    b.invert();
    a.build(polygonsOf(b));
    a.invert();
    return polygonsOf(a);
}

/** Solo quello che A e B hanno in comune. */
inline std::vector<Polygon> intersect(const std::vector<Polygon>& A, const std::vector<Polygon>& B) {
    Node a(A), b(B);
    a.invert();
    b.clipTo(a);
    b.invert();
    a.clipTo(b);
    b.clipTo(a);
    a.build(polygonsOf(b));
    a.invert();
    return polygonsOf(a);
}

// ---- Solidi delle forme delle Part (in spazio "unitario" -0.5..0.5) ----------------------------

/** Rende le facce di un solido convesso rivolte verso fuori (il centro e' l'origine). */
inline void orientOutward(std::vector<Polygon>& polys) {
    for (auto& p : polys) {
        if (!p.plane.valid) continue;
        glm::vec3 c(0.f);
        for (const auto& v : p.v) c += v;
        c /= (float)p.v.size();
        if (glm::dot(p.plane.n, c) < 0.f) p.flip();
    }
}

inline std::vector<Polygon> unitBox() {
    const float h = 0.5f;
    const glm::vec3 c[8] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h}, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}};
    const int f[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 4, 7, 3}, {1, 2, 6, 5}, {0, 1, 5, 4}, {3, 7, 6, 2}};
    std::vector<Polygon> out;
    for (const auto& q : f) out.emplace_back(std::vector<glm::vec3>{c[q[0]], c[q[1]], c[q[2]], c[q[3]]});
    orientOutward(out);
    return out;
}

/** Cuneo come quello di Blocky (render/PartMeshes.h): sale verso +Z. */
inline std::vector<Polygon> unitWedge() {
    const float h = 0.5f;
    std::vector<Polygon> out;
    out.emplace_back(std::vector<glm::vec3>{{-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}});   // sotto
    out.emplace_back(std::vector<glm::vec3>{{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}});       // dietro
    out.emplace_back(std::vector<glm::vec3>{{-h, -h, -h}, {-h, -h, h}, {-h, h, h}});               // lato
    out.emplace_back(std::vector<glm::vec3>{{h, -h, -h}, {h, h, h}, {h, -h, h}});                  // lato
    out.emplace_back(std::vector<glm::vec3>{{-h, -h, -h}, {-h, h, h}, {h, h, h}, {h, -h, -h}});     // rampa
    // Il cuneo non e' centrato sull'origine come un cubo: si orienta col suo baricentro.
    glm::vec3 g(0.f);
    int n = 0;
    for (const auto& p : out) for (const auto& v : p.v) { g += v; ++n; }
    g /= (float)n;
    for (auto& p : out) {
        glm::vec3 c(0.f);
        for (const auto& v : p.v) c += v;
        c /= (float)p.v.size();
        if (glm::dot(p.plane.n, c - g) < 0.f) p.flip();
    }
    return out;
}

/** Cilindro con l'asse su Y, raggio 0.5 (come Blocky). */
inline std::vector<Polygon> unitCylinder(int slices = 24) {
    std::vector<Polygon> out;
    std::vector<glm::vec3> top, bottom;
    for (int i = 0; i < slices; ++i) {
        const float a0 = (float)i / slices * 6.2831853f, a1 = (float)(i + 1) / slices * 6.2831853f;
        const glm::vec3 p0{0.5f * std::cos(a0), 0.f, 0.5f * std::sin(a0)}, p1{0.5f * std::cos(a1), 0.f, 0.5f * std::sin(a1)};
        out.emplace_back(std::vector<glm::vec3>{p0 + glm::vec3(0, -0.5f, 0), p1 + glm::vec3(0, -0.5f, 0), p1 + glm::vec3(0, 0.5f, 0),
                                                p0 + glm::vec3(0, 0.5f, 0)});
        top.push_back(p0 + glm::vec3(0, 0.5f, 0));
        bottom.push_back(p0 + glm::vec3(0, -0.5f, 0));
    }
    out.emplace_back(top);
    out.emplace_back(bottom);
    orientOutward(out);
    return out;
}

/** Sfera di raggio 0.5 (stretta in un'ellissoide se la Part non e' cubica). */
inline std::vector<Polygon> unitSphere(int slices = 18, int stacks = 12) {
    std::vector<Polygon> out;
    auto pt = [&](int i, int j) {
        const float th = (float)i / slices * 6.2831853f, ph = (float)j / stacks * 3.14159265f;
        return glm::vec3(0.5f * std::sin(ph) * std::cos(th), 0.5f * std::cos(ph), 0.5f * std::sin(ph) * std::sin(th));
    };
    for (int i = 0; i < slices; ++i)
        for (int j = 0; j < stacks; ++j) {
            std::vector<glm::vec3> v;
            v.push_back(pt(i, j));
            if (j > 0) v.push_back(pt(i + 1, j));
            v.push_back(pt(i + 1, j + 1));
            if (j < stacks - 1) v.push_back(pt(i, j + 1));
            if (v.size() >= 3) out.emplace_back(v);
        }
    orientOutward(out);
    return out;
}

/** Porta un solido unitario nel mondo con una matrice (posizione * rotazione * misura). */
inline std::vector<Polygon> transformed(const std::vector<Polygon>& unit, const glm::mat4& m, int tag = 0) {
    std::vector<Polygon> out;
    out.reserve(unit.size());
    for (const auto& p : unit) {
        std::vector<glm::vec3> v;
        v.reserve(p.v.size());
        for (const auto& x : p.v) v.push_back(glm::vec3(m * glm::vec4(x, 1.f)));
        Polygon q(v, tag);
        // una misura negativa (specchio) rovescerebbe le facce: si riorientano
        if (glm::determinant(glm::mat3(m)) < 0.f) q.flip();
        if (q.plane.valid) out.push_back(q);
    }
    return out;
}

/** Triangoli (a ventaglio) di un insieme di poligoni. */
inline void triangulate(const std::vector<Polygon>& polys, std::vector<glm::vec3>& tris, std::vector<glm::vec3>& normals) {
    for (const auto& p : polys) {
        if (p.v.size() < 3 || !p.plane.valid) continue;
        for (size_t i = 1; i + 1 < p.v.size(); ++i) {
            tris.push_back(p.v[0]);
            tris.push_back(p.v[i]);
            tris.push_back(p.v[i + 1]);
            normals.push_back(p.plane.n);
        }
    }
}

/** Volume con segno (positivo per un solido chiuso orientato verso fuori). */
inline float volume(const std::vector<Polygon>& polys) {
    double v = 0.0;
    for (const auto& p : polys)
        for (size_t i = 1; i + 1 < p.v.size(); ++i)
            v += glm::dot(p.v[0], glm::cross(p.v[i], p.v[i + 1])) / 6.0;
    return (float)v;
}

} // namespace csg
