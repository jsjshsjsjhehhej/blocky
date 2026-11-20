// Unione / Negazione / Interseca / Separa come in Roblox Studio (scheda
// Modello > Solidi, tasto destro, Ctrl+Maiusc+G / N / U).
//
//   Unione:    le part selezionate diventano un solo oggetto ("Union"); le part
//              negate (rosa) vengono sottratte e fanno buchi.
//   Interseca: resta solo il volume che le part hanno in comune.
//   Nega:      la part diventa negativa (rosa trasparente).
//   Separa:    l'unione torna nelle part di prima.
//
// Il risultato e' un MeshPart con la sua mesh in un file OBJ (cartella dati
// /unions): si vede, si salva e si riapre come un modello importato. Dentro
// l'OBJ, come commenti, ci sono anche le part originali: cosi' Separa funziona
// anche dopo aver chiuso e riaperto il place. Il motore e' engine/Csg.h.
// Incluso da StudioUiPanel.h.

#include "engine/Csg.h"

#include <fstream>
#include <sstream>

namespace studio_csg {

/** Il solido di una part, nel mondo. */
static std::vector<csg::Polygon> solidOf(const WorkspacePart& p, int tag) {
    const glm::mat4 m = partModelMatrix(p);
    switch (p.shape) {
    case PartShape::Ball: return csg::transformed(csg::unitSphere(), m, tag);
    case PartShape::Cylinder: return csg::transformed(csg::unitCylinder(), m, tag);
    case PartShape::Wedge: return csg::transformed(csg::unitWedge(), m, tag);
    default: return csg::transformed(csg::unitBox(), m, tag);
    }
}

static std::string clean(std::string s) {
    for (char& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return s.empty() ? std::string("Part") : s;
}

/** Scrive la mesh (centrata in `center`) e le part originali in un OBJ. */
static bool writeObj(const std::string& path, const std::vector<csg::Polygon>& polys, const glm::vec3& center,
                     const std::vector<WorkspacePart>& originals) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << "# Blocky Union (Unione dello Studio)\n";
    f << "# blkcenter " << center.x << ' ' << center.y << ' ' << center.z << '\n';
    for (const auto& p : originals) {
        f << "# blkpart " << clean(p.name) << '\t' << (int)p.shape << '\t' << (int)p.material << '\t' << (p.negative ? 1 : 0) << '\t'
          << p.position.x << ' ' << p.position.y << ' ' << p.position.z << '\t' << p.size.x << ' ' << p.size.y << ' ' << p.size.z << '\t'
          << p.rotation.x << ' ' << p.rotation.y << ' ' << p.rotation.z << '\t' << p.color.r << ' ' << p.color.g << ' ' << p.color.b << '\t'
          << p.transparency << '\t' << (p.anchored ? 1 : 0) << '\t' << (p.canCollide ? 1 : 0) << '\n';
    }
    std::vector<glm::vec3> tris, normals;
    csg::triangulate(polys, tris, normals);
    for (const auto& v : tris) f << "v " << (v.x - center.x) << ' ' << (v.y - center.y) << ' ' << (v.z - center.z) << '\n';
    for (const auto& n : normals) f << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
    for (size_t t = 0; t < normals.size(); ++t) {
        const size_t b = t * 3 + 1;
        f << "f " << b << "//" << (t + 1) << ' ' << (b + 1) << "//" << (t + 1) << ' ' << (b + 2) << "//" << (t + 1) << '\n';
    }
    return (bool)f;
}

/** Le part originali salvate nell'OBJ di un'unione. */
static std::vector<WorkspacePart> readOriginals(const std::string& path, glm::vec3& center) {
    std::vector<WorkspacePart> out;
    std::ifstream f(path, std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("# blkcenter ", 0) == 0) {
            std::sscanf(line.c_str() + 12, "%f %f %f", &center.x, &center.y, &center.z);
            continue;
        }
        if (line.rfind("# blkpart ", 0) != 0) continue;
        std::vector<std::string> col;
        std::stringstream ss(line.substr(10));
        std::string c;
        while (std::getline(ss, c, '\t')) col.push_back(c);
        if (col.size() < 11) continue;
        WorkspacePart p;
        p.name = col[0];
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
        out.push_back(p);
    }
    return out;
}

static bool isUnionModel(const WorkspaceModel& m) {
    return m.sourcePath.find("/unions/") != std::string::npos || m.sourcePath.find("\\unions\\") != std::string::npos;
}

// Lo stato che Annulla rimette: part, Model, vincoli, GUI agganciate, MeshPart.
struct WorldCut {
    std::vector<WorkspacePart> parts;
    std::vector<PartGroup> groups;
    std::vector<WeldConstraint> welds;
    std::vector<WorkspaceLabel> labels;
    std::vector<WorkspaceModel> models;
};
static std::shared_ptr<WorldCut> snapshot() {
    auto s = std::make_shared<WorldCut>();
    s->parts = g_world.parts;
    s->groups = g_world.groups;
    s->welds = g_world.welds;
    s->labels = g_world.labels;
    s->models = g_world.models;
    return s;
}
static void restoreCut(const WorldCut& s) {
    g_world.parts = s.parts;
    g_world.groups = s.groups;
    g_world.welds = s.welds;
    g_world.labels = s.labels;
    g_world.models = s.models;
    g_wsMeshes.clear();   // le mesh dei MeshPart si ricaricano al prossimo disegno
    g_wsRigged.clear();
    studioMultiClear();
    selectPart(-1);
    selectModel(-1);
    studioFollowRebaseline();
    markPlaceDirty();
}
static void recordCut(const std::shared_ptr<WorldCut>& before, const std::shared_ptr<WorldCut>& after) {
    g_undo.recordCustom([before, after](bool undoing) { restoreCut(undoing ? *before : *after); });
    g_extrasLast.reset();   // anche le GUI fanno parte del passo: niente voce doppia
}

/** Toglie le part (indici qualsiasi) tenendo in ordine vincoli, figli e GUI agganciate. */
static void eraseParts(std::vector<int> idx) {
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    idx.erase(std::unique(idx.begin(), idx.end()), idx.end());
    for (int i : idx) {
        if (!partIndexValid(i)) continue;
        g_world.parts.erase(g_world.parts.begin() + i);
        partconstraints::onPartErased(g_world, i);
        for (auto& l : g_world.labels) {
            if (l.adorneePart == i) l.adorneePart = -1;
            else if (l.adorneePart > i) --l.adorneePart;
        }
        for (auto& p : g_world.parts) {
            if (p.parentIndex == i) p.parentIndex = -1;
            else if (p.parentIndex > i) --p.parentIndex;
        }
    }
}

} // namespace studio_csg

/** Unione (mode 0) o Interseca (mode 1) delle part selezionate. */
static void studioCsgCombine(int mode) {
    using namespace studio_csg;
    const std::vector<int> sel = studioActivePartIndices();
    if (sel.size() < 2) { g_exportMsg = mode == 0 ? "Unione: seleziona almeno 2 part" : "Interseca: seleziona almeno 2 part"; return; }
    std::vector<WorkspacePart> originals;
    for (int i : sel) {
        const WorkspacePart& p = g_world.parts[(size_t)i];
        if (p.locked) { g_exportMsg = "\"" + p.name + "\" e' bloccata: sbloccala prima"; return; }
        if (!p.script.empty()) { g_exportMsg = "\"" + p.name + "\" ha uno script: spostalo o toglilo prima di unire"; return; }
        originals.push_back(p);
    }
    std::vector<csg::Polygon> result;
    bool first = true;
    int positives = 0;
    for (size_t k = 0; k < originals.size(); ++k) {
        const WorkspacePart& p = originals[k];
        if (p.negative) continue;
        ++positives;
        auto s = solidOf(p, (int)k);
        if (first) { result = std::move(s); first = false; }
        else result = mode == 0 ? csg::unite(result, s) : csg::intersect(result, s);
    }
    if (positives == 0) { g_exportMsg = "Serve almeno una part normale (non negata)"; return; }
    for (size_t k = 0; k < originals.size(); ++k)
        if (originals[k].negative) result = csg::subtract(result, solidOf(originals[k], (int)k));
    if (result.empty() || csg::volume(result) < 1e-4f) { g_exportMsg = "Il risultato e' vuoto: le part non si toccano?"; return; }

    glm::vec3 lo(1e9f), hi(-1e9f);
    for (const auto& p : result) for (const auto& v : p.v) { lo = glm::min(lo, v); hi = glm::max(hi, v); }
    const glm::vec3 center = (lo + hi) * 0.5f;
    const float maxDim = std::max({hi.x - lo.x, hi.y - lo.y, hi.z - lo.z, 0.01f});

    const std::string dir = blockyDataDir() + "/unions";
    plat::makeDirs(dir);
    static int counter = 0;
    char name[96];
    std::snprintf(name, sizeof(name), "/union_%lld_%d.obj", (long long)std::time(nullptr), ++counter);
    const std::string path = dir + name;
    if (!writeObj(path, result, center, originals)) { g_exportMsg = "Unione: impossibile scrivere " + path; return; }

    auto before = snapshot();
    eraseParts(sel);
    // Il MeshPart: il loader porta ogni mesh a kWorldModelTargetSize, la scala la riporta giusta.
    const WorkspacePart& look = *std::find_if(originals.begin(), originals.end(), [](const WorkspacePart& p) { return !p.negative; });
    auto& m = g_world.addModel(mode == 0 ? "Union" : "Intersection", path, center);
    m.scale = glm::vec3(maxDim / kWorldModelTargetSize);
    m.rotation = glm::vec3(0.f);
    m.tint = look.color;
    m.canCollide = look.canCollide;
    g_wsMeshes.clear();
    g_wsRigged.clear();
    recordCut(before, snapshot());
    studioMultiClear();
    selectModel((int)g_world.models.size() - 1);
    g_showProps = true;
    markPlaceDirty();
    g_exportMsg = std::string(mode == 0 ? "Unione" : "Intersezione") + " di " + std::to_string(originals.size()) + " part fatta";
}

/** Nega / togli la negazione alle part selezionate. */
static void studioCsgNegate() {
    const std::vector<int> sel = studioActivePartIndices();
    if (sel.empty()) { g_exportMsg = "Nega: seleziona delle part"; return; }
    bool anyPositive = false;
    for (int i : sel) anyPositive |= !g_world.parts[(size_t)i].negative;
    g_undo.beginBatch();
    for (int i : sel) {
        WorkspacePart& p = g_world.parts[(size_t)i];
        if (p.locked) continue;
        const WorkspacePart before = p;
        p.negative = anyPositive;
        g_undo.recordEdit(i, before, p);
    }
    g_undo.endBatch();
    markPlaceDirty();
    g_exportMsg = anyPositive ? "Part negate: nell'Unione faranno un buco" : "Negazione tolta";
}

/** Separa: l'unione selezionata torna nelle part originali. */
static void studioCsgSeparate() {
    using namespace studio_csg;
    if (g_selectedModel < 0 || g_selectedModel >= (int)g_world.models.size() || !isUnionModel(g_world.models[(size_t)g_selectedModel])) {
        g_exportMsg = "Separa: seleziona un'Unione";
        return;
    }
    glm::vec3 oldCenter(0.f);   // dove stava l'unione quando e' stata fatta (scritto nell'OBJ)
    const std::vector<WorkspacePart> parts = readOriginals(g_world.models[(size_t)g_selectedModel].sourcePath, oldCenter);
    if (parts.empty()) { g_exportMsg = "Separa: dentro l'unione non ci sono le part originali"; return; }
    auto before = snapshot();
    // Se l'unione e' stata spostata dopo, le part la seguono.
    const WorkspaceModel& m = g_world.models[(size_t)g_selectedModel];
    const glm::vec3 shift = m.position - oldCenter;
    g_world.models.erase(g_world.models.begin() + g_selectedModel);
    g_wsMeshes.clear();
    g_wsRigged.clear();
    const int first = (int)g_world.parts.size();
    for (auto p : parts) {
        p.position += shift;
        g_world.parts.push_back(p);
    }
    recordCut(before, snapshot());
    selectModel(-1);
    studioMultiClear();
    for (int i = first; i < (int)g_world.parts.size(); ++i) studioMultiAdd(i);
    selectPart(first);
    markPlaceDirty();
    g_exportMsg = "Separata in " + std::to_string(parts.size()) + " part";
}

/** Ctrl+Maiusc+G unisci, Ctrl+Maiusc+N nega, Ctrl+Maiusc+U separa (come Roblox Studio). */
static void studioCsgPollHotkeys() {
    if (g_screen != AppScreen::Studio || g_studioTesting) return;
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || !io.KeyCtrl) return;
    if (io.KeyAlt && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_G, false)) studioGroupSelectionAsFolder();   // Ctrl+Alt+G
    if (!io.KeyShift) return;
    if (ImGui::IsKeyPressed(ImGuiKey_G, false)) studioCsgCombine(0);
    if (ImGui::IsKeyPressed(ImGuiKey_N, false)) studioCsgNegate();
    if (ImGui::IsKeyPressed(ImGuiKey_U, false)) studioCsgSeparate();
}

/** Il MeshPart di indice i e' un'Unione (si puo' separare)? */
static bool studio_csg_isUnion(int modelIndex) {
    return modelIndex >= 0 && modelIndex < (int)g_world.models.size() && studio_csg::isUnionModel(g_world.models[(size_t)modelIndex]);
}
