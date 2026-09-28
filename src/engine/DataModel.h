#pragma once

#include "AnimTrack.h"
#include "ModelAnim.h"
#include "PartMaterial.h"
#include "PartShape.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cctype>
#include <map>
#include <string>
#include <vector>

enum class ScriptType : int { Legacy = 0, Server = 1, Client = 2 };

struct WorkspacePart {
    std::string name = "Part";
    glm::vec3 position{0};
    glm::vec3 size{4, 1, 2};
    glm::vec3 rotation{0};
    glm::vec3 color{0.64f, 0.64f, 0.64f};
    bool anchored = true;
    bool canCollide = true;
    bool useStud = true;
    PartShape shape = PartShape::Block;
    PartMaterial material = PartMaterial::Plastic;
    float emissive = 0.f;
    bool pointLight = false;
    float lightRange = 20.f;
    float lightBrightness = 1.25f;
    bool canDestroy = true;
    float health = 100.f;
    float maxHealth = 100.f;
    bool boomOnTouch = false;
    float boomAutoSecs = 0.f;
    std::string script;
    ScriptType scriptType = ScriptType::Legacy;

    bool isNeon() const { return material == PartMaterial::Neon; }
    bool isTransparent() const {
        return material == PartMaterial::Glass || material == PartMaterial::Ice
            || material == PartMaterial::ForceField;
    }

    float effectiveEmissive() const {
        float bias = 0.f, sp = 0.f, ro = 0.f, a = 1.f;
        partMaterialShading(material, sp, ro, bias, a);
        return std::max(emissive, bias);
    }

    void setMaterial(PartMaterial m) {
        material = m;
        if (m == PartMaterial::Neon) {
            emissive = std::max(emissive, 1.f);
            useStud = false;
        } else if (m == PartMaterial::SmoothPlastic || m == PartMaterial::Glass
                   || m == PartMaterial::Metal || m == PartMaterial::Ice
                   || m == PartMaterial::ForceField) {
            useStud = false;
        }
    }
};

struct WorkspaceModel {
    std::string name = "Model";
    std::string sourcePath;
    glm::vec3 position{0, 2, 0};
    glm::vec3 rotation{0};
    glm::vec3 scale{1, 1, 1};
    glm::vec3 tint{1, 1, 1};
    ModelRigInfo rig;
    ModelAnimClip anim;
    ModelAnimator animator;
    std::map<std::string, std::string> clipAlias;
    bool rigged = false;
    bool canCollide = false;
    std::string script;
    ScriptType scriptType = ScriptType::Legacy;
};

struct WorkspaceLabel {
    std::string name = "TextLabel";
    std::string text = "TextLabel";
    std::string className = "TextLabel";
    float anchorX = 0.5f;
    float anchorY = 0.12f;
    float offsetX = 0.f;
    float offsetY = 0.f;
    float sizeW = 0.f;
    float sizeH = 0.f;
    glm::vec3 color{1.f, 1.f, 1.f};
    float fontScale = 1.f;
    glm::vec3 bgColor{0.f, 0.f, 0.f};
    float bgTransparency = 0.35f;
    bool visible = true;
    std::string script;
    ScriptType scriptType = ScriptType::Legacy;

    bool isButton() const {
        return className == "TextButton" || className == "Button";
    }
};

struct SpawnLocation {
    glm::vec3 position{0, 0, 0};
    float yaw = -90.f;
};

struct WorkspaceFriend {
    std::string name = "Amico";
    glm::vec3 position{0, 0, 0};
    float yaw = 180.f;
};

enum class BackpackTool : int {
    None = 0,
    Sword = 1,
    Pistol = 2,
    Torch = 3,
};

// Quanto si alza il braccio destro quando si ha qualcosa in mano.
// Sta qui, in un posto solo, perche lo usano sia il disegno del corpo sia
// quello della mesh tenuta: se i due numeri divergessero, l oggetto
// resterebbe staccato dalla mano.
// -90 = braccio orizzontale, puntato in avanti. Era -55, cioe alzato a meta:
// un oggetto tenuto cosi sembra appoggiato al fianco invece che impugnato.
inline constexpr float kEquippedArmHoldDeg = -90.f;

// Lunghezza di riferimento di un oggetto tenuto in mano, in stud: circa quanto
// un avambraccio del personaggio, che e la scala giusta per un attrezzo.
inline constexpr float kGripTargetLen = 3.2f;

struct BackpackItem {
    std::string id;
    std::string name = "Item";
    int iconHue = 0;
    BackpackTool tool = BackpackTool::None;
    std::string meshPath;
    std::string animId;

    // Come l oggetto sta in mano (l equivalente del Grip di Roblox).
    //
    // Prima non esisteva: la mesh veniva piazzata con uno scostamento fisso e
    // alla sua dimensione originale, quindi un modello importato poteva
    // risultare minuscolo, gigantesco o girato di traverso, e non c era modo
    // di correggerlo.
    //
    // gripScale MOLTIPLICA la dimensione adattata automaticamente, non quella
    // grezza del file: 1 = come l adatta il motore, 2 = il doppio, 0.5 = meta.
    // Cosi il numero significa sempre la stessa cosa a prescindere da come il
    // modello e stato esportato, e "piu grande" e semplicemente un numero piu
    // alto invece di un valore assoluto da indovinare.
    glm::vec3 gripOffset{0.f};
    glm::vec3 gripRotation{0.f};   // gradi, X Y Z
    float     gripScale = 1.f;
};

inline glm::mat4 partRotationMatrix(const glm::vec3& rotDeg) {
    glm::mat4 r(1.f);
    r = glm::rotate(r, glm::radians(rotDeg.y), {0, 1, 0});
    r = glm::rotate(r, glm::radians(rotDeg.x), {1, 0, 0});
    r = glm::rotate(r, glm::radians(rotDeg.z), {0, 0, 1});
    return r;
}

inline glm::mat4 partModelMatrix(const WorkspacePart& p) {
    return glm::translate(glm::mat4(1.f), p.position)
         * partRotationMatrix(p.rotation)
         * glm::scale(glm::mat4(1.f), p.size);
}

class DataModel {
public:
    std::string placeName = "Baseplate";
    std::vector<WorkspacePart> parts;
    std::vector<WorkspaceModel> models;
    std::vector<WorkspaceLabel> labels;
    std::vector<WorkspaceFriend> friends;
    std::vector<BackpackItem> backpack;
    static constexpr int kHotbarSlots = 9;
    int equippedBackpack = -1;
    SpawnLocation spawn;

    void clear() {
        parts.clear();
        models.clear();
        labels.clear();
        friends.clear();
        backpack.clear();
        equippedBackpack = -1;
        placeName = "Baseplate";
        spawn = SpawnLocation{};
    }

    BackpackItem& addBackpackItem(const std::string& name, int hue = 0,
                                  BackpackTool tool = BackpackTool::None) {
        BackpackItem it;
        it.name = name.empty() ? ("Item" + std::to_string(backpack.size() + 1)) : name;
        it.id = "bp_" + std::to_string(backpack.size() + 1) + "_" + it.name;
        it.iconHue = hue % 6;
        it.tool = tool;
        if (it.tool == BackpackTool::None) {
            std::string low = it.name;
            for (char& c : low) c = (char)std::tolower((unsigned char)c);
            if (low.find("sword") != std::string::npos || low.find("spada") != std::string::npos)
                it.tool = BackpackTool::Sword;
            else if (low.find("pistol") != std::string::npos || low.find("pistola") != std::string::npos
                     || low.find("gun") != std::string::npos || low.find("fucile") != std::string::npos)
                it.tool = BackpackTool::Pistol;
            else if (low.find("torch") != std::string::npos || low.find("torcia") != std::string::npos)
                it.tool = BackpackTool::Torch;
        }
        backpack.push_back(std::move(it));
        return backpack.back();
    }

    void equipBackpackSlot(int idx, bool toggleSame = true) {
        if (idx < 0 || idx >= kHotbarSlots) return;
        if (toggleSame && equippedBackpack == idx) equippedBackpack = -1;
        else equippedBackpack = idx;
    }

    void cycleBackpack(int dir) {
        const int n = kHotbarSlots;
        if (equippedBackpack < 0) {
            equippedBackpack = (dir >= 0) ? 0 : (n - 1);
            return;
        }
        int next = equippedBackpack + (dir >= 0 ? 1 : -1);
        if (next < 0) next = n - 1;
        if (next >= n) next = 0;
        equippedBackpack = next;
    }

    void loadBaseplatePlace() {
        clear();
        placeName = "Baseplate";
        addBaseplate({120, 2, 120}, {0.55f, 0.56f, 0.55f});
        addPart("Part", {8, 2, -4}, {4, 4, 4}, {0.64f, 0.64f, 0.64f});
        addLabel("Welcome", "Benvenuto su Blocky", 0.5f, 0.10f, {0.35f, 0.75f, 1.f});
    }

    void loadGardenPlace() {
        clear();
        placeName = "Grow a Garden";
        addBaseplate({120, 2, 120}, {0.25f, 0.62f, 0.28f});
        for (int i = -1; i <= 1; ++i)
            for (int j = -1; j <= 1; ++j)
                addPart("Plot_" + std::to_string(i) + "_" + std::to_string(j),
                    {i * 12.f, 0.5f, j * 12.f}, {5, 1, 5}, {0.45f, 0.28f, 0.12f});
        addLabel("GardenTitle", "GARDEN", 0.5f, 0.08f, {1.f, 0.92f, 0.35f});
    }

    void loadObbyPlace() {
        clear();
        placeName = "Mega Obby";
        addBaseplate({120, 2, 120}, {0.55f, 0.56f, 0.55f});
        for (int i = 0; i < 8; ++i)
            addPart("Step" + std::to_string(i),
                {i * 4.f, 1.f + i * 1.5f, 0}, {3, 1, 3},
                {0.2f + i * 0.08f, 0.5f, 0.9f - i * 0.06f});
        spawn.position = {0, 0, 0};
        addLabel("Start", "START", 0.15f, 0.12f, {0.3f, 1.f, 0.5f}, "Label");
        addLabel("Finish", "FINISH", 0.85f, 0.12f, {1.f, 0.4f, 0.3f}, "Label");
    }

    void loadArenaPlace() {
        clear();
        placeName = "Arena";
        addBaseplate({80, 2, 80}, {0.35f, 0.38f, 0.42f});
        addPart("WallN", {0, 4, -38}, {76, 8, 2}, {0.55f, 0.2f, 0.2f});
        addPart("WallS", {0, 4, 38}, {76, 8, 2}, {0.55f, 0.2f, 0.2f});
        addPart("WallE", {38, 4, 0}, {2, 8, 76}, {0.55f, 0.2f, 0.2f});
        addPart("WallW", {-38, 4, 0}, {2, 8, 76}, {0.55f, 0.2f, 0.2f});
        addPart("PadA", {-12, 1, 0}, {10, 2, 10}, {0.2f, 0.45f, 0.9f});
        addPart("PadB", {12, 1, 0}, {10, 2, 10}, {0.9f, 0.35f, 0.2f});
        spawn.position = {0, 0, 16};
        addLabel("ArenaTitle", "ARENA", 0.5f, 0.08f, {1.f, 0.55f, 0.25f});
    }

    void loadParkPlace() {
        clear();
        placeName = "Park";
        addBaseplate({120, 2, 120}, {0.28f, 0.68f, 0.32f});
        auto tree = [&](float x, float z) {
            addPart("Trunk", {x, 2, z}, {1.2f, 4, 1.2f}, {0.42f, 0.26f, 0.12f});
            addPart("Leaves", {x, 5.5f, z}, {4, 3.5f, 4}, {0.18f, 0.55f, 0.22f});
        };
        tree(-16, -12); tree(14, -8); tree(-10, 14); tree(18, 10);
        addPart("Pond", {0, 0.2f, 0}, {16, 0.4f, 10}, {0.2f, 0.45f, 0.85f});
        addPart("Bench", {8, 0.8f, 8}, {5, 1.2f, 1.5f}, {0.5f, 0.32f, 0.18f});
        spawn.position = {0, 0, 18};
        addLabel("ParkSign", "PARCO BLOCKY", 0.5f, 0.10f, {0.95f, 0.95f, 1.f}, "Label");
    }

    void loadCityPlace() {
        clear();
        placeName = "City";
        addBaseplate({140, 2, 140}, {0.32f, 0.34f, 0.36f});
        addPart("RoadX", {0, 0.15f, 0}, {120, 0.3f, 10}, {0.18f, 0.18f, 0.2f});
        addPart("RoadZ", {0, 0.15f, 0}, {10, 0.3f, 120}, {0.18f, 0.18f, 0.2f});
        for (int i = 0; i < 4; ++i) {
            float x = -30.f + i * 20.f;
            float h = 10.f + (i % 3) * 6.f;
            addPart("Bldg" + std::to_string(i),
                {x, h * 0.5f, -22.f}, {8, h, 8},
                {0.55f + i * 0.08f, 0.58f, 0.65f});
            addPart("BldgB" + std::to_string(i),
                {x, (h - 2) * 0.5f, 22.f}, {8, h - 2, 8},
                {0.45f, 0.5f + i * 0.06f, 0.7f});
        }
        spawn.position = {0, 0, 30};
        addLabel("CitySign", "CITY", 0.5f, 0.08f, {0.4f, 0.85f, 1.f});
    }

    WorkspacePart* findPart(const std::string& name) {
        for (auto& p : parts)
            if (p.name == name) return &p;
        return nullptr;
    }

    WorkspacePart& addPart(const std::string& name, glm::vec3 pos, glm::vec3 size, glm::vec3 col) {
        WorkspacePart p;
        p.name = name;
        p.position = pos;
        p.size = size;
        p.color = col;
        parts.push_back(std::move(p));
        return parts.back();
    }

    // La Baseplate e' semplicemente una Part chiamata "Baseplate" (come su
    // Roblox) — nessun campo speciale: gizmo, materiale, script funzionano
    // gia' perche' e' una WorkspacePart come le altre. Posizionata cosi' la
    // faccia superiore resta a Y=0, come nel vecchio rendering fisso.
    WorkspacePart& addBaseplate(glm::vec3 size, glm::vec3 col) {
        WorkspacePart& p = addPart("Baseplate", {0.f, -size.y * 0.5f, 0.f}, size, col);
        p.material = PartMaterial::Plastic;
        return p;
    }

    WorkspaceModel& addModel(const std::string& name, const std::string& path, glm::vec3 pos) {
        WorkspaceModel m;
        m.name = name;
        m.sourcePath = path;
        m.position = pos;
        models.push_back(std::move(m));
        return models.back();
    }

    WorkspaceLabel& addLabel(const std::string& name, const std::string& text,
                             float ax = 0.5f, float ay = 0.12f,
                             glm::vec3 col = {1, 1, 1},
                             const char* className = "TextLabel") {
        WorkspaceLabel l;
        l.name = name;
        l.text = text;
        l.anchorX = ax;
        l.anchorY = ay;
        l.color = col;
        l.className = className ? className : "TextLabel";
        labels.push_back(std::move(l));
        return labels.back();
    }

    int nextPartIndex() const { return (int)parts.size() + 1; }
    int nextModelIndex() const { return (int)models.size() + 1; }
    int nextLabelIndex() const { return (int)labels.size() + 1; }
};
