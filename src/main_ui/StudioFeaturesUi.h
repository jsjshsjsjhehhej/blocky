#pragma once

/**
 * Extra feature Roblox Studio–like per Blocky Studio.
 * Incluso solo dal path main.cpp (come StudioUiPanel.h).
 *
 * Snap/grid, view modes, camera bookmarks, Explorer search/filter/context,
 * Insert Object, Material palette, command bar Luau, align/lock/anchor,
 * paint tool, selection multi, status bar.
 */

#include "engine/BrickColor.h"
#include "engine/PartMaterial.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Stato Studio (feature pack)
// ---------------------------------------------------------------------------

// g_studioSnap* / grid / viewIsolate: PartPrefabs.h (early).
static bool  g_showInsertObject = false;
// g_showCommandBar: PartPrefabs.h (early, toolbox chrome).
static bool  g_showMaterialPalette = false;
static bool  g_showSnapSettings = false;
static bool  g_showCameraBookmarks = false;
static bool  g_showBrickColorPicker = false;
static bool  g_showTransformTypeIn = false;
static bool  g_showBatchRename = false;
static bool  g_showFindPart = false;
static bool  g_showTagsWindow = false;
static bool  g_showPlaceSettings = false;
static bool  g_showPhysicsBatch = false;
static bool  g_showScriptTemplates = false;
static bool  g_showPerfOverlay = false;
static bool  g_showHotkeySheet = false;
static bool  g_showSelectionHistory = false;
static bool  g_showCopyPasteProps = false;
static bool  g_showQuickPrefabs = false;
static bool  g_paintToolActive = false;
static int   g_paintBrickIndex = 21; // Bright red default
static char  g_explorerFilterBuf[96] = {};
static bool  g_explorerScriptsOnly = false;
static bool  g_explorerLockedOnly = false;
static bool  g_explorerUnanchoredOnly = false;
static int   g_outputFilterMode = 0; // 0 all · 1 print · 2 warn · 3 error
static char  g_commandBarBuf[512] = {};
static char  g_insertSearchBuf[64] = {};
static std::vector<int> g_multiParts;

struct StudioCamBookmark {
    bool used = false;
    char name[32] = {};
    glm::vec3 pos{0, 12, 24};
    float yaw = 0.f;
    float pitch = -20.f;
};
static StudioCamBookmark g_camBookmarks[8];

struct StudioInsertItem {
    const char* category;
    const char* name;
    const char* hint;
    int kind; // 0 part block, 1 ball, 2 cyl, 3 wedge, 4 neon, 5 light, 6 explode,
              // 7 spawn, 8 label, 9 button, 10 sound, 11 model, 12 script, 13 pistol, 14 sword
};

static const StudioInsertItem kInsertItems[] = {
    {"Parts", "Part", "Block 4×1×2", 0},
    {"Parts", "Ball", "Sfera", 1},
    {"Parts", "Cylinder", "Cilindro", 2},
    {"Parts", "Wedge", "Rampa", 3},
    {"Parts", "Neon Part", "Material Neon + luce", 4},
    {"Parts", "PointLight Part", "Part con PointLight", 5},
    {"Parts", "Explode Part", "Part esplosiva", 6},
    {"Parts", "SpawnLocation", "Punto spawn player", 7},
    {"UI", "TextLabel", "Testo ScreenGui", 8},
    {"UI", "TextButton", "Bottone ScreenGui", 9},
    {"UI", "ScreenGui", "Schermo (come StarterGui)", 32},
    {"UI", "Frame", "Riquadro", 33},
    {"UI", "ScrollingFrame", "Riquadro che scorre", 34},
    {"UI", "TextBox", "Casella di testo", 37},
    {"UI", "ImageLabel", "Immagine", 38},
    {"UI", "ImageButton", "Immagine cliccabile", 39},
    {"UI", "UIListLayout", "Lista verticale", 40},
    {"UI", "UIGridLayout", "Griglia", 41},
    {"UI", "UIPadding", "Margine interno", 42},
    {"UI", "UICorner", "Angoli tondi", 43},
    {"UI", "UIStroke", "Bordo", 44},
    {"UI", "BillboardGui", "Scritta sopra una Part", 45},
    {"UI", "SurfaceGui", "Scritta sulla faccia di una Part", 46},
    {"Audio", "Sound", "Emitter audio", 10},
    {"Models", "Import Model…", "FBX / GLB", 11},
    {"Scripting", "Script on selection", "Aggiunge Script alla Part", 12},
    {"Tools", "Pistol (StarterPack)", "Arma hotbar", 13},
    {"Tools", "Sword (StarterPack)", "Spada hotbar", 14},
    {"Tools", "Tool + Handle", "Tool Roblox con Part Handle", 31},
    {"Gameplay", "Coin Pad", "Pad che da Currency", 15},
    {"Gameplay", "Kill Brick", "Part che fa danno al tocco", 16},
    {"Gameplay", "Speed Pad", "Aumenta WalkSpeed", 17},
    {"Gameplay", "Jump Pad", "Boost JumpPower", 18},
    {"Gameplay", "Teleport Pad", "Teletrasporto", 19},
    {"Gameplay", "Checkpoint", "Salva posizione spawn", 20},
    {"Lighting", "Spotlight Part", "Luce conica (approx PointLight)", 21},
    {"Decor", "Truss-like Pillar", "Pilastro alto", 22},
    {"Decor", "Glass Pane", "Vetro trasparente", 23},
    {"Decor", "ForceField Cube", "Cubo ForceField", 24},
    {"Physics", "Bounce Pad", "Elasticity alta + rimbalzo player", 25},
    {"Physics", "Ice Floor", "Friction 0 — scivola", 26},
    {"Physics", "Push Crate", "Cassa dinamica da spingere", 27},
    {"Physics", "Heavy Block", "Density alta, difficile da muovere", 28},
    {"Physics", "Ball (physics)", "Palla elastica non ancorata", 29},
    {"Physics", "Launch Pad", "Touched → ApplyImpulse su player (script)", 30},
};

static constexpr int kInsertItemCount = (int)(sizeof(kInsertItems) / sizeof(kInsertItems[0]));

// ---------------------------------------------------------------------------
// Helpers selezione / lock
// ---------------------------------------------------------------------------

static bool partIndexValid(int i) {
    return i >= 0 && i < (int)g_world.parts.size();
}

static bool partIsLocked(int i) {
    return partIndexValid(i) && g_world.parts[i].locked;
}

static bool selectedPartLocked() {
    return partIsLocked(g_selectedPart);
}

static void studioMultiClear() { g_multiParts.clear(); }

static void studioMultiAdd(int i) {
    if (!partIndexValid(i)) return;
    for (int x : g_multiParts) if (x == i) return;
    g_multiParts.push_back(i);
}

static void studioMultiToggle(int i) {
    if (!partIndexValid(i)) return;
    for (size_t k = 0; k < g_multiParts.size(); ++k) {
        if (g_multiParts[k] == i) {
            g_multiParts.erase(g_multiParts.begin() + (std::ptrdiff_t)k);
            return;
        }
    }
    g_multiParts.push_back(i);
}

static bool studioMultiContains(int i) {
    for (int x : g_multiParts) if (x == i) return true;
    return false;
}

static std::vector<int> studioActivePartIndices() {
    std::vector<int> out;
    if (!g_multiParts.empty()) {
        for (int i : g_multiParts)
            if (partIndexValid(i)) out.push_back(i);
        return out;
    }
    if (partIndexValid(g_selectedPart)) out.push_back(g_selectedPart);
    return out;
}

// ---------------------------------------------------------------------------
// Operazioni batch (stile Studio)
// ---------------------------------------------------------------------------

static void studioFocusSelection() {
    glm::vec3 target{0, 4, 0};
    if (partIndexValid(g_selectedPart))
        target = g_world.parts[g_selectedPart].position;
    else if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size())
        target = g_world.models[g_selectedModel].position;
    else if (g_selectedSpawn)
        target = g_world.spawn.position;
    g_cam.pos = target - g_cam.front() * 18.f + glm::vec3(0, 6, 0);
    g_exportMsg = "Camera → selezione";
}

static void studioIsolateToggle() {
    g_viewIsolate = !g_viewIsolate;
    g_isolatePart = g_viewIsolate ? g_selectedPart : -1;
    g_exportMsg = g_viewIsolate ? "Isolate ON" : "Isolate OFF";
}

static void studioSetLockedOnSelection(bool locked) {
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i)) continue;
        g_world.parts[i].locked = locked;
    }
    markPlaceDirty();
    g_exportMsg = locked ? "Locked" : "Unlocked";
}

static void studioSetAnchoredOnSelection(bool anchored) {
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].anchored = anchored;
    }
    markPlaceDirty();
}

static void studioSetCanCollideOnSelection(bool v) {
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].canCollide = v;
    }
    markPlaceDirty();
}

static void studioApplyColorToSelection(const glm::vec3& c) {
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].color = c;
    }
    markPlaceDirty();
}

static void studioApplyMaterialToSelection(PartMaterial m) {
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].setMaterial(m);
    }
    markPlaceDirty();
}

static void studioApplyTransparencyToSelection(float t) {
    t = std::clamp(t, 0.f, 1.f);
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].transparency = t;
    }
    markPlaceDirty();
}

static void studioSelectAllParts() {
    studioMultiClear();
    for (int i = 0; i < (int)g_world.parts.size(); ++i)
        g_multiParts.push_back(i);
    if (!g_multiParts.empty()) selectPart(g_multiParts.back());
    g_exportMsg = std::to_string(g_multiParts.size()) + " parti selezionate";
}

static size_t studioMultiSelectionSize() { return g_multiParts.size(); }

static void studioDeleteUnlockedSelection() {
    auto idxs = studioActivePartIndices();
    std::sort(idxs.begin(), idxs.end(), std::greater<int>());
    // Dal piu' alto al piu' basso: un unico Ctrl+Z le rimette tutte, ognuna
    // al suo posto.
    g_undo.beginBatch();
    for (int i : idxs) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_undo.recordDelete(i, g_world.parts[i]);
        g_world.parts.erase(g_world.parts.begin() + i);
        partconstraints::onPartErased(g_world, i);
    }
    g_undo.endBatch();
    studioMultiClear();
    selectPart(-1);
    markPlaceDirty();
}

static void studioDuplicateSelectionOffset() {
    auto idxs = studioActivePartIndices();
    if (idxs.empty()) { duplicateSelected(); return; }
    std::vector<int> neu;
    for (int i : idxs) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        WorkspacePart p = g_world.parts[i];
        p.netId = 0;
        p.position.x += g_studioSnapMove > 0.f ? g_studioSnapMove * 4.f : 1.f;
        p.name = p.name + "_copy";
        g_world.parts.push_back(p);
        neu.push_back((int)g_world.parts.size() - 1);
    }
    studioMultiClear();
    for (int i : neu) studioMultiAdd(i);
    if (!neu.empty()) selectPart(neu.back());
    markPlaceDirty();
    g_exportMsg = "Duplicate +" + std::to_string(neu.size());
}

enum class StudioAlignMode { Min, Center, Max };

static void studioTagSelection(int kind) {
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        auto& p = g_world.parts[i];
        if (kind == 0) {
            p.decalEnabled = true;
            p.decalFace = 5.f;
            p.decalR = 0.90f; p.decalG = 0.16f; p.decalB = 0.12f;
            p.decalTransparency = 0.f;
        } else if (kind == 1) {
            p.highlightEnabled = true;
            p.highlightR = 1.f; p.highlightG = 0.82f; p.highlightB = 0.15f;
        } else if (kind == 2) {
            p.selectionBox = true;
        } else if (kind == 3) {
            p.dragDetector = true;
        }
    }
    markPlaceDirty();
}

static void studioAlignSelection(int axis, StudioAlignMode mode) {
    auto idxs = studioActivePartIndices();
    if (idxs.size() < 2) {
        g_exportMsg = "Align: seleziona 2+ parti (Ctrl+click)";
        return;
    }
    float ref = 0.f;
    bool first = true;
    for (int i : idxs) {
        if (!partIndexValid(i)) continue;
        float v = g_world.parts[i].position[axis];
        if (first) { ref = v; first = false; continue; }
        if (mode == StudioAlignMode::Min) ref = std::min(ref, v);
        else if (mode == StudioAlignMode::Max) ref = std::max(ref, v);
        else ref += v;
    }
    if (mode == StudioAlignMode::Center && !idxs.empty())
        ref /= (float)idxs.size();
    for (int i : idxs) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].position[axis] = ref;
    }
    markPlaceDirty();
    g_exportMsg = "Align axis";
}

static void studioDistributeSelection(int axis) {
    auto idxs = studioActivePartIndices();
    if (idxs.size() < 3) {
        g_exportMsg = "Distribute: serve 3+ parti";
        return;
    }
    std::sort(idxs.begin(), idxs.end(), [&](int a, int b) {
        return g_world.parts[a].position[axis] < g_world.parts[b].position[axis];
    });
    float a0 = g_world.parts[idxs.front()].position[axis];
    float a1 = g_world.parts[idxs.back()].position[axis];
    for (size_t k = 0; k < idxs.size(); ++k) {
        int i = idxs[k];
        if (g_world.parts[i].locked) continue;
        float t = (idxs.size() == 1) ? 0.f : (float)k / (float)(idxs.size() - 1);
        g_world.parts[i].position[axis] = a0 + (a1 - a0) * t;
    }
    markPlaceDirty();
}

static void studioSaveCameraBookmark(int slot) {
    if (slot < 0 || slot >= 8) return;
    auto& b = g_camBookmarks[slot];
    b.used = true;
    b.pos = g_cam.pos;
    b.yaw = g_cam.yaw;
    b.pitch = g_cam.pitch;
    std::snprintf(b.name, sizeof(b.name), "Cam %d", slot + 1);
    g_exportMsg = std::string("Bookmark salvato: ") + b.name;
}

static void studioLoadCameraBookmark(int slot) {
    if (slot < 0 || slot >= 8 || !g_camBookmarks[slot].used) return;
    const auto& b = g_camBookmarks[slot];
    g_cam.pos = b.pos;
    g_cam.yaw = b.yaw;
    g_cam.pitch = b.pitch;
    g_exportMsg = std::string("Bookmark: ") + b.name;
}

static void studioPaintAtSelection() {
    int n = 0;
    const auto* pal = brickcolor::palette(&n);
    int idx = g_paintBrickIndex;
    const brickcolor::Entry* e = nullptr;
    for (int i = 0; i < n; ++i)
        if (pal[i].number == idx) { e = &pal[i]; break; }
    if (!e && n > 0) e = &pal[0];
    if (!e) return;
    studioApplyColorToSelection(brickcolor::toLinear(*e));
}

// ---------------------------------------------------------------------------
// Insert Object actions
// ---------------------------------------------------------------------------

static void insertRobloxWidget(const char* className);   // Studio2020Ui.h

static void studioRunInsertKind(int kind) {
    switch (kind) {
    case 0: insertNewPart(); break;
    case 1: insertShapedPart(PartShape::Ball); break;
    case 2: insertShapedPart(PartShape::Cylinder); break;
    case 3: insertShapedPart(PartShape::Wedge); break;
    case 4: insertNeonPart(); break;
    case 5: insertPointLightPart(); break;
    case 6: insertExplodePart(); break;
    case 7: {
        g_world.spawn.position = g_cam.pos + g_cam.front() * 10.f;
        g_world.spawn.position.y = std::max(0.f, g_world.spawn.position.y);
        selectSpawn();
        markPlaceDirty();
        break;
    }
    case 8: insertNewTextLabel(); break;
    case 9: insertNewTextButton(); break;
    case 10: insertSound(); break;
    case 11: insertImportedModel(); break;
    case 12: addScriptToSelected(); break;
    case 13: g_world.addBackpackItem("Pistola", 4, BackpackTool::Pistol); break;
    case 14: g_world.addBackpackItem("Sword", 3, BackpackTool::Sword); break;
    case 15: {
        WorkspacePart& p = g_world.addPart("CoinPad",
            g_cam.pos + g_cam.front() * 8.f, {6, 1, 6}, {1.f, 0.85f, 0.2f});
        p.scriptType = ScriptType::Server;
        p.script =
R"LUA(local Currency = game:GetService("CurrencyService")
part.Touched:Connect(function()
    Currency.Add("Coins", 1)
    game.Particles:Coins(part.Position, 3)
end)
)LUA";
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 16: {
        WorkspacePart& p = g_world.addPart("KillBrick",
            g_cam.pos + g_cam.front() * 8.f, {6, 1, 6}, {0.85f, 0.12f, 0.12f});
        p.scriptType = ScriptType::Server;
        p.script =
R"LUA(part.Touched:Connect(function()
    Humanoid.TakeDamage(25)
end)
)LUA";
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 17: {
        WorkspacePart& p = g_world.addPart("SpeedPad",
            g_cam.pos + g_cam.front() * 8.f, {6, 1, 6}, {0.2f, 0.75f, 1.f});
        p.scriptType = ScriptType::Server;
        p.script =
R"LUA(part.Touched:Connect(function()
    Humanoid.SetWalkSpeed(28)
    task.delay(3, function() Humanoid.SetWalkSpeed(16) end)
end)
)LUA";
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 18: {
        WorkspacePart& p = g_world.addPart("JumpPad",
            g_cam.pos + g_cam.front() * 8.f, {6, 1, 6}, {0.35f, 1.f, 0.35f});
        p.scriptType = ScriptType::Server;
        p.script =
R"LUA(part.Touched:Connect(function()
    Humanoid.SetJumpPower(90)
    task.delay(2, function() Humanoid.SetJumpPower(50) end)
end)
)LUA";
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 19: {
        WorkspacePart& p = g_world.addPart("TeleportPad",
            g_cam.pos + g_cam.front() * 8.f, {4, 1, 4}, {0.7f, 0.3f, 1.f});
        p.scriptType = ScriptType::Server;
        p.script =
R"LUA(part.Touched:Connect(function()
    local dest = part:GetPosition() + Vector3.new(0, 20, 0)
    Humanoid.SetPosition(dest.x, dest.y, dest.z)
end)
)LUA";
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 20: {
        WorkspacePart& p = g_world.addPart("Checkpoint",
            g_cam.pos + g_cam.front() * 8.f, {4, 1, 4}, {0.2f, 0.9f, 0.5f});
        p.scriptType = ScriptType::Server;
        p.script =
R"LUA(part.Touched:Connect(function()
    local pos = part:GetPosition()
    print("Checkpoint @", pos.x, pos.y, pos.z)
end)
)LUA";
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 21: insertPointLightPart(); break;
    case 22: {
        WorkspacePart& p = g_world.addPart("Pillar",
            g_cam.pos + g_cam.front() * 8.f, {2, 16, 2}, {0.7f, 0.7f, 0.72f});
        p.material = PartMaterial::Concrete;
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 23: {
        WorkspacePart& p = g_world.addPart("GlassPane",
            g_cam.pos + g_cam.front() * 8.f, {8, 6, 0.4f}, {0.7f, 0.85f, 1.f});
        p.setMaterial(PartMaterial::Glass);
        p.transparency = 0.45f;
        p.useStud = false;
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 24: {
        WorkspacePart& p = g_world.addPart("ForceField",
            g_cam.pos + g_cam.front() * 8.f, {5, 5, 5}, {0.4f, 0.9f, 1.f});
        p.setMaterial(PartMaterial::ForceField);
        p.transparency = 0.35f;
        p.canCollide = false;
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 25: {
        WorkspacePart& p = g_world.addPart("BouncePad",
            g_cam.pos + g_cam.front() * 8.f, {8, 1, 8}, {0.2f, 1.f, 0.45f});
        p.anchored = true;
        p.elasticity = 1.f;
        p.friction = 0.3f;
        p.setMaterial(PartMaterial::Neon);
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 26: {
        WorkspacePart& p = g_world.addPart("IceFloor",
            g_cam.pos + g_cam.front() * 8.f, {16, 1, 16}, {0.7f, 0.9f, 1.f});
        p.anchored = true;
        p.friction = 0.02f;
        p.elasticity = 0.05f;
        p.setMaterial(PartMaterial::Ice);
        p.transparency = 0.15f;
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 27: {
        WorkspacePart& p = g_world.addPart("Crate",
            g_cam.pos + g_cam.front() * 8.f, {4, 4, 4}, {0.72f, 0.48f, 0.22f});
        p.anchored = false;
        p.density = 0.6f;
        p.friction = 0.55f;
        p.elasticity = 0.15f;
        p.linearDamping = 0.08f;
        p.angularDamping = 0.12f;
        p.setMaterial(PartMaterial::Wood);
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 28: {
        WorkspacePart& p = g_world.addPart("HeavyBlock",
            g_cam.pos + g_cam.front() * 8.f, {5, 5, 5}, {0.35f, 0.35f, 0.4f});
        p.anchored = false;
        p.density = 8.f;
        p.friction = 0.9f;
        p.elasticity = 0.02f;
        p.setMaterial(PartMaterial::Metal);
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 29: {
        WorkspacePart& p = g_world.addPart("PhysBall",
            g_cam.pos + g_cam.front() * 8.f, {3, 3, 3}, {1.f, 0.35f, 0.2f});
        p.anchored = false;
        p.shape = PartShape::Ball;
        p.density = 0.5f;
        p.elasticity = 0.85f;
        p.friction = 0.25f;
        p.linearDamping = 0.02f;
        p.angularDamping = 0.02f;
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 30: {
        WorkspacePart& p = g_world.addPart("LaunchPad",
            g_cam.pos + g_cam.front() * 8.f, {6, 1, 6}, {1.f, 0.55f, 0.1f});
        p.anchored = true;
        p.elasticity = 0.2f;
        p.scriptType = ScriptType::Server;
        p.script =
R"LUA(part.Touched:Connect(function()
    Humanoid.SetJumpPower(120)
    task.delay(0.4, function() Humanoid.SetJumpPower(50) end)
end)
)LUA";
        selectPart((int)g_world.parts.size() - 1);
        markPlaceDirty();
        break;
    }
    case 31: {
        // Tool di Studio: entra nella hotbar e il Handle (una Part) si
        // disegna in mano quando lo equipaggi. Le altre Part parentate
        // restano attaccate al Handle.
        BackpackItem& tool = g_world.addBackpackItem("Tool", 1, BackpackTool::None);
        tool.requiresHandle = true;
        tool.toolTip = "Tool";
        WorkspacePart& handle = g_world.addPart("Handle", {0, 0, 0}, {1.f, 4.f, 1.f},
                                                {0.75f, 0.78f, 0.82f});
        handle.material = PartMaterial::Metal;
        g_world.attachPartToTool((int)g_world.parts.size() - 1, (int)g_world.backpack.size() - 1);
        g_selectedBackpack = (int)g_world.backpack.size() - 1;
        g_gripPreviewSlot = g_selectedBackpack;
        markPlaceDirty();
        break;
    }
    case 32: insertRobloxWidget("ScreenGui"); break;
    case 33: insertRobloxWidget("Frame"); break;
    case 34: insertRobloxWidget("ScrollingFrame"); break;
    case 35: insertRobloxWidget("TextLabel"); break;
    case 36: insertRobloxWidget("TextButton"); break;
    case 37: insertRobloxWidget("TextBox"); break;
    case 38: insertRobloxWidget("ImageLabel"); break;
    case 39: insertRobloxWidget("ImageButton"); break;
    case 40: insertRobloxWidget("UIListLayout"); break;
    case 41: insertRobloxWidget("UIGridLayout"); break;
    case 42: insertRobloxWidget("UIPadding"); break;
    case 43: insertRobloxWidget("UICorner"); break;
    case 44: insertRobloxWidget("UIStroke"); break;
    case 45: insertRobloxWidget("BillboardGui"); break;
    case 46: insertRobloxWidget("SurfaceGui"); break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// UI: Material palette
// ---------------------------------------------------------------------------

static void drawMaterialPaletteBody(PartMaterial* applyTo = nullptr) {
    ImGui::TextDisabled("Materiali Part (come Studio)");
    const int cols = 3;
    int shown = 0;
    for (int mi = 0; mi < (int)PartMaterial::Count; ++mi) {
        if (shown % cols != 0) ImGui::SameLine();
        ImGui::PushID(mi);
        const char* nm = partMaterialName((PartMaterial)mi);
        bool sel = applyTo && *applyTo == (PartMaterial)mi;
        if (ImGui::Selectable(nm, sel, 0, {110, 22})) {
            if (applyTo) {
                *applyTo = (PartMaterial)mi;
            } else {
                studioApplyMaterialToSelection((PartMaterial)mi);
            }
            markPlaceDirty();
        }
        ImGui::PopID();
        ++shown;
    }
}

// ---------------------------------------------------------------------------
// UI: Insert Object
// ---------------------------------------------------------------------------

static void drawInsertObjectWindowPro();   // StudioToolsPro.h
static void drawInsertObjectPopup() { drawInsertObjectWindowPro(); }

// ---------------------------------------------------------------------------
// UI: Command bar (eval Luau)
// ---------------------------------------------------------------------------

static void drawStudioCommandBar() {
    if (!g_showCommandBar || g_studioTesting) return;
    const float h = 28.f;
    ImGui::SetNextWindowPos({0.f, (float)g_h - h}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({(float)g_w, h}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6.f, 4.f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.12f, 0.12f, 0.14f, 0.98f});
    ImGui::Begin("##CmdBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
    ImGui::TextDisabled(">");
    ImGui::SameLine();
    ImGui::SetNextItemWidth((float)g_w - 120.f);
    bool run = ImGui::InputText("##cmd", g_commandBarBuf, sizeof(g_commandBarBuf),
                                ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("Run") || run) {
        if (g_commandBarBuf[0]) {
            std::string code = g_commandBarBuf;
            // Wrap expression prints for convenience: `= expr` → print(expr)
            if (code[0] == '=')
                code = "print(" + code.substr(1) + ")";
            g_lua.printLog.push_back(std::string("> ") + g_commandBarBuf);
            if (!g_lua.runString(code)) {
                if (!g_lua.lastError.empty())
                    g_lua.printLog.push_back(g_lua.lastError);
            }
            g_commandBarBuf[0] = 0;
            g_showLuaOutput = true;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// UI: Explorer toolbar + context + filtered rows
// ---------------------------------------------------------------------------

static bool studioExplorerPartVisible(int i) {
    if (!partIndexValid(i)) return false;
    const auto& p = g_world.parts[i];
    if (g_explorerScriptsOnly && p.script.empty()) return false;
    if (g_explorerLockedOnly && !p.locked) return false;
    if (g_explorerUnanchoredOnly && p.anchored) return false;
    if (g_explorerFilterBuf[0]) {
        std::string hay = p.name + " " + partShapeName(p.shape) + " " + partMaterialName(p.material);
        for (char& c : hay) c = (char)std::tolower((unsigned char)c);
        std::string q = g_explorerFilterBuf;
        for (char& c : q) c = (char)std::tolower((unsigned char)c);
        if (hay.find(q) == std::string::npos) return false;
    }
    return true;
}

static void drawExplorerToolbar() {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##exfilter", "Cerca in Explorer…",
                             g_explorerFilterBuf, sizeof(g_explorerFilterBuf));
    ImGui::Checkbox("Script", &g_explorerScriptsOnly);
    ImGui::SameLine();
    ImGui::Checkbox("Locked", &g_explorerLockedOnly);
    ImGui::SameLine();
    ImGui::Checkbox("!Anchor", &g_explorerUnanchoredOnly);
}

static void drawExplorerPartContextMenu(int i) {
    if (!ImGui::BeginPopupContextItem()) return;
    if (ImGui::MenuItem("Focus (F)")) { selectPart(i); studioFocusSelection(); }
    if (ImGui::MenuItem("Rename")) { selectPart(i); g_showProps = true; }
    if (ImGui::MenuItem("Duplicate")) { selectPart(i); studioDuplicateSelectionOffset(); }
    if (ImGui::MenuItem(g_world.parts[i].locked ? "Unlock" : "Lock")) {
        g_world.parts[i].locked = !g_world.parts[i].locked;
        markPlaceDirty();
    }
    if (ImGui::MenuItem(g_world.parts[i].anchored ? "Unanchor" : "Anchor")) {
        if (!g_world.parts[i].locked) {
            g_world.parts[i].anchored = !g_world.parts[i].anchored;
            markPlaceDirty();
        }
    }
    if (ImGui::BeginMenu("Material")) {
        for (int mi = 0; mi < (int)PartMaterial::Count; ++mi) {
            if (ImGui::MenuItem(partMaterialName((PartMaterial)mi))) {
                if (!g_world.parts[i].locked) {
                    g_world.parts[i].setMaterial((PartMaterial)mi);
                    markPlaceDirty();
                }
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("BrickColor quick")) {
        static const char* quick[] = {
            "White", "Black", "Bright red", "Bright blue", "Bright yellow",
            "Bright green", "Bright orange", "Hot pink", "Really black", "Institutional white"
        };
        for (const char* nm : quick) {
            if (ImGui::MenuItem(nm)) {
                if (const auto* e = brickcolor::findByName(nm)) {
                    if (!g_world.parts[i].locked) {
                        g_world.parts[i].color = brickcolor::toLinear(*e);
                        markPlaceDirty();
                    }
                }
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Add Script")) {
        selectPart(i);
        addScriptToSelected();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete", "Del", false, !g_world.parts[i].locked)) {
        selectPart(i);
        if (!selectedPartLocked()) deleteSelected();
    }
    ImGui::EndPopup();
}

static bool drawExplorerPartRow(int i) {
    if (!studioExplorerPartVisible(i)) return false;
    auto& p = g_world.parts[i];
    bool sel = (i == g_selectedPart) && !g_editingPartScript;
    if (studioMultiContains(i)) sel = true;
    std::string label;
    if (p.locked) label += "[L] ";
    label += p.name;
    if (p.shape != PartShape::Block)
        label += std::string(" [") + partShapeName(p.shape) + "]";
    if (p.transparency > 0.01f) label += " [T]";
    if (p.isNeon()) label += " [Neon]";
    else if (p.pointLight) label += " [Light]";
    else if (p.material != PartMaterial::Plastic)
        label += std::string(" [") + partMaterialName(p.material) + "]";
    label += "##part" + std::to_string(i);

    if (ImGui::Selectable(label.c_str(), sel)) {
        if (ImGui::GetIO().KeyCtrl) {
            studioMultiToggle(i);
            selectPart(i);
        } else {
            studioMultiClear();
            selectPart(i);
            g_editingPartScript = false;
        }
    }
    // Trascinabile su un Model nell'Explorer (StudioGroupsUi.h).
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("BLOCKY_PART", &i, sizeof(int));
        ImGui::Text("%s%s", p.name.c_str(),
                    studioMultiContains(i) && g_multiParts.size() > 1 ? " (+ selezione)" : "");
        ImGui::EndDragDropSource();
    }
    drawExplorerPartContextMenu(i);
    if (!p.script.empty()) {
        ImGui::Indent();
        bool scriptSel = (i == g_selectedPart) && g_editingPartScript;
        std::string slabel = std::string("Script##sc") + std::to_string(i);
        if (ImGui::Selectable(slabel.c_str(), scriptSel)) {
            selectPart(i);
            g_editingPartScript = true;
            g_showProps = true;
        }
        ImGui::Unindent();
    }
    return true;
}

// ---------------------------------------------------------------------------
// UI: Snap / View / Bookmarks / Align ribbon extras
// ---------------------------------------------------------------------------

static void drawSnapSettingsWindow() {
    if (!g_showSnapSettings) return;
    ImGui::SetNextWindowSize({320.f, 220.f}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Snap / Grid", &g_showSnapSettings)) {
        ImGui::Checkbox("Snap enabled", &g_studioSnapEnabled);
        ImGui::SliderFloat("Move snap", &g_studioSnapMove, kMinPartSize, 4.f, "%.3f");
        ImGui::SliderFloat("Rotate snap °", &g_studioSnapRotate, 0.1f, 45.f, "%.1f");
        ImGui::SliderFloat("Scale snap", &g_studioSnapScale, kMinPartSize, 2.f, "%.3f");
        ImGui::Separator();
        ImGui::Checkbox("Show grid", &g_studioGridVisible);
        ImGui::SliderFloat("Grid size", &g_studioGridSize, 0.05f, 16.f, "%.2f");
        ImGui::Checkbox("Spazio locale del gizmo (Ctrl+L)", &g_studioLocalSpace);
        ImGui::TextDisabled("Ctrl = no snap · Shift = passo / 10");
    }
    ImGui::End();
}

static void drawCameraBookmarksWindow() {
    if (!g_showCameraBookmarks) return;
    ImGui::SetNextWindowSize({300.f, 260.f}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Camera Bookmarks", &g_showCameraBookmarks)) {
        ImGui::TextWrapped("Salva / ripristina punti di vista (come Studio).");
        for (int i = 0; i < 8; ++i) {
            ImGui::PushID(i);
            auto& b = g_camBookmarks[i];
            if (b.used) {
                ImGui::InputText("##nm", b.name, sizeof(b.name));
                ImGui::SameLine();
                if (ImGui::Button("Go")) studioLoadCameraBookmark(i);
                ImGui::SameLine();
                if (ImGui::Button("Set")) studioSaveCameraBookmark(i);
            } else {
                ImGui::TextDisabled("Slot %d vuoto", i + 1);
                ImGui::SameLine();
                if (ImGui::Button("Save here")) studioSaveCameraBookmark(i);
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
}

static void drawMaterialPaletteWindow() {
    if (!g_showMaterialPalette) return;
    ImGui::SetNextWindowSize({360.f, 300.f}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Material Palette", &g_showMaterialPalette)) {
        ImGui::TextWrapped("Applica il materiale alle Part selezionate.");
        drawMaterialPaletteBody(nullptr);
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// UI: View menu extras + Model menu
// ---------------------------------------------------------------------------

static void studioZoomExtents();
static void studioGroupSelectionAsModel();

static void drawStudioFeaturesViewMenu() {
    ImGui::Separator();
    ImGui::TextDisabled("Studio tools");
    ImGui::MenuItem("Insert Object…", "Ctrl+I", &g_showInsertObject);
    ImGui::MenuItem("Command Bar", nullptr, &g_showCommandBar);
    ImGui::MenuItem("Snap / Grid…", nullptr, &g_showSnapSettings);
    ImGui::MenuItem("Camera Bookmarks…", nullptr, &g_showCameraBookmarks);
    ImGui::MenuItem("Material Palette…", nullptr, &g_showMaterialPalette);
    ImGui::MenuItem("BrickColor Palette…", nullptr, &g_showBrickColorPicker);
    ImGui::MenuItem("Transform Type-In…", nullptr, &g_showTransformTypeIn);
    ImGui::MenuItem("Batch Rename…", nullptr, &g_showBatchRename);
    ImGui::MenuItem("Find Part…", nullptr, &g_showFindPart);
    ImGui::MenuItem("Tags (CollectionService)…", nullptr, &g_showTagsWindow);
    ImGui::MenuItem("Place Settings…", nullptr, &g_showPlaceSettings);
    ImGui::MenuItem("Physics Properties…", nullptr, &g_showPhysicsBatch);
    ImGui::MenuItem("Script Templates…", nullptr, &g_showScriptTemplates);
    ImGui::MenuItem("Copy/Paste Properties…", nullptr, &g_showCopyPasteProps);
    ImGui::MenuItem("Selection History…", nullptr, &g_showSelectionHistory);
    ImGui::MenuItem("Perf overlay", nullptr, &g_showPerfOverlay);
    ImGui::MenuItem("Hotkey cheat sheet…", nullptr, &g_showHotkeySheet);
    ImGui::MenuItem("Quick Prefabs…", nullptr, &g_showQuickPrefabs);
    ImGui::Separator();
    ImGui::MenuItem("Wireframe view", nullptr, &g_viewWireframe);
    ImGui::MenuItem("Collision view", nullptr, &g_viewCollision);
    ImGui::MenuItem("Isolate selection", nullptr, &g_viewIsolate);
    if (ImGui::MenuItem("Focus on selection", "F"))
        studioFocusSelection();
    if (ImGui::MenuItem("Zoom extents"))
        studioZoomExtents();
    ImGui::Separator();
    if (ImGui::MenuItem("Select all Parts", "Ctrl+A"))
        studioSelectAllParts();
    if (ImGui::MenuItem("Duplicate (+offset)", "Ctrl+D"))
        studioDuplicateSelectionOffset();
    if (ImGui::MenuItem("Lock selection"))
        studioSetLockedOnSelection(true);
    if (ImGui::MenuItem("Unlock selection"))
        studioSetLockedOnSelection(false);
    if (ImGui::MenuItem("Anchor selection"))
        studioSetAnchoredOnSelection(true);
    if (ImGui::MenuItem("Unanchor selection"))
        studioSetAnchoredOnSelection(false);
    if (ImGui::MenuItem("Group selection as Model"))
        studioGroupSelectionAsModel();
    // Studio 2020 menu items drawn from Studio2020Ui.h (included after this file).
}

static void drawStudioFeaturesEditMenu() {
    if (ImGui::BeginMenu("Align / Distribute")) {
        if (ImGui::MenuItem("Align X Center")) studioAlignSelection(0, StudioAlignMode::Center);
        if (ImGui::MenuItem("Align Y Center")) studioAlignSelection(1, StudioAlignMode::Center);
        if (ImGui::MenuItem("Align Z Center")) studioAlignSelection(2, StudioAlignMode::Center);
        ImGui::Separator();
        if (ImGui::MenuItem("Distribute X")) studioDistributeSelection(0);
        if (ImGui::MenuItem("Distribute Y")) studioDistributeSelection(1);
        if (ImGui::MenuItem("Distribute Z")) studioDistributeSelection(2);
        ImGui::EndMenu();
    }
}

// ---------------------------------------------------------------------------
// Output filter helper
// ---------------------------------------------------------------------------

static bool studioOutputLineVisible(const std::string& line) {
    const bool isErr = line.find("error") != std::string::npos
                    || line.find("Error") != std::string::npos;
    const bool isWarn = line.find("warn") != std::string::npos
                     || line.find("Warn") != std::string::npos;
    if (g_outputFilterMode == 1) return !isErr && !isWarn;
    if (g_outputFilterMode == 2) return isWarn;
    if (g_outputFilterMode == 3) return isErr;
    return true;
}

static void drawOutputFilterButtons() {
    ImGui::SameLine();
    ImGui::RadioButton("All", &g_outputFilterMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Print", &g_outputFilterMode, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Warn", &g_outputFilterMode, 2);
    ImGui::SameLine();
    ImGui::RadioButton("Error", &g_outputFilterMode, 3);
}

// ---------------------------------------------------------------------------
// Status bar (selection info)
// ---------------------------------------------------------------------------

static void drawStudioStatusBar() {
    if (g_studioTesting) return;
    const float h = 22.f;
    const float y = (float)g_h - (g_showCommandBar ? 28.f : 0.f) - h;
    ImGui::SetNextWindowPos({0.f, y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({(float)g_w, h}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.f, 2.f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.16f, 0.16f, 0.18f, 1.f});
    ImGui::Begin("##StatusBar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);
    char buf[256];
    if (partIndexValid(g_selectedPart)) {
        const auto& p = g_world.parts[g_selectedPart];
        char multi[32] = {};
        if (g_multiParts.size() > 1)
            std::snprintf(multi, sizeof(multi), " · +%d multi", (int)g_multiParts.size() - 1);
        std::snprintf(buf, sizeof(buf),
            "Part \"%s\" · %s · %s · pos (%.1f, %.1f, %.1f) · size (%.1f, %.1f, %.1f)%s%s",
            p.name.c_str(), partShapeName(p.shape), partMaterialName(p.material),
            p.position.x, p.position.y, p.position.z,
            p.size.x, p.size.y, p.size.z,
            p.locked ? " · LOCKED" : "", multi);
    } else if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size()) {
        std::snprintf(buf, sizeof(buf), "Model \"%s\"", g_world.models[g_selectedModel].name.c_str());
    } else if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size()) {
        const auto& l = g_world.labels[(size_t)g_selectedLabel];
        std::snprintf(buf, sizeof(buf),
            "%s \"%s\" · pos %.0f,%.0f · size %.0fx%.0f · Z %d%s",
            l.className.c_str(), l.name.c_str(), l.offsetX, l.offsetY, l.sizeW, l.sizeH, l.zIndex,
            l.uiAttached ? " · acceso" : "");
    } else {
        std::snprintf(buf, sizeof(buf), "%d parts · snap %s · grid %.0f · %s",
            (int)g_world.parts.size(),
            g_studioSnapEnabled ? "ON" : "OFF",
            g_studioGridSize,
            g_paintToolActive ? "PAINT" : "edit");
    }
    ImGui::TextUnformatted(buf);
    ImGui::SameLine();
    const float destra = ImGui::GetWindowWidth() - 8.f;
    char meta[96];
    std::snprintf(meta, sizeof(meta), "%d parti", (int)g_world.parts.size());
    const float metaW = ImGui::CalcTextSize(meta).x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), destra - metaW - 210.f));
    if (ImGui::SmallButton(g_studioSnapEnabled ? "Snap" : "Snap off"))
        g_studioSnapEnabled = !g_studioSnapEnabled;
    ImGui::SameLine();
    char griglia[32];
    if (g_studioGridVisible)
        std::snprintf(griglia, sizeof(griglia), "Griglia %.0f", g_studioGridSize);
    else
        std::snprintf(griglia, sizeof(griglia), "Griglia off");
    if (ImGui::SmallButton(griglia)) {
        g_studioGridVisible = !g_studioGridVisible;
        if (g_studioGridVisible && g_studioGridSize < 0.5f) g_studioGridSize = 4.f;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%.0f fps", g_fps);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// Hotkeys feature pack
// ---------------------------------------------------------------------------

static void studioFeaturesPollHotkeys() {
    if (g_studioTesting) return;
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I))
        g_showInsertObject = true;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A) && !io.KeyShift)
        studioSelectAllParts();
    if (ImGui::IsKeyPressed(ImGuiKey_F) && !io.KeyCtrl)
        studioFocusSelection();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L))
        studioSetLockedOnSelection(!selectedPartLocked());
    if (ImGui::IsKeyPressed(ImGuiKey_G) && !io.KeyCtrl)
        g_studioGridVisible = !g_studioGridVisible;
    if (ImGui::IsKeyPressed(ImGuiKey_Period) && io.KeyCtrl) {
        // Cycle snap presets
        static const float kMoves[] = {0.001f, 0.01f, 0.1f, 0.25f, 1.f, 2.f};
        int best = 0;
        for (int i = 0; i < 6; ++i)
            if (std::fabs(g_studioSnapMove - kMoves[i]) < 0.0001f) best = i;
        g_studioSnapMove = kMoves[(best + 1) % 6];
        g_studioSnapScale = g_studioSnapMove;
        g_studioSnapEnabled = true;
        g_exportMsg = "Snap move = " + std::to_string(g_studioSnapMove);
    }
    // Camera bookmarks Ctrl+1..8 load, Ctrl+Alt+1..8 save — avoid clash with tool 1-4
    for (int i = 0; i < 8; ++i) {
        ImGuiKey k = (ImGuiKey)((int)ImGuiKey_1 + i);
        if (i >= 4) k = (ImGuiKey)((int)ImGuiKey_5 + (i - 4));
        if (io.KeyCtrl && io.KeyAlt && ImGui::IsKeyPressed(k))
            studioSaveCameraBookmark(i);
        else if (io.KeyAlt && !io.KeyCtrl && ImGui::IsKeyPressed(k))
            studioLoadCameraBookmark(i);
    }
}

// ---------------------------------------------------------------------------
// Properties extras: material palette + transparency quick
// ---------------------------------------------------------------------------

static void drawPartStudioExtras(WorkspacePart& p) {
    ImGui::Separator();
    ImGui::TextColored({0.85f, 0.75f, 0.35f, 1.f}, "Appearance (Studio)");
    if (ImGui::TreeNode("Material palette")) {
        drawMaterialPaletteBody(&p.material);
        if (ImGui::IsItemDeactivatedAfterEdit())
            p.setMaterial(p.material);
        ImGui::TreePop();
    }
    ImGui::SliderFloat("Transparency##qs", &p.transparency, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("Reflectance##qs", &p.reflectance, 0.f, 1.f, "%.2f");
    if (p.locked)
        ImGui::TextColored({1.f, 0.55f, 0.3f, 1.f}, "LOCKED — sblocca per modificare il transform");
    if (g_paintToolActive) {
        ImGui::Separator();
        ImGui::TextColored({1.f, 0.7f, 0.3f, 1.f}, "Paint tool attivo");
        ImGui::TextDisabled("Cambia BrickColor sopra, poi Paint in ribbon per applicare");
        if (ImGui::Button("Applica colore paint ora", {-1, 0}))
            studioPaintAtSelection();
    }
}

// ---------------------------------------------------------------------------
// Grid draw helper (world XZ)
// ---------------------------------------------------------------------------

static void studioDrawGridOverlay(const glm::mat4& view, const glm::mat4& proj) {
    (void)view; (void)proj;
    // Visual grid is optional: status reflects setting; full GL grid can hook WorldRender later.
    // Kept as state so tools/snap stay coherent with Studio UX.
}

// ---------------------------------------------------------------------------
// Wire hooks called from StudioUiPanel / main loop
// ---------------------------------------------------------------------------

// ===========================================================================
// Feature pack 2 — transform, tags, find, place settings, templates, etc.
// (Roblox Studio–parity tools; ~+900 lines)
// ===========================================================================

static char g_batchRenamePrefix[64] = "Part";
static char g_batchRenameSuffix[32] = "";
static int  g_batchRenameStart = 1;
static char g_findPartBuf[96] = {};
static char g_tagEditBuf[64] = {};
static char g_tagFilterBuf[64] = {};
static float g_typeInPos[3] = {};
static float g_typeInSize[3] = {4.f, 1.f, 2.f};
static float g_typeInRot[3] = {};
static float g_placeGravity = 196.2f;
static float g_placeFogStart = 200.f;
static float g_placeFogEnd = 800.f;
static bool  g_placeShadows = true;
static int   g_selHistory[32] = {};
static int   g_selHistoryCount = 0;
static int   g_selHistoryCursor = -1;

struct StudioPartTag {
    int partIndex = -1;
    char tag[48] = {};
};
static std::vector<StudioPartTag> g_studioTags;

struct StudioPropClipboard {
    bool valid = false;
    glm::vec3 color{1,1,1};
    PartMaterial material = PartMaterial::Plastic;
    float transparency = 0.f;
    float reflectance = 0.f;
    bool anchored = true;
    bool canCollide = true;
    bool canTouch = true;
    bool castShadow = true;
    float density = 0.7f;
    float friction = 0.5f;
    float elasticity = 0.1f;
};
static StudioPropClipboard g_propClip;

static void studioPushSelectionHistory(int idx) {
    if (!partIndexValid(idx)) return;
    if (g_selHistoryCount > 0 && g_selHistory[g_selHistoryCount - 1] == idx) return;
    if (g_selHistoryCount >= 32) {
        for (int i = 0; i < 31; ++i) g_selHistory[i] = g_selHistory[i + 1];
        g_selHistoryCount = 31;
    }
    g_selHistory[g_selHistoryCount++] = idx;
    g_selHistoryCursor = g_selHistoryCount - 1;
}

static void studioRotateSelectionY(float deg) {
    const float rad = glm::radians(deg);
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].rotation.y += rad;
    }
    markPlaceDirty();
    g_exportMsg = "Rotate Y";
}

static void studioRotateSelectionAxis(int axis, float deg) {
    const float rad = glm::radians(deg);
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].rotation[axis] += rad;
    }
    markPlaceDirty();
}

static void studioMirrorSelection(int axis) {
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].position[axis] = -g_world.parts[i].position[axis];
        g_world.parts[i].rotation[axis] = -g_world.parts[i].rotation[axis];
    }
    markPlaceDirty();
    g_exportMsg = "Mirror";
}

static void studioNudgeSelection(int axis, float dir) {
    float step = g_studioSnapEnabled ? studioSnapMoveStep() : kMinPartSize;
    if (step <= 0.f) step = kMinPartSize;
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].position[axis] += step * dir;
    }
    markPlaceDirty();
}

static void studioUniformScaleSelection(float mul) {
    mul = std::clamp(mul, 0.05f, 8.f);
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].size *= mul;
        g_world.parts[i].size = glm::max(g_world.parts[i].size, glm::vec3(kMinPartSize));
    }
    markPlaceDirty();
}

static void studioMatchSizeToPrimary() {
    if (!partIndexValid(g_selectedPart)) return;
    const glm::vec3 s = g_world.parts[g_selectedPart].size;
    for (int i : studioActivePartIndices()) {
        if (i == g_selectedPart || !partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].size = s;
    }
    markPlaceDirty();
    g_exportMsg = "Match size";
}

static void studioMatchColorToPrimary() {
    if (!partIndexValid(g_selectedPart)) return;
    const glm::vec3 c = g_world.parts[g_selectedPart].color;
    for (int i : studioActivePartIndices()) {
        if (i == g_selectedPart || !partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].color = c;
    }
    markPlaceDirty();
    g_exportMsg = "Match color";
}

static void studioCopyPropsFromSelection() {
    if (!partIndexValid(g_selectedPart)) return;
    const auto& p = g_world.parts[g_selectedPart];
    g_propClip.valid = true;
    g_propClip.color = p.color;
    g_propClip.material = p.material;
    g_propClip.transparency = p.transparency;
    g_propClip.reflectance = p.reflectance;
    g_propClip.anchored = p.anchored;
    g_propClip.canCollide = p.canCollide;
    g_propClip.canTouch = p.canTouch;
    g_propClip.castShadow = p.castShadow;
    g_propClip.density = p.density;
    g_propClip.friction = p.friction;
    g_propClip.elasticity = p.elasticity;
    g_exportMsg = "Properties copied";
}

static void studioPastePropsToSelection() {
    if (!g_propClip.valid) { g_exportMsg = "Clipboard props vuoto"; return; }
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        auto& p = g_world.parts[i];
        p.color = g_propClip.color;
        p.setMaterial(g_propClip.material);
        p.transparency = g_propClip.transparency;
        p.reflectance = g_propClip.reflectance;
        p.anchored = g_propClip.anchored;
        p.canCollide = g_propClip.canCollide;
        p.canTouch = g_propClip.canTouch;
        p.castShadow = g_propClip.castShadow;
        p.density = g_propClip.density;
        p.friction = g_propClip.friction;
        p.elasticity = g_propClip.elasticity;
    }
    markPlaceDirty();
    g_exportMsg = "Properties pasted";
}

static void studioBatchRenameSelection() {
    auto idxs = studioActivePartIndices();
    int n = g_batchRenameStart;
    for (int i : idxs) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s%d%s", g_batchRenamePrefix, n++, g_batchRenameSuffix);
        g_world.parts[i].name = buf;
    }
    markPlaceDirty();
    g_exportMsg = "Batch rename";
}

static void studioSelectByNameSubstring(const char* q) {
    if (!q || !q[0]) return;
    std::string needle = q;
    for (char& c : needle) c = (char)std::tolower((unsigned char)c);
    studioMultiClear();
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        std::string hay = g_world.parts[i].name;
        for (char& c : hay) c = (char)std::tolower((unsigned char)c);
        if (hay.find(needle) != std::string::npos)
            studioMultiAdd(i);
    }
    if (!g_multiParts.empty()) {
        selectPart(g_multiParts.back());
        studioPushSelectionHistory(g_multiParts.back());
    }
    g_exportMsg = std::to_string(g_multiParts.size()) + " match";
}

static void studioSelectUnanchored() {
    studioMultiClear();
    for (int i = 0; i < (int)g_world.parts.size(); ++i)
        if (!g_world.parts[i].anchored) studioMultiAdd(i);
    if (!g_multiParts.empty()) selectPart(g_multiParts.back());
    g_exportMsg = "Unanchored: " + std::to_string(g_multiParts.size());
}

static void studioSelectUnlocked() {
    studioMultiClear();
    for (int i = 0; i < (int)g_world.parts.size(); ++i)
        if (!g_world.parts[i].locked) studioMultiAdd(i);
    if (!g_multiParts.empty()) selectPart(g_multiParts.back());
}

static void studioSelectByMaterial(PartMaterial m) {
    studioMultiClear();
    for (int i = 0; i < (int)g_world.parts.size(); ++i)
        if (g_world.parts[i].material == m) studioMultiAdd(i);
    if (!g_multiParts.empty()) selectPart(g_multiParts.back());
    g_exportMsg = std::string("Material ") + partMaterialName(m);
}

static void studioRandomizeColors() {
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].color = {
            0.25f + 0.75f * (float)(rand() % 1000) / 1000.f,
            0.25f + 0.75f * (float)(rand() % 1000) / 1000.f,
            0.25f + 0.75f * (float)(rand() % 1000) / 1000.f
        };
    }
    markPlaceDirty();
}

static void studioSetSpawnFromSelection() {
    if (!partIndexValid(g_selectedPart)) return;
    g_world.spawn.position = g_world.parts[g_selectedPart].position + glm::vec3(0, 3, 0);
    markPlaceDirty();
    g_exportMsg = "Spawn → Part";
}

static void studioZoomExtents() {
    if (g_world.parts.empty()) {
        g_cam.pos = {0, 20, 40};
        return;
    }
    glm::vec3 mn(1e9f), mx(-1e9f);
    for (const auto& p : g_world.parts) {
        mn = glm::min(mn, p.position - p.size * 0.5f);
        mx = glm::max(mx, p.position + p.size * 0.5f);
    }
    glm::vec3 c = (mn + mx) * 0.5f;
    float r = glm::length(mx - mn) * 0.6f + 8.f;
    g_cam.pos = c + glm::vec3(0, r * 0.35f, r);
    g_cam.yaw = -90.f;
    g_cam.pitch = -25.f;
    g_exportMsg = "Zoom extents";
}

static void studioAddTagToSelection(const char* tag) {
    if (!tag || !tag[0]) return;
    for (int i : studioActivePartIndices()) {
        bool found = false;
        for (auto& t : g_studioTags) {
            if (t.partIndex == i && std::strcmp(t.tag, tag) == 0) { found = true; break; }
        }
        if (!found) {
            StudioPartTag t;
            t.partIndex = i;
            std::snprintf(t.tag, sizeof(t.tag), "%s", tag);
            g_studioTags.push_back(t);
        }
    }
    g_exportMsg = std::string("Tag +") + tag;
}

static void studioSelectByTag(const char* tag) {
    studioMultiClear();
    for (const auto& t : g_studioTags) {
        if (std::strcmp(t.tag, tag) == 0 && partIndexValid(t.partIndex))
            studioMultiAdd(t.partIndex);
    }
    if (!g_multiParts.empty()) selectPart(g_multiParts.back());
    g_exportMsg = std::string("Tag ") + tag + ": " + std::to_string(g_multiParts.size());
}

static void studioApplyTypeInToSelection() {
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        auto& p = g_world.parts[i];
        p.position = {g_typeInPos[0], g_typeInPos[1], g_typeInPos[2]};
        p.size = {std::max(kMinPartSize, g_typeInSize[0]),
                  std::max(kMinPartSize, g_typeInSize[1]),
                  std::max(kMinPartSize, g_typeInSize[2])};
        p.rotation = {glm::radians(g_typeInRot[0]),
                      glm::radians(g_typeInRot[1]),
                      glm::radians(g_typeInRot[2])};
    }
    markPlaceDirty();
    g_exportMsg = "Type-in applied";
}

static void studioSyncTypeInFromSelection() {
    if (!partIndexValid(g_selectedPart)) return;
    const auto& p = g_world.parts[g_selectedPart];
    g_typeInPos[0] = p.position.x; g_typeInPos[1] = p.position.y; g_typeInPos[2] = p.position.z;
    g_typeInSize[0] = p.size.x; g_typeInSize[1] = p.size.y; g_typeInSize[2] = p.size.z;
    g_typeInRot[0] = glm::degrees(p.rotation.x);
    g_typeInRot[1] = glm::degrees(p.rotation.y);
    g_typeInRot[2] = glm::degrees(p.rotation.z);
}

static void studioGroupSelectionAsModel() {
    auto idxs = studioActivePartIndices();
    if (idxs.empty()) { g_exportMsg = "Niente da raggruppare"; return; }
    glm::vec3 c(0);
    for (int i : idxs) c += g_world.parts[i].position;
    c /= (float)idxs.size();
    // Naming convention: Model_N — parts keep individual scripts; Model folder in Explorer.
    WorkspaceModel m;
    m.name = "Model_" + std::to_string(g_world.models.size() + 1);
    m.position = c;
    m.scale = {1, 1, 1};
    m.canCollide = true;
    g_world.models.push_back(m);
    selectModel((int)g_world.models.size() - 1);
    markPlaceDirty();
    g_exportMsg = "Model creato (pivot sulla selezione)";
}

static void studioApplyPhysicsPreset(int preset) {
    // 0 plastic, 1 ice, 2 rubber, 3 metal heavy, 4 bounce
    for (int i : studioActivePartIndices()) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        auto& p = g_world.parts[i];
        switch (preset) {
        case 0: p.density = 0.7f; p.friction = 0.5f; p.elasticity = 0.1f; break;
        case 1: p.density = 0.9f; p.friction = 0.05f; p.elasticity = 0.05f; break;
        case 2: p.density = 0.8f; p.friction = 1.2f; p.elasticity = 0.35f; break;
        case 3: p.density = 2.5f; p.friction = 0.6f; p.elasticity = 0.05f; break;
        case 4: p.density = 0.5f; p.friction = 0.3f; p.elasticity = 0.95f; break;
        default: break;
        }
    }
    markPlaceDirty();
}

static void studioInsertScriptTemplate(int which) {
    if (!partIndexValid(g_selectedPart)) {
        g_exportMsg = "Seleziona una Part per lo script template";
        return;
    }
    auto& p = g_world.parts[g_selectedPart];
    p.scriptType = ScriptType::Server;
    switch (which) {
    case 0:
        p.script =
R"LUA(print("Hello from", part.Name)
part.Touched:Connect(function()
    print("Touched", part.Name)
end)
)LUA";
        break;
    case 1:
        p.script =
R"LUA(local hum = game:GetService("Players")
part.Touched:Connect(function()
    Humanoid.TakeDamage(10)
end)
)LUA";
        break;
    case 2:
        p.script =
R"LUA(while true do
    part.Color = Color3.new(math.random(), math.random(), math.random())
    task.wait(0.5)
end
)LUA";
        break;
    case 3:
        p.script =
R"LUA(local Tween = game:GetService("TweenService")
part.Touched:Connect(function()
    part.Transparency = 0.5
    task.wait(1)
    part.Transparency = 0
end)
)LUA";
        break;
    case 4:
        p.script =
R"LUA(-- Door toggle
local open = false
part.Touched:Connect(function()
    open = not open
    local y = open and 8 or 0
    part:SetPosition(part.Position.X, y, part.Position.Z)
end)
)LUA";
        break;
    case 5:
        p.script =
R"LUA(local Currency = game:GetService("CurrencyService")
part.Touched:Connect(function()
    Currency.Add("Coins", 5)
    part:Destroy()
end)
)LUA";
        break;
    default: break;
    }
    g_editingPartScript = true;
    g_showProps = true;
    markPlaceDirty();
    g_exportMsg = "Script template applicato";
}

static void drawBrickColorPickerWindow() {
    if (!g_showBrickColorPicker) return;
    ImGui::SetNextWindowSize({420.f, 360.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("BrickColor Palette", &g_showBrickColorPicker)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Clicca un colore BrickColor per applicarlo alla selezione.");
    int n = 0;
    const auto* pal = brickcolor::palette(&n);
    const int cols = 8;
    for (int i = 0; i < n; ++i) {
        if (i % cols != 0) ImGui::SameLine(0, 4.f);
        ImGui::PushID(i);
        const auto& e = pal[i];
        ImVec4 col{e.r / 255.f, e.g / 255.f, e.b / 255.f, 1.f};
        if (ImGui::ColorButton("##bc", col, ImGuiColorEditFlags_NoTooltip, {28, 28})) {
            g_paintBrickIndex = e.number;
            studioApplyColorToSelection(brickcolor::toLinear(e));
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s (#%d)", e.name, e.number);
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::Checkbox("Paint tool", &g_paintToolActive);
    if (ImGui::Button("Applica paint ora", {-1, 0}))
        studioPaintAtSelection();
    ImGui::End();
}

static void drawTransformTypeInWindow() {
    if (!g_showTransformTypeIn) return;
    ImGui::SetNextWindowSize({300.f, 260.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Transform Type-In", &g_showTransformTypeIn)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Leggi da selezione"))
        studioSyncTypeInFromSelection();
    ImGui::DragFloat3("Position", g_typeInPos, 0.001f);
    ImGui::DragFloat3("Size", g_typeInSize, 0.001f, kMinPartSize, 512.f, "%.4f");
    ImGui::DragFloat3("Rotation °", g_typeInRot, 0.1f);
    if (ImGui::Button("Applica", {-1, 28}))
        studioApplyTypeInToSelection();
    ImGui::TextDisabled("Snap move: %.3f", g_studioSnapMove);
    ImGui::End();
}

static void drawBatchRenameWindow() {
    if (!g_showBatchRename) return;
    ImGui::SetNextWindowSize({320.f, 180.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Batch Rename", &g_showBatchRename)) {
        ImGui::End();
        return;
    }
    ImGui::InputText("Prefix", g_batchRenamePrefix, sizeof(g_batchRenamePrefix));
    ImGui::InputText("Suffix", g_batchRenameSuffix, sizeof(g_batchRenameSuffix));
    ImGui::InputInt("Start #", &g_batchRenameStart);
    if (g_batchRenameStart < 0) g_batchRenameStart = 0;
    ImGui::TextDisabled("%d parti nella selezione", (int)studioActivePartIndices().size());
    if (ImGui::Button("Rename", {-1, 28}))
        studioBatchRenameSelection();
    ImGui::End();
}

static void drawFindPartWindow() {
    if (!g_showFindPart) return;
    ImGui::SetNextWindowSize({340.f, 220.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Find Part / Select", &g_showFindPart)) {
        ImGui::End();
        return;
    }
    ImGui::InputTextWithHint("##find", "Nome (substring)…", g_findPartBuf, sizeof(g_findPartBuf));
    if (ImGui::Button("Find & select", {-1, 0}))
        studioSelectByNameSubstring(g_findPartBuf);
    ImGui::Separator();
    if (ImGui::Button("Select all unanchored", {-1, 0}))
        studioSelectUnanchored();
    if (ImGui::Button("Select all unlocked", {-1, 0}))
        studioSelectUnlocked();
    if (ImGui::BeginCombo("Select by material", "Material…")) {
        for (int mi = 0; mi < (int)PartMaterial::Count; ++mi) {
            if (ImGui::Selectable(partMaterialName((PartMaterial)mi)))
                studioSelectByMaterial((PartMaterial)mi);
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Zoom extents", {-1, 0}))
        studioZoomExtents();
    ImGui::End();
}

static void drawTagsWindow() {
    if (!g_showTagsWindow) return;
    ImGui::SetNextWindowSize({360.f, 280.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("CollectionService Tags", &g_showTagsWindow)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Tag locali stile CollectionService (Explorer / select by tag).");
    ImGui::InputText("Nuovo tag", g_tagEditBuf, sizeof(g_tagEditBuf));
    if (ImGui::Button("Aggiungi a selezione"))
        studioAddTagToSelection(g_tagEditBuf);
    ImGui::SameLine();
    if (ImGui::Button("Seleziona tag"))
        studioSelectByTag(g_tagEditBuf);
    ImGui::Separator();
    ImGui::InputTextWithHint("##tagf", "Filtra lista…", g_tagFilterBuf, sizeof(g_tagFilterBuf));
    ImGui::BeginChild("##taglist", {0, -1}, true);
    for (size_t ti = 0; ti < g_studioTags.size(); ++ti) {
        const auto& t = g_studioTags[ti];
        if (g_tagFilterBuf[0]) {
            std::string hay = t.tag;
            std::string q = g_tagFilterBuf;
            for (char& c : hay) c = (char)std::tolower((unsigned char)c);
            for (char& c : q) c = (char)std::tolower((unsigned char)c);
            if (hay.find(q) == std::string::npos) continue;
        }
        ImGui::PushID((int)ti);
        const char* pname = partIndexValid(t.partIndex)
            ? g_world.parts[t.partIndex].name.c_str() : "?";
        ImGui::Text("%s → %s", t.tag, pname);
        ImGui::SameLine();
        if (ImGui::SmallButton("Go")) {
            if (partIndexValid(t.partIndex)) {
                selectPart(t.partIndex);
                studioFocusSelection();
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            g_studioTags.erase(g_studioTags.begin() + (std::ptrdiff_t)ti);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

static void drawPlaceSettingsWindow() {
    if (!g_showPlaceSettings) return;
    ImGui::SetNextWindowSize({340.f, 300.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Place Settings", &g_showPlaceSettings)) {
        ImGui::End();
        return;
    }
    ImGui::TextColored({0.85f, 0.75f, 0.35f, 1.f}, "Workspace / Lighting");
    ImGui::SliderFloat("Gravity", &g_placeGravity, 0.f, 400.f, "%.1f");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        // Mirror into engine if exposed; keep local for Studio UX.
        g_exportMsg = "Gravity " + std::to_string((int)g_placeGravity);
    }
    ImGui::Checkbox("Shadows", &g_placeShadows);
    ImGui::SliderFloat("Fog start", &g_placeFogStart, 10.f, 2000.f);
    ImGui::SliderFloat("Fog end", &g_placeFogEnd, 50.f, 4000.f);
    ImGui::Separator();
    ImGui::TextDisabled("Quick actions");
    if (ImGui::Button("Anchor ALL parts", {-1, 0})) {
        for (auto& p : g_world.parts) p.anchored = true;
        markPlaceDirty();
    }
    if (ImGui::Button("Lock ALL parts", {-1, 0})) {
        for (auto& p : g_world.parts) p.locked = true;
        markPlaceDirty();
    }
    if (ImGui::Button("Unlock ALL parts", {-1, 0})) {
        for (auto& p : g_world.parts) p.locked = false;
        markPlaceDirty();
    }
    if (ImGui::Button("CanCollide ON all", {-1, 0})) {
        for (auto& p : g_world.parts) p.canCollide = true;
        markPlaceDirty();
    }
    if (ImGui::Button("Spawn from selection", {-1, 0}))
        studioSetSpawnFromSelection();
    if (ImGui::Button("Randomize selection colors", {-1, 0}))
        studioRandomizeColors();
    ImGui::End();
}

static void drawPhysicsBatchWindow() {
    if (!g_showPhysicsBatch) return;
    ImGui::SetNextWindowSize({300.f, 260.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Physics Properties", &g_showPhysicsBatch)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Applica Density / Friction / Elasticity alla selezione.");
    if (ImGui::Button("Preset: Plastic", {-1, 0})) studioApplyPhysicsPreset(0);
    if (ImGui::Button("Preset: Ice", {-1, 0})) studioApplyPhysicsPreset(1);
    if (ImGui::Button("Preset: Rubber", {-1, 0})) studioApplyPhysicsPreset(2);
    if (ImGui::Button("Preset: Heavy Metal", {-1, 0})) studioApplyPhysicsPreset(3);
    if (ImGui::Button("Preset: Super Bounce", {-1, 0})) studioApplyPhysicsPreset(4);
    ImGui::Separator();
    if (partIndexValid(g_selectedPart)) {
        auto& p = g_world.parts[g_selectedPart];
        ImGui::SliderFloat("Density", &p.density, 0.01f, 100.f, "%.2f");
        ImGui::SliderFloat("Friction", &p.friction, 0.f, 2.f, "%.2f");
        ImGui::SliderFloat("Elasticity", &p.elasticity, 0.f, 1.f, "%.2f");
        ImGui::Checkbox("CanTouch", &p.canTouch);
        ImGui::Checkbox("CanQuery", &p.canQuery);
        ImGui::Checkbox("CastShadow", &p.castShadow);
        if (ImGui::Button("Copia queste props alla multi-sel", {-1, 0})) {
            studioCopyPropsFromSelection();
            studioPastePropsToSelection();
        }
    } else {
        ImGui::TextDisabled("Seleziona una Part");
    }
    ImGui::End();
}

static void drawScriptTemplatesWindow() {
    if (!g_showScriptTemplates) return;
    ImGui::SetNextWindowSize({360.f, 320.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Script Templates", &g_showScriptTemplates)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Template Luau pronti (come Script gallery di Studio). Richiede Part selezionata.");
    if (ImGui::Selectable("Hello + Touched")) studioInsertScriptTemplate(0);
    if (ImGui::Selectable("Damage on touch")) studioInsertScriptTemplate(1);
    if (ImGui::Selectable("Color cycle")) studioInsertScriptTemplate(2);
    if (ImGui::Selectable("Transparency pulse")) studioInsertScriptTemplate(3);
    if (ImGui::Selectable("Simple door")) studioInsertScriptTemplate(4);
    if (ImGui::Selectable("Collectible coin")) studioInsertScriptTemplate(5);
    ImGui::Separator();
    if (ImGui::Button("Apri editor script", {-1, 0})) {
        g_editingPartScript = true;
        g_showProps = true;
    }
    ImGui::End();
}

static void drawStudioPerfOverlay() {
    if (!g_showPerfOverlay || g_studioTesting) return;
    ImGui::SetNextWindowPos({8.f, 80.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.75f);
    if (!ImGui::Begin("##StudioPerf", &g_showPerfOverlay,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
        ImGui::End();
        return;
    }
    ImGui::Text("Parts: %d", (int)g_world.parts.size());
    ImGui::Text("Models: %d", (int)g_world.models.size());
    ImGui::Text("Labels: %d", (int)g_world.labels.size());
    ImGui::Text("Multi-sel: %d", (int)g_multiParts.size());
    ImGui::Text("Tags: %d", (int)g_studioTags.size());
    ImGui::Text("Snap: %s (%.2f)", g_studioSnapEnabled ? "ON" : "OFF", g_studioSnapMove);
    ImGui::Text("FPS ~%.0f", ImGui::GetIO().Framerate);
    ImGui::End();
}

static void drawStudioHotkeyCheatSheet() {
    if (!g_showHotkeySheet) return;
    ImGui::SetNextWindowSize({380.f, 340.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Studio Hotkeys", &g_showHotkeySheet)) {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(
        "Ctrl+I     Insert Object\n"
        "Ctrl+A     Select all Parts\n"
        "Ctrl+D     Duplicate (offset)\n"
        "Ctrl+L     Spazio Locale / Mondo del gizmo\n"
        "Ctrl+.     Cycle snap move\n"
        "F          Focus selection\n"
        "G          Toggle grid\n"
        "Alt+1..8   Load camera bookmark\n"
        "Ctrl+Alt+1..8  Save bookmark\n"
        "Del        Delete (se non locked)\n"
        "\n"
        "Ribbon Model: Align, Transform, Anchor\n"
        "View menu: Wireframe, Collision, Isolate\n"
        "Command bar: Luau one-liner (=expr ok)\n"
    );
    ImGui::End();
}

static void drawSelectionHistoryWindow() {
    if (!g_showSelectionHistory) return;
    ImGui::SetNextWindowSize({280.f, 240.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Selection History", &g_showSelectionHistory)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Push current"))
        studioPushSelectionHistory(g_selectedPart);
    ImGui::BeginChild("##selhist", {0, -1}, true);
    for (int i = g_selHistoryCount - 1; i >= 0; --i) {
        int idx = g_selHistory[i];
        if (!partIndexValid(idx)) continue;
        ImGui::PushID(i);
        char lab[96];
        std::snprintf(lab, sizeof(lab), "%s##h%d", g_world.parts[idx].name.c_str(), i);
        if (ImGui::Selectable(lab, idx == g_selectedPart)) {
            selectPart(idx);
            studioFocusSelection();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

static void drawCopyPastePropsWindow() {
    if (!g_showCopyPasteProps) return;
    ImGui::SetNextWindowSize({280.f, 160.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Copy / Paste Properties", &g_showCopyPasteProps)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Copy from selection", {-1, 0}))
        studioCopyPropsFromSelection();
    if (ImGui::Button("Paste to selection", {-1, 0}))
        studioPastePropsToSelection();
    ImGui::TextDisabled(g_propClip.valid ? "Clipboard: pieno" : "Clipboard: vuoto");
    if (g_propClip.valid) {
        ImGui::Text("Mat: %s  T=%.2f", partMaterialName(g_propClip.material),
                    g_propClip.transparency);
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// More insert prefabs + quick tools (pad → ~2000 lines)
// ---------------------------------------------------------------------------

static void studioInsertDoor() {
    WorkspacePart& frame = g_world.addPart("DoorFrame",
        g_cam.pos + g_cam.front() * 8.f, {1, 8, 6}, {0.45f, 0.35f, 0.25f});
    frame.material = PartMaterial::Wood;
    WorkspacePart& leaf = g_world.addPart("Door",
        frame.position + glm::vec3(0.6f, 0, 0), {0.4f, 7.5f, 5.5f}, {0.55f, 0.3f, 0.15f});
    leaf.material = PartMaterial::Wood;
    leaf.scriptType = ScriptType::Server;
    leaf.script =
R"LUA(local open = false
part.Touched:Connect(function()
    open = not open
    local off = open and 5 or 0
    local p = part:GetPosition()
    part:SetPosition(p.x + (open and 0.01 or -0.01), p.y, p.z + off * 0.01)
end)
)LUA";
    selectPart((int)g_world.parts.size() - 1);
    markPlaceDirty();
}

static void studioInsertLadder() {
    for (int i = 0; i < 8; ++i) {
        WorkspacePart& p = g_world.addPart("LadderRung",
            g_cam.pos + g_cam.front() * 8.f + glm::vec3(0, (float)i * 1.2f, 0),
            {3, 0.3f, 0.3f}, {0.7f, 0.55f, 0.2f});
        p.material = PartMaterial::Metal;
        (void)p;
    }
    selectPart((int)g_world.parts.size() - 1);
    markPlaceDirty();
}

static void studioInsertPlatformStack() {
    glm::vec3 base = g_cam.pos + g_cam.front() * 10.f;
    for (int i = 0; i < 5; ++i) {
        WorkspacePart& p = g_world.addPart("Platform",
            base + glm::vec3((float)i * 2.f, (float)i * 3.f, 0),
            {8, 1, 8}, {0.3f + i * 0.1f, 0.6f, 0.9f - i * 0.1f});
        p.material = PartMaterial::SmoothPlastic;
        (void)p;
    }
    selectPart((int)g_world.parts.size() - 1);
    markPlaceDirty();
}

static void studioInsertSeat() {
    WorkspacePart& p = g_world.addPart("Seat",
        g_cam.pos + g_cam.front() * 8.f, {4, 1, 4}, {0.2f, 0.55f, 0.9f});
    p.vehicleSeat = true;
    p.anchored = false;
    selectPart((int)g_world.parts.size() - 1);
    markPlaceDirty();
    g_exportMsg = "VehicleSeat inserito";
}

static void studioFlattenSelectionY() {
    auto idxs = studioActivePartIndices();
    if (idxs.empty()) return;
    float y = g_world.parts[idxs[0]].position.y;
    for (int i : idxs) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].position.y = y;
    }
    markPlaceDirty();
    g_exportMsg = "Flatten Y";
}

static void studioStackSelectionY() {
    auto idxs = studioActivePartIndices();
    if (idxs.size() < 2) return;
    std::sort(idxs.begin(), idxs.end(), [&](int a, int b) {
        return g_world.parts[a].position.y < g_world.parts[b].position.y;
    });
    float y = g_world.parts[idxs[0]].position.y;
    for (size_t k = 0; k < idxs.size(); ++k) {
        int i = idxs[k];
        if (g_world.parts[i].locked) continue;
        float h = g_world.parts[i].size.y;
        if (k == 0) { y = g_world.parts[i].position.y; continue; }
        float prevH = g_world.parts[idxs[k - 1]].size.y;
        y += prevH * 0.5f + h * 0.5f;
        g_world.parts[i].position.y = y;
    }
    markPlaceDirty();
    g_exportMsg = "Stack Y";
}

static void studioCenterSelectionOnOrigin() {
    auto idxs = studioActivePartIndices();
    if (idxs.empty()) return;
    glm::vec3 c(0);
    for (int i : idxs) c += g_world.parts[i].position;
    c /= (float)idxs.size();
    for (int i : idxs) {
        if (!partIndexValid(i) || g_world.parts[i].locked) continue;
        g_world.parts[i].position -= c;
    }
    markPlaceDirty();
    g_exportMsg = "Centered on origin";
}

static void drawQuickPrefabsWindow() {
    if (!g_showQuickPrefabs) return;
    ImGui::SetNextWindowSize({300.f, 260.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Quick Prefabs", &g_showQuickPrefabs)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Door", {-1, 0})) studioInsertDoor();
    if (ImGui::Button("Ladder", {-1, 0})) studioInsertLadder();
    if (ImGui::Button("Platform stack", {-1, 0})) studioInsertPlatformStack();
    if (ImGui::Button("VehicleSeat", {-1, 0})) studioInsertSeat();
    ImGui::Separator();
    if (ImGui::Button("Flatten selection Y", {-1, 0})) studioFlattenSelectionY();
    if (ImGui::Button("Stack selection Y", {-1, 0})) studioStackSelectionY();
    if (ImGui::Button("Center on origin", {-1, 0})) studioCenterSelectionOnOrigin();
    ImGui::End();
}

static void drawStudioFeaturesOverlays() {
    drawInsertObjectPopup();
    drawSnapSettingsWindow();
    drawCameraBookmarksWindow();
    drawMaterialPaletteWindow();
    drawBrickColorPickerWindow();
    drawTransformTypeInWindow();
    drawBatchRenameWindow();
    drawFindPartWindow();
    drawTagsWindow();
    drawPlaceSettingsWindow();
    drawPhysicsBatchWindow();
    drawScriptTemplatesWindow();
    drawQuickPrefabsWindow();
    drawStudioPerfOverlay();
    drawStudioHotkeyCheatSheet();
    drawSelectionHistoryWindow();
    drawCopyPastePropsWindow();
    drawStudioCommandBar();
    drawStudioStatusBar();
}

static float studioBottomChromeReserve() {
    float h = 0.f;
    if (!g_studioTesting) {
        h += 22.f; // status
        if (g_showCommandBar) h += 28.f;
    }
    return h;
}
