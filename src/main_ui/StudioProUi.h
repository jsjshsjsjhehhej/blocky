#pragma once

#include "engine/StudioProApis.h"
#include "engine/DataStore.h"

/**
 * Studio Pro pack — ~2000 linee di UI stile Roblox Studio + tool creatore.
 * Game Settings, Attributes, Tags, Sound browser, Material Manager,
 * Insert Object, Performance, Teams, leaderstats, ProximityPrompt placer,
 * DataStore browser, Lighting presets, Align/Distribute, Script Analysis,
 * Device emulator, Publish, Soft-shutdown, Context actions, Http toggle.
 * Incluso solo da main.cpp (come StudioFeaturesUi / Studio2020Ui).
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// ===========================================================================
// Stato globale Pro
// ===========================================================================

static bool g_showGameSettings = false;
static bool g_showAttributesEditor = false;
static bool g_showTagsEditor = false;
static bool g_showSoundBrowser = false;
static bool g_showMaterialManager = false;
static bool g_showProInsertObject = false;
static bool g_showPerfStats = false;
static bool g_showTeamsEditor = false;
static bool g_showLeaderstatsEditor = false;
static bool g_showPromptPlacer = false;
static bool g_showDataStoreBrowser = false;
static bool g_showLightingPresets = false;
static bool g_showAlignTools = false;
static bool g_showScriptAnalysis = false;
static bool g_showDeviceEmulator = false;
static bool g_showPublishDialog = false;
static bool g_showSoftShutdown = false;
static bool g_showContextActions = false;
static bool g_showHttpServicePanel = false;
static bool g_showCameraPanel = false;
static bool g_showPhysicsPanel = false;
static bool g_showSpawnEditor = false;
static bool g_showBadgePanel = false;
static bool g_showLocalization = false;
static bool g_showPackageVersions = false;
static bool g_showStreamingPanel = false;
static bool g_showFilterPanel = false;
static bool g_showSkyboxPicker = false;
static bool g_showPostFxPanel = false;
static bool g_showEmotePanel = false;
static bool g_showAccessoryPanel = false;
static bool g_showCollisionDebug = false;
static bool g_showNetworkOwner = false;
static bool g_showHistoryPanel = false;
static bool g_showToolboxPro = false;
static bool g_showCommandPalette = false;
static bool g_showQuickActions = false;
static bool g_showBatchTools = false;
static bool g_showSelectionSets = false;
static bool g_showStudioPrefs = false;
static bool g_showNotifCenter = false;
static bool g_showTerrainExtras = false;
static bool g_showHumanoidDefaults = false;
static bool g_showRemotesBrowser = false;
static bool g_showColorPalette = false;
static bool g_showBrickColor = false;
static bool g_showPartCounter = false;
static bool g_showProScriptTemplates = false;
static bool g_showMobilePreview = false;
static bool g_showSecurityPanel = false;
static bool g_showAnimIdPanel = false;
static bool g_showToolGripPanel = false;
static bool g_showChatSettings = false;
static bool g_showFriendPanel = false;
static bool g_showKitGenerators = false;
static bool g_showMirrorTools = false;
static bool g_showUnionHint = false;
static bool g_showPluginBar = false;
static bool g_showPivotTools = false;
static bool g_showBulkScripts = false;
static bool g_showPlaceValidator = false;
static bool g_showHotkeyCheatsheet = false;

static char g_proSearch[128] = {};
static char g_attrKey[64] = "Coins";
static char g_attrVal[128] = "0";
static char g_tagName[64] = "Interactable";
static char g_soundPath[256] = "sounds/jump.wav";
static float g_soundVol = 1.f;
static bool g_soundLoop = false;
static int g_matFilter = 0;
static char g_insertFilter[64] = {};
static int g_insertCategory = 0;
static char g_teamName[32] = "Red";
static float g_teamColor[3] = {1.f, 0.2f, 0.2f};
static char g_statName[16] = "Coins";
static char g_promptName[32] = "Door";
static char g_promptText[32] = "Open";
static float g_promptRadius = 8.f;
static char g_dsStoreName[64] = "PlayerData";
static char g_dsKey[64] = "coins";
static char g_dsValue[128] = "0";
static int g_lightingPreset = 0;
static int g_alignAxis = 1; // Y
static int g_devicePreset = 0;
static char g_publishTitle[64] = "My Place";
static char g_publishDesc[256] = "";
static bool g_httpEnabled = true;
static bool g_streamingEnabled = false;
static bool g_filteringEnabled = true;
static float g_camFovPro = 70.f;
static float g_physGravity = 80.f;
static float g_physSteps = 1.f;
static char g_badgeName[32] = "FirstWin";
static char g_localeKey[64] = "HELLO";
static char g_localeVal[128] = "Hello!";
static char g_skyboxId[64] = "default";
static float g_bloomPro = 0.35f;
static float g_saturationPro = 1.f;
static char g_cmdPalette[128] = {};
static int g_historySel = -1;
static std::vector<std::string> g_proHistory;
static std::vector<std::string> g_scriptAnalysisHits;
static std::map<std::string, std::string> g_partAttributes; // "partIdx:key" -> val
static std::map<std::string, bool> g_contextActionsBound;

static void proPushHistory(const char* msg) {
    if (!msg || !msg[0]) return;
    g_proHistory.push_back(msg);
    if (g_proHistory.size() > 80) g_proHistory.erase(g_proHistory.begin());
}

static std::string proAttrKey(int partIdx, const char* k) {
    return std::to_string(partIdx) + ":" + (k ? k : "");
}

// ===========================================================================
// Helpers
// ===========================================================================

static void proBeginWindow(const char* title, bool* open, float w = 440.f, float h = 360.f) {
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_FirstUseEver);
    ImGui::Begin(title, open);
}

static bool proFilterMatch(const char* hay, const char* needle) {
    if (!needle || !needle[0]) return true;
    if (!hay) return false;
    std::string h = hay, n = needle;
    for (char& c : h) c = (char)std::tolower((unsigned char)c);
    for (char& c : n) c = (char)std::tolower((unsigned char)c);
    return h.find(n) != std::string::npos;
}

static void proSepTitle(const char* t) {
    ImGui::Spacing();
    ImGui::TextColored({0.55f, 0.85f, 1.f, 1.f}, "%s", t);
    ImGui::Separator();
}

// ===========================================================================
// 1. Game Settings
// ===========================================================================

static void drawGameSettingsWindow() {
    if (!g_showGameSettings) return;
    proBeginWindow("Game Settings##pro", &g_showGameSettings, 480.f, 420.f);
    proSepTitle("Place");
    ImGui::InputText("Place Name", g_publishTitle, sizeof(g_publishTitle));
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        g_world.placeName = g_publishTitle;
        markPlaceDirty();
        proPushHistory("Place name updated");
    }
    ImGui::InputTextMultiline("Description", g_publishDesc, sizeof(g_publishDesc), {-1.f, 60.f});
    proSepTitle("Security");
    ImGui::Checkbox("FilteringEnabled (display)", &g_filteringEnabled);
    ImGui::Checkbox("HttpService Enabled", &g_httpEnabled);
    ImGui::Checkbox("StreamingEnabled", &g_streamingEnabled);
    proSepTitle("Defaults");
    ImGui::SliderFloat("Default FOV", &g_camFovPro, 40.f, 120.f);
    ImGui::SliderFloat("Gravity scale", &g_physGravity, 20.f, 200.f);
    if (ImGui::Button("Apply spawn from camera", {-1, 0})) {
        g_world.spawn.position = g_playerPos;
        g_world.spawn.yaw = g_playerYaw;
        markPlaceDirty();
        proPushHistory("Spawn set from camera");
    }
    ImGui::TextDisabled("Studio Pro · Game Settings");
    ImGui::End();
}

// ===========================================================================
// 2. Attributes Editor
// ===========================================================================

static void drawAttributesEditorWindow() {
    if (!g_showAttributesEditor) return;
    proBeginWindow("Attributes##pro", &g_showAttributesEditor, 420.f, 380.f);
    if (!partIndexValid(g_selectedPart)) {
        ImGui::TextWrapped("Seleziona una Part per modificare Attributes (GetAttribute/SetAttribute).");
        ImGui::End();
        return;
    }
    auto& p = g_world.parts[g_selectedPart];
    ImGui::Text("Part: %s [#%d]", p.name.c_str(), g_selectedPart);
    ImGui::InputText("Key", g_attrKey, sizeof(g_attrKey));
    ImGui::InputText("Value", g_attrVal, sizeof(g_attrVal));
    if (ImGui::Button("SetAttribute", {-1, 0})) {
        g_partAttributes[proAttrKey(g_selectedPart, g_attrKey)] = g_attrVal;
        studio_pro_apis::attributes()[proAttrKey(g_selectedPart, g_attrKey)] = g_attrVal;
        markPlaceDirty();
        proPushHistory("SetAttribute");
        g_exportMsg = std::string("Attribute ") + g_attrKey + " = " + g_attrVal;
    }
    ImGui::Separator();
    ImGui::TextDisabled("Attributes su questa Part:");
    const std::string prefix = std::to_string(g_selectedPart) + ":";
    for (auto it = g_partAttributes.begin(); it != g_partAttributes.end();) {
        if (it->first.compare(0, prefix.size(), prefix) != 0) { ++it; continue; }
        std::string key = it->first.substr(prefix.size());
        ImGui::BulletText("%s = %s", key.c_str(), it->second.c_str());
        ImGui::SameLine();
        char btn[64];
        std::snprintf(btn, sizeof(btn), "X##a%s", key.c_str());
        if (ImGui::SmallButton(btn)) {
            it = g_partAttributes.erase(it);
            markPlaceDirty();
        } else ++it;
    }
    ImGui::End();
}

// ===========================================================================
// 3. Tags Editor (CollectionService)
// ===========================================================================

static void drawTagsEditorWindow() {
    if (!g_showTagsEditor) return;
    proBeginWindow("Tags (CollectionService)##pro", &g_showTagsEditor);
    if (!partIndexValid(g_selectedPart)) {
        ImGui::TextWrapped("Seleziona una Part. Tag via CollectionService:AddTag.");
        ImGui::End();
        return;
    }
    ImGui::InputText("Tag", g_tagName, sizeof(g_tagName));
    if (ImGui::Button("AddTag", {120, 0}) && g_tagName[0]) {
        if (g_lua.tags) {
            // Tag system uses string ids — best-effort via Lua if available
        }
        char cmd[192];
        std::snprintf(cmd, sizeof(cmd),
            "local p=workspace:FindFirstChild(%q) if p then game:GetService('CollectionService'):AddTag(p,%q) end",
            g_world.parts[g_selectedPart].name.c_str(), g_tagName);
        // Store local mirror
        g_partAttributes[proAttrKey(g_selectedPart, (std::string("tag:") + g_tagName).c_str())] = "1";
        markPlaceDirty();
        proPushHistory("AddTag");
        g_exportMsg = std::string("Tag added: ") + g_tagName;
    }
    ImGui::SameLine();
    if (ImGui::Button("RemoveTag", {120, 0}) && g_tagName[0]) {
        g_partAttributes.erase(proAttrKey(g_selectedPart, (std::string("tag:") + g_tagName).c_str()));
        markPlaceDirty();
        proPushHistory("RemoveTag");
    }
    ImGui::Separator();
    ImGui::TextDisabled("Tag locali (mirror Attributes):");
    const std::string prefix = std::to_string(g_selectedPart) + ":tag:";
    for (const auto& kv : g_partAttributes) {
        if (kv.first.compare(0, prefix.size(), prefix) == 0)
            ImGui::BulletText("%s", kv.first.c_str() + prefix.size());
    }
    ImGui::End();
}

// ===========================================================================
// 4. Sound Browser
// ===========================================================================

static void drawSoundBrowserWindow() {
    if (!g_showSoundBrowser) return;
    proBeginWindow("Sound Browser##pro", &g_showSoundBrowser, 460.f, 320.f);
    ImGui::InputText("SoundId / path", g_soundPath, sizeof(g_soundPath));
    ImGui::SliderFloat("Volume", &g_soundVol, 0.f, 2.f);
    ImGui::Checkbox("Looped", &g_soundLoop);
    if (ImGui::Button("Play preview", {-1, 0})) {
        if (g_lua.soundService) {
            // SoundService play if available
        }
        g_sounds.playPath(g_soundPath, g_soundLoop);
        proPushHistory("Sound preview");
    }
    if (ImGui::Button("Stop all", {-1, 0}))
        g_sounds.stopAll();
    ImGui::Separator();
    ImGui::TextDisabled("Presets");
    const char* presets[] = {
        "sounds/jump.wav", "sounds/click.wav", "sounds/coin.wav",
        "sounds/explode.wav", "sounds/music_loop.wav", "sounds/hit.wav"
    };
    for (const char* pr : presets) {
        if (ImGui::Selectable(pr))
            std::snprintf(g_soundPath, sizeof(g_soundPath), "%s", pr);
    }
    ImGui::End();
}

// ===========================================================================
// 5. Material Manager
// ===========================================================================

static void drawMaterialManagerWindow() {
    if (!g_showMaterialManager) return;
    proBeginWindow("Material Manager##pro", &g_showMaterialManager, 400.f, 480.f);
    ImGui::TextWrapped("Applica materiale Studio a Part selezionata o a tutte le Part filtrate.");
    const char* mats[] = {
        "Plastic", "SmoothPlastic", "Neon", "Wood", "WoodPlanks", "Marble", "Slate",
        "Concrete", "Granite", "Brick", "Pebble", "Cobblestone", "Rock", "Sandstone",
        "Metal", "CorrodedMetal", "DiamondPlate", "Foil", "Grass", "Ice", "Glass",
        "ForceField", "Fabric", "Carpet", "Leather"
    };
    static int matSel = 0;
    ImGui::Combo("Material", &matSel, mats, (int)(sizeof(mats) / sizeof(mats[0])));
    if (ImGui::Button("Apply to selection", {-1, 0}) && partIndexValid(g_selectedPart)) {
        g_world.parts[g_selectedPart].setMaterial(partMaterialFromName(mats[matSel]));
        markPlaceDirty();
        proPushHistory("Material applied");
    }
    if (ImGui::Button("Apply to ALL parts", {-1, 0})) {
        for (auto& p : g_world.parts)
            p.setMaterial(partMaterialFromName(mats[matSel]));
        markPlaceDirty();
        proPushHistory("Material all");
    }
    ImGui::Separator();
    ImGui::Text("Counts by material:");
    std::map<std::string, int> counts;
    for (const auto& p : g_world.parts)
        counts[partMaterialName(p.material)]++;
    for (const auto& kv : counts)
        ImGui::BulletText("%s: %d", kv.first.c_str(), kv.second);
    ImGui::End();
}

// ===========================================================================
// 6. Insert Object (grande)
// ===========================================================================

static void drawInsertObjectWindow() {
    if (!g_showProInsertObject) return;
    proBeginWindow("Insert Object##pro", &g_showProInsertObject, 360.f, 520.f);
    ImGui::InputText("Filter", g_insertFilter, sizeof(g_insertFilter));
    const char* cats[] = { "Parts", "UI", "Scripts", "Gameplay", "Effects", "Constraints" };
    ImGui::Combo("Category", &g_insertCategory, cats, 6);
    ImGui::Separator();
    struct Ins { const char* name; const char* cat; };
    static const Ins items[] = {
        {"Part", "Parts"}, {"SpawnLocation", "Parts"}, {"WedgePart", "Parts"},
        {"CornerWedgePart", "Parts"}, {"TrussPart", "Parts"}, {"MeshPart", "Parts"},
        {"Model", "Parts"}, {"Folder", "Parts"},
        {"ScreenGui", "UI"}, {"Frame", "UI"}, {"ScrollingFrame", "UI"},
        {"TextLabel", "UI"}, {"TextButton", "UI"}, {"ImageLabel", "UI"},
        {"ImageButton", "UI"}, {"UIListLayout", "UI"}, {"UIPadding", "UI"},
        {"UIGridLayout", "UI"}, {"UICorner", "UI"}, {"UIStroke", "UI"},
        {"Script", "Scripts"}, {"LocalScript", "Scripts"}, {"ModuleScript", "Scripts"},
        {"BindableEvent", "Scripts"}, {"BindableFunction", "Scripts"},
        {"RemoteEvent", "Scripts"}, {"RemoteFunction", "Scripts"},
        {"ProximityPrompt", "Gameplay"}, {"ClickDetector", "Gameplay"},
        {"TouchTransmitter", "Gameplay"}, {"Tool", "Gameplay"}, {"HopperBin", "Gameplay"},
        {"Humanoid", "Gameplay"}, {"Team", "Gameplay"}, {"SpawnLocation", "Gameplay"},
        {"Sound", "Effects"}, {"ParticleEmitter", "Effects"}, {"Fire", "Effects"},
        {"Smoke", "Effects"}, {"Sparkles", "Effects"}, {"PointLight", "Effects"},
        {"SpotLight", "Effects"}, {"SurfaceLight", "Effects"}, {"Beam", "Effects"},
        {"Trail", "Effects"}, {"Explosion", "Effects"},
        {"WeldConstraint", "Constraints"}, {"Motor6D", "Constraints"},
        {"RopeConstraint", "Constraints"}, {"RodConstraint", "Constraints"},
        {"SpringConstraint", "Constraints"}, {"HingeConstraint", "Constraints"},
        {"BallSocketConstraint", "Constraints"}, {"PrismaticConstraint", "Constraints"},
        {"CylindricalConstraint", "Constraints"}, {"AlignPosition", "Constraints"},
        {"AlignOrientation", "Constraints"}, {"VectorForce", "Constraints"},
        {"LineForce", "Constraints"}, {"Torque", "Constraints"},
        {"AngularVelocity", "Constraints"}, {"LinearVelocity", "Constraints"},
    };
    ImGui::BeginChild("##inslist", {0, -36}, true);
    for (const auto& it : items) {
        if (std::strcmp(it.cat, cats[g_insertCategory]) != 0) continue;
        if (!proFilterMatch(it.name, g_insertFilter)) continue;
        if (ImGui::Selectable(it.name)) {
            // Dispatch insert
            if (std::strcmp(it.name, "Part") == 0) insertNewPart();
            else if (std::strcmp(it.name, "Model") == 0) insertImportedModel();
            else if (std::strcmp(it.name, "Frame") == 0) insertNewFrame();
            else if (std::strcmp(it.name, "ImageLabel") == 0) insertNewImageLabel();
            else if (std::strcmp(it.name, "TextLabel") == 0 || std::strcmp(it.name, "TextButton") == 0) {
                WorkspaceLabel l;
                l.name = it.name;
                l.className = it.name;
                l.text = it.name;
                if (std::strcmp(it.name, "TextButton") == 0) l.bgTransparency = 0.f;
                g_world.labels.push_back(std::move(l));
                g_selectedLabel = (int)g_world.labels.size() - 1;
                markPlaceDirty();
            } else if (std::strcmp(it.name, "Script") == 0 || std::strcmp(it.name, "LocalScript") == 0
                       || std::strcmp(it.name, "ModuleScript") == 0) {
                auto& s = g_world.addShared(SharedService::ServerScriptService, it.name,
                    std::string(it.name) + std::to_string(g_world.instances.size() + 1));
                if (std::strcmp(it.name, "ModuleScript") == 0)
                    s.source = "local M = {}\nreturn M\n";
                else
                    s.source = "print(script.Name)\n";
                markPlaceDirty();
            } else if (std::strcmp(it.name, "RemoteEvent") == 0 || std::strcmp(it.name, "RemoteFunction") == 0
                       || std::strcmp(it.name, "Folder") == 0) {
                g_world.addShared(SharedService::ReplicatedStorage, it.name,
                    std::string(it.name) + std::to_string(g_world.instances.size() + 1));
                markPlaceDirty();
            } else if (std::strcmp(it.name, "ProximityPrompt") == 0) {
                g_showPromptPlacer = true;
            } else if (std::strcmp(it.name, "WeldConstraint") == 0) {
                studioCreateWeldFromSelection();
            } else if (std::strcmp(it.name, "SpawnLocation") == 0) {
                insertNewPart();
                if (partIndexValid(g_selectedPart)) {
                    g_world.parts[g_selectedPart].name = "SpawnLocation";
                    g_world.parts[g_selectedPart].color = {0.2f, 0.9f, 0.4f};
                    g_world.spawn.position = g_world.parts[g_selectedPart].position;
                }
                markPlaceDirty();
            } else {
                g_exportMsg = std::string("Insert: ") + it.name + " (placeholder Part)";
                insertNewPart();
                if (partIndexValid(g_selectedPart))
                    g_world.parts[g_selectedPart].name = it.name;
            }
            proPushHistory(it.name);
        }
    }
    ImGui::EndChild();
    ImGui::TextDisabled("%d categories · Studio Pro Insert", 6);
    ImGui::End();
}


// ===========================================================================
// 7. Performance Stats
// ===========================================================================

static void drawPerfStatsWindow() {
    if (!g_showPerfStats) return;
    proBeginWindow("Performance Stats##pro", &g_showPerfStats, 380.f, 300.f);
    ImGui::Text("FPS (approx): %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Parts: %d", (int)g_world.parts.size());
    ImGui::Text("Models: %d", (int)g_world.models.size());
    ImGui::Text("Labels/UI: %d", (int)g_world.labels.size());
    ImGui::Text("Shared instances: %d", (int)g_world.instances.size());
    ImGui::Text("Welds: %d", (int)g_world.welds.size());
    ImGui::Text("Lua ok: %s", g_lua.ok ? "yes" : "no");
    ImGui::Text("Net role: %d", (int)g_net.role);
    ImGui::Separator();
    ImGui::TextDisabled("Quality");
    ImGui::Text("Shadows: %s", g_engine.quality.shadows ? "on" : "off");
    ImGui::Text("Lua budget us: %d", g_engine.quality.luaBudgetUs);
    if (ImGui::Button("Force GC note", {-1, 0}))
        proPushHistory("Perf check");
    ImGui::End();
}

// ===========================================================================
// 8. Teams Editor
// ===========================================================================

static void drawTeamsEditorWindow() {
    if (!g_showTeamsEditor) return;
    proBeginWindow("Teams##pro", &g_showTeamsEditor, 400.f, 340.f);
    ImGui::InputText("Team Name", g_teamName, sizeof(g_teamName));
    ImGui::ColorEdit3("Color", g_teamColor);
    if (ImGui::Button("Create Team", {-1, 0})) {
        g_teams.create(g_teamName, g_teamColor[0], g_teamColor[1], g_teamColor[2]);
        proPushHistory("Team created");
    }
    if (ImGui::Button("Join Team", {-1, 0})) {
        g_teams.join(g_teamName);
        proPushHistory("Joined team");
    }
    if (ImGui::Button("Leave Team", {-1, 0}))
        g_teams.leave();
    ImGui::Separator();
    ImGui::Text("Mine: %s", g_teams.mine().c_str());
    for (const auto& t : g_teams.all()) {
        ImGui::ColorButton("##tc", {t.r, t.g, t.b, 1.f}, 0, {14, 14});
        ImGui::SameLine();
        ImGui::Text("%s (%d)", t.name.c_str(), t.members);
    }
    ImGui::End();
}

// ===========================================================================
// 9. Leaderstats Editor
// ===========================================================================

static void drawLeaderstatsEditorWindow() {
    if (!g_showLeaderstatsEditor) return;
    proBeginWindow("leaderstats##pro", &g_showLeaderstatsEditor, 380.f, 300.f);
    ImGui::InputText("Stat", g_statName, sizeof(g_statName));
    static float statVal = 0.f;
    ImGui::InputFloat("Value", &statVal);
    if (ImGui::Button("Set", {-1, 0})) {
        g_classifica.set(g_statName, (double)statVal);
        proPushHistory("leaderstats set");
    }
    ImGui::SameLine();
    if (ImGui::Button("Add +1")) {
        double o = 0;
        g_classifica.add(g_statName, 1.0, o);
        statVal = (float)o;
    }
    ImGui::Separator();
    ImGui::Text("Local: %s", g_classifica.localPlayer().c_str());
    ImGui::TextDisabled("Usa anche: Players.LocalPlayer.leaderstats.Coins.Value");
    ImGui::End();
}

// ===========================================================================
// 10. ProximityPrompt Placer
// ===========================================================================

static void drawPromptPlacerWindow() {
    if (!g_showPromptPlacer) return;
    proBeginWindow("ProximityPrompt##pro", &g_showPromptPlacer, 400.f, 280.f);
    ImGui::InputText("Name", g_promptName, sizeof(g_promptName));
    ImGui::InputText("ActionText", g_promptText, sizeof(g_promptText));
    ImGui::SliderFloat("MaxActivationDistance", &g_promptRadius, 2.f, 32.f);
    glm::vec3 pos = g_playerPos;
    int partIdx = -1;
    if (partIndexValid(g_selectedPart)) {
        pos = g_world.parts[g_selectedPart].position;
        partIdx = g_selectedPart;
        ImGui::Text("Attach to: %s", g_world.parts[g_selectedPart].name.c_str());
    } else {
        ImGui::TextDisabled("Nessuna Part — usera posizione player");
    }
    if (ImGui::Button("Create ProximityPrompt", {-1, 0})) {
        g_prossimita.add(g_promptName, g_promptText, pos.x, pos.y, pos.z, g_promptRadius, partIdx);
        proPushHistory("ProximityPrompt");
        g_exportMsg = std::string("Prompt: ") + g_promptName;
    }
    ImGui::Separator();
    ImGui::Text("Nearest: %s", g_prossimita.nearest().c_str());
    ImGui::Text("Count: %d", g_prossimita.count());
    ImGui::End();
}

// ===========================================================================
// 11. DataStore Browser
// ===========================================================================

static void drawDataStoreBrowserWindow() {
    if (!g_showDataStoreBrowser) return;
    proBeginWindow("DataStore Browser##pro", &g_showDataStoreBrowser, 460.f, 360.f);
    ImGui::InputText("Store", g_dsStoreName, sizeof(g_dsStoreName));
    ImGui::InputText("Key", g_dsKey, sizeof(g_dsKey));
    ImGui::InputText("Value", g_dsValue, sizeof(g_dsValue));
    if (ImGui::Button("GetAsync")) {
        std::string sn = blk::datastore::sanitizeKeyName(g_dsStoreName);
        std::string kn = blk::datastore::sanitizeKeyName(g_dsKey);
        auto& s = blk::datastore::db().store(sn);
        auto it = s.find(kn);
        if (it == s.end()) std::snprintf(g_dsValue, sizeof(g_dsValue), "");
        else if (it->second.isNumber)
            std::snprintf(g_dsValue, sizeof(g_dsValue), "%.10g", it->second.number);
        else
            std::snprintf(g_dsValue, sizeof(g_dsValue), "%s", it->second.text.c_str());
        proPushHistory("DataStore Get");
    }
    ImGui::SameLine();
    if (ImGui::Button("SetAsync")) {
        std::string sn = blk::datastore::sanitizeKeyName(g_dsStoreName);
        std::string kn = blk::datastore::sanitizeKeyName(g_dsKey);
        blk::datastore::Value v;
        char* end = nullptr;
        double num = std::strtod(g_dsValue, &end);
        if (end && *end == 0 && g_dsValue[0]) {
            v.isNumber = true;
            v.number = num;
        } else {
            v.isNumber = false;
            v.text = g_dsValue;
        }
        blk::datastore::db().store(sn)[kn] = v;
        blk::datastore::db().save(sn);
        proPushHistory("DataStore Set");
    }
    ImGui::SameLine();
    if (ImGui::Button("RemoveAsync")) {
        std::string sn = blk::datastore::sanitizeKeyName(g_dsStoreName);
        std::string kn = blk::datastore::sanitizeKeyName(g_dsKey);
        blk::datastore::db().store(sn).erase(kn);
        blk::datastore::db().save(sn);
        proPushHistory("DataStore Remove");
    }
    ImGui::Separator();
    ImGui::TextDisabled("Keys nello store (max 40 mostrate):");
    std::string sn = blk::datastore::sanitizeKeyName(g_dsStoreName);
    auto& s = blk::datastore::db().store(sn);
    int n = 0;
    for (const auto& kv : s) {
        if (n++ >= 40) { ImGui::TextDisabled("…"); break; }
        if (kv.second.isNumber)
            ImGui::BulletText("%s = %.6g", kv.first.c_str(), kv.second.number);
        else
            ImGui::BulletText("%s = \"%s\"", kv.first.c_str(), kv.second.text.c_str());
    }
    ImGui::End();
}

// ===========================================================================
// 12. Lighting Presets
// ===========================================================================

static void drawLightingPresetsWindow() {
    if (!g_showLightingPresets) return;
    proBeginWindow("Lighting Presets##pro", &g_showLightingPresets, 400.f, 340.f);
    const char* presets[] = {
        "Noon", "Sunset", "Night", "Foggy", "Horror", "Cartoon", "Studio Default"
    };
    ImGui::Combo("Preset", &g_lightingPreset, presets, 7);
    if (ImGui::Button("Apply preset", {-1, 0})) {
        switch (g_lightingPreset) {
        case 0: // Noon
            g_lighting.ambient = 0.55f;
            g_lighting.outdoorAmbient = {0.55f, 0.55f, 0.6f};
            g_lighting.clockTime = 12.f;
            break;
        case 1: // Sunset
            g_lighting.ambient = 0.48f;
            g_lighting.outdoorAmbient = {0.7f, 0.45f, 0.3f};
            g_lighting.clockTime = 18.f;
            break;
        case 2: // Night
            g_lighting.ambient = 0.12f;
            g_lighting.outdoorAmbient = {0.08f, 0.1f, 0.18f};
            g_lighting.clockTime = 0.f;
            break;
        case 3: // Foggy
            g_lighting.ambient = 0.4f;
            g_lighting.fogEnd = 120.f;
            break;
        case 4: // Horror
            g_lighting.ambient = 0.15f;
            g_lighting.outdoorAmbient = {0.15f, 0.05f, 0.05f};
            g_lighting.clockTime = 22.f;
            break;
        case 5: // Cartoon
            g_lighting.ambient = 0.75f;
            g_lighting.outdoorAmbient = {0.85f, 0.8f, 0.95f};
            break;
        default:
            g_lighting.ambient = 0.42f;
            g_lighting.clockTime = 14.f;
            break;
        }
        markPlaceDirty();
        proPushHistory("Lighting preset");
    }
    ImGui::SliderFloat("ClockTime", &g_lighting.clockTime, 0.f, 24.f);
    ImGui::SliderFloat("Ambient", &g_lighting.ambient, 0.f, 1.f);
    ImGui::ColorEdit3("OutdoorAmbient", &g_lighting.outdoorAmbient.x);
    ImGui::SliderFloat("FogEnd", &g_lighting.fogEnd, 50.f, 2000.f);
    ImGui::End();
}

// ===========================================================================
// 13. Align / Distribute
// ===========================================================================

static void drawAlignToolsWindow() {
    if (!g_showAlignTools) return;
    proBeginWindow("Align / Distribute##pro", &g_showAlignTools, 360.f, 280.f);
    const char* axes[] = { "X", "Y", "Z" };
    ImGui::Combo("Axis", &g_alignAxis, axes, 3);
    if (ImGui::Button("Align selection to first Part", {-1, 0})) {
        if (partIndexValid(g_selectedPart) && g_world.parts.size() >= 2) {
            float ref = 0.f;
            const auto& a = g_world.parts[g_selectedPart].position;
            ref = g_alignAxis == 0 ? a.x : (g_alignAxis == 1 ? a.y : a.z);
            for (auto& p : g_world.parts) {
                if (g_alignAxis == 0) p.position.x = ref;
                else if (g_alignAxis == 1) p.position.y = ref;
                else p.position.z = ref;
            }
            markPlaceDirty();
            proPushHistory("Align");
        }
    }
    if (ImGui::Button("Distribute along axis", {-1, 0})) {
        if (g_world.parts.size() < 2) { ImGui::End(); return; }
        std::vector<int> idx(g_world.parts.size());
        for (int i = 0; i < (int)idx.size(); ++i) idx[i] = i;
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            const auto& pa = g_world.parts[a].position;
            const auto& pb = g_world.parts[b].position;
            float va = g_alignAxis == 0 ? pa.x : (g_alignAxis == 1 ? pa.y : pa.z);
            float vb = g_alignAxis == 0 ? pb.x : (g_alignAxis == 1 ? pb.y : pb.z);
            return va < vb;
        });
        float a0 = 0, a1 = 0;
        {
            const auto& p0 = g_world.parts[idx.front()].position;
            const auto& p1 = g_world.parts[idx.back()].position;
            a0 = g_alignAxis == 0 ? p0.x : (g_alignAxis == 1 ? p0.y : p0.z);
            a1 = g_alignAxis == 0 ? p1.x : (g_alignAxis == 1 ? p1.y : p1.z);
        }
        for (int i = 0; i < (int)idx.size(); ++i) {
            float t = (idx.size() == 1) ? 0.f : (float)i / (float)(idx.size() - 1);
            float v = a0 + (a1 - a0) * t;
            auto& p = g_world.parts[idx[i]].position;
            if (g_alignAxis == 0) p.x = v;
            else if (g_alignAxis == 1) p.y = v;
            else p.z = v;
        }
        markPlaceDirty();
        proPushHistory("Distribute");
    }
    if (ImGui::Button("Stack +Y (1 stud gap)", {-1, 0})) {
        float y = 0.f;
        for (auto& p : g_world.parts) {
            p.position.y = y + p.size.y * 0.5f;
            y += p.size.y + 1.f;
        }
        markPlaceDirty();
    }
    ImGui::End();
}

// ===========================================================================
// 14. Script Analysis
// ===========================================================================

static void drawScriptAnalysisWindow() {
    if (!g_showScriptAnalysis) return;
    proBeginWindow("Script Analysis##pro", &g_showScriptAnalysis, 520.f, 400.f);
    if (ImGui::Button("Scan place scripts", {-1, 0})) {
        g_scriptAnalysisHits.clear();
        auto check = [&](const std::string& where, const std::string& src) {
            if (src.empty()) return;
            if (src.find("while true do") != std::string::npos && src.find("wait") == std::string::npos
                && src.find("task.wait") == std::string::npos)
                g_scriptAnalysisHits.push_back(where + ": possible infinite loop (no wait)");
            if (src.find("getfenv") != std::string::npos)
                g_scriptAnalysisHits.push_back(where + ": getfenv (blocked in sandbox?)");
            if (src.find("loadstring") != std::string::npos)
                g_scriptAnalysisHits.push_back(where + ": loadstring");
            if (src.size() > 50000)
                g_scriptAnalysisHits.push_back(where + ": very large script");
            if (src.find("FireServer") != std::string::npos && src.find("OnServerEvent") == std::string::npos)
                g_scriptAnalysisHits.push_back(where + ": FireServer without local OnServerEvent (ok if other script)");
        };
        for (int i = 0; i < (int)g_world.parts.size(); ++i)
            check("Part:" + g_world.parts[i].name, g_world.parts[i].script);
        for (int i = 0; i < (int)g_world.instances.size(); ++i)
            check(g_world.instances[i].className + ":" + g_world.instances[i].name,
                  g_world.instances[i].source);
        if (g_scriptAnalysisHits.empty())
            g_scriptAnalysisHits.push_back("Nessun problema ovvio trovato.");
        proPushHistory("Script Analysis");
    }
    ImGui::BeginChild("##sa", {0, -28}, true);
    for (const auto& h : g_scriptAnalysisHits)
        ImGui::TextWrapped("%s", h.c_str());
    ImGui::EndChild();
    ImGui::Text("%d hits", (int)g_scriptAnalysisHits.size());
    ImGui::End();
}

// ===========================================================================
// 15. Device Emulator
// ===========================================================================

static void drawDeviceEmulatorWindow() {
    if (!g_showDeviceEmulator) return;
    proBeginWindow("Device Emulator##pro", &g_showDeviceEmulator, 360.f, 260.f);
    const char* devices[] = {
        "Desktop", "iPhone SE", "iPhone 14", "iPad", "Android Phone", "Android Tablet"
    };
    ImGui::Combo("Device", &g_devicePreset, devices, 6);
    int dw = g_w, dh = g_h;
    switch (g_devicePreset) {
    case 1: dw = 375; dh = 667; break;
    case 2: dw = 390; dh = 844; break;
    case 3: dw = 768; dh = 1024; break;
    case 4: dw = 360; dh = 800; break;
    case 5: dw = 800; dh = 1280; break;
    default: break;
    }
    ImGui::Text("Target safe area: %d x %d", dw, dh);
    ImGui::TextWrapped("In Play, lo UI ScreenGui usa gli anchor; questo pannello aiuta a progettare layout mobile.");
    ImGui::Checkbox("Show touch controls hint", &g_engine.quality.mobile);
    ImGui::End();
}

// ===========================================================================
// 16. Publish Dialog
// ===========================================================================

static void drawPublishDialogWindow() {
    if (!g_showPublishDialog) return;
    proBeginWindow("Publish to Blocky##pro", &g_showPublishDialog, 440.f, 320.f);
    ImGui::InputText("Title", g_publishTitle, sizeof(g_publishTitle));
    ImGui::InputTextMultiline("Description", g_publishDesc, sizeof(g_publishDesc), {-1.f, 80.f});
    ImGui::Text("Parts: %d · Scripts shared: %d", (int)g_world.parts.size(),
                (int)g_world.instances.size());
    if (ImGui::Button("Save Place (local)", {-1, 0})) {
        g_world.placeName = g_publishTitle;
        // trigger existing save if available
        markPlaceDirty();
        proPushHistory("Publish save");
        g_exportMsg = "Place marked dirty — usa Salva Place";
    }
    if (ImGui::Button("Copy place summary", {-1, 0})) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s — %d parts", g_publishTitle, (int)g_world.parts.size());
        ImGui::SetClipboardText(buf);
    }
    ImGui::End();
}

// ===========================================================================
// 17. Soft Shutdown
// ===========================================================================

static void drawSoftShutdownWindow() {
    if (!g_showSoftShutdown) return;
    proBeginWindow("Soft Shutdown##pro", &g_showSoftShutdown, 360.f, 200.f);
    ImGui::TextWrapped("Chiude Test/Play in modo controllato (kick client, salva DataStore).");
    if (ImGui::Button("End Test Session", {-1, 0})) {
        if (g_studioTesting)
            stopStudioTest(glfwGetCurrentContext());
        proPushHistory("Soft shutdown test");
    }
    if (ImGui::Button("Flush DataStores note", {-1, 0}))
        proPushHistory("DataStores already save on SetAsync");
    ImGui::End();
}

// ===========================================================================
// 18. Context Actions
// ===========================================================================

static void drawContextActionsWindow() {
    if (!g_showContextActions) return;
    proBeginWindow("ContextActionService##pro", &g_showContextActions, 400.f, 300.f);
    static char actName[32] = "Sprint";
    ImGui::InputText("Action", actName, sizeof(actName));
    if (ImGui::Button("BindAction (UI mirror)", {-1, 0})) {
        g_contextActionsBound[actName] = true;
        proPushHistory("BindAction");
    }
    if (ImGui::Button("UnbindAction", {-1, 0}))
        g_contextActionsBound.erase(actName);
    ImGui::Separator();
    for (const auto& kv : g_contextActionsBound)
        ImGui::BulletText("%s — bound", kv.first.c_str());
    ImGui::TextDisabled("API Luau: ContextActionService:BindAction");
    ImGui::End();
}

// ===========================================================================
// 19. HttpService Panel
// ===========================================================================

static void drawHttpServicePanel() {
    if (!g_showHttpServicePanel) return;
    proBeginWindow("HttpService##pro", &g_showHttpServicePanel, 400.f, 260.f);
    ImGui::Checkbox("HttpEnabled", &g_httpEnabled);
    ImGui::TextWrapped("JSONEncode / JSONDecode disponibili in Luau (locale). "
                       "Richieste HTTP remote restano disabilitate per sicurezza sandbox.");
    static char jsonIn[256] = "{\"a\":1}";
    static char jsonOut[256] = {};
    ImGui::InputText("JSON sample", jsonIn, sizeof(jsonIn));
    if (ImGui::Button("Echo encode length")) {
        std::snprintf(jsonOut, sizeof(jsonOut), "len=%d", (int)std::strlen(jsonIn));
    }
    ImGui::Text("%s", jsonOut);
    ImGui::End();
}

// ===========================================================================
// 20. Camera Panel
// ===========================================================================

static void drawCameraPanelWindow() {
    if (!g_showCameraPanel) return;
    proBeginWindow("Camera##pro", &g_showCameraPanel, 360.f, 260.f);
    ImGui::SliderFloat("FOV", &g_camFovPro, 40.f, 120.f);
    ImGui::Checkbox("First person", &g_firstPerson);
    if (ImGui::Button("Reset orbit", {-1, 0})) {
        g_orbitYaw = g_playerYaw;
        g_orbitPitch = 20.f;
    }
    ImGui::Text("Player yaw: %.1f", g_playerYaw);
    ImGui::TextDisabled("CameraSubject = Humanoid (default)");
    ImGui::End();
}

// ===========================================================================
// 21. Physics Panel
// ===========================================================================

static void drawPhysicsPanelWindow() {
    if (!g_showPhysicsPanel) return;
    proBeginWindow("Physics Settings##pro", &g_showPhysicsPanel, 380.f, 300.f);
    ImGui::SliderFloat("Gravity (display)", &g_physGravity, 20.f, 200.f);
    ImGui::SliderFloat("Solver iterations (hint)", &g_physSteps, 1.f, 8.f);
    if (partIndexValid(g_selectedPart)) {
        auto& p = g_world.parts[g_selectedPart];
        ImGui::Separator();
        ImGui::Text("Selected: %s", p.name.c_str());
        ImGui::Checkbox("Anchored", &p.anchored);
        ImGui::Checkbox("CanCollide", &p.canCollide);
        ImGui::Checkbox("CanTouch", &p.canTouch);
        ImGui::SliderFloat("Density", &p.density, 0.01f, 100.f);
        ImGui::SliderFloat("Friction", &p.friction, 0.f, 2.f);
        ImGui::SliderFloat("Elasticity", &p.elasticity, 0.f, 1.f);
        if (ImGui::IsItemDeactivatedAfterEdit()) markPlaceDirty();
    }
    ImGui::End();
}

// ===========================================================================
// 22. Spawn Editor
// ===========================================================================

static void drawSpawnEditorWindow() {
    if (!g_showSpawnEditor) return;
    proBeginWindow("SpawnLocation##pro", &g_showSpawnEditor, 360.f, 240.f);
    ImGui::DragFloat3("Spawn Position", &g_world.spawn.position.x, 0.1f);
    ImGui::SliderFloat("Spawn Yaw", &g_world.spawn.yaw, -180.f, 180.f);
    if (ImGui::Button("Set from player", {-1, 0})) {
        g_world.spawn.position = g_playerPos;
        g_world.spawn.yaw = g_playerYaw;
        markPlaceDirty();
    }
    if (ImGui::Button("Teleport player to spawn", {-1, 0}))
        respawnPlayerAtSpawn();
    ImGui::End();
}

// ===========================================================================
// 23–30. Badge, Localization, Package, Streaming, Filter, Skybox, PostFX, History
// ===========================================================================

static void drawBadgePanelWindow() {
    if (!g_showBadgePanel) return;
    proBeginWindow("Badges##pro", &g_showBadgePanel, 360.f, 220.f);
    ImGui::InputText("Badge", g_badgeName, sizeof(g_badgeName));
    if (ImGui::Button("Award (local)", {-1, 0})) {
        g_badge.award(g_badgeName);
        proPushHistory("Badge");
    }
    ImGui::TextDisabled("BadgeService-like locale");
    ImGui::End();
}

static void drawLocalizationWindow() {
    if (!g_showLocalization) return;
    proBeginWindow("Localization##pro", &g_showLocalization, 400.f, 260.f);
    ImGui::InputText("Key", g_localeKey, sizeof(g_localeKey));
    ImGui::InputText("Value", g_localeVal, sizeof(g_localeVal));
    static std::map<std::string, std::string> locale;
    if (ImGui::Button("Set entry"))
        locale[g_localeKey] = g_localeVal;
    for (const auto& kv : locale)
        ImGui::BulletText("%s = %s", kv.first.c_str(), kv.second.c_str());
    ImGui::End();
}

static void drawPackageVersionsWindow() {
    if (!g_showPackageVersions) return;
    proBeginWindow("Packages / Versions##pro", &g_showPackageVersions, 400.f, 240.f);
    ImGui::Text("Place: %s", g_world.placeName.c_str());
    ImGui::Text("BlockyPlace format: v2 (instances[])");
    ImGui::Text("History entries: %d", (int)g_proHistory.size());
    ImGui::TextDisabled("Versioning locale via place saves");
    ImGui::End();
}

static void drawStreamingPanelWindow() {
    if (!g_showStreamingPanel) return;
    proBeginWindow("StreamingEnabled##pro", &g_showStreamingPanel, 360.f, 200.f);
    ImGui::Checkbox("StreamingEnabled", &g_streamingEnabled);
    ImGui::TextWrapped("Quando ON, i client dovrebbero caricare solo chunk vicini. "
                       "In Blocky e' un flag creatore — streaming chunk completo e' roadmap.");
    ImGui::End();
}

static void drawFilterPanelWindow() {
    if (!g_showFilterPanel) return;
    proBeginWindow("FilteringEnabled##pro", &g_showFilterPanel, 360.f, 200.f);
    ImGui::Checkbox("FilteringEnabled", &g_filteringEnabled);
    ImGui::TextWrapped("Con FilteringEnabled, solo il server (Host) applica cambiamenti fisici critici; "
                       "i client usano RemoteEvent. Gia' il modello di rete Blocky.");
    ImGui::End();
}

static void drawSkyboxPickerWindow() {
    if (!g_showSkyboxPicker) return;
    proBeginWindow("Skybox##pro", &g_showSkyboxPicker, 360.f, 240.f);
    ImGui::InputText("Sky id", g_skyboxId, sizeof(g_skyboxId));
    const char* skies[] = { "default", "sunset", "space", "cloudy", "night" };
    for (const char* s : skies)
        if (ImGui::Selectable(s)) std::snprintf(g_skyboxId, sizeof(g_skyboxId), "%s", s);
    ImGui::TextDisabled("Sky applicato a Lighting se supportato dal renderer");
    ImGui::End();
}

static void drawPostFxPanelWindow() {
    if (!g_showPostFxPanel) return;
    proBeginWindow("Post-Processing##pro", &g_showPostFxPanel, 360.f, 240.f);
    ImGui::SliderFloat("Bloom", &g_bloomPro, 0.f, 2.f);
    ImGui::SliderFloat("Saturation", &g_saturationPro, 0.f, 2.f);
    if (ImGui::Button("Apply to lighting bloom", {-1, 0})) {
        g_lighting.atmosphereEnabled = true; g_lighting.sunRaysIntensity = g_bloomPro;
        markPlaceDirty();
    }
    ImGui::End();
}

static void drawHistoryPanelWindow() {
    if (!g_showHistoryPanel) return;
    proBeginWindow("History##pro", &g_showHistoryPanel, 400.f, 360.f);
    if (ImGui::Button("Clear")) g_proHistory.clear();
    ImGui::BeginChild("##hist", {0, -8}, true);
    for (int i = (int)g_proHistory.size() - 1; i >= 0; --i)
        ImGui::Text("%02d  %s", i, g_proHistory[i].c_str());
    ImGui::EndChild();
    ImGui::End();
}


// ===========================================================================
// Emote / Accessory / Collision debug / Network owner
// ===========================================================================

static void drawEmotePanelWindow() {
    if (!g_showEmotePanel) return;
    proBeginWindow("Emotes##pro", &g_showEmotePanel, 340.f, 260.f);
    const char* emotes[] = { "Wave", "Dance", "Point", "Cheer", "Laugh", "Sit" };
    for (const char* e : emotes) {
        if (ImGui::Button(e, {-1, 0})) {
            g_animator.play(e);
            proPushHistory(e);
        }
    }
    ImGui::End();
}

static void drawAccessoryPanelWindow() {
    if (!g_showAccessoryPanel) return;
    proBeginWindow("Accessories##pro", &g_showAccessoryPanel, 360.f, 240.f);
    ImGui::TextWrapped("Apri Avatar Publisher (F8) per accessori completi. Qui: scorciatoie.");
    if (ImGui::Button("Open Avatar Catalog", {-1, 0}))
        enterAvatarPublisher();
    if (ImGui::Button("Rig Player", {-1, 0}))
        beginRigPlayer();
    ImGui::End();
}

static void drawCollisionDebugWindow() {
    if (!g_showCollisionDebug) return;
    proBeginWindow("Collision Debug##pro", &g_showCollisionDebug, 400.f, 280.f);
    int anch = 0, col = 0, touch = 0;
    for (const auto& p : g_world.parts) {
        if (p.anchored) ++anch;
        if (p.canCollide) ++col;
        if (p.canTouch) ++touch;
    }
    ImGui::Text("Anchored: %d", anch);
    ImGui::Text("CanCollide: %d", col);
    ImGui::Text("CanTouch: %d", touch);
    if (ImGui::Button("Anchor all", {-1, 0})) {
        for (auto& p : g_world.parts) p.anchored = true;
        markPlaceDirty();
    }
    if (ImGui::Button("Unanchor non-baseplate", {-1, 0})) {
        for (auto& p : g_world.parts)
            if (p.name.find("Baseplate") == std::string::npos) p.anchored = false;
        markPlaceDirty();
    }
    ImGui::End();
}

static void drawNetworkOwnerWindow() {
    if (!g_showNetworkOwner) return;
    proBeginWindow("Network Ownership##pro", &g_showNetworkOwner, 400.f, 240.f);
    ImGui::Text("LocalId: %d", (int)g_net.localId);
    ImGui::Text("Role: %s",
        g_net.role == NetRole::Host ? "Host" :
        g_net.role == NetRole::Client ? "Client" : "Offline");
    ImGui::TextWrapped("In Studio Test offline, sei Host+Client. "
                       "SetNetworkOwner non e' esposto: ownership = Host per Parts.");
    ImGui::End();
}

// ===========================================================================
// Toolbox Pro + Command Palette + Quick Actions
// ===========================================================================

static void drawToolboxProWindow() {
    if (!g_showToolboxPro) return;
    proBeginWindow("Toolbox Pro##pro", &g_showToolboxPro, 320.f, 480.f);
    ImGui::InputText("Search", g_proSearch, sizeof(g_proSearch));
    ImGui::Separator();
    struct ToolItem { const char* name; void (*fn)(); };
    auto go = [](const char* n, bool* flag) {
        if (proFilterMatch(n, g_proSearch) && ImGui::Selectable(n)) *flag = true;
    };
    go("Game Settings", &g_showGameSettings);
    go("Attributes", &g_showAttributesEditor);
    go("Tags", &g_showTagsEditor);
    go("Sound Browser", &g_showSoundBrowser);
    go("Material Manager", &g_showMaterialManager);
    go("Insert Object", &g_showProInsertObject);
    go("Performance", &g_showPerfStats);
    go("Teams", &g_showTeamsEditor);
    go("leaderstats", &g_showLeaderstatsEditor);
    go("ProximityPrompt", &g_showPromptPlacer);
    go("DataStore", &g_showDataStoreBrowser);
    go("Lighting Presets", &g_showLightingPresets);
    go("Align Tools", &g_showAlignTools);
    go("Script Analysis", &g_showScriptAnalysis);
    go("Device Emulator", &g_showDeviceEmulator);
    go("Publish", &g_showPublishDialog);
    go("Soft Shutdown", &g_showSoftShutdown);
    go("Context Actions", &g_showContextActions);
    go("HttpService", &g_showHttpServicePanel);
    go("Camera", &g_showCameraPanel);
    go("Physics", &g_showPhysicsPanel);
    go("Spawn", &g_showSpawnEditor);
    go("Badges", &g_showBadgePanel);
    go("Localization", &g_showLocalization);
    go("Packages", &g_showPackageVersions);
    go("Streaming", &g_showStreamingPanel);
    go("Filtering", &g_showFilterPanel);
    go("Skybox", &g_showSkyboxPicker);
    go("PostFX", &g_showPostFxPanel);
    go("History", &g_showHistoryPanel);
    go("Emotes", &g_showEmotePanel);
    go("Accessories", &g_showAccessoryPanel);
    go("Collision Debug", &g_showCollisionDebug);
    go("Network Owner", &g_showNetworkOwner);
    ImGui::End();
}

static void drawCommandPaletteWindow() {
    if (!g_showCommandPalette) return;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.35f});
    proBeginWindow("Command Palette##pro", &g_showCommandPalette, 480.f, 320.f);
    ImGui::SetKeyboardFocusHere();
    bool go = ImGui::InputText("##cmd", g_cmdPalette, sizeof(g_cmdPalette),
                               ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::TextDisabled("Digita: part, play, terrain, weld, teams, datastore, prompt…");
    auto run = [&](const char* cmd) {
        std::string c = cmd;
        for (char& ch : c) ch = (char)std::tolower((unsigned char)ch);
        if (c == "part") insertNewPart();
        else if (c == "play") startStudioTest(glfwGetCurrentContext());
        else if (c == "stop") { if (g_studioTesting) stopStudioTest(glfwGetCurrentContext()); }
        else if (c == "terrain") g_showTerrainEditor = true;
        else if (c == "weld") studioCreateWeldFromSelection();
        else if (c == "teams") g_showTeamsEditor = true;
        else if (c == "datastore") g_showDataStoreBrowser = true;
        else if (c == "prompt") g_showPromptPlacer = true;
        else if (c == "insert") g_showProInsertObject = true;
        else if (c == "settings") g_showGameSettings = true;
        else if (c == "material") g_showMaterialManager = true;
        else if (c == "analyze") g_showScriptAnalysis = true;
        else if (c == "publish") g_showPublishDialog = true;
        else g_exportMsg = std::string("Unknown command: ") + cmd;
        g_showCommandPalette = false;
        g_cmdPalette[0] = 0;
        proPushHistory(cmd);
    };
    if (go && g_cmdPalette[0]) run(g_cmdPalette);
    const char* sugg[] = { "part", "play", "stop", "terrain", "weld", "teams",
                           "datastore", "prompt", "insert", "settings", "material",
                           "analyze", "publish" };
    for (const char* s : sugg) {
        if (proFilterMatch(s, g_cmdPalette) && ImGui::Selectable(s))
            run(s);
    }
    ImGui::End();
}

static void drawQuickActionsWindow() {
    if (!g_showQuickActions) return;
    proBeginWindow("Quick Actions##pro", &g_showQuickActions, 300.f, 400.f);
    if (ImGui::Button("Part", {-1, 0})) insertNewPart();
    if (ImGui::Button("SpawnLocation", {-1, 0})) {
        insertNewPart();
        if (partIndexValid(g_selectedPart)) {
            g_world.parts[g_selectedPart].name = "SpawnLocation";
            g_world.parts[g_selectedPart].color = {0.2f, 0.85f, 0.35f};
            g_world.spawn.position = g_world.parts[g_selectedPart].position;
        }
    }
    if (ImGui::Button("Kill brick (red)", {-1, 0})) {
        insertNewPart();
        if (partIndexValid(g_selectedPart)) {
            auto& p = g_world.parts[g_selectedPart];
            p.name = "KillBrick";
            p.color = {0.9f, 0.1f, 0.1f};
            p.script =
                "script.Parent.Touched:Connect(function()\n"
                "  Humanoid:TakeDamage(100)\n"
                "end)\n";
            p.scriptType = ScriptType::Server;
        }
    }
    if (ImGui::Button("Checkpoint", {-1, 0})) {
        insertNewPart();
        if (partIndexValid(g_selectedPart)) {
            auto& p = g_world.parts[g_selectedPart];
            p.name = "Checkpoint";
            p.color = {0.2f, 0.5f, 1.f};
            p.script =
                "script.Parent.Touched:Connect(function()\n"
                "  print(\"checkpoint\")\n"
                "end)\n";
        }
    }
    if (ImGui::Button("Coin (+1 leaderstat)", {-1, 0})) {
        insertNewPart();
        if (partIndexValid(g_selectedPart)) {
            auto& p = g_world.parts[g_selectedPart];
            p.name = "Coin";
            p.color = {1.f, 0.85f, 0.1f};
            p.size = {1.5f, 1.5f, 1.5f};
            p.script =
                "script.Parent.Touched:Connect(function()\n"
                "  local ls = game.Players.LocalPlayer.leaderstats\n"
                "  ls.Coins.Value = (ls.Coins.Value or 0) + 1\n"
                "  script.Parent:Destroy()\n"
                "end)\n";
            p.scriptType = ScriptType::Client;
        }
    }
    if (ImGui::Button("Door + Prompt", {-1, 0})) {
        insertNewPart();
        if (partIndexValid(g_selectedPart)) {
            auto& p = g_world.parts[g_selectedPart];
            p.name = "Door";
            p.size = {1.f, 8.f, 4.f};
            g_prossimita.add("DoorPrompt", "Open", p.position.x, p.position.y, p.position.z, 10.f, g_selectedPart);
        }
    }
    if (ImGui::Button("Module + Remote kit", {-1, 0})) {
        auto& m = g_world.addShared(SharedService::ReplicatedStorage, "ModuleScript", "GameModule");
        m.source = "local M = {}\nfunction M.ping() return \"pong\" end\nreturn M\n";
        g_world.addShared(SharedService::ReplicatedStorage, "RemoteEvent", "Remotes");
        g_world.addShared(SharedService::ReplicatedStorage, "RemoteFunction", "GetData");
        markPlaceDirty();
        proPushHistory("Module kit");
    }
    ImGui::End();
}

// ===========================================================================
// Batch rename / recolor / resize tools
// ===========================================================================

static char g_batchPrefix[32] = "Part";
static float g_batchColor[3] = {0.64f, 0.64f, 0.64f};
static float g_batchScale = 1.f;

static void drawBatchToolsInline() {
    proSepTitle("Batch");
    ImGui::InputText("Rename prefix", g_batchPrefix, sizeof(g_batchPrefix));
    if (ImGui::Button("Rename all Parts", {-1, 0})) {
        for (int i = 0; i < (int)g_world.parts.size(); ++i) {
            char n[48];
            std::snprintf(n, sizeof(n), "%s%d", g_batchPrefix, i + 1);
            g_world.parts[i].name = n;
        }
        markPlaceDirty();
    }
    ImGui::ColorEdit3("Recolor", g_batchColor);
    if (ImGui::Button("Recolor all", {-1, 0})) {
        for (auto& p : g_world.parts)
            p.color = {g_batchColor[0], g_batchColor[1], g_batchColor[2]};
        markPlaceDirty();
    }
    ImGui::SliderFloat("Scale all", &g_batchScale, 0.25f, 4.f);
    if (ImGui::Button("Apply scale", {-1, 0})) {
        for (auto& p : g_world.parts) p.size *= g_batchScale;
        g_batchScale = 1.f;
        markPlaceDirty();
    }
}

static void drawBatchToolsWindow() {
    if (!g_showBatchTools) return;
    proBeginWindow("Batch Tools##pro", &g_showBatchTools, 360.f, 300.f);
    drawBatchToolsInline();
    ImGui::End();
}

// ===========================================================================
// Explorer filter / selection sets
// ===========================================================================

static char g_explorerFilterPro[64] = {};
static std::vector<int> g_selectionSet;

static void drawSelectionSetsWindow() {
    if (!g_showSelectionSets) return;
    proBeginWindow("Selection Sets##pro", &g_showSelectionSets, 360.f, 320.f);
    ImGui::InputText("Name filter", g_explorerFilterPro, sizeof(g_explorerFilterPro));
    if (ImGui::Button("Capture current selection", {-1, 0})) {
        g_selectionSet.clear();
        if (partIndexValid(g_selectedPart)) g_selectionSet.push_back(g_selectedPart);
    }
    if (ImGui::Button("Add selection to set", {-1, 0}) && partIndexValid(g_selectedPart)) {
        if (std::find(g_selectionSet.begin(), g_selectionSet.end(), g_selectedPart) == g_selectionSet.end())
            g_selectionSet.push_back(g_selectedPart);
    }
    if (ImGui::Button("Select set (first)", {-1, 0}) && !g_selectionSet.empty())
        g_selectedPart = g_selectionSet.front();
    if (ImGui::Button("Delete set Parts", {-1, 0})) {
        std::sort(g_selectionSet.begin(), g_selectionSet.end(), std::greater<int>());
        for (int i : g_selectionSet) {
            if (i >= 0 && i < (int)g_world.parts.size()) {
                g_world.parts.erase(g_world.parts.begin() + i);
                partconstraints::onPartErased(g_world, i);
            }
        }
        g_selectionSet.clear();
        g_selectedPart = -1;
        markPlaceDirty();
    }
    ImGui::Separator();
    ImGui::Text("Set (%d):", (int)g_selectionSet.size());
    for (int i : g_selectionSet) {
        if (i >= 0 && i < (int)g_world.parts.size())
            ImGui::BulletText("[%d] %s", i, g_world.parts[i].name.c_str());
    }
    ImGui::Separator();
    ImGui::TextDisabled("Filtered parts:");
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        if (!proFilterMatch(g_world.parts[i].name.c_str(), g_explorerFilterPro)) continue;
        if (ImGui::Selectable(g_world.parts[i].name.c_str(), g_selectedPart == i))
            g_selectedPart = i;
    }
    ImGui::End();
}

// ===========================================================================
// Grid / Snap / Studio prefs
// ===========================================================================

static float g_proGridSize = 1.f;
static bool g_proShowGrid = true;
static bool g_proSnap = true;

static void drawStudioPrefsWindow() {
    if (!g_showStudioPrefs) return;
    proBeginWindow("Studio Preferences##pro", &g_showStudioPrefs, 380.f, 280.f);
    ImGui::Checkbox("Show grid", &g_proShowGrid);
    ImGui::Checkbox("Snap to grid", &g_proSnap);
    ImGui::SliderFloat("Grid size", &g_proGridSize, 0.1f, 8.f);
    ImGui::Checkbox("Performance window", &g_showPerfStats);
    ImGui::Checkbox("History window", &g_showHistoryPanel);
    if (ImGui::Button("Reset UI layout hints", {-1, 0}))
        proPushHistory("UI layout reset (visual only)");
    ImGui::End();
}

// ===========================================================================
// Output helpers / notifications strip
// ===========================================================================

static void drawNotifCenterWindow() {
    if (!g_showNotifCenter) return;
    proBeginWindow("Notification Center##pro", &g_showNotifCenter, 400.f, 300.f);
    if (ImGui::Button("Test notification"))
        g_notifiche.push("Hello from Notification Center", 3.f);
    ImGui::TextDisabled("Usa Notifiche / NotificationService da Luau");
    ImGui::End();
}

// ===========================================================================
// Terrain extras / sea level
// ===========================================================================

static float g_seaLevel = 0.f;
static void drawTerrainExtrasWindow() {
    if (!g_showTerrainExtras) return;
    proBeginWindow("Terrain Extras##pro", &g_showTerrainExtras, 360.f, 240.f);
    ImGui::SliderFloat("Sea level (hint)", &g_seaLevel, -20.f, 40.f);
    if (ImGui::Button("Open Terrain Editor", {-1, 0}))
        g_showTerrainEditor = true;
    ImGui::TextWrapped("Sea level e' un riferimento creatore; collega a water volume se disponibile.");
    ImGui::End();
}

// ===========================================================================
// Humanoid defaults
// ===========================================================================

static void drawHumanoidDefaultsWindow() {
    if (!g_showHumanoidDefaults) return;
    proBeginWindow("Humanoid Defaults##pro", &g_showHumanoidDefaults, 380.f, 300.f);
    ImGui::SliderFloat("MaxHealth", &g_player.humanoid.maxHealth, 1.f, 500.f);
    ImGui::SliderFloat("Health", &g_player.humanoid.health, 0.f, g_player.humanoid.maxHealth);
    ImGui::SliderFloat("WalkSpeed", &g_player.humanoid.walkSpeed, 4.f, 50.f);
    ImGui::SliderFloat("JumpPower", &g_player.humanoid.jumpPower, 0.f, 100.f);
    if (ImGui::Button("Apply as script overrides", {-1, 0})) {
        scriptControls().walkSpeedOverride = g_player.humanoid.walkSpeed;
        scriptControls().jumpPowerOverride = g_player.humanoid.jumpPower;
    }
    if (ImGui::Button("Kill (test Died)", {-1, 0}))
        g_player.humanoid.takeDamage(9999.f);
    if (ImGui::Button("Respawn", {-1, 0}))
        respawnPlayerAtSpawn();
    ImGui::Text("State: %s", g_player.humanoid.getState());
    ImGui::End();
}

// ===========================================================================
// Remote / Module kit browser
// ===========================================================================

static void drawRemotesBrowserWindow() {
    if (!g_showRemotesBrowser) return;
    proBeginWindow("Remotes & Modules##pro", &g_showRemotesBrowser, 440.f, 360.f);
    for (int i = 0; i < (int)g_world.instances.size(); ++i) {
        const auto& s = g_world.instances[i];
        ImGui::BulletText("[%s] %s (%s)",
            s.service == SharedService::ServerStorage ? "SS" :
            s.service == SharedService::ServerScriptService ? "SSS" : "RS",
            s.name.c_str(), s.className.c_str());
    }
    if (g_world.instances.empty())
        ImGui::TextDisabled("Nessuna instance — usa Insert Object o Quick Actions");
    if (ImGui::Button("Add RemoteEvent to RS", {-1, 0})) {
        g_world.addShared(SharedService::ReplicatedStorage, "RemoteEvent",
            "Remote" + std::to_string(g_world.instances.size() + 1));
        markPlaceDirty();
    }
    if (ImGui::Button("Add ModuleScript to RS", {-1, 0})) {
        auto& m = g_world.addShared(SharedService::ReplicatedStorage, "ModuleScript",
            "Module" + std::to_string(g_world.instances.size() + 1));
        m.source = "local M = {}\nreturn M\n";
        markPlaceDirty();
    }
    ImGui::End();
}

// ===========================================================================
// Menus / Ribbon / Overlays / Hotkeys
// ===========================================================================

static void drawStudioProViewMenu() {
    ImGui::Separator();
    ImGui::TextDisabled("Studio Pro");
    ImGui::MenuItem("Toolbox Pro…", nullptr, &g_showToolboxPro);
    ImGui::MenuItem("Command Palette…", "Ctrl+P", &g_showCommandPalette);
    ImGui::MenuItem("Quick Actions…", nullptr, &g_showQuickActions);
    ImGui::MenuItem("Insert Object…", nullptr, &g_showProInsertObject);
    ImGui::MenuItem("Game Settings…", nullptr, &g_showGameSettings);
    ImGui::MenuItem("Attributes…", nullptr, &g_showAttributesEditor);
    ImGui::MenuItem("Tags…", nullptr, &g_showTagsEditor);
    ImGui::MenuItem("Material Manager…", nullptr, &g_showMaterialManager);
    ImGui::MenuItem("Sound Browser…", nullptr, &g_showSoundBrowser);
    ImGui::MenuItem("DataStore Browser…", nullptr, &g_showDataStoreBrowser);
    ImGui::MenuItem("Teams…", nullptr, &g_showTeamsEditor);
    ImGui::MenuItem("leaderstats…", nullptr, &g_showLeaderstatsEditor);
    ImGui::MenuItem("ProximityPrompt…", nullptr, &g_showPromptPlacer);
    ImGui::MenuItem("Lighting Presets…", nullptr, &g_showLightingPresets);
    ImGui::MenuItem("Align / Distribute…", nullptr, &g_showAlignTools);
    ImGui::MenuItem("Script Analysis…", nullptr, &g_showScriptAnalysis);
    ImGui::MenuItem("Device Emulator…", nullptr, &g_showDeviceEmulator);
    ImGui::MenuItem("Publish…", nullptr, &g_showPublishDialog);
    ImGui::MenuItem("Performance…", nullptr, &g_showPerfStats);
    ImGui::MenuItem("Camera…", nullptr, &g_showCameraPanel);
    ImGui::MenuItem("Physics…", nullptr, &g_showPhysicsPanel);
    ImGui::MenuItem("Spawn…", nullptr, &g_showSpawnEditor);
    ImGui::MenuItem("Humanoid Defaults…", nullptr, &g_showHumanoidDefaults);
    ImGui::MenuItem("Remotes & Modules…", nullptr, &g_showRemotesBrowser);
    ImGui::MenuItem("Batch Tools…", nullptr, &g_showBatchTools);
    ImGui::MenuItem("Selection Sets…", nullptr, &g_showSelectionSets);
    ImGui::MenuItem("Studio Preferences…", nullptr, &g_showStudioPrefs);
    ImGui::MenuItem("History…", nullptr, &g_showHistoryPanel);
    ImGui::MenuItem("Color Palette…", nullptr, &g_showColorPalette);
    ImGui::MenuItem("BrickColor…", nullptr, &g_showBrickColor);
    ImGui::MenuItem("Place Stats…", nullptr, &g_showPartCounter);
    ImGui::MenuItem("Script Templates…", nullptr, &g_showProScriptTemplates);
    ImGui::MenuItem("Security…", nullptr, &g_showSecurityPanel);
    ImGui::MenuItem("Game Kits…", nullptr, &g_showKitGenerators);
    ImGui::MenuItem("Mirror / Flip…", nullptr, &g_showMirrorTools);
    ImGui::MenuItem("Plugin Bar", nullptr, &g_showPluginBar);
    ImGui::MenuItem("Pivot Tools…", nullptr, &g_showPivotTools);
    ImGui::MenuItem("Bulk Scripts…", nullptr, &g_showBulkScripts);
    ImGui::MenuItem("Place Validator…", nullptr, &g_showPlaceValidator);
    ImGui::MenuItem("Hotkeys…", nullptr, &g_showHotkeyCheatsheet);
}

static void drawStudioProEditMenu() {
    ImGui::Separator();
    ImGui::TextDisabled("Studio Pro");
    if (ImGui::MenuItem("Insert Object…")) g_showProInsertObject = true;
    if (ImGui::MenuItem("Quick Actions…")) g_showQuickActions = true;
    if (ImGui::MenuItem("Align Tools…")) g_showAlignTools = true;
    if (ImGui::MenuItem("Batch Tools…")) g_showBatchTools = true;
}

static void drawStudioProRibbonExtras() {
    if (studio::ribbonBigButton("ins", studio::Icon::NewFile, "Insert", g_showProInsertObject))
        g_showProInsertObject = !g_showProInsertObject;
    if (studio::ribbonBigButton("cmd", studio::Icon::Folder, "Cmd", g_showCommandPalette))
        g_showCommandPalette = !g_showCommandPalette;
    if (studio::ribbonBigButton("qa", studio::Icon::Play, "Quick", g_showQuickActions))
        g_showQuickActions = !g_showQuickActions;
    if (studio::ribbonBigButton("attr", studio::Icon::Script, "Attr", g_showAttributesEditor))
        g_showAttributesEditor = !g_showAttributesEditor;
    if (studio::ribbonBigButton("gs", studio::Icon::Package, "Settings", g_showGameSettings))
        g_showGameSettings = !g_showGameSettings;
}

static void drawStudioProOverlaysExtra();
static void drawStudioProOverlays() {
    drawGameSettingsWindow();
    drawAttributesEditorWindow();
    drawTagsEditorWindow();
    drawSoundBrowserWindow();
    drawMaterialManagerWindow();
    drawInsertObjectWindow();
    drawPerfStatsWindow();
    drawTeamsEditorWindow();
    drawLeaderstatsEditorWindow();
    drawPromptPlacerWindow();
    drawDataStoreBrowserWindow();
    drawLightingPresetsWindow();
    drawAlignToolsWindow();
    drawScriptAnalysisWindow();
    drawDeviceEmulatorWindow();
    drawPublishDialogWindow();
    drawSoftShutdownWindow();
    drawContextActionsWindow();
    drawHttpServicePanel();
    drawCameraPanelWindow();
    drawPhysicsPanelWindow();
    drawSpawnEditorWindow();
    drawBadgePanelWindow();
    drawLocalizationWindow();
    drawPackageVersionsWindow();
    drawStreamingPanelWindow();
    drawFilterPanelWindow();
    drawSkyboxPickerWindow();
    drawPostFxPanelWindow();
    drawHistoryPanelWindow();
    drawEmotePanelWindow();
    drawAccessoryPanelWindow();
    drawCollisionDebugWindow();
    drawNetworkOwnerWindow();
    drawToolboxProWindow();
    drawCommandPaletteWindow();
    drawQuickActionsWindow();
    drawBatchToolsWindow();
    drawSelectionSetsWindow();
    drawStudioPrefsWindow();
    drawNotifCenterWindow();
    drawTerrainExtrasWindow();
    drawHumanoidDefaultsWindow();
    drawRemotesBrowserWindow();
    drawStudioProOverlaysExtra();
}

static void studioProPollHotkeys() {
    if (g_studioTesting) return;
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) return;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P) && !io.KeyShift)
        g_showCommandPalette = true;
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_I))
        g_showProInsertObject = true;
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_G))
        g_showGameSettings = true;
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A))
        g_showAttributesEditor = true;
}


// ===========================================================================
// Extra Studio Pro panels (fill feature surface for creators)
// ===========================================================================

static float g_paletteColor[3] = {0.64f, 0.64f, 0.64f};
static void drawColorPaletteWindow() {
    if (!g_showColorPalette) return;
    proBeginWindow("Color Palette##pro", &g_showColorPalette, 340.f, 280.f);
    ImGui::ColorPicker3("##cp", g_paletteColor);
    if (ImGui::Button("Apply to selection", {-1, 0}) && partIndexValid(g_selectedPart)) {
        g_world.parts[g_selectedPart].color = {g_paletteColor[0], g_paletteColor[1], g_paletteColor[2]};
        markPlaceDirty();
    }
    const char* named[] = {
        "White", "Black", "Red", "Green", "Blue", "Yellow", "Orange", "Pink", "Cyan", "Brown"
    };
    const float cols[][3] = {
        {1,1,1},{0.1f,0.1f,0.1f},{0.9f,0.15f,0.15f},{0.2f,0.75f,0.25f},{0.2f,0.45f,0.95f},
        {1.f,0.9f,0.15f},{1.f,0.55f,0.1f},{1.f,0.45f,0.75f},{0.2f,0.9f,0.95f},{0.45f,0.3f,0.15f}
    };
    for (int i = 0; i < 10; ++i) {
        if (ImGui::ColorButton(named[i], {cols[i][0], cols[i][1], cols[i][2], 1.f})) {
            g_paletteColor[0] = cols[i][0]; g_paletteColor[1] = cols[i][1]; g_paletteColor[2] = cols[i][2];
        }
        if ((i % 5) != 4) ImGui::SameLine();
    }
    ImGui::End();
}

static void drawBrickColorWindow() {
    if (!g_showBrickColor) return;
    proBeginWindow("BrickColor##pro", &g_showBrickColor, 360.f, 300.f);
    ImGui::TextWrapped("BrickColor stile Roblox — applica nome colore alla Part.");
    const char* bricks[] = {
        "Bright red", "Bright blue", "Bright green", "Bright yellow",
        "Institutional white", "Really black", "Deep orange", "Hot pink",
        "Cyan", "Lime green", "Royal purple", "Gold"
    };
    for (const char* b : bricks) {
        if (ImGui::Selectable(b) && partIndexValid(g_selectedPart)) {
            // Map roughly
            glm::vec3 c{0.64f};
            if (std::strstr(b, "red")) c = {0.9f, 0.15f, 0.15f};
            else if (std::strstr(b, "blue")) c = {0.15f, 0.4f, 0.95f};
            else if (std::strstr(b, "green")) c = {0.2f, 0.8f, 0.25f};
            else if (std::strstr(b, "yellow") || std::strstr(b, "Gold")) c = {1.f, 0.85f, 0.1f};
            else if (std::strstr(b, "white")) c = {0.95f, 0.95f, 0.97f};
            else if (std::strstr(b, "black")) c = {0.08f, 0.08f, 0.1f};
            else if (std::strstr(b, "orange")) c = {1.f, 0.5f, 0.1f};
            else if (std::strstr(b, "pink")) c = {1.f, 0.4f, 0.7f};
            else if (std::strstr(b, "Cyan")) c = {0.2f, 0.9f, 0.95f};
            else if (std::strstr(b, "purple")) c = {0.55f, 0.25f, 0.85f};
            g_world.parts[g_selectedPart].color = c;
            markPlaceDirty();
        }
    }
    ImGui::End();
}

static void drawPartCounterWindow() {
    if (!g_showPartCounter) return;
    proBeginWindow("Place Stats##pro", &g_showPartCounter, 320.f, 240.f);
    ImGui::Text("Parts: %d", (int)g_world.parts.size());
    ImGui::Text("Models: %d", (int)g_world.models.size());
    ImGui::Text("UI labels: %d", (int)g_world.labels.size());
    ImGui::Text("Instances: %d", (int)g_world.instances.size());
    ImGui::Text("Welds: %d", (int)g_world.welds.size());
    ImGui::Text("Backpack items: %d", (int)g_world.backpack.size());
    float vol = 0.f;
    for (const auto& p : g_world.parts)
        vol += p.size.x * p.size.y * p.size.z;
    ImGui::Text("Approx volume: %.1f", vol);
    ImGui::End();
}

static void drawProScriptTemplatesWindow() {
    if (!g_showProScriptTemplates) return;
    proBeginWindow("Script Templates##pro", &g_showProScriptTemplates, 440.f, 360.f);
    struct Tmpl { const char* name; const char* code; };
    static const Tmpl tm[] = {
        {"Hello", "print(\"Hello from\", script.Parent and script.Parent.Name or \"?\")\n"},
        {"Touched damage",
         "script.Parent.Touched:Connect(function()\n  Humanoid:TakeDamage(10)\nend)\n"},
        {"Heartbeat spin",
         "game:GetService(\"RunService\").Heartbeat:Connect(function(dt)\n"
         "  local p = script.Parent\n  if p then p.Orientation = p.Orientation + Vector3.new(0, 90*dt, 0) end\nend)\n"},
        {"Remote listen",
         "local re = game.ReplicatedStorage:WaitForChild(\"Remotes\")\n"
         "re.OnServerEvent:Connect(function(player, payload)\n  print(player, payload)\nend)\n"},
        {"DataStore coins",
         "local ds = game:GetService(\"DataStoreService\"):GetDataStore(\"PlayerData\")\n"
         "local v = ds:GetAsync(\"coins\") or 0\nprint(\"coins\", v)\nds:SetAsync(\"coins\", v)\n"},
        {"Proximity",
         "local pp = Instance.new(\"ProximityPrompt\", script.Parent)\n"
         "pp.Triggered:Connect(function(name)\n  print(\"triggered\", name)\nend)\n"},
        {"leaderstats",
         "local ls = game.Players.LocalPlayer.leaderstats\nls.Coins.Value = (ls.Coins.Value or 0) + 1\n"},
        {"Module require",
         "local M = require(game.ReplicatedStorage:WaitForChild(\"GameModule\"))\nprint(M)\n"},
    };
    for (const auto& t : tm) {
        if (ImGui::Button(t.name, {-1, 0}) && partIndexValid(g_selectedPart)) {
            g_world.parts[g_selectedPart].script = t.code;
            g_world.parts[g_selectedPart].scriptType = ScriptType::Server;
            markPlaceDirty();
            g_exportMsg = std::string("Template: ") + t.name;
            proPushHistory(t.name);
        }
    }
    ImGui::End();
}

static void drawMobilePreviewWindow() {
    if (!g_showMobilePreview) return;
    proBeginWindow("Mobile Preview##pro", &g_showMobilePreview, 360.f, 220.f);
    ImGui::Checkbox("Mobile quality", &g_engine.quality.mobile);
    ImGui::TextWrapped("Attiva layout touch e riduce ombre. Usa Device Emulator per risoluzioni.");
    if (ImGui::Button("Open Device Emulator", {-1, 0}))
        g_showDeviceEmulator = true;
    ImGui::End();
}

static void drawSecurityPanelWindow() {
    if (!g_showSecurityPanel) return;
    proBeginWindow("Security##pro", &g_showSecurityPanel, 400.f, 260.f);
    ImGui::Checkbox("FilteringEnabled", &g_filteringEnabled);
    ImGui::Checkbox("HttpEnabled", &g_httpEnabled);
    ImGui::TextWrapped("Script sandbox: budget Heartbeat, niente loadstring, DataStore sanitizzato.");
    ImGui::Text("Lua budget us: %d", g_engine.quality.luaBudgetUs);
    ImGui::End();
}

static void drawAnimIdPanelWindow() {
    if (!g_showAnimIdPanel) return;
    proBeginWindow("AnimationId##pro", &g_showAnimIdPanel, 360.f, 220.f);
    if (ImGui::Button("Open Anim Publisher", {-1, 0}))
        g_showPlayerAnimPublisher = true;
    if (ImGui::Button("Play Idle", {-1, 0})) g_animator.play("Idle");
    if (ImGui::Button("Play Walk", {-1, 0})) g_animator.play("Walk");
    if (ImGui::Button("Stop", {-1, 0})) g_animator.stop();
    ImGui::End();
}

static void drawToolGripPanelWindow() {
    if (!g_showToolGripPanel) return;
    proBeginWindow("Tool Grip##pro", &g_showToolGripPanel, 360.f, 240.f);
    ImGui::Text("Backpack items: %d", (int)g_world.backpack.size());
    ImGui::TextWrapped("Modifica grip dal pannello StarterPack / Tool editor esistente.");
    ImGui::End();
}

static void drawChatSettingsWindow() {
    if (!g_showChatSettings) return;
    proBeginWindow("Chat Settings##pro", &g_showChatSettings, 360.f, 200.f);
    ImGui::TextWrapped("Chat in-game: menu Chat. GetService(\"Chat\") disponibile.");
    ImGui::End();
}

static void drawFriendPanelWindow() {
    if (!g_showFriendPanel) return;
    proBeginWindow("Friends##pro", &g_showFriendPanel, 360.f, 220.f);
    ImGui::Text("Friends in place: %d", (int)g_world.friends.size());
    for (const auto& f : g_world.friends)
        ImGui::BulletText("%s", f.name.c_str());
    ImGui::End();
}

// ===========================================================================
// Macro tools: obby kit / tycoon kit / FPS kit (generators)
// ===========================================================================

static void drawStudioProOverlaysExtra2();
static void drawStudioProOverlaysExtra3();

static void proKitObby() {
    // Baseplate already assumed; add stages
    for (int i = 0; i < 8; ++i) {
        WorkspacePart p;
        p.name = "ObbyStep" + std::to_string(i + 1);
        p.position = { (float)i * 8.f, 1.f + (float)(i % 3) * 2.f, 0.f };
        p.size = { 6.f, 1.f, 6.f };
        p.color = { 0.2f + 0.08f * i, 0.6f, 0.9f };
        g_world.parts.push_back(std::move(p));
    }
    WorkspacePart kill;
    kill.name = "Lava";
    kill.position = { 28.f, 0.5f, 0.f };
    kill.size = { 20.f, 1.f, 20.f };
    kill.color = { 0.9f, 0.2f, 0.05f };
    kill.script = "script.Parent.Touched:Connect(function()\n  Humanoid:TakeDamage(100)\nend)\n";
    kill.scriptType = ScriptType::Server;
    g_world.parts.push_back(std::move(kill));
    markPlaceDirty();
    proPushHistory("Obby kit");
}

static void proKitTycoon() {
    WorkspacePart pad;
    pad.name = "TycoonPad";
    pad.position = { 0.f, 1.f, 20.f };
    pad.size = { 12.f, 1.f, 12.f };
    pad.color = { 0.3f, 0.8f, 0.4f };
    pad.script =
        "script.Parent.Touched:Connect(function()\n"
        "  local ls = game.Players.LocalPlayer.leaderstats\n"
        "  ls.Coins.Value = (ls.Coins.Value or 0) + 5\n"
        "end)\n";
    pad.scriptType = ScriptType::Client;
    g_world.parts.push_back(std::move(pad));
    auto& dsHint = g_world.addShared(SharedService::ServerScriptService, "Script", "TycoonSave");
    dsHint.source =
        "local ds = game:GetService(\"DataStoreService\"):GetDataStore(\"Tycoon\")\n"
        "print(\"tycoon save ready\", ds)\n";
    g_classifica.set("Coins", 0);
    markPlaceDirty();
    proPushHistory("Tycoon kit");
}

static void proKitFps() {
    WorkspacePart cover;
    cover.name = "Cover";
    cover.position = { 10.f, 3.f, 10.f };
    cover.size = { 8.f, 6.f, 2.f };
    cover.color = { 0.45f, 0.45f, 0.5f };
    g_world.parts.push_back(std::move(cover));
    g_world.addShared(SharedService::ReplicatedStorage, "RemoteEvent", "Shoot");
    g_world.addShared(SharedService::ReplicatedStorage, "RemoteEvent", "Hit");
    markPlaceDirty();
    proPushHistory("FPS kit");
}

static void drawKitGeneratorsWindow() {
    if (!g_showKitGenerators) return;
    proBeginWindow("Game Kits##pro", &g_showKitGenerators, 380.f, 280.f);
    ImGui::TextWrapped("Genera pezzi base per generi comuni (modificabili).");
    if (ImGui::Button("Obby kit", {-1, 0})) proKitObby();
    if (ImGui::Button("Tycoon kit", {-1, 0})) proKitTycoon();
    if (ImGui::Button("FPS kit", {-1, 0})) proKitFps();
    ImGui::End();
}

static void drawMirrorToolsWindow() {
    if (!g_showMirrorTools) return;
    proBeginWindow("Mirror / Flip##pro", &g_showMirrorTools, 340.f, 220.f);
    if (!partIndexValid(g_selectedPart)) {
        ImGui::TextDisabled("Seleziona una Part");
        ImGui::End();
        return;
    }
    auto& p = g_world.parts[g_selectedPart];
    if (ImGui::Button("Mirror X", {-1, 0})) { p.position.x = -p.position.x; markPlaceDirty(); }
    if (ImGui::Button("Mirror Z", {-1, 0})) { p.position.z = -p.position.z; markPlaceDirty(); }
    if (ImGui::Button("Flip Size XZ", {-1, 0})) {
        std::swap(p.size.x, p.size.z);
        markPlaceDirty();
    }
    if (ImGui::Button("Duplicate mirrored X", {-1, 0})) {
        WorkspacePart c = p;
        c.name = p.name + "_M";
        c.position.x = -c.position.x;
        g_world.parts.push_back(std::move(c));
        markPlaceDirty();
    }
    ImGui::End();
}

static void drawUnionHintWindow() {
    if (!g_showUnionHint) return;
    proBeginWindow("Solid Modeling##pro", &g_showUnionHint, 400.f, 200.f);
    ImGui::TextWrapped("Union / Negate / Separate non sono ancora CSG reali. "
                       "Usa Model + WeldConstraint come workaround Studio.");
    if (ImGui::Button("Weld selection", {-1, 0}))
        studioCreateWeldFromSelection();
    ImGui::End();
}

static void drawPluginBarWindow() {
    if (!g_showPluginBar) return;
    ImGui::SetNextWindowPos({8.f, 120.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({72.f, 320.f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Plugins##pro", &g_showPluginBar,
                 ImGuiWindowFlags_NoScrollbar);
    if (ImGui::Button("Ins", {-1, 0})) g_showProInsertObject = true;
    if (ImGui::Button("Mat", {-1, 0})) g_showMaterialManager = true;
    if (ImGui::Button("Snd", {-1, 0})) g_showSoundBrowser = true;
    if (ImGui::Button("DS", {-1, 0})) g_showDataStoreBrowser = true;
    if (ImGui::Button("Aln", {-1, 0})) g_showAlignTools = true;
    if (ImGui::Button("Kit", {-1, 0})) g_showKitGenerators = true;
    if (ImGui::Button("Ana", {-1, 0})) g_showScriptAnalysis = true;
    ImGui::End();
}

static void drawStudioProOverlaysExtra2() {
    drawKitGeneratorsWindow();
    drawMirrorToolsWindow();
    drawUnionHintWindow();
    drawPluginBarWindow();
}

// Patch overlays/menus to include extras
static void drawStudioProOverlaysExtra() {
    drawColorPaletteWindow();
    drawBrickColorWindow();
    drawPartCounterWindow();
    drawProScriptTemplatesWindow();
    drawMobilePreviewWindow();
    drawSecurityPanelWindow();
    drawAnimIdPanelWindow();
    drawToolGripPanelWindow();
    drawChatSettingsWindow();
    drawFriendPanelWindow();
    drawStudioProOverlaysExtra2();
    drawStudioProOverlaysExtra3();
}


// ===========================================================================
// Feature index (Studio Pro) — reference for creators / docs
// ===========================================================================
// 01 Game Settings          02 Attributes           03 Tags
// 04 Sound Browser          05 Material Manager     06 Insert Object
// 07 Performance            08 Teams                09 leaderstats
// 10 ProximityPrompt        11 DataStore Browser    12 Lighting Presets
// 13 Align/Distribute       14 Script Analysis      15 Device Emulator
// 16 Publish                17 Soft Shutdown        18 Context Actions
// 19 HttpService            20 Camera               21 Physics
// 22 Spawn                  23 Badges               24 Localization
// 25 Packages               26 Streaming            27 Filtering
// 28 Skybox                 29 PostFX               30 History
// 31 Emotes                 32 Accessories          33 Collision Debug
// 34 Network Owner          35 Toolbox Pro          36 Command Palette
// 37 Quick Actions          38 Batch Tools          39 Selection Sets
// 40 Studio Prefs           41 Notifications        42 Terrain Extras
// 43 Humanoid Defaults      44 Remotes Browser      45 Color Palette
// 46 BrickColor             47 Place Stats          48 Script Templates
// 49 Mobile Preview         50 Security             51 AnimId
// 52 Tool Grip              53 Chat                 54 Friends
// 55 Game Kits              56 Mirror/Flip          57 Solid Modeling
// 58 Plugin Bar
// + Must-need APIs: Attributes, HttpService JSON, ContextActionService,
//   TeleportService, GetService aliases, Part:SetAttribute/GetAttribute.

// ===========================================================================
// More creator tools to reach Studio Pro surface area
// ===========================================================================

static void drawPivotToolsWindow() {
    if (!g_showPivotTools) return;
    proBeginWindow("Pivot Tools##pro", &g_showPivotTools, 360.f, 240.f);
    if (!partIndexValid(g_selectedPart)) {
        ImGui::TextDisabled("Seleziona Part");
        ImGui::End();
        return;
    }
    auto& p = g_world.parts[g_selectedPart];
    if (ImGui::Button("Pivot to origin", {-1, 0})) {
        p.position = {0.f, p.size.y * 0.5f, 0.f};
        markPlaceDirty();
    }
    if (ImGui::Button("Pivot to spawn", {-1, 0})) {
        p.position = g_world.spawn.position + glm::vec3(0.f, p.size.y * 0.5f, 0.f);
        markPlaceDirty();
    }
    if (ImGui::Button("Center model selection Y", {-1, 0})) {
        float minY = 1e9f;
        for (const auto& q : g_world.parts) minY = std::min(minY, q.position.y - q.size.y * 0.5f);
        for (auto& q : g_world.parts) q.position.y -= minY;
        markPlaceDirty();
    }
    ImGui::End();
}

static void drawBulkScriptsWindow() {
    if (!g_showBulkScripts) return;
    proBeginWindow("Bulk Scripts##pro", &g_showBulkScripts, 420.f, 300.f);
    static char bulk[1024] =
        "script.Parent.Touched:Connect(function()\n  print(script.Parent.Name)\nend)\n";
    ImGui::InputTextMultiline("##bulk", bulk, sizeof(bulk), {-1.f, 140.f});
    if (ImGui::Button("Apply to ALL Parts without script", {-1, 0})) {
        int n = 0;
        for (auto& p : g_world.parts) {
            if (!p.script.empty()) continue;
            p.script = bulk;
            p.scriptType = ScriptType::Server;
            ++n;
        }
        markPlaceDirty();
        g_exportMsg = "Bulk scripts: " + std::to_string(n);
    }
    if (ImGui::Button("Clear all Part scripts", {-1, 0})) {
        for (auto& p : g_world.parts) p.script.clear();
        markPlaceDirty();
    }
    ImGui::End();
}

static void drawPlaceValidatorWindow() {
    if (!g_showPlaceValidator) return;
    proBeginWindow("Place Validator##pro", &g_showPlaceValidator, 460.f, 340.f);
    static std::vector<std::string> issues;
    if (ImGui::Button("Validate", {-1, 0})) {
        issues.clear();
        if (g_world.parts.empty()) issues.push_back("Nessuna Part");
        bool hasSpawn = false;
        for (const auto& p : g_world.parts)
            if (p.name.find("Spawn") != std::string::npos) hasSpawn = true;
        if (!hasSpawn) issues.push_back("Nessuna Part Spawn* (opzionale)");
        int scripts = 0;
        for (const auto& p : g_world.parts) if (!p.script.empty()) ++scripts;
        for (const auto& s : g_world.instances)
            if (s.className == "Script" || s.className == "LocalScript") ++scripts;
        if (scripts == 0) issues.push_back("Nessuno script — il place e' solo scenografia");
        for (int i = 0; i < (int)g_world.parts.size(); ++i) {
            const auto& p = g_world.parts[i];
            if (p.size.x <= 0 || p.size.y <= 0 || p.size.z <= 0)
                issues.push_back("Part size non valida: " + p.name);
            if (!std::isfinite(p.position.x))
                issues.push_back("Part position NaN: " + p.name);
        }
        if (issues.empty()) issues.push_back("OK — nessun problema bloccante");
        proPushHistory("Validate");
    }
    for (const auto& s : issues) ImGui::BulletText("%s", s.c_str());
    ImGui::End();
}

static void drawHotkeyCheatsheetWindow() {
    if (!g_showHotkeyCheatsheet) return;
    proBeginWindow("Hotkeys##pro", &g_showHotkeyCheatsheet, 400.f, 320.f);
    ImGui::Text("Ctrl+P          Command Palette");
    ImGui::Text("Ctrl+Shift+I    Insert Object");
    ImGui::Text("Ctrl+Shift+G    Game Settings");
    ImGui::Text("Ctrl+Shift+A    Attributes");
    ImGui::Text("Ctrl+Shift+F    Find/Replace (2020)");
    ImGui::Text("Ctrl+T          Terrain Editor");
    ImGui::Text("E (in Play)     ProximityPrompt");
    ImGui::Text("F8              Avatar Catalog");
    ImGui::Separator();
    ImGui::TextDisabled("Studio Pro cheatsheet");
    ImGui::End();
}

static void drawStudioProOverlaysExtra3() {
    drawPivotToolsWindow();
    drawBulkScriptsWindow();
    drawPlaceValidatorWindow();
    drawHotkeyCheatsheetWindow();
}

