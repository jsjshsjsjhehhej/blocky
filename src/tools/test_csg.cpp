// Test del CSG (engine/Csg.h): Unione / Sottrazione / Intersezione come Roblox
// Studio, controllate coi volumi (che si sanno calcolare a mano).

#include "engine/Csg.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>

static int g_fail = 0;
static void check(bool ok, const char* what, float got) {
    std::printf("[csg] %s %s (%.4f)\n", ok ? "OK  " : "FAIL", what, got);
    if (!ok) ++g_fail;
}
static bool near(float a, float b, float eps) { return std::fabs(a - b) <= eps; }

static std::vector<csg::Polygon> box(glm::vec3 pos, glm::vec3 size) {
    return csg::transformed(csg::unitBox(), glm::translate(glm::mat4(1.f), pos) * glm::scale(glm::mat4(1.f), size));
}

int main() {
    const auto a = box({0, 0, 0}, {1, 1, 1});
    check(near(csg::volume(a), 1.f, 1e-4f), "un cubo 1x1x1 ha volume 1", csg::volume(a));
    const auto b = box({0.5f, 0, 0}, {1, 1, 1});   // si sovrappone per meta'
    const float u = csg::volume(csg::unite(a, b));
    check(near(u, 1.5f, 1e-3f), "unione di due cubi sovrapposti a meta' = 1.5", u);
    const float s = csg::volume(csg::subtract(a, b));
    check(near(s, 0.5f, 1e-3f), "cubo meno cubo = 0.5", s);
    const float in = csg::volume(csg::intersect(a, b));
    check(near(in, 0.5f, 1e-3f), "intersezione = 0.5", in);
    const auto far = box({5, 0, 0}, {1, 1, 1});
    const float d = csg::volume(csg::unite(a, far));
    check(near(d, 2.f, 1e-3f), "unione di due cubi lontani = 2", d);
    const float none = csg::volume(csg::intersect(a, far));
    check(near(none, 0.f, 1e-4f), "intersezione di cubi lontani = vuota", none);

    // buco: un cubo 4x4x4 meno un cubo 2x2x2 al centro
    const float hole = csg::volume(csg::subtract(box({0, 0, 0}, {4, 4, 4}), box({0, 0, 0}, {2, 2, 2})));
    check(near(hole, 56.f, 1e-2f), "cubo 4^3 con un buco 2^3 = 56", hole);

    const float sph = csg::volume(csg::unitSphere());
    check(sph > 0.48f && sph < 0.5236f, "sfera di diametro 1 ~ 0.52 (un po' meno: e' fatta di facce)", sph);
    const float cyl = csg::volume(csg::unitCylinder());
    check(cyl > 0.76f && cyl < 0.786f, "cilindro di diametro 1, alto 1 ~ 0.785", cyl);
    const float wedge = csg::volume(csg::unitWedge());
    check(near(wedge, 0.5f, 1e-4f), "il cuneo e' meta' cubo", wedge);

    // ruotato di 45 gradi: il volume non cambia
    const auto rot = csg::transformed(csg::unitBox(), glm::rotate(glm::mat4(1.f), glm::radians(45.f), glm::vec3(0, 1, 0)));
    const float rv = csg::volume(csg::unite(a, rot));
    check(rv > 1.f && rv < 2.f, "unione con un cubo ruotato: tra 1 e 2", rv);

    std::vector<glm::vec3> tris, normals;
    csg::triangulate(csg::unite(a, b), tris, normals);
    check(!tris.empty() && tris.size() == normals.size() * 3, "si trasforma in triangoli per la mesh", (float)tris.size());

    std::printf("[csg] %s\n", g_fail ? "CI SONO ERRORI" : "tutto OK");
    return g_fail ? 1 : 0;
}
