// Selezione di part/modelli/suoni/etichette/player, editor del rig del
// player, import backpack da file droppato, playback dell'animazione dei
// modelli, e il callback dei click del mouse in Studio (select/gizmo/
// insert). Non e' un header riusabile: usa direttamente le globali di
// main.cpp (g_selectedPart, g_selectedModel, g_world, ecc.). Compila solo
// se incluso da main.cpp esattamente in questo punto.

#include "engine/BrickColor.h"
#include <cmath>

static bool rayHitAabb(const glm::vec3& o, const glm::vec3& d,
                       const glm::vec3& center, const glm::vec3& size, float& tOut) {
    glm::vec3 half = size * 0.5f;
    glm::vec3 bmin = center - half;
    glm::vec3 bmax = center + half;
    float tmin = 0.f, tmax = 1e6f;
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(d[i]) < 1e-8f) {
            if (o[i] < bmin[i] || o[i] > bmax[i]) return false;
            continue;
        }
        float inv = 1.f / d[i];
        float t0 = (bmin[i] - o[i]) * inv;
        float t1 = (bmax[i] - o[i]) * inv;
        if (t0 > t1) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmax < tmin) return false;
    }
    if (tmax < 0.f) return false;
    tOut = tmin >= 0.f ? tmin : tmax;
    return true;
}

static void screenToRay(float mx, float my, const glm::mat4& view, const glm::mat4& proj,
                        glm::vec3& origin, glm::vec3& dir) {
    float nx = (2.f * mx) / (float)g_w - 1.f;
    float ny = 1.f - (2.f * my) / (float)g_h;
    glm::mat4 inv = glm::inverse(proj * view);
    glm::vec4 n4 = inv * glm::vec4(nx, ny, -1.f, 1.f);
    glm::vec4 f4 = inv * glm::vec4(nx, ny,  1.f, 1.f);
    n4 /= n4.w;
    f4 /= f4.w;
    origin = glm::vec3(n4);
    dir = glm::normalize(glm::vec3(f4 - n4));
}

static void syncPartEditors() {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) {
        g_partNameBuf[0] = 0;
        g_partScriptBuf[0] = 0;
        return;
    }
    const auto& p = g_world.parts[g_selectedPart];
    std::snprintf(g_partNameBuf, sizeof(g_partNameBuf), "%s", p.name.c_str());
    std::snprintf(g_partScriptBuf, sizeof(g_partScriptBuf), "%s", p.script.c_str());
}

static void clearPlayerSelection() {
    g_selectedPlayer = false;
    g_selectedPlayerPart = -1;
}

static void clearSpecialSelection() {
    g_selectedSpawn = false;
}

static void clearAllSelection() {
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    g_selectedService = -1;
    clearPlayerSelection();
    clearSpecialSelection();
    g_editingPartScript = false;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
}

static void selectService(int id) {
    clearAllSelection();
    g_selectedService = id;
    g_showProps = true;
}

// Ricliccare un servizio gia' selezionato lo deseleziona, invece di
// riselezionarlo sempre uguale — prima non c'era alcun modo di tornare a
// "niente selezionato" una volta cliccato un servizio.
static void toggleService(int id) {
    if (g_selectedService == id) clearAllSelection();
    else selectService(id);
}

static void selectPart(int idx) {
    g_selectedPart = idx;
    if (idx >= 0) {
        g_selectedModel = -1;
        g_selectedSound = -1;
        g_selectedLabel = -1;
        // Anche l'oggetto del Backpack: senza questa riga il pannello
        // proprieta' restava incollato sull'oggetto (la catena controlla
        // prima la Part, ma la scheda dell'item continuava a comparire) e
        // cliccare una Part sembrava non fare niente.
        g_selectedBackpack = -1;
        clearPlayerSelection();
        clearSpecialSelection();
        g_showProps = true;
    }
    g_scaleDragging = false;
    g_moveDragging = false;
    g_rotateDragging = false;
    syncPartEditors();
}

static void selectSpawn() {
    clearAllSelection();
    g_selectedSpawn = true;
    g_showProps = true;
}

static void drawPartColorPicker(glm::vec3& color) {
    ImGui::TextColored({0.15f, 0.55f, 0.95f, 1.f}, "BrickColor");
    const brickcolor::Entry* match = brickcolor::nearest(color);
    ImGui::TextDisabled("Corrente ≈ %s  (#%d)", match ? match->name : "?", match ? match->number : 0);

    ImGui::ColorEdit3("##partcolor", &color.x,
        ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel
        | ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::ColorEdit3("RGB", &color.x, ImGuiColorEditFlags_Float))
        markPlaceDirty();

    static char filter[64] = {};
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##bcfilter", "Cerca colore (Bright red, Lime…)", filter, sizeof(filter));

    int n = 0;
    const brickcolor::Entry* all = brickcolor::palette(&n);
    const float cell = 22.f;
    const int cols = 10;
    ImGui::BeginChild("##brickpalette", {0.f, 168.f}, true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    int shown = 0;
    for (int i = 0; i < n; ++i) {
        if (!brickcolor::nameContains(all[i].name, filter)) continue;
        if (shown % cols != 0) ImGui::SameLine(0.f, 3.f);
        ImGui::PushID(i);
        glm::vec3 lin = brickcolor::toLinear(all[i]);
        ImVec4 col{lin.x, lin.y, lin.z, 1.f};
        const bool selected = match && match->number == all[i].number
            && std::fabs(color.x - lin.x) < 0.02f
            && std::fabs(color.y - lin.y) < 0.02f
            && std::fabs(color.z - lin.z) < 0.02f;
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Border, {1.f, 1.f, 1.f, 1.f});
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.f);
        }
        if (ImGui::ColorButton(all[i].name, col, flags, {cell, cell})) {
            color = lin;
            markPlaceDirty();
        }
        if (selected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\nRGB %d %d %d · #%d",
                all[i].name, all[i].r, all[i].g, all[i].b, all[i].number);
        ImGui::PopID();
        ++shown;
    }
    if (shown == 0)
        ImGui::TextDisabled("Nessun colore per \"%s\"", filter);
    ImGui::EndChild();
    ImGui::TextDisabled("%d colori · click = applica alla Part", n);
}

static void selectModel(int idx) {
    g_selectedModel = idx;
    g_selectedPart = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    clearPlayerSelection();
    clearSpecialSelection();
    g_editingPartScript = false;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    if (idx >= 0) g_showProps = true;
}

static void selectSound(int idx) {
    g_selectedSound = idx;
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedLabel = -1;
    clearPlayerSelection();
    clearSpecialSelection();
    g_editingPartScript = false;
    if (idx >= 0 && idx < (int)g_sounds.sounds.size())
        std::snprintf(g_soundPathBuf, sizeof(g_soundPathBuf), "%s",
            g_sounds.sounds[idx].soundId.c_str());
    if (idx >= 0) g_showProps = true;
}

static void selectLabel(int idx) {
    clearAllSelection();
    g_selectedLabel = idx;
    if (idx >= 0 && idx < (int)g_world.labels.size()) {
        const auto& l = g_world.labels[idx];
        std::snprintf(g_labelNameBuf, sizeof(g_labelNameBuf), "%s", l.name.c_str());
        std::snprintf(g_labelTextBuf, sizeof(g_labelTextBuf), "%s", l.text.c_str());
        std::snprintf(g_labelScriptBuf, sizeof(g_labelScriptBuf), "%s", l.script.c_str());
        g_showProps = true;
    }
}

static void selectPlayer(int partIdx = -1) {
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    clearSpecialSelection();
    g_editingPartScript = false;
    g_selectedPlayer = true;
    g_selectedPlayerPart = partIdx;
    g_showProps = true;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
}

static void openPlayerRigEditor() {
    g_player.humanoid.userRigged = true;
    g_player.humanoid.rigEnabled = true;
    g_showPlayerRigPrompt = false;
    g_showPlayerRigEditor = true;
    g_playerRigEditorNeedsPlace = true;
    if (!g_selectedPlayer) selectPlayer(-1);
    g_exportMsg = "Rig Player aperto — modifica arti R6";
}

static void beginRigPlayer() {
    selectPlayer(-1);
    if (g_player.humanoid.userRigged) {
        openPlayerRigEditor();
        return;
    }
    g_showPlayerRigPrompt = true;
    g_exportMsg = "Player non riggato — conferma per creare il Rig R6";
}

static void confirmPlayerRig(bool yes) {
    g_showPlayerRigPrompt = false;
    if (!yes) {
        g_exportMsg = "Rig Player annullato";
        return;
    }
    openPlayerRigEditor();
}

static bool nameLooksLikeGunItem(std::string s) {
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s.find("pistola") != std::string::npos
        || s.find("pistol") != std::string::npos
        || s.find("gun") != std::string::npos
        || s.find("fucile") != std::string::npos
        || s.find("shotgun") != std::string::npos
        || s.find("pompa") != std::string::npos
        || s.find("revolver") != std::string::npos;
}

static std::string stripFileExt(std::string s) {
    auto slash = s.find_last_of("\\/");
    if (slash != std::string::npos) s = s.substr(slash + 1);
    auto dot = s.find_last_of('.');
    if (dot != std::string::npos) s = s.substr(0, dot);
    return s;
}

static void offerBackpackForGunImport(const std::string& path) {
    if (!nameLooksLikeGunItem(path)) return;
    g_backpackOfferName = stripFileExt(path);
    if (g_backpackOfferName.empty()) g_backpackOfferName = "Pistola";
    g_backpackOfferPath = path;
    g_showBackpackOffer = true;
}

static void confirmBackpackOffer(bool yes) {
    g_showBackpackOffer = false;
    if (!yes) {
        g_exportMsg = "Ok — " + g_backpackOfferName + " resta solo come modello (non in Backpack)";
        g_backpackOfferName.clear();
        g_backpackOfferPath.clear();
        return;
    }
    auto& it = g_world.addBackpackItem(g_backpackOfferName, 4, BackpackTool::Pistol);
    if (!g_backpackOfferPath.empty())
        it.meshPath = g_backpackOfferPath;
    g_selectedBackpack = (int)g_world.backpack.size() - 1;
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_showExplorer = true;
    g_showProps = true;
    g_exportMsg = "\"" + it.name + "\" aggiunta al Backpack (tool Pistol) — slot 1-9 in Test";
    g_backpackOfferName.clear();
    g_backpackOfferPath.clear();
}

static void bindPhysBridgeToLua(); // ScriptsAndPhysics.h

static void bindLuaServices() {
    bindPhysBridgeToLua();
    g_lua.soundService = &g_sounds;
    g_lua.lighting = &g_lighting;
    g_lua.animator = &g_animator;
    g_lua.particles = &g_particles;
    g_lua.net = &g_net;
    g_lua.playerHumanoid = &g_player.humanoid;
    g_lua.nav = &g_nav;
    g_lua.navRebuild = &rebuildNavGrid;
    g_lua.cameraShake = &triggerCameraShake;
    g_lua.playerPos = &g_playerPos;
    g_lua.playerYaw = &g_playerYaw;
    g_lua.tags = &g_tag;
    g_lua.classifica = &g_classifica;
    g_lua.proximity = &g_prossimita;
    g_lua.cameraFov = &g_cam.fov;
    g_lua.firstPersonFlag = &g_firstPerson;
    const bool inPlay = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    g_lua.runIsStudio = (g_screen == AppScreen::Studio && !g_studioTesting);
    g_lua.runIsClient = inPlay;
    g_lua.runIsServer = (g_net.role == NetRole::Host || g_net.role == NetRole::Offline);
    g_lua.voxelRaycast = [](const glm::vec3& origin, const glm::vec3& dir, float maxDist,
                            LuaRuntime::VoxelHit& out) -> bool {
        if (g_terrain.empty()) return false;
        Terrain::Hit h = g_terrain.raycast(origin, dir, maxDist);
        if (!h.ok) return false;
        out.ok = true;
        out.pos = h.pos;
        out.normal = glm::vec3((float)h.nx, (float)h.ny, (float)h.nz);
        out.dist = h.t;
        out.block = h.id;
        out.material = Terrain::enumMaterialName(h.id);
        return true;
    };
    g_lua.getAimRay = [](glm::vec3& o, glm::vec3& d) -> bool {
        GLFWwindow* win = glfwGetCurrentContext();
        if (!win) return false;
        double mx = 0, my = 0;
        if (g_playCursorLocked) {
            mx = (double)g_w * 0.5;
            my = (double)g_h * 0.5;
        } else {
            glfwGetCursorPos(win, &mx, &my);
        }
        screenToRay((float)mx, (float)my, g_lastView, g_lastProj, o, d);
        if (!std::isfinite(o.x) || !std::isfinite(o.y) || !std::isfinite(o.z)
            || !std::isfinite(d.x) || !std::isfinite(d.y) || !std::isfinite(d.z))
            return false;
        return glm::length(d) > 1e-6f;
    };
}

static void applyModelAnimSample(WorkspaceModel& m, float t) {
    auto k = m.anim.sample(t);
    m.position = k.position;
    m.rotation = k.rotation;
    m.scale = k.scale;
}

static void openModelAnimEditor(int modelIdx) {
    if (modelIdx < 0 || modelIdx >= (int)g_world.models.size()) return;
    auto& m = g_world.models[modelIdx];
    if (!m.rig.simpleRig) {
        m.rig.simpleRig = true;
        m.rig.rootName = "Root";
    }
    m.anim.ensureSeed(m.position, m.rotation, m.scale);
    m.anim.duration = std::max(m.anim.duration, 0.5f);
    if (m.anim.name.empty()) m.anim.name = "Anim";
    g_animEditModel = modelIdx;
    g_animSelKey = 0;
    g_animScrub = 0.f;
    g_animPlaying = false;
    g_showAnimEditor = true;
    g_showRigPrompt = false;
    g_animEditorNeedsPlace = true;
    applyModelAnimSample(m, 0.f);
    selectModel(modelIdx);
    g_tool = StudioTool::Move;
    g_exportMsg = "Anima: " + m.name + " — muovi il Model, poi Set Frame (K)";
}

static void askRigOrOpenAnimator(int modelIdx) {
    if (modelIdx < 0 || modelIdx >= (int)g_world.models.size()) return;
    selectModel(modelIdx);
    auto& m = g_world.models[modelIdx];
    if (m.rig.hasAnimRig()) {
        openModelAnimEditor(modelIdx);
        return;
    }
    g_animEditModel = modelIdx;
    g_showRigPrompt = true;
    g_showAnimEditor = false;
    g_exportMsg = "Serve un Rig Root per animare questo Model";
}

static void beginAnimateSelectedModel() {
    if (g_selectedModel < 0 || g_selectedModel >= (int)g_world.models.size()) {
        if (!g_world.models.empty()) {
            selectModel(0);
        } else {
            g_exportMsg = "Inserisci un Model (toolbar Model), poi Anima";
            g_showExplorer = true;
            g_showProps = true;
            return;
        }
    }
    askRigOrOpenAnimator(g_selectedModel);
}

static void confirmModelRig(bool yes) {
    int idx = g_animEditModel;
    g_showRigPrompt = false;
    if (idx < 0 || idx >= (int)g_world.models.size()) {
        g_animEditModel = -1;
        return;
    }
    if (!yes) {
        g_exportMsg = "Apro comunque con Rig Root";
        openModelAnimEditor(idx);
        return;
    }
    auto& m = g_world.models[idx];
    m.rig.simpleRig = true;
    m.rig.rootName = "Root";
    g_exportMsg = "Rig Root creato — puoi animare " + m.name;
    openModelAnimEditor(idx);
}

static float g_modelAnimClock = 0.f;

static void updateModelAnimPlayback(float dt) {
    if (g_animPlaying && g_animEditModel >= 0 && g_animEditModel < (int)g_world.models.size()) {
        auto& m = g_world.models[g_animEditModel];
        g_animScrub += dt;
        if (m.anim.looped) {
            float dur = std::max(0.001f, m.anim.duration);
            if (g_animScrub > dur) g_animScrub = std::fmod(g_animScrub, dur);
        } else if (g_animScrub >= m.anim.duration) {
            g_animScrub = m.anim.duration;
            g_animPlaying = false;
        }
        applyModelAnimSample(m, g_animScrub);
    }

    const bool inPlay = (g_screen == AppScreen::Playing)
        || (g_screen == AppScreen::Studio && g_studioTesting);
    if (!inPlay) return;
    g_modelAnimClock += dt;
    for (int i = 0; i < (int)g_world.models.size(); ++i) {
        if (g_showAnimEditor && i == g_animEditModel && g_animPlaying)
            continue;
        auto& m = g_world.models[i];
        if (!m.anim.hasMotion()) continue;
        float dur = std::max(0.001f, m.anim.duration);
        float t = m.anim.looped ? std::fmod(g_modelAnimClock, dur)
                                : std::min(g_modelAnimClock, dur);
        applyModelAnimSample(m, t);
    }
}

// Snap/grid early — GizmoSystem needs studioSnap*Step before PartPrefabs UI.
static bool  g_studioSnapEnabled = true;
static float g_studioSnapMove = 0.25f;
static float g_studioSnapRotate = 5.f;
static float g_studioSnapScale = 0.25f;
static bool  g_studioLocalSpace = false;
// Clic destro fermo nel viewport: il menu lo apre drawViewportContextMenu (StudioExplorer.h).
static bool   g_viewportCtxRequest = false;
static ImVec2 g_viewportCtxPos{0.f, 0.f};
static bool  g_studioGridVisible = true;
static float g_studioGridSize = 4.f;
static bool  g_viewIsolate = false;
static int   g_isolatePart = -1;
static bool  g_viewWireframe = false;
static bool  g_viewCollision = false;
static bool  g_showCommandBar = true;
// Terrain brush (Studio 2020) — state early; paint fn in Studio2020Ui.h
static bool  g_terrainToolActive = false;
static bool  g_terrainBrushErase = false;
static int   g_terrainBrushMat = 1; // Terrain::Grass
static int   g_terrainBrushRadius = 1;
static void studioTerrainPaintAtMouse(float mx, float my, bool erase);

static bool studioGizmoShiftKey() {
    return glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
        || glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
}
static float studioSnapMoveStep() {
    if (!g_studioSnapEnabled) return 0.f;
    if (studioGizmoShiftKey()) return std::max(0.05f, g_studioSnapMove * 0.2f);
    return g_studioSnapMove;
}
static float studioSnapRotateStep() {
    if (!g_studioSnapEnabled) return 0.f;
    if (studioGizmoShiftKey()) return 1.f;
    return g_studioSnapRotate;
}
static float studioSnapScaleStep() {
    if (!g_studioSnapEnabled) return 0.f;
    if (studioGizmoShiftKey()) return std::max(0.05f, g_studioSnapScale * 0.4f);
    return g_studioSnapScale;
}

#include "main_ui/GizmoSystem.h"
#include "main_ui/WorldEditingTools.h"
#include "main_ui/PartPrefabs.h"
#include "main_ui/UserToolbox.h"

#include "main_ui/ScriptsAndPhysics.h"
// StudioFeaturesUi.h / StudioSelectionUi.h (inclusi dopo questo file).
static void studioMultiClear();
static void studioBeginMarquee(float mx, float my);
static bool studioMultiContains(int i);

static void onMouseBtn(GLFWwindow* win, int btn, int action, int) {
#ifdef __ANDROID__
    // Ogni volta che un dito si appoggia, il punto di partenza dello sguardo
    // riparte da li.
    //
    // Prima questo se ne accorgeva il gestore del MOVIMENTO, che pero non viene
    // chiamato quando il dito si stacca: staccando a destra e riappoggiando a
    // sinistra, lo spostamento veniva calcolato rispetto al punto vecchio e la
    // telecamera scattava indietro di colpo. E il "torna a dov era prima".
    if (btn == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        g_firstMouse = true;
#endif
    if (btn == GLFW_MOUSE_BUTTON_RIGHT) {
        // Come Roblox Studio: tasto destro trascinato = gira la camera; clic
        // destro fermo = menu della part sotto il mouse (StudioExplorer.h).
        {
            static double s_x = 0, s_y = 0, s_t = 0;
            static bool s_overUi = false;
            double cx = 0, cy = 0;
            glfwGetCursorPos(win, &cx, &cy);
            if (action == GLFW_PRESS) {
                s_x = cx; s_y = cy; s_t = glfwGetTime();
                s_overUi = ImGui::GetIO().WantCaptureMouse;
            } else if (action == GLFW_RELEASE && !s_overUi && g_screen == AppScreen::Studio
                       && g_mode == CamMode::Studio && !g_studioTesting
                       && std::hypot(cx - s_x, cy - s_y) < 6.0 && glfwGetTime() - s_t < 0.6) {
                const ScenePick hit = pickSceneAtMouse((float)s_x, (float)s_y);
                if (hit.kind == PickKind::Part) {
                    if (!studioMultiContains(hit.index) && hit.index != g_selectedPart) applyScenePick(hit);
                } else if (hit.kind != PickKind::None) {
                    applyScenePick(hit);
                }
                g_viewportCtxRequest = true;
                g_viewportCtxPos = {(float)s_x, (float)s_y};
            }
        }
        g_rmbLook = (action == GLFW_PRESS);
        g_firstMouse = true;
        if (g_rmbLook) endCatalogGizmo();
        if (g_screen == AppScreen::Playing
            || (g_screen == AppScreen::Studio && g_studioTesting)) {
            syncPlayCursor(win);
        } else {
            glfwSetInputMode(win, GLFW_CURSOR,
                g_rmbLook ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }
    }
    if (btn == GLFW_MOUSE_BUTTON_LEFT && g_screen == AppScreen::AvatarPublisher
        && !g_rmbLook && !ImGui::GetIO().WantCaptureMouse) {
        if (action == GLFW_RELEASE) {
            endCatalogGizmo();
        } else if (action == GLFW_PRESS && g_catalog.selected >= 0
                   && g_catalog.selected < (int)g_catalog.items.size()) {
            double mx = 0, my = 0;
            glfwGetCursorPos(win, &mx, &my);
            int axis = 0;
            bool hit = false;
            if (g_catalog.tool == AccelTool::Move)
                hit = pickCatalogMoveAxis((float)mx, (float)my, axis);
            else if (g_catalog.tool == AccelTool::Rotate) {
                if (gizmoShift() && pickCatalogRotateAxis((float)mx, (float)my, axis))
                    hit = true;
                else if (pickCatalogRotateFree((float)mx, (float)my)
                         || pickCatalogRotateAxis((float)mx, (float)my, axis)) {
                    hit = true;
                    axis = -1;
                }
            } else
                hit = pickCatalogScaleAxis((float)mx, (float)my, axis);
            if (hit) beginCatalogGizmo(axis, (float)mx, (float)my);
        }
    }
    if (btn == GLFW_MOUSE_BUTTON_LEFT && g_screen == AppScreen::Studio
        && g_mode == CamMode::Studio && !g_rmbLook && !ImGui::GetIO().WantCaptureMouse) {
        if (action == GLFW_RELEASE) {
            endFreeDrag();
            endScaleDrag();
            endMoveDrag();
            endRotateDrag();
            endLabelDrag();
            return;
        }
        if (action == GLFW_PRESS) {
            double mx = 0, my = 0;
            glfwGetCursorPos(win, &mx, &my);
            int axis = 0, sign = 0;
            if (g_tool == StudioTool::Scale && pickScaleHandle((float)mx, (float)my, axis, sign)) {
                beginScaleDrag(axis, sign, (float)mx, (float)my);
            } else if (g_tool == StudioTool::Move && pickMoveHandle((float)mx, (float)my, axis)) {
                beginMoveDrag(axis, (float)mx, (float)my);
            } else if (g_tool == StudioTool::Rotate && pickRotateHandle((float)mx, (float)my, axis)) {
                beginRotateDrag(axis, (float)mx, (float)my);
            } else if (g_terrainToolActive) {
                studioTerrainPaintAtMouse((float)mx, (float)my, g_terrainBrushErase
                    || (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS));
            } else {
                int labHit = pickScreenLabelAt((float)mx, (float)my);
                if (labHit >= 0) {
                    selectLabel(labHit);
                    beginLabelDrag(labHit, (float)mx, (float)my);
                } else {
                    ScenePick hit = pickSceneAtMouse((float)mx, (float)my);
                    if (hit.kind == PickKind::None) {
                        // Clic sul vuoto: deseleziona tutto (Ctrl tiene la
                        // selezione) e parte la selezione a riquadro.
                        if (!ImGui::GetIO().KeyCtrl) {
                            applyScenePick(hit);
                            studioMultiClear();
                        }
                        studioBeginMarquee((float)mx, (float)my);
                    } else {
                        applyScenePick(hit);
                        if (hit.kind == PickKind::Part && !ImGui::GetIO().KeyCtrl) armFreeDrag((float)mx, (float)my);
                    }
                }
            }
        }
    }
    if (btn == GLFW_MOUSE_BUTTON_LEFT && playCameraActive()
        && !ImGui::GetIO().WantCaptureMouse) {
        double mx = 0, my = 0;
        glfwGetCursorPos(win, &mx, &my);
        if (action == GLFW_PRESS) {
            if (!g_playCursorLocked) {
                int hit = pickScreenLabelAt((float)mx, (float)my);
                if (hit >= 0 && g_world.labels[hit].isButton()) {
                    g_uiPressedLabel = hit;
                    return;
                }
            }
            trySwordAttack();
            tryUseEquippedItem();
        } else if (action == GLFW_RELEASE) {
            if (g_uiPressedLabel >= 0) {
                int hit = g_playCursorLocked ? g_uiPressedLabel
                    : pickScreenLabelAt((float)mx, (float)my);
                if (hit == g_uiPressedLabel)
                    fireGuiButton(g_uiPressedLabel);
                g_uiPressedLabel = -1;
            }
        }
    }
}
