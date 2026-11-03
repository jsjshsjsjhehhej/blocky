#pragma once

#include "AnimTrack.h"
#include "Humanoid.h"
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

// Script = server/host · LocalScript = client · ModuleScript = solo require
// Legacy = compat vecchi place (gira ovunque come prima).
enum class ScriptType : int {
    Legacy = 0,
    Server = 1,       // Script
    Client = 2,       // LocalScript
    ModuleScript = 3
};

inline const char* scriptTypeLabel(ScriptType t) {
    switch (t) {
    case ScriptType::Server: return "Script (Server)";
    case ScriptType::Client: return "LocalScript (Client)";
    case ScriptType::ModuleScript: return "ModuleScript";
    default: return "Legacy (compat)";
    }
}

struct WorkspacePart {
    std::string name = "Part";
    // ID stabile per replica rete (Host lo assegna). 0 = non ancora in rete.
    uint32_t netId = 0;
    glm::vec3 position{0};
    glm::vec3 size{4, 1, 2};
    glm::vec3 rotation{0};
    glm::vec3 color{0.64f, 0.64f, 0.64f};
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help) T field = def;
#include "PartProps.def"
#undef PART_PROP
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
    // Hierarchy Workspace: -1 = figlio diretto di Workspace; altrimenti indice Part padre.
    int parentIndex = -1;
    // Model (Ctrl+G) di cui fa parte: id stabile di PartGroup, 0 = nessuno.
    // Un id e non un indice: cancellare altre part non lo sposta.
    int groupId = 0;
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
    // Studio 2020: PrimaryPart + Pivot (WorldPivot ≈ position + pivotOffset).
    int primaryPart = -1;          // index in DataModel::parts, -1 = nessuno
    glm::vec3 pivotOffset{0.f};
    std::string script;
    ScriptType scriptType = ScriptType::Legacy;
};

// Model di Roblox Studio (Ctrl+G): un contenitore di part, annidabile. Le
// part vi appartengono tramite WorkspacePart::groupId; la logica e' in
// PartGroups.h.
struct PartGroup {
    int id = 0;                 // >0, stabile per tutta la vita del place
    std::string name = "Model";
    int parent = 0;             // Model che lo contiene, 0 = Workspace
};

// Vincolo fra due Part. Il nome resta WeldConstraint (API e finestre gia'
// esistenti), ma ora copre i vincoli principali di Roblox; li simula Blocky
// Physics (Jolt), vedi BlockyPhysics::syncJoints.
enum class ConstraintKind : int {
    Weld = 0,    // fisse fra loro
    Hinge,       // ruotano attorno a un asse (porte, pendoli, ruote)
    Rope,        // stanno entro una distanza massima
    Spring,      // tornano alla distanza di riposo (molla)
    Rod,         // distanza fissa (asta rigida)
    Count
};

inline const char* constraintKindName(ConstraintKind k) {
    switch (k) {
    case ConstraintKind::Weld:   return "WeldConstraint";
    case ConstraintKind::Hinge:  return "HingeConstraint";
    case ConstraintKind::Rope:   return "RopeConstraint";
    case ConstraintKind::Spring: return "SpringConstraint";
    case ConstraintKind::Rod:    return "RodConstraint";
    case ConstraintKind::Count:  break;
    }
    return "WeldConstraint";
}

struct WeldConstraint {
    std::string name = "WeldConstraint";
    int part0 = -1;
    int part1 = -1;
    bool enabled = true;
    // Offset di part1 rispetto a part0 (calcolato alla creazione).
    glm::vec3 relPos{0.f};
    glm::vec3 relRot{0.f};

    ConstraintKind kind = ConstraintKind::Weld;
    float length = 0.f;        // Rope/Rod/Spring, stud. 0 = la distanza all'avvio
    float stiffness = 3.f;     // Spring: frequenza in Hz (piu' alta = piu' dura)
    float damping = 0.3f;      // Spring: 0 = oscilla a lungo, 1 = si ferma subito
    glm::vec3 pivot{0.f};      // Hinge: punto della cerniera, rispetto a part0 (suo spazio locale)
    glm::vec3 axis{0, 1, 0};   // Hinge: asse, nello spazio locale di part0
};

// Instance condivisa (ReplicatedStorage / ServerStorage / SSS): Folder,
// ModuleScript, RemoteEvent/Function, Script — hierarchy leggera con parentIndex.
enum class SharedService : int {
    ReplicatedStorage = 0,
    ServerStorage = 1,
    ServerScriptService = 2,
};

struct SharedInstance {
    std::string name = "Instance";
    // Folder | ModuleScript | RemoteEvent | RemoteFunction | Script | LocalScript
    std::string className = "Folder";
    SharedService service = SharedService::ReplicatedStorage;
    int parentIndex = -1; // -1 = radice del service; altrimenti indice in DataModel::instances
    std::string source;   // ModuleScript / Script source
    ScriptType scriptType = ScriptType::ModuleScript;
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
    // UI hierarchy: -1 = ScreenGui root; altrimenti indice label padre (Frame).
    int parentIndex = -1;
    int layoutOrder = 0;
    float scrollY = 0.f; // ScrollingFrame
    // BillboardGui / SurfaceGui: part di ancoraggio (-1 = schermo/ancoraggio 2D).
    int adorneePart = -1;
    float studsOffsetY = 2.5f;
    std::string script;
    ScriptType scriptType = ScriptType::Legacy;

    bool isButton() const {
        return className == "TextButton" || className == "Button"
            || className == "ImageButton";
    }
    bool isFrame() const {
        return className == "Frame" || className == "ScrollingFrame";
    }
    bool isImage() const {
        return className == "ImageLabel" || className == "ImageButton";
    }
    bool isLayout() const {
        return className == "UIListLayout" || className == "UIGridLayout"
            || className == "UIPadding";
    }
    bool isWorldGui() const {
        return className == "BillboardGui" || className == "SurfaceGui";
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

    // Parti del corpo mentre l item e equipaggiato. Vuoto = solo il default
    // del braccio destro (kEquippedArmHoldDeg). Pieno = quelle parti usano
    // questa posa al posto della camminata / salto.
    std::vector<HoldPartPose> holdPose;
};

inline glm::mat4 partRotationMatrix(const glm::vec3& rotDeg) {
    glm::mat4 r(1.f);
    r = glm::rotate(r, glm::radians(rotDeg.y), {0, 1, 0});
    r = glm::rotate(r, glm::radians(rotDeg.x), {1, 0, 0});
    r = glm::rotate(r, glm::radians(rotDeg.z), {0, 0, 1});
    return r;
}

inline glm::vec3 partHitboxSize(const WorkspacePart& p) {
    return glm::max(p.size * glm::vec3(p.hitboxScaleX, p.hitboxScaleY, p.hitboxScaleZ),
                    glm::vec3(0.05f));
}

inline glm::mat4 partModelMatrix(const WorkspacePart& p) {
    return glm::translate(glm::mat4(1.f), p.position)
         * partRotationMatrix(p.rotation)
         * glm::scale(glm::mat4(1.f), p.size);
}

inline glm::mat4 partHitboxMatrix(const WorkspacePart& p) {
    return glm::translate(glm::mat4(1.f), p.position)
         * partRotationMatrix(p.rotation)
         * glm::scale(glm::mat4(1.f), partHitboxSize(p));
}

class DataModel {
public:
    std::string placeName = "Baseplate";
    std::vector<WorkspacePart> parts;
    std::vector<WorkspaceModel> models;
    std::vector<WorkspaceLabel> labels;
    std::vector<WorkspaceFriend> friends;
    std::vector<BackpackItem> backpack;
    std::vector<WeldConstraint> welds;
    std::vector<SharedInstance> instances;
    std::vector<PartGroup> groups;
    int nextGroupId = 1;
    static constexpr int kHotbarSlots = 9;
    int equippedBackpack = -1;
    SpawnLocation spawn;

    void clear() {
        parts.clear();
        models.clear();
        labels.clear();
        friends.clear();
        backpack.clear();
        welds.clear();
        instances.clear();
        groups.clear();
        nextGroupId = 1;
        equippedBackpack = -1;
        placeName = "Baseplate";
        spawn = SpawnLocation{};
    }

    int findSharedChild(SharedService svc, int parentIdx, const std::string& name) const {
        for (int i = 0; i < (int)instances.size(); ++i) {
            const auto& it = instances[i];
            if (it.service == svc && it.parentIndex == parentIdx && it.name == name)
                return i;
        }
        return -1;
    }

    SharedInstance& addShared(SharedService svc, const std::string& className,
                              const std::string& name, int parentIdx = -1) {
        SharedInstance s;
        s.service = svc;
        s.className = className.empty() ? "Folder" : className;
        s.name = name.empty() ? s.className : name;
        s.parentIndex = parentIdx;
        if (s.className == "ModuleScript")
            s.scriptType = ScriptType::ModuleScript;
        else if (s.className == "LocalScript")
            s.scriptType = ScriptType::Client;
        else if (s.className == "Script")
            s.scriptType = ScriptType::Server;
        instances.push_back(std::move(s));
        return instances.back();
    }

    int findPartChild(int parentIdx, const std::string& name) const {
        for (int i = 0; i < (int)parts.size(); ++i) {
            if (parts[i].parentIndex == parentIdx && parts[i].name == name)
                return i;
        }
        return -1;
    }

    WeldConstraint& addWeld(int a, int b) {
        WeldConstraint w;
        w.name = "WeldConstraint" + std::to_string(welds.size() + 1);
        w.part0 = a;
        w.part1 = b;
        if (a >= 0 && a < (int)parts.size() && b >= 0 && b < (int)parts.size()) {
            w.relPos = parts[b].position - parts[a].position;
            w.relRot = parts[b].rotation - parts[a].rotation;
        }
        welds.push_back(w);
        return welds.back();
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

    /** FPS kit: arena + hitscan pistol (CombatService) — no C++ edits needed. */
    void loadFpsKitPlace() {
        loadArenaPlace();
        placeName = "FPS Kit";
        addBackpackItem("Pistola", 4, BackpackTool::Pistol);
        WorkspacePart& gun = addPart("GunScript", {0, 1, 14}, {2, 1, 2}, {0.15f, 0.15f, 0.18f});
        gun.scriptType = ScriptType::Server;
        gun.script =
R"LUA(local Combat = game:GetService("CombatService")
local UIS = game:GetService("UserInputService")
local cd = 0
print("FPS Kit: click sinistro per sparare (hitscan)")

game.RunService.Heartbeat:Connect(function(dt)
    cd = math.max(0, cd - dt)
    if cd > 0 then return end
    if not UIS.IsMouseButtonPressed(Enum.UserInputType.MouseButton1) then return end
    cd = 0.18
    local origin = Humanoid.GetPosition() + Vector3.new(0, 1.6, 0)
    local look = Humanoid.GetLook()
    local hit = workspace:Raycast(origin, look, 120)
    if hit and hit.Instance then
        Combat.Damage(hit.Instance.Name, 25)
        game.Particles:Sparks(hit.Position, 5)
        print("Hit " .. hit.Instance.Name)
    else
        Combat.Shoot("bullet", origin.x, origin.y, origin.z,
            look.x * 80, look.y * 80, look.z * 80, 1.2, 0.2)
    end
    Camera.Shake(1.2, 0.06)
end)
)LUA";
        WorkspacePart& dummy = addPart("TargetDummy", {0, 3, -10}, {3, 6, 3}, {0.9f, 0.25f, 0.25f});
        dummy.canDestroy = true;
        dummy.health = 100.f;
        dummy.maxHealth = 100.f;
        addLabel("FpsHint", "LMB = spara · TargetDummy = bersaglio", 0.5f, 0.14f, {1.f, 0.85f, 0.4f});
    }

    /** Tycoon: pads give Coins via CurrencyService. */
    void loadTycoonKitPlace() {
        clear();
        placeName = "Tycoon Kit";
        addBaseplate({100, 2, 100}, {0.22f, 0.55f, 0.28f});
        spawn.position = {0, 0, 12};
        addPart("Dropper", {0, 4, -8}, {6, 2, 6}, {0.35f, 0.55f, 0.95f});
        WorkspacePart& pad = addPart("CoinPad", {0, 0.6f, 0}, {8, 1.2f, 8}, {1.f, 0.85f, 0.2f});
        pad.scriptType = ScriptType::Server;
        pad.script =
R"LUA(local Currency = game:GetService("CurrencyService")
local Inventory = game:GetService("InventoryService")
local last = -10
print("Tycoon Kit: resta sul pad dorato per guadagnare Coins")

part.Touched:Connect(function()
    local t = 0
    if tick then t = tick() elseif os and os.clock then t = os.clock() end
    if t - last < 0.6 then return end
    last = t
    local n = Currency.Add("Coins", 5)
    Inventory.Add("coin", "Coin", 1)
    game.Particles:Coins(part.Position, 4)
    print("Coins: " .. tostring(n))
end)
)LUA";
        WorkspacePart& shop = addPart("ShopPad", {12, 0.6f, 0}, {6, 1.2f, 6}, {0.55f, 0.35f, 0.95f});
        shop.scriptType = ScriptType::Server;
        shop.script =
R"LUA(local Currency = game:GetService("CurrencyService")
part.Touched:Connect(function()
    if Currency.Spend("Coins", 25) then
        Backpack.Give("Sword")
        print("Comprato Sword per 25 Coins")
    else
        print("Servono 25 Coins")
    end
end)
)LUA";
        addLabel("TycoonTitle", "TYCOON — pad oro = Coins · viola = shop", 0.5f, 0.10f, {1.f, 0.92f, 0.4f});
    }

    /** Survival: spawn hostile NPC + hunger/inventory loop. */
    void loadSurvivalKitPlace() {
        clear();
        placeName = "Survival Kit";
        addBaseplate({120, 2, 120}, {0.28f, 0.48f, 0.30f});
        spawn.position = {0, 0, 10};
        for (int i = 0; i < 5; ++i)
            addPart("Rock" + std::to_string(i),
                {(float)(i * 7 - 14), 1.5f, -6.f}, {3, 3, 3}, {0.45f, 0.45f, 0.48f});
        WorkspacePart& spawner = addPart("MobDirector", {0, 1, 8}, {2, 1, 2}, {0.2f, 0.7f, 0.3f});
        spawner.scriptType = ScriptType::Server;
        spawner.script =
R"LUA(local Npc = game:GetService("NpcService")
local Inventory = game:GetService("InventoryService")
local Quests = game:GetService("QuestService")
Quests.Start("survive1", "Sopravvivi e raccogli drop", 0)
Quests.AddStep("survive1", "loot", "Raccogli un drop")
Inventory.Add("wood", "Wood", 3)
print("Survival Kit: zombie in arrivo — Mobs.Hurt / Pickup")

task.spawn(function()
    task.wait(1)
    Npc.Spawn("zombie", 8, 1, -12)
    Npc.Spawn("pig", -10, 1, -8)
    print("NPC spawnati: " .. tostring(Npc.Count()))
end)

game.RunService.Heartbeat:Connect(function()
    local pos = Humanoid.GetPosition()
    local drop = Npc.Pickup(pos.x, pos.y, pos.z, 2.5)
    if drop then
        Inventory.Add(drop.id, drop.id, drop.count)
        Quests.Advance("survive1", "loot")
        print("Loot: " .. drop.id)
    end
end)
)LUA";
        addLabel("SurvTitle", "SURVIVAL — NPC + inventario + quest", 0.5f, 0.10f, {0.55f, 1.f, 0.55f});
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
