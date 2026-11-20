#pragma once

// Model di Roblox Studio per Blocky: Ctrl+G raggruppa, Ctrl+U separa, i Model
// si annidano, si salvano nel .blocky e si spostano/ruotano come un pezzo solo.
//
// Solo dati e geometria, niente ImGui ne' globali: l'interfaccia e' in
// main_ui/StudioGroupsUi.h, i test in tools/test_groups.cpp.
//
// Le part puntano al loro Model con un id stabile (WorkspacePart::groupId),
// non con un indice: cancellare o riordinare le part non rompe i Model.

#include "DataModel.h"
#include "JsonScan.h"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace partgroups {

inline PartGroup* find(DataModel& dm, int id) {
    if (id <= 0) return nullptr;
    for (auto& g : dm.groups)
        if (g.id == id) return &g;
    return nullptr;
}

inline const PartGroup* find(const DataModel& dm, int id) {
    return find(const_cast<DataModel&>(dm), id);
}

inline int parentOf(const DataModel& dm, int id) {
    const PartGroup* g = find(dm, id);
    return g ? g->parent : 0;
}

// ancestor contiene group (o e' lui)? Il limite ai passi protegge da cicli
// in un file rovinato.
inline bool isWithin(const DataModel& dm, int group, int ancestor) {
    for (int steps = 0; group > 0 && steps < 256; ++steps) {
        if (group == ancestor) return true;
        group = parentOf(dm, group);
    }
    return false;
}

// Model piu' esterno che contiene la part (0 = la part non e' in un Model).
// Le Folder non contano: una part in una Folder si seleziona da sola.
inline int topGroupOf(const DataModel& dm, int partIndex) {
    if (partIndex < 0 || partIndex >= (int)dm.parts.size()) return 0;
    int g = dm.parts[(size_t)partIndex].groupId;
    int best = 0;
    for (int steps = 0; g > 0 && steps < 256; ++steps) {
        const PartGroup* pg = find(dm, g);
        if (!pg) break;
        if (!pg->folder) best = g;
        const int p = pg->parent;
        if (p <= 0) break;
        g = p;
    }
    return best;
}

// Model (non Folder) piu' interno che contiene la part e ha DragDetector.
// 0 = nessuno: il drag resta sulla singola part.
inline int dragGroupOf(const DataModel& dm, int partIndex) {
    if (partIndex < 0 || partIndex >= (int)dm.parts.size()) return 0;
    for (int g = dm.parts[(size_t)partIndex].groupId, steps = 0; g > 0 && steps < 256; ++steps) {
        const PartGroup* pg = find(dm, g);
        if (!pg) break;
        if (!pg->folder && pg->dragDetector) return g;
        g = pg->parent;
    }
    return 0;
}

// Tutte le part del Model, compresi i Model annidati.
inline std::vector<int> memberParts(const DataModel& dm, int groupId) {
    std::vector<int> out;
    if (groupId <= 0) return out;
    for (int i = 0; i < (int)dm.parts.size(); ++i)
        if (isWithin(dm, dm.parts[(size_t)i].groupId, groupId)) out.push_back(i);
    return out;
}

inline std::vector<int> childGroups(const DataModel& dm, int parentId) {
    std::vector<int> out;
    for (const auto& g : dm.groups)
        if (g.parent == parentId) out.push_back(g.id);
    return out;
}

inline int findByName(const DataModel& dm, const std::string& name) {
    for (const auto& g : dm.groups)
        if (g.name == name) return g.id;
    return 0;
}

inline int addGroup(DataModel& dm, const std::string& name, int parent) {
    PartGroup g;
    g.id = dm.nextGroupId++;
    g.name = name.empty() ? "Model" : name;
    g.parent = find(dm, parent) ? parent : 0;
    dm.groups.push_back(g);
    return g.id;
}

// Ctrl+G sulle part selezionate. Un Model selezionato per intero entra nel
// nuovo Model cosi' com'e' (annidato); una part sciolta, o un pezzo di un
// Model selezionato solo in parte, entra direttamente. Ritorna l'id (0 se
// la selezione e' vuota).
inline int group(DataModel& dm, const std::vector<int>& selection, const std::string& name = "Model") {
    std::set<int> sel;
    for (int i : selection)
        if (i >= 0 && i < (int)dm.parts.size()) sel.insert(i);
    if (sel.empty()) return 0;

    const int id = addGroup(dm, name, 0);
    std::set<int> wholeTops;
    for (int i : sel) {
        const int top = topGroupOf(dm, i);
        if (top <= 0 || top == id) continue;
        bool all = true;
        for (int m : memberParts(dm, top))
            if (!sel.count(m)) { all = false; break; }
        if (all) wholeTops.insert(top);
    }
    for (int top : wholeTops) find(dm, top)->parent = id;
    for (int i : sel) {
        if (wholeTops.count(topGroupOf(dm, i)) || topGroupOf(dm, i) == id) continue;
        dm.parts[(size_t)i].groupId = id;
    }
    return id;
}

// Ctrl+U: le part e i Model figli salgono di un livello, il Model sparisce.
inline bool ungroup(DataModel& dm, int groupId) {
    PartGroup* g = find(dm, groupId);
    if (!g) return false;
    const int up = g->parent;
    for (auto& p : dm.parts)
        if (p.groupId == groupId) p.groupId = up;
    for (auto& c : dm.groups)
        if (c.parent == groupId) c.parent = up;
    dm.groups.erase(std::remove_if(dm.groups.begin(), dm.groups.end(),
                                   [&](const PartGroup& x) { return x.id == groupId; }),
                    dm.groups.end());
    return true;
}

// Model rimasti senza part (cancellate) spariscono, come in Studio.
inline int pruneEmpty(DataModel& dm) {
    int removed = 0;
    for (bool again = true; again;) {
        again = false;
        for (size_t k = 0; k < dm.groups.size(); ++k) {
            const int id = dm.groups[k].id;
            if (memberParts(dm, id).empty()) {
                ungroup(dm, id);
                ++removed;
                again = true;
                break;
            }
        }
    }
    return removed;
}

// Duplicato di una selezione: i Model selezionati per intero diventano Model
// nuovi (stesso nome e struttura); una part copiata da sola resta nel Model
// della part originale, come in Roblox. originals[k] -> copies[k].
inline void remapCopies(DataModel& dm, const std::vector<int>& originals, const std::vector<int>& copies) {
    std::set<int> sel(originals.begin(), originals.end());
    // Le copie hanno ancora il Model dell'originale: non contano come membri.
    const std::set<int> copySet(copies.begin(), copies.end());
    std::set<int> whole;
    for (int i : originals) {
        for (int g = (i >= 0 && i < (int)dm.parts.size()) ? dm.parts[(size_t)i].groupId : 0;
             g > 0; g = parentOf(dm, g)) {
            bool all = true;
            for (int m : memberParts(dm, g))
                if (!sel.count(m) && !copySet.count(m)) { all = false; break; }
            if (all) whole.insert(g);
        }
    }
    std::map<int, int> clone;
    // Prima i Model piu' esterni, cosi' i figli trovano gia' il padre clonato.
    std::vector<int> order(whole.begin(), whole.end());
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        int da = 0, db = 0;
        for (int g = a; g > 0 && da < 256; g = parentOf(dm, g)) ++da;
        for (int g = b; g > 0 && db < 256; g = parentOf(dm, g)) ++db;
        return da < db;
    });
    for (int g : order) {
        const PartGroup src = *find(dm, g);
        const int parent = clone.count(src.parent) ? clone[src.parent] : src.parent;
        clone[g] = addGroup(dm, src.name, parent);
        if (PartGroup* ng = find(dm, clone[g])) {
            ng->folder = src.folder;
            ng->dragDetector = src.dragDetector;
            ng->pivotOffset = src.pivotOffset;
        }
    }
    for (size_t k = 0; k < originals.size() && k < copies.size(); ++k) {
        const int c = copies[k];
        if (c < 0 || c >= (int)dm.parts.size()) continue;
        const int g = dm.parts[(size_t)c].groupId;
        if (clone.count(g)) dm.parts[(size_t)c].groupId = clone[g];
    }
}

// ---- Geometria ------------------------------------------------------------

inline glm::quat toQuat(const glm::vec3& eulerDeg) {
    return glm::quat_cast(glm::mat3(partRotationMatrix(eulerDeg)));
}

inline glm::vec3 toEuler(const glm::quat& q) {
    float y = 0.f, x = 0.f, z = 0.f;
    glm::extractEulerAngleYXZ(glm::mat4_cast(glm::normalize(q)), y, x, z);
    return glm::degrees(glm::vec3(x, y, z));
}

inline bool boundsOf(const DataModel& dm, const std::vector<int>& idx, glm::vec3& mn, glm::vec3& mx) {
    bool any = false;
    for (int i : idx) {
        if (i < 0 || i >= (int)dm.parts.size()) continue;
        const glm::mat4 m = partModelMatrix(dm.parts[(size_t)i]);
        for (int c = 0; c < 8; ++c) {
            const glm::vec3 v = glm::vec3(m * glm::vec4((c & 1) ? 0.5f : -0.5f, (c & 2) ? 0.5f : -0.5f,
                                                        (c & 4) ? 0.5f : -0.5f, 1.f));
            mn = any ? glm::min(mn, v) : v;
            mx = any ? glm::max(mx, v) : v;
            any = true;
        }
    }
    return any;
}

// Il pivot di un Model: centro del suo ingombro (come il WorldPivot di
// default di Roblox).
inline glm::vec3 pivot(const DataModel& dm, int groupId) {
    glm::vec3 mn(0.f), mx(0.f);
    glm::vec3 c = boundsOf(dm, memberParts(dm, groupId), mn, mx) ? (mn + mx) * 0.5f : glm::vec3(0.f);
    if (const PartGroup* g = find(dm, groupId)) c += g->pivotOffset;
    return c;
}

// Sposta tutto il Model cosi' che il pivot finisca in target (Model:MoveTo).
inline void moveTo(DataModel& dm, int groupId, const glm::vec3& target) {
    const glm::vec3 d = target - pivot(dm, groupId);
    for (int i : memberParts(dm, groupId)) dm.parts[(size_t)i].position += d;
}

// Scala un insieme di part attorno al centro del loro ingombro (Model:ScaleTo
// relativo): si ingrandiscono le part E le distanze fra loro.
inline void scaleParts(DataModel& dm, const std::vector<int>& idx, float factor) {
    if (factor <= 0.f || !std::isfinite(factor)) return;
    glm::vec3 mn(0.f), mx(0.f);
    if (!boundsOf(dm, idx, mn, mx)) return;
    const glm::vec3 c = (mn + mx) * 0.5f;
    for (int i : idx) {
        if (i < 0 || i >= (int)dm.parts.size()) continue;
        WorkspacePart& p = dm.parts[(size_t)i];
        p.position = c + (p.position - c) * factor;
        p.size = glm::clamp(p.size * factor, glm::vec3(kMinPartSize), glm::vec3(100000.f));
    }
}

struct Pose {
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};   // gradi, come WorkspacePart
};

// La part "principale" della selezione e' passata da before ad after (gizmo,
// Properties, Transform Type-In): le altre la seguono come un corpo rigido,
// stessa traslazione e stessa rotazione attorno alla principale.
inline void applyFollow(std::vector<WorkspacePart>& parts, const std::vector<int>& followers,
                        const Pose& before, const Pose& after) {
    const glm::quat q0 = toQuat(before.rotation), q1 = toQuat(after.rotation);
    const glm::quat dq = q1 * glm::inverse(q0);
    const bool rotated = std::abs(glm::dot(q0, q1)) < 0.999999f;
    for (int i : followers) {
        if (i < 0 || i >= (int)parts.size()) continue;
        WorkspacePart& p = parts[(size_t)i];
        p.position = after.position + dq * (p.position - before.position);
        if (rotated) p.rotation = toEuler(dq * toQuat(p.rotation));
    }
}

// ---- Salvataggio ----------------------------------------------------------

// Riga di primo livello del .blocky:  "groups": [{"id":1,"name":"Model","parent":0}],
inline void writeJson(std::ostream& o, const DataModel& dm) {
    o << "  \"groups\": [";
    for (size_t k = 0; k < dm.groups.size(); ++k) {
        const PartGroup& g = dm.groups[k];
        std::string name;
        for (char c : g.name)   // niente virgolette o barre che romperebbero il JSON
            if (c != '"' && c != '\\' && (unsigned char)c >= 32) name += c;
        o << (k ? "," : "") << "{\"id\":" << g.id << ",\"name\":\"" << name
          << "\",\"parent\":" << g.parent << (g.folder ? ",\"folder\":1" : "")
          << (g.dragDetector ? ",\"drag\":1" : "")
          << ",\"pivotx\":" << g.pivotOffset.x << ",\"pivoty\":" << g.pivotOffset.y
          << ",\"pivotz\":" << g.pivotOffset.z << "}";
    }
    o << "],\n";
}

// Dopo aver caricato le part (con il loro groupId): legge i Model e ripara
// quello che non torna (padre mancante, cicli, part che puntano al vuoto).
inline void readJson(const std::string& json, DataModel& dm) {
    dm.groups.clear();
    dm.nextGroupId = 1;
    const size_t key = blk::jsonscan::topKey(json, "groups");
    if (key != std::string::npos) {
        const size_t open = json.find('[', key);
        const size_t close = open == std::string::npos ? open : json.find(']', open);
        for (size_t pos = open; open != std::string::npos && close != std::string::npos;) {
            pos = json.find('{', pos + 1);
            if (pos == std::string::npos || pos > close) break;
            const size_t end = json.find('}', pos);
            const std::string obj = json.substr(pos, end - pos);
            auto num = [&](const char* k) {
                const size_t at = obj.find(std::string("\"") + k + "\":");
                return at == std::string::npos ? 0 : std::atoi(obj.c_str() + at + std::strlen(k) + 3);
            };
            auto fnum = [&](const char* k) {
                const size_t at = obj.find(std::string("\"") + k + "\":");
                return at == std::string::npos ? 0.f : (float)std::atof(obj.c_str() + at + std::strlen(k) + 3);
            };
            PartGroup g;
            g.id = num("id");
            g.parent = num("parent");
            g.folder = num("folder") != 0;
            g.dragDetector = num("drag") != 0;
            g.pivotOffset = { fnum("pivotx"), fnum("pivoty"), fnum("pivotz") };
            const size_t n = obj.find("\"name\":\"");
            if (n != std::string::npos) {
                const size_t s = n + 8, e = obj.find('"', s);
                g.name = obj.substr(s, e == std::string::npos ? std::string::npos : e - s);
            }
            if (g.id > 0 && !find(dm, g.id)) {
                dm.groups.push_back(g);
                dm.nextGroupId = std::max(dm.nextGroupId, g.id + 1);
            }
        }
    }
    for (auto& g : dm.groups) {
        if (!find(dm, g.parent)) g.parent = 0;
        for (int p = g.parent, steps = 0; p > 0; p = parentOf(dm, p))
            if (p == g.id || ++steps > 256) { g.parent = 0; break; }
    }
    for (auto& p : dm.parts)
        if (!find(dm, p.groupId)) p.groupId = 0;
}

} // namespace partgroups
