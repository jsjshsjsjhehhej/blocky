#pragma once

// Vincoli fra part (WeldConstraint in DataModel.h): creazione con valori
// sensati, salvataggio nel .blocky e saldature fra part ANCORATE.
//
// La simulazione vera (part non ancorate) la fa Blocky Physics con Jolt
// (BlockyPhysics::syncJoints). Qui niente Jolt ne' ImGui: si prova da solo
// in tools/test_constraints.cpp.

#include "DataModel.h"
#include "PartGroups.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <string>
#include <vector>

namespace partconstraints {

inline bool validPart(const DataModel& dm, int i) { return i >= 0 && i < (int)dm.parts.size(); }

// Nuovo vincolo fra due part, con i parametri ricavati da come sono messe
// adesso: corda/asta/molla lunghe quanto la distanza attuale, cerniera a meta'
// strada con asse verticale (porta). Ritorna l'indice in dm.welds, o -1.
inline int create(DataModel& dm, ConstraintKind kind, int part0, int part1) {
    if (!validPart(dm, part0) || !validPart(dm, part1) || part0 == part1) return -1;
    // Nome unico (gli script lo usano per ritrovarlo): Rope1, Rope2, ...
    std::string name;
    for (int n = (int)dm.welds.size() + 1;; ++n) {
        name = std::string(constraintKindName(kind)) + std::to_string(n);
        bool taken = false;
        for (const auto& w : dm.welds) taken = taken || w.name == name;
        if (!taken) break;
    }
    WeldConstraint& c = dm.addWeld(part0, part1);
    c.kind = kind;
    c.name = name;
    const WorkspacePart& a = dm.parts[(size_t)part0];
    const WorkspacePart& b = dm.parts[(size_t)part1];
    c.length = glm::length(b.position - a.position);
    if (kind == ConstraintKind::Hinge) {
        // Il punto a meta' strada, nello spazio locale di part0.
        const glm::mat3 r0(partRotationMatrix(a.rotation));
        c.pivot = glm::transpose(r0) * ((a.position + b.position) * 0.5f - a.position);
        c.axis = glm::vec3(0, 1, 0);
    }
    return (int)dm.welds.size() - 1;
}

// Dopo aver cancellato la part `removed`: i vincoli che la usavano spariscono,
// gli indici delle part dopo di lei scalano di uno. Senza, un vincolo
// collegherebbe in silenzio due part sbagliate.
inline void onPartErased(DataModel& dm, int removed) {
    for (size_t k = 0; k < dm.welds.size();) {
        WeldConstraint& c = dm.welds[k];
        if (c.part0 == removed || c.part1 == removed) {
            dm.welds.erase(dm.welds.begin() + (long)k);
            continue;
        }
        if (c.part0 > removed) --c.part0;
        if (c.part1 > removed) --c.part1;
        ++k;
    }
}

// ---- Saldature fra part ANCORATE --------------------------------------------
//
// La fisica non le muove (ancorate = ferme), ma uno script si': una
// piattaforma mossa da Lua deve portarsi dietro le part saldate. Ogni frame
// part1 = part0 * offset, con offset preso all'inizio della partita.
struct AnchoredWeldFollow {
    std::vector<glm::mat4> rel;    // stesso indice di dm.welds
    std::vector<int> pairs;        // part0*65536+part1 al momento della presa
    bool ready = false;

    void reset() { rel.clear(); pairs.clear(); ready = false; }

    static glm::mat4 rigid(const WorkspacePart& p) {
        return glm::translate(glm::mat4(1.f), p.position) * partRotationMatrix(p.rotation);
    }

    void apply(DataModel& dm) {
        if (pairs.size() != dm.welds.size()) ready = false;
        if (ready)
            for (size_t k = 0; k < dm.welds.size(); ++k)
                if (pairs[k] != dm.welds[k].part0 * 65536 + dm.welds[k].part1) { ready = false; break; }
        if (!ready) {
            rel.assign(dm.welds.size(), glm::mat4(1.f));
            pairs.assign(dm.welds.size(), -1);
            for (size_t k = 0; k < dm.welds.size(); ++k) {
                const WeldConstraint& c = dm.welds[k];
                pairs[k] = c.part0 * 65536 + c.part1;
                if (validPart(dm, c.part0) && validPart(dm, c.part1))
                    rel[k] = glm::inverse(rigid(dm.parts[(size_t)c.part0])) * rigid(dm.parts[(size_t)c.part1]);
            }
            ready = true;
            return;
        }
        for (size_t k = 0; k < dm.welds.size(); ++k) {
            const WeldConstraint& c = dm.welds[k];
            if (!c.enabled || c.kind != ConstraintKind::Weld) continue;
            if (!validPart(dm, c.part0) || !validPart(dm, c.part1)) continue;
            WorkspacePart& follower = dm.parts[(size_t)c.part1];
            // Solo se la part da trascinare e' ancorata: quelle libere le
            // tiene insieme la fisica, spostarle a mano la combatterebbe.
            if (!follower.anchored || follower.locked) continue;
            const glm::mat4 m = rigid(dm.parts[(size_t)c.part0]) * rel[k];
            follower.position = glm::vec3(m[3]);
            follower.rotation = partgroups::toEuler(glm::quat_cast(glm::mat3(m)));
        }
    }
};

// ---- Salvataggio -------------------------------------------------------------

// Riga di primo livello del .blocky:  "constraints": [{...}],
inline void writeJson(std::ostream& o, const DataModel& dm) {
    o << "  \"constraints\": [";
    for (size_t k = 0; k < dm.welds.size(); ++k) {
        const WeldConstraint& c = dm.welds[k];
        std::string name;
        for (char ch : c.name)   // niente caratteri che romperebbero il JSON o il parser
            if (!std::strchr("\"\\{}[]", ch) && (unsigned char)ch >= 32) name += ch;
        char buf[512];
        std::snprintf(buf, sizeof(buf),
                      "%s{\"name\":\"%s\",\"kind\":%d,\"part0\":%d,\"part1\":%d,\"enabled\":%s,"
                      "\"length\":%g,\"stiffness\":%g,\"damping\":%g,"
                      "\"pivot\":[%g,%g,%g],\"axis\":[%g,%g,%g]}",
                      k ? "," : "", name.c_str(), (int)c.kind, c.part0, c.part1,
                      c.enabled ? "true" : "false", c.length, c.stiffness, c.damping,
                      c.pivot.x, c.pivot.y, c.pivot.z, c.axis.x, c.axis.y, c.axis.z);
        o << buf;
    }
    o << "],\n";
}

// Dopo aver caricato le part. Vincoli che puntano a part inesistenti, o con
// un tipo sconosciuto (file di una versione futura), si scartano.
inline void readJson(const std::string& json, DataModel& dm) {
    dm.welds.clear();
    const size_t key = json.find("\"constraints\"");
    if (key == std::string::npos) return;
    const size_t open = json.find('[', key);
    if (open == std::string::npos) return;
    // Fine dell'elenco contando le parentesi: dentro ci sono anche gli array
    // pivot/axis, quindi la prima ']' non basta.
    size_t arrEnd = std::string::npos;
    for (size_t i = open, depth = 0; i < json.size(); ++i) {
        if (json[i] == '[') ++depth;
        else if (json[i] == ']' && --depth == 0) { arrEnd = i; break; }
    }
    if (arrEnd == std::string::npos) return;
    size_t pos = open;
    while (true) {
        pos = json.find('{', pos + 1);
        if (pos == std::string::npos || pos > arrEnd) break;
        const size_t end = json.find('}', pos);
        if (end == std::string::npos || end > arrEnd) break;
        const std::string obj = json.substr(pos, end - pos);
        auto num = [&](const char* k, double def) {
            const std::string pat = std::string("\"") + k + "\":";
            const size_t at = obj.find(pat);
            return at == std::string::npos ? def : std::strtod(obj.c_str() + at + pat.size(), nullptr);
        };
        auto vec = [&](const char* k, glm::vec3 def) {
            const std::string pat = std::string("\"") + k + "\":[";
            const size_t at = obj.find(pat);
            if (at == std::string::npos) return def;
            float x = 0, y = 0, z = 0;
            if (std::sscanf(obj.c_str() + at + pat.size(), "%f,%f,%f", &x, &y, &z) != 3) return def;
            return glm::vec3(x, y, z);
        };
        WeldConstraint c;
        const size_t n = obj.find("\"name\":\"");
        if (n != std::string::npos) {
            const size_t s = n + 8, e = obj.find('"', s);
            c.name = obj.substr(s, e == std::string::npos ? std::string::npos : e - s);
        }
        const int kind = (int)num("kind", 0);
        c.part0 = (int)num("part0", -1);
        c.part1 = (int)num("part1", -1);
        c.enabled = obj.find("\"enabled\":false") == std::string::npos;
        c.length = (float)num("length", 0);
        c.stiffness = (float)num("stiffness", 3);
        c.damping = (float)num("damping", 0.3);
        c.pivot = vec("pivot", glm::vec3(0.f));
        c.axis = vec("axis", glm::vec3(0, 1, 0));
        pos = end;
        if (kind < 0 || kind >= (int)ConstraintKind::Count) continue;
        if (!validPart(dm, c.part0) || !validPart(dm, c.part1) || c.part0 == c.part1) continue;
        c.kind = (ConstraintKind)kind;
        if (glm::length(c.axis) < 1e-4f) c.axis = {0, 1, 0};
        dm.welds.push_back(c);
    }
}

} // namespace partconstraints
