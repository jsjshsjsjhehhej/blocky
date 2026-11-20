#pragma once

// Unione / sottrazione / intersezione chiamate dagli script (GeometryService).
// Usa lo stesso CSG dello Studio (engine/Csg.h) e lascia un MeshPart nel mondo.

#include "Csg.h"
#include "DataModel.h"
#include "GameHub.h"
#include "platform/Platform.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace blocky_csg {

inline constexpr float kMeshFit = 5.5f; // come kWorldModelTargetSize in main.cpp

struct Result {
    glm::vec3 center{0.f};
    glm::vec3 size{1.f};
    glm::vec3 color{0.64f, 0.64f, 0.64f};
    std::string path;
    std::string name = "Union";
};

inline std::vector<csg::Polygon> solidOf(const WorkspacePart& p, int tag) {
    const glm::mat4 m = partModelMatrix(p);
    switch (p.shape) {
    case PartShape::Ball: return csg::transformed(csg::unitSphere(), m, tag);
    case PartShape::Cylinder: return csg::transformed(csg::unitCylinder(), m, tag);
    case PartShape::Wedge: return csg::transformed(csg::unitWedge(), m, tag);
    default: return csg::transformed(csg::unitBox(), m, tag);
    }
}

inline bool writeObj(const std::string& path, const std::vector<csg::Polygon>& polys, const glm::vec3& center,
                     const std::vector<WorkspacePart>& originals) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "# Blocky Union (GeometryService)\n";
    f << "# blkcenter " << center.x << ' ' << center.y << ' ' << center.z << '\n';
    auto clean = [](std::string s) {
        for (char& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
        return s.empty() ? std::string("Part") : s;
    };
    for (const auto& p : originals) {
        f << "# blkpart " << clean(p.name) << '\t' << (int)p.shape << '\t' << (int)p.material << '\t'
          << (p.negative ? 1 : 0) << '\t'
          << p.position.x << ' ' << p.position.y << ' ' << p.position.z << '\t'
          << p.size.x << ' ' << p.size.y << ' ' << p.size.z << '\t'
          << p.rotation.x << ' ' << p.rotation.y << ' ' << p.rotation.z << '\t'
          << p.color.r << ' ' << p.color.g << ' ' << p.color.b << '\t'
          << p.transparency << '\t' << (p.anchored ? 1 : 0) << '\t' << (p.canCollide ? 1 : 0) << '\n';
    }
    std::vector<glm::vec3> tris, normals;
    csg::triangulate(polys, tris, normals);
    for (const auto& v : tris)
        f << "v " << (v.x - center.x) << ' ' << (v.y - center.y) << ' ' << (v.z - center.z) << '\n';
    for (const auto& n : normals) f << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
    for (size_t t = 0; t < normals.size(); ++t) {
        const size_t b = t * 3 + 1;
        f << "f " << b << "//" << (t + 1) << ' ' << (b + 1) << "//" << (t + 1) << ' ' << (b + 2) << "//" << (t + 1) << '\n';
    }
    return (bool)f;
}

// mode: 0 unione (le part negative fanno un buco), 1 sottrazione della prima meno le altre, 2 intersezione.
inline bool combine(DataModel& world, const std::vector<int>& idx, int mode, Result& out, std::string& err) {
    if (idx.size() < 2) { err = "CSG: servono almeno 2 Part"; return false; }
    std::vector<WorkspacePart> originals;
    originals.reserve(idx.size());
    for (int i : idx) {
        if (i < 0 || i >= (int)world.parts.size()) { err = "CSG: Part fuori dal mondo"; return false; }
        const WorkspacePart& p = world.parts[(size_t)i];
        if (p.locked) { err = "CSG: \"" + p.name + "\" e' bloccata"; return false; }
        originals.push_back(p);
    }

    std::vector<csg::Polygon> result;
    bool first = true;
    auto take = [&](const WorkspacePart& p, int tag, int how) {
        auto s = solidOf(p, tag);
        if (first) { result = std::move(s); first = false; return; }
        if (how == 1) result = csg::subtract(result, s);
        else if (how == 2) result = csg::intersect(result, s);
        else result = csg::unite(result, s);
    };

    if (mode == 1) {
        take(originals[0], 0, 0);
        for (size_t k = 1; k < originals.size(); ++k) take(originals[k], (int)k, 1);
        out.name = "Subtraction";
        out.color = originals[0].color;
    } else if (mode == 2) {
        for (size_t k = 0; k < originals.size(); ++k) take(originals[k], (int)k, 2);
        out.name = "Intersection";
        out.color = originals[0].color;
    } else {
        int positives = 0;
        for (size_t k = 0; k < originals.size(); ++k) {
            if (originals[k].negative) continue;
            ++positives;
            take(originals[k], (int)k, 0);
            if (positives == 1) out.color = originals[k].color;
        }
        if (positives == 0) { err = "CSG: serve almeno una Part non negata"; return false; }
        for (size_t k = 0; k < originals.size(); ++k)
            if (originals[k].negative) result = csg::subtract(result, solidOf(originals[k], (int)k));
        out.name = "Union";
    }

    if (result.empty() || csg::volume(result) < 1e-4f) {
        err = "CSG: il risultato e' vuoto";
        return false;
    }

    glm::vec3 lo(1e9f), hi(-1e9f);
    for (const auto& p : result)
        for (const auto& v : p.v) { lo = glm::min(lo, v); hi = glm::max(hi, v); }
    out.center = (lo + hi) * 0.5f;
    out.size = glm::max(hi - lo, glm::vec3(0.05f));

    const std::string dir = blockyDataDir() + "/unions";
    plat::makeDirs(dir);
    static int counter = 0;
    char file[96];
    std::snprintf(file, sizeof(file), "/union_%lld_%d.obj", (long long)std::time(nullptr), ++counter);
    out.path = dir + file;
    if (!writeObj(out.path, result, out.center, originals)) {
        err = "CSG: impossibile scrivere la mesh";
        return false;
    }

    auto& m = world.addModel(out.name, out.path, out.center);
    const float maxDim = std::max({out.size.x, out.size.y, out.size.z, 0.05f});
    m.scale = glm::vec3(maxDim / kMeshFit);
    m.rotation = glm::vec3(0.f);
    m.tint = out.color;
    m.canCollide = originals[0].canCollide;
    return true;
}

// Rimette nel mondo le Part salvate nell'OBJ (# blkpart) e toglie la mesh dell'unione.
inline bool separate(DataModel& world, const std::string& path, std::vector<int>& outIdx, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "SeparateAsync: mesh non trovata"; return false; }
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("# blkpart ", 0) != 0) continue;
        std::vector<std::string> col;
        std::stringstream ss(line.substr(10));
        std::string c;
        while (std::getline(ss, c, '\t')) col.push_back(c);
        if (col.size() < 11) continue;
        WorkspacePart p;
        p.name = col[0].empty() ? "Part" : col[0];
        p.className = "Part";
        p.shape = (PartShape)std::clamp(std::atoi(col[1].c_str()), 0, (int)PartShape::Count - 1);
        p.material = (PartMaterial)std::clamp(std::atoi(col[2].c_str()), 0, (int)PartMaterial::Count - 1);
        p.negative = col[3] == "1";
        std::sscanf(col[4].c_str(), "%f %f %f", &p.position.x, &p.position.y, &p.position.z);
        std::sscanf(col[5].c_str(), "%f %f %f", &p.size.x, &p.size.y, &p.size.z);
        std::sscanf(col[6].c_str(), "%f %f %f", &p.rotation.x, &p.rotation.y, &p.rotation.z);
        std::sscanf(col[7].c_str(), "%f %f %f", &p.color.r, &p.color.g, &p.color.b);
        p.transparency = (float)std::atof(col[8].c_str());
        p.anchored = col[9] == "1";
        p.canCollide = col[10] == "1";
        p.useStud = p.shape == PartShape::Block && p.material == PartMaterial::Plastic;
        world.parts.push_back(std::move(p));
        outIdx.push_back((int)world.parts.size() - 1);
    }
    if (outIdx.empty()) { err = "SeparateAsync: questa unione non ha i pezzi originali"; return false; }
    world.models.erase(std::remove_if(world.models.begin(), world.models.end(),
                                      [&](const WorkspaceModel& m) { return m.sourcePath == path; }),
                       world.models.end());
    return true;
}

} // namespace blocky_csg
