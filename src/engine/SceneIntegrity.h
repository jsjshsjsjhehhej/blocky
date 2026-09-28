#pragma once


#include <cmath>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/DataModel.h"

namespace SceneIntegrity {

enum class Severity { Info = 0, Warn = 1, Error = 2 };

inline const char* severityName(Severity s) {
    switch (s) {
    case Severity::Error: return "ERRORE";
    case Severity::Warn:  return "AVVISO";
    default:              return "INFO";
    }
}

struct Issue {
    Severity severity = Severity::Info;
    std::string subject;
    std::string rule;
    std::string detail;
    bool repairable = false;
};

struct Result {
    std::vector<Issue> issues;
    int errors = 0;
    int warnings = 0;
    int repairable = 0;

    void add(Severity sev, const std::string& subject, const std::string& rule,
             const std::string& detail, bool canRepair = false) {
        issues.push_back({sev, subject, rule, detail, canRepair});
        if (sev == Severity::Error) ++errors;
        else if (sev == Severity::Warn) ++warnings;
        if (canRepair) ++repairable;
    }

    bool clean() const { return errors == 0 && warnings == 0; }
    bool loadable() const { return errors == 0; }
};


inline bool finite3(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline constexpr float kSaneCoord = 100000.f;

inline bool saneCoord(const glm::vec3& v) {
    return std::fabs(v.x) < kSaneCoord && std::fabs(v.y) < kSaneCoord
        && std::fabs(v.z) < kSaneCoord;
}


inline Result check(const DataModel& world) {
    Result r;

    if (!finite3(world.spawn.position)) {
        r.add(Severity::Error, "SpawnLocation", "posizione-non-finita",
              "NaN o infinito: il player nascerebbe fuori dal mondo", true);
    } else if (!saneCoord(world.spawn.position)) {
        r.add(Severity::Warn, "SpawnLocation", "posizione-assurda",
              "coordinate oltre ogni scala utile", true);
    }
    if (!std::isfinite(world.spawn.yaw)) {
        r.add(Severity::Error, "SpawnLocation", "yaw-non-finito", "rotazione NaN", true);
    }

    std::unordered_map<std::string, int> nameCount;

    for (const WorkspacePart& p : world.parts) {
        const std::string& id = p.name;
        nameCount[id]++;

        if (id.empty())
            r.add(Severity::Warn, "(senza nome)", "nome-vuoto",
                  "una Part senza nome non è raggiungibile da Lua", true);

        if (!finite3(p.position))
            r.add(Severity::Error, id, "posizione-non-finita",
                  "NaN o infinito: sporca l'intero frame", true);
        else if (!saneCoord(p.position))
            r.add(Severity::Warn, id, "posizione-assurda",
                  "coordinate oltre ogni scala utile", true);

        if (!finite3(p.size))
            r.add(Severity::Error, id, "size-non-finita", "NaN o infinito", true);
        else if (p.size.x <= 0.f || p.size.y <= 0.f || p.size.z <= 0.f)
            r.add(Severity::Error, id, "size-nulla",
                  "una dimensione a zero produce una matrice singolare", true);

        if (!finite3(p.rotation))
            r.add(Severity::Error, id, "rotazione-non-finita", "NaN o infinito", true);

        if (!finite3(p.color))
            r.add(Severity::Warn, id, "colore-non-finito", "componente NaN", true);

        if ((int)p.shape < 0 || p.shape >= PartShape::Count)
            r.add(Severity::Error, id, "shape-sconosciuta",
                  "valore fuori da Enum.PartType: probabile file di una build diversa", true);

        if ((int)p.material < 0 || p.material >= PartMaterial::Count)
            r.add(Severity::Error, id, "materiale-sconosciuto",
                  "valore fuori da Enum.Material", true);

        if (!std::isfinite(p.emissive) || p.emissive < 0.f || p.emissive > 4.f)
            r.add(Severity::Warn, id, "emissive-fuori-scala",
                  "atteso fra 0 e 4", true);

        if (p.pointLight) {
            if (!std::isfinite(p.lightRange) || p.lightRange <= 0.f || p.lightRange > 500.f)
                r.add(Severity::Warn, id, "range-luce-fuori-scala",
                      "atteso fra 0 e 500 studs", true);
            if (!std::isfinite(p.lightBrightness) || p.lightBrightness < 0.f)
                r.add(Severity::Warn, id, "luce-non-valida", "brightness negativa", true);
        }

        if (!std::isfinite(p.maxHealth) || p.maxHealth <= 0.f)
            r.add(Severity::Warn, id, "maxhealth-non-valida", "deve essere positiva", true);
        else if (p.health > p.maxHealth)
            r.add(Severity::Warn, id, "health-oltre-massimo",
                  "la Part nasce già oltre il proprio massimo", true);
    }

    for (const auto& [name, count] : nameCount) {
        if (count > 1 && !name.empty())
            r.add(Severity::Warn, name, "nome-duplicato",
                  std::to_string(count) + " Part con lo stesso nome: "
                  "findPart ne troverà sempre e solo una", true);
    }

    for (const WorkspaceModel& m : world.models) {
        if (m.sourcePath.empty()) {
            r.add(Severity::Error, m.name, "riferimento-vuoto",
                  "il modello non punta a nessun file");
            continue;
        }
        std::error_code ec;
        if (!std::filesystem::exists(m.sourcePath, ec))
            r.add(Severity::Error, m.name, "asset-mancante",
                  "file non trovato: " + m.sourcePath);
    }

    for (const WorkspaceLabel& l : world.labels) {
        if (l.text.empty())
            r.add(Severity::Info, l.name, "testo-vuoto", "label senza testo");

        if (!std::isfinite(l.anchorX) || !std::isfinite(l.anchorY) ||
            l.anchorX < 0.f || l.anchorX > 1.f || l.anchorY < 0.f || l.anchorY > 1.f)
            r.add(Severity::Warn, l.name, "ancora-fuori-schermo",
                  "gli anchor vanno da 0 a 1", true);

        if (!std::isfinite(l.fontScale) || l.fontScale <= 0.f)
            r.add(Severity::Warn, l.name, "scala-font-non-valida", "deve essere positiva", true);
    }

    for (const BackpackItem& it : world.backpack) {
        if (it.meshPath.empty()) continue;
        std::error_code ec;
        if (!std::filesystem::exists(it.meshPath, ec))
            r.add(Severity::Warn, it.name, "mesh-mancante",
                  "il tool non avrà modello in mano: " + it.meshPath);
    }

    if (world.equippedBackpack >= DataModel::kHotbarSlots)
        r.add(Severity::Warn, "Backpack", "slot-fuori-range",
              "slot equipaggiato oltre la hotbar", true);

    return r;
}


inline int repair(DataModel& world) {
    int fixed = 0;

    auto fixVec = [&](glm::vec3& v, const glm::vec3& fallback) {
        if (!finite3(v) || !saneCoord(v)) { v = fallback; ++fixed; }
    };

    fixVec(world.spawn.position, {0.f, 0.f, 0.f});
    if (!std::isfinite(world.spawn.yaw)) { world.spawn.yaw = -90.f; ++fixed; }

    std::unordered_map<std::string, int> used;

    for (WorkspacePart& p : world.parts) {
        fixVec(p.position, {0.f, 2.f, 0.f});
        fixVec(p.rotation, {0.f, 0.f, 0.f});

        if (!finite3(p.size)) { p.size = {4.f, 1.f, 2.f}; ++fixed; }
        if (p.size.x <= 0.f || p.size.y <= 0.f || p.size.z <= 0.f) {
            p.size = glm::max(p.size, glm::vec3(0.05f));
            ++fixed;
        }

        if (!finite3(p.color)) { p.color = {0.64f, 0.64f, 0.64f}; ++fixed; }

        if ((int)p.shape < 0 || p.shape >= PartShape::Count) { p.shape = PartShape::Block; ++fixed; }
        if ((int)p.material < 0 || p.material >= PartMaterial::Count) {
            p.material = PartMaterial::Plastic;
            ++fixed;
        }

        if (!std::isfinite(p.emissive) || p.emissive < 0.f || p.emissive > 4.f) {
            p.emissive = std::isfinite(p.emissive) ? std::min(std::max(p.emissive, 0.f), 4.f) : 0.f;
            ++fixed;
        }

        if (p.pointLight) {
            if (!std::isfinite(p.lightRange) || p.lightRange <= 0.f || p.lightRange > 500.f) {
                p.lightRange = 20.f;
                ++fixed;
            }
            if (!std::isfinite(p.lightBrightness) || p.lightBrightness < 0.f) {
                p.lightBrightness = 1.25f;
                ++fixed;
            }
        }

        if (!std::isfinite(p.maxHealth) || p.maxHealth <= 0.f) { p.maxHealth = 100.f; ++fixed; }
        if (!std::isfinite(p.health) || p.health > p.maxHealth) { p.health = p.maxHealth; ++fixed; }

        if (p.name.empty()) { p.name = "Part"; ++fixed; }

        int& n = used[p.name];
        if (++n > 1) {
            p.name += "_" + std::to_string(n);
            ++fixed;
        }
    }

    for (WorkspaceLabel& l : world.labels) {
        if (!std::isfinite(l.anchorX) || l.anchorX < 0.f || l.anchorX > 1.f) { l.anchorX = 0.5f; ++fixed; }
        if (!std::isfinite(l.anchorY) || l.anchorY < 0.f || l.anchorY > 1.f) { l.anchorY = 0.12f; ++fixed; }
        if (!std::isfinite(l.fontScale) || l.fontScale <= 0.f) { l.fontScale = 1.f; ++fixed; }
    }

    if (world.equippedBackpack >= DataModel::kHotbarSlots) {
        world.equippedBackpack = -1;
        ++fixed;
    }

    return fixed;
}

}
