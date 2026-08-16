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

/** Part nel Workspace (stile Roblox Instance Part). */
struct WorkspacePart {
    std::string name = "Part";
    glm::vec3 position{0};
    glm::vec3 size{4, 1, 2};
    glm::vec3 rotation{0}; // gradi Euler XYZ (Orientation)
    glm::vec3 color{0.64f, 0.64f, 0.64f};
    bool anchored = true;
    bool canCollide = true; // false = No Collision (attraversa)
    bool useStud = true;
    PartShape shape = PartShape::Block;
    PartMaterial material = PartMaterial::Plastic;
    /** 0 = opaco, 1+ = glow (Neon / ForceField). */
    float emissive = 0.f;
    /** PointLight dinamico: illumina le parti vicine. */
    bool pointLight = false;
    float lightRange = 20.f;
    float lightBrightness = 1.25f;
    /** Se false, Destroy/Explode/spada non rimuovono la Part. */
    bool canDestroy = true;
    /** Vita residua (spada / esplosioni a catena). <=0 → Destroy. */
    float health = 100.f;
    float maxHealth = 100.f;
    /** Se true, in Test il tocco/avvicinamento player fa boom C++ (anche senza Lua). */
    bool boomOnTouch = false;
    /** Se > 0: in Test esplode da solo dopo N secondi (0 = solo al tocco). */
    float boomAutoSecs = 0.f;
    std::string script; // Script Lua figlio (vuoto = nessuno)

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

/** Modello 3D importato nel Workspace (mesh esterna). */
struct WorkspaceModel {
    std::string name = "Model";
    std::string sourcePath;
    glm::vec3 position{0, 2, 0};
    glm::vec3 rotation{0};
    glm::vec3 scale{1, 1, 1};
    glm::vec3 tint{1, 1, 1};
    ModelRigInfo rig;
    ModelAnimClip anim;
    /** Tracce caricate dal file: walk, patrol, chase... comandate da Lua. */
    ModelAnimator animator;
    /**
     * Nome tuo → nome nel file. Blender esporta "Armature|ChasingAction": se
     * lo script cerca quello, basta rinominare l'azione per romperlo. Con
     * l'alias lo script dice "chase" e l'associazione si aggiusta in un punto
     * solo. Vuoto = si usa la ricerca per sottostringa.
     */
    std::map<std::string, std::string> clipAlias;
    /** True se il file importato ha uno scheletro: deciso al caricamento. */
    bool rigged = false;
    /**
     * Blocca il player. Default false: i modelli finora non collidevano, e
     * renderli solidi d'ufficio incastrerebbe il player dentro quelli
     * decorativi dei place già salvati. Si accende quando serve.
     */
    bool canCollide = false;
};

/**
 * Label / TextLabel / TextButton — UI a schermo (ScreenGui), non testo 3D nel mondo.
 * Posizione = anchor (0..1 sullo schermo) + offset in pixel.
 * sizeW/sizeH = dimensioni box (0 = auto dal testo).
 */
struct WorkspaceLabel {
    std::string name = "TextLabel";
    std::string text = "TextLabel";
    std::string className = "TextLabel"; // "TextLabel" | "Label" | "TextButton"
    float anchorX = 0.5f;
    float anchorY = 0.12f;
    float offsetX = 0.f;
    float offsetY = 0.f;
    float sizeW = 0.f; // 0 = auto
    float sizeH = 0.f;
    glm::vec3 color{1.f, 1.f, 1.f};
    float fontScale = 1.f;   // 0.5 – 2.5
    glm::vec3 bgColor{0.f, 0.f, 0.f}; // background nero stile Roblox classico
    float bgTransparency = 0.35f; // 0 = opaco, 1 = solo testo+outline
    bool visible = true;

    bool isButton() const {
        return className == "TextButton" || className == "Button";
    }
};

struct SpawnLocation {
    glm::vec3 position{0, 0, 0};
    float yaw = -90.f;
};

/** Player amico (NPC) nel place — in Test/Play segue il locale. */
struct WorkspaceFriend {
    std::string name = "Amico";
    glm::vec3 position{0, 0, 0};
    float yaw = 180.f;
};

/** Tipo tool inventariato (Play/Test). */
enum class BackpackTool : int {
    None = 0,
    Sword = 1,
    Pistol = 2,
};

/** Item nel Backpack del place (appare nell'inventario in Play/Test). */
struct BackpackItem {
    std::string id;
    std::string name = "Item";
    int iconHue = 0; // 0..5 palette colore cerchio
    BackpackTool tool = BackpackTool::None;
    /** Mesh opzionale (OBJ/GLB da Blender) per disegnare il tool in mano. */
    std::string meshPath;
    /** AnimationId da tenere mentre e' equipaggiata (es. PistolFire). */
    std::string animId;
};

inline glm::mat4 partRotationMatrix(const glm::vec3& rotDeg) {
    glm::mat4 r(1.f);
    r = glm::rotate(r, glm::radians(rotDeg.y), {0, 1, 0});
    r = glm::rotate(r, glm::radians(rotDeg.x), {1, 0, 0});
    r = glm::rotate(r, glm::radians(rotDeg.z), {0, 0, 1});
    return r;
}

/** Model matrix: Translate * Rotate * Scale (stile Roblox). */
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
    static constexpr int kHotbarSlots = 9; // slot 1-9 sempre selezionabili (anche vuoti)
    int equippedBackpack = -1; // indice hotbar 0..8, -1 = nessuno (anche se slot empty)
    SpawnLocation spawn;
    bool hasBaseplate = true;
    glm::vec3 baseplateSize{120, 2, 120};
    glm::vec3 baseplateColor{0.55f, 0.56f, 0.55f};

    void clear() {
        parts.clear();
        models.clear();
        labels.clear();
        friends.clear();
        backpack.clear();
        equippedBackpack = -1;
        hasBaseplate = true;
        placeName = "Baseplate";
        spawn = SpawnLocation{};
        baseplateSize = {120, 2, 120};
        baseplateColor = {0.55f, 0.56f, 0.55f};
    }

    BackpackItem& addBackpackItem(const std::string& name, int hue = 0,
                                  BackpackTool tool = BackpackTool::None) {
        BackpackItem it;
        it.name = name.empty() ? ("Item" + std::to_string(backpack.size() + 1)) : name;
        it.id = "bp_" + std::to_string(backpack.size() + 1) + "_" + it.name;
        it.iconHue = hue % 6;
        it.tool = tool;
        // Nome → tool automatico
        if (it.tool == BackpackTool::None) {
            std::string low = it.name;
            for (char& c : low) c = (char)std::tolower((unsigned char)c);
            if (low.find("sword") != std::string::npos || low.find("spada") != std::string::npos)
                it.tool = BackpackTool::Sword;
            else if (low.find("pistol") != std::string::npos || low.find("pistola") != std::string::npos
                     || low.find("gun") != std::string::npos || low.find("fucile") != std::string::npos)
                it.tool = BackpackTool::Pistol;
        }
        backpack.push_back(std::move(it));
        return backpack.back();
    }

    /** Slot hotbar 0..8. toggleSame=true → stesso tasto = deseleziona. Funziona anche se empty. */
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
        addPart("Part", {8, 2, -4}, {4, 4, 4}, {0.64f, 0.64f, 0.64f});
        addLabel("Welcome", "Benvenuto su Blocky", 0.5f, 0.10f, {0.35f, 0.75f, 1.f});
    }

    void loadGardenPlace() {
        clear();
        placeName = "Grow a Garden";
        hasBaseplate = true;
        baseplateColor = {0.25f, 0.62f, 0.28f};
        for (int i = -1; i <= 1; ++i)
            for (int j = -1; j <= 1; ++j)
                addPart("Plot_" + std::to_string(i) + "_" + std::to_string(j),
                    {i * 12.f, 0.5f, j * 12.f}, {5, 1, 5}, {0.45f, 0.28f, 0.12f});
        addLabel("GardenTitle", "GARDEN", 0.5f, 0.08f, {1.f, 0.92f, 0.35f});
    }

    void loadObbyPlace() {
        clear();
        placeName = "Mega Obby";
        for (int i = 0; i < 8; ++i)
            addPart("Step" + std::to_string(i),
                {i * 4.f, 1.f + i * 1.5f, 0}, {3, 1, 3},
                {0.2f + i * 0.08f, 0.5f, 0.9f - i * 0.06f});
        spawn.position = {0, 0, 0};
        addLabel("Start", "START", 0.15f, 0.12f, {0.3f, 1.f, 0.5f}, "Label");
        addLabel("Finish", "FINISH", 0.85f, 0.12f, {1.f, 0.4f, 0.3f}, "Label");
    }

    /** Arena PvP: muri + piattaforme. */
    void loadArenaPlace() {
        clear();
        placeName = "Arena";
        baseplateColor = {0.35f, 0.38f, 0.42f};
        baseplateSize = {80, 2, 80};
        addPart("WallN", {0, 4, -38}, {76, 8, 2}, {0.55f, 0.2f, 0.2f});
        addPart("WallS", {0, 4, 38}, {76, 8, 2}, {0.55f, 0.2f, 0.2f});
        addPart("WallE", {38, 4, 0}, {2, 8, 76}, {0.55f, 0.2f, 0.2f});
        addPart("WallW", {-38, 4, 0}, {2, 8, 76}, {0.55f, 0.2f, 0.2f});
        addPart("PadA", {-12, 1, 0}, {10, 2, 10}, {0.2f, 0.45f, 0.9f});
        addPart("PadB", {12, 1, 0}, {10, 2, 10}, {0.9f, 0.35f, 0.2f});
        spawn.position = {0, 0, 16};
        addLabel("ArenaTitle", "ARENA", 0.5f, 0.08f, {1.f, 0.55f, 0.25f});
    }

    /** Parco: alberi stilizzati + laghetto. */
    void loadParkPlace() {
        clear();
        placeName = "Park";
        baseplateColor = {0.28f, 0.68f, 0.32f};
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

    /** Città: strade + edifici semplici. */
    void loadCityPlace() {
        clear();
        placeName = "City";
        baseplateColor = {0.32f, 0.34f, 0.36f};
        baseplateSize = {140, 2, 140};
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
