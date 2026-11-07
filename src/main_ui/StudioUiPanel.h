#pragma once

/**
 * NON e' un header riusabile come quelli in src/ui/ o src/engine/ — e'
 * il corpo di drawStudioUi() spostato qui SOLO per accorciare main.cpp.
 * Legge/scrive le static globali di main.cpp e chiama le sue funzioni
 * helper direttamente per nome (main.cpp le dichiara tutte in anticipo
 * nel suo blocco di forward-declaration, ~righe 302-377). Compila SOLO
 * se incluso da main.cpp, esattamente nel punto in cui questa funzione
 * viveva prima — mai da studio_main.cpp, BlockyApi.cpp o altre TU (non
 * condividono nessuno di questi simboli, fallirebbe la compilazione).
 *
 * Shell stile Roblox Studio: menu, ribbon (Home/Avatar/UI/Script/Model/
 * Test), Toolbox SX, Explorer+Properties DX stacked, Output basso,
 * editor script, Guida Lua, Chat — + HUD Test/Play se g_studioTesting.
 *
 * Dock extras (Assets, Console, window search) in StudioPlatformPanels.h.
 */

#include "main_ui/StudioFeaturesUi.h"
#include "main_ui/StudioGroupsUi.h"
#include "main_ui/StudioSelectionUi.h"
#include "main_ui/StudioMultiplayerTest.h"
#include "main_ui/Studio2020Ui.h"
#include "main_ui/StudioProUi.h"
#include "main_ui/StudioPlatformPanels.h"
#include "main_ui/StudioDevTools.h"
#include "main_ui/AnimationStudioUi.h"
#include "main_ui/StudioExplorer.h"
#include "main_ui/StudioPropertyGrid.h"

static void drawScriptCreatedGuidePopup() {
    if (!g_showScriptCreatedGuide) return;
    ImGui::OpenPopup("Dove puoi scriptare?##scriptcreated");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Dove puoi scriptare?##scriptcreated", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored({0.45f, 0.9f, 0.7f, 1.f}, "Hai creato uno script!");
        ImGui::Separator();
        ImGui::TextWrapped(
            "Puoi mettere script su:\n\n"
            "- Una Part: script.Parent e' quella part.\n"
            "- Un Model: script.Parent e' quel model.\n"
            "- Un TextButton: script.Parent e' il bottone —\n"
            "  script.Parent:Connect(function() ... end) gira al click.\n\n"
            "Gira solo premendo Test/Play (F5) — non mentre modifichi.");
        ImGui::Spacing();
        if (ImGui::Button("Ho capito", {160, 32})) {
            g_showScriptCreatedGuide = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Apri Guida Lua completa", {-1, 32})) {
            g_showLuaGuide = true;
            g_showScriptCreatedGuide = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ---- Barra dei comandi (ribbon) ----------------------------------------
//
// Ogni scheda ha pochi gruppi con un titolo; i comandi rari stanno nei menu
// a tendina (ribbonMenuButton) invece che in decine di pulsanti; ogni
// comando c'e' in un posto solo. Tutte le scritte passano da L() (ui/Loc.h):
// le traduzioni sono in assets/lang/<lingua>.json.

static void ribbonPartMenuItems() {
    if (ImGui::MenuItem(L("rb.block"))) insertNewPart();
    if (ImGui::MenuItem(L("rb.ball"))) insertShapedPart(PartShape::Ball);
    if (ImGui::MenuItem(L("rb.cylinder"))) insertShapedPart(PartShape::Cylinder);
    if (ImGui::MenuItem(L("rb.wedge"))) insertShapedPart(PartShape::Wedge);
    ImGui::Separator();
    if (ImGui::MenuItem(L("rb.neon"))) insertNeonPart();
    if (ImGui::MenuItem(L("rb.light"))) insertPointLightPart();
    if (ImGui::MenuItem(L("rb.explode"))) insertExplodePart();
    ImGui::Separator();
    if (ImGui::MenuItem(L("rb.spawn"))) studioRunInsertKind(7);
}

// Script, LocalScript o ModuleScript sulla part selezionata, come il menu
// "Inserisci oggetto" di Roblox Studio.
static void ribbonScriptMenuItems() {
    const bool sel = g_selectedPart >= 0;
    if (ImGui::MenuItem(L("rb.sc.server"), nullptr, false, sel)) studioAddScriptOfType(ScriptType::Server);
    if (ImGui::MenuItem(L("rb.sc.local"), nullptr, false, sel)) studioAddScriptOfType(ScriptType::Client);
    if (ImGui::MenuItem(L("rb.sc.module"), nullptr, false, sel)) studioAddScriptOfType(ScriptType::ModuleScript);
    if (!sel) ImGui::TextDisabled("%s", L("rb.sc.needsel"));
    ImGui::Separator();
    if (ImGui::MenuItem(L("rb.templates"), nullptr, g_showScriptTemplates)) g_showScriptTemplates = !g_showScriptTemplates;
}

// Barra di accesso rapido (in alto a destra sulla striscia delle schede):
// Salva, Annulla, Ripeti, come in Roblox Studio.
static void drawRibbonQuickAccess(ImVec2 stripMin, float stripW) {
    const float s = studio::kRibbonTabHeight - 6.f;
    const ImVec2 after = ImGui::GetCursorScreenPos();
    float x = stripMin.x + stripW - 3.f * (s + 4.f) - 6.f;
    const float y = stripMin.y + 3.f;
    if (studio::ribbonQuickButton("qsave", studio::Icon::Save, L("rb.q.save"), {x, y})) doSavePlace();
    x += s + 4.f;
    if (studio::ribbonQuickButton("qundo", studio::Icon::Undo, L("rb.q.undo"), {x, y}, g_undo.canUndo()))
        studioDoUndo();
    x += s + 4.f;
    if (studio::ribbonQuickButton("qredo", studio::Icon::Redo, L("rb.q.redo"), {x, y}, g_undo.canRedo()))
        studioDoRedo();
    ImGui::SetCursorScreenPos(after);   // torna sotto le schede (segue un Dummy)
}

static void drawStudioRibbonBody(int& tab) {
    static const char* kTabKeys[] = {"rb.tab.home", "rb.tab.model", "rb.tab.avatar", "rb.tab.ui",
                                     "rb.tab.script", "rb.tab.test", "rb.tab.view"};
    const char* tabs[7];
    for (int i = 0; i < 7; ++i) tabs[i] = L(kTabKeys[i]);
    if (tab < 0 || tab > 6) tab = 0;
    const ImVec2 stripMin = ImGui::GetCursorScreenPos();
    const float stripW = ImGui::GetContentRegionAvail().x;
    studio::ribbonTabs(tabs, 7, tab);
    drawRibbonQuickAccess(stripMin, stripW);
    ImGui::Dummy({0.f, 3.f});
    // Se i gruppi non ci stanno (finestra stretta, lingue con parole lunghe)
    // la barra scorre in orizzontale: rotella sopra la barra o le frecce ai lati.
    static float s_scroll[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    static float s_contentW[7] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    float& scroll = s_scroll[tab];
    const float bodyW = ImGui::GetWindowWidth() - 16.f;
    const float maxScroll = std::max(0.f, s_contentW[tab] - bodyW);
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        const ImGuiIO& io = ImGui::GetIO();
        scroll -= (io.MouseWheel + io.MouseWheelH) * 60.f;
    }
    scroll = std::clamp(scroll, 0.f, maxScroll);
    const float indent = 8.f - scroll;
    const float contentX0 = ImGui::GetWindowPos().x + indent;
    ImGui::Indent(indent);

    using studio::Icon;
    auto tool = [](const char* id, Icon icon, const char* key, StudioTool t) {
        if (studio::ribbonBigButton(id, icon, L(key), g_tool == t)) {
            g_tool = t;
            g_scaleDragging = g_moveDragging = g_rotateDragging = false;
        }
    };
    auto toolsGroup = [&](const char* idPrefix) {
        studio::ribbonBeginGroup();
        std::string p = idPrefix;
        tool((p + "sel").c_str(), Icon::Cursor, "rb.select", StudioTool::Select);
        tool((p + "mov").c_str(), Icon::Move, "rb.move", StudioTool::Move);
        tool((p + "scl").c_str(), Icon::Scale, "rb.scale", StudioTool::Scale);
        tool((p + "rot").c_str(), Icon::Rotate, "rb.rotate", StudioTool::Rotate);
        studio::ribbonEndGroup(L("rb.g.tools"));
    };
    auto toggle = [](const char* id, Icon icon, const char* key, bool& flag) {
        if (studio::ribbonBigButton(id, icon, L(key), flag)) flag = !flag;
    };
    auto small = [](const char* id, Icon icon, const char* key, bool enabled = true) {
        return studio::ribbonSmallButton(id, icon, L(key), false, enabled);
    };
    GLFWwindow* win = glfwGetCurrentContext();
    const bool hasSel = g_selectedPart >= 0 || studioMultiSelectionSize() > 0;

    if (tab == 0) {   // ---- HOME ----
        studio::ribbonBeginGroup();
        if (studio::ribbonBigButton("paste", Icon::NewFile, L("rb.paste"), false, !g_clipboard.empty()))
            studioPasteClipboard();
        studio::ribbonBeginStack();
        if (small("copy", Icon::Duplicate, "rb.copy", hasSel)) studioCopySelection();
        if (small("cut", Icon::Trash, "rb.cut", hasSel)) studioCutSelection();
        if (small("dup", Icon::Duplicate, "rb.duplicate", hasSel)) studioDuplicateSelection();
        studio::ribbonEndStack();
        studio::ribbonEndGroup(L("rb.g.clipboard"));

        toolsGroup("h");

        studio::ribbonBeginGroup();
        if (studio::ribbonMenuButton("hpart", Icon::Cube, L("rb.part"))) {
            ribbonPartMenuItems();
            ImGui::EndPopup();
        }
        if (studio::ribbonBigButton("hmdl", Icon::Package, L("rb.import"))) insertImportedModel();
        toggle("htbx", Icon::Folder, "rb.toolbox", g_showToolbox);
        studio::ribbonBeginStack();
        if (small("hobj", Icon::NewFile, "rb.object")) g_showInsertObject = true;
        if (small("hscr", Icon::Script, "rb.scriptmenu")) ImGui::OpenPopup("hscr##menu");
        if (ImGui::BeginPopup("hscr##menu")) {
            ribbonScriptMenuItems();
            ImGui::EndPopup();
        }
        if (small("hui", Icon::NewFile, "rb.uimenu")) ImGui::OpenPopup("hui##menu");
        if (ImGui::BeginPopup("hui##menu")) {
            if (ImGui::MenuItem(L("rb.textlabel"))) insertNewTextLabel();
            if (ImGui::MenuItem(L("rb.textbutton"))) insertNewTextButton();
            if (ImGui::MenuItem(L("rb.frame"))) insertNewFrame();
            if (ImGui::MenuItem(L("rb.image"))) insertNewImageLabel();
            ImGui::EndPopup();
        }
        studio::ribbonEndStack();
        studio::ribbonEndGroup(L("rb.g.insert"));

        studio::ribbonBeginGroup();
        studio::ribbonBeginStack();
        if (small("hgrp", Icon::Package, "rb.group", hasSel)) studioGroupSelection();
        if (small("hugrp", Icon::Package, "rb.ungroup", !studioSelectedWholeGroups().empty())) studioUngroupSelection();
        if (small("hlock", Icon::Anchor, "rb.lock", hasSel)) studioSetLockedOnSelection(!selectedPartLocked());
        studio::ribbonEndStack();
        studio::ribbonBeginStack();
        if (small("hanc", Icon::Anchor, "rb.anchor", hasSel)) studioSetAnchoredOnSelection(true);
        if (small("huanc", Icon::Anchor, "rb.unanchor", hasSel)) studioSetAnchoredOnSelection(false);
        if (small("hpaint", Icon::Light, "rb.paint")) {
            g_paintToolActive = !g_paintToolActive;
            if (g_paintToolActive) studioPaintAtSelection();
        }
        studio::ribbonEndStack();
        studio::ribbonEndGroup(L("rb.g.edit"));

        studio::ribbonBeginGroup();
        if (!g_studioTesting) {
            if (studio::ribbonBigButton("hplay", Icon::Play, L("rb.play"))) startStudioTest(win);
        } else if (studio::ribbonBigButton("hstop", Icon::Stop, L("rb.stop"), true)) {
            stopStudioTest(win);
        }
        studio::ribbonBeginStack();
        if (small("hhere", Icon::Play, "rb.playhere", !g_studioTesting)) studioPlayHere(win);
        if (small("hmp", Icon::Head, "rb.multi")) g_showMpTest = !g_showMpTest;
        studio::ribbonEndStack();
        studio::ribbonEndGroup(L("rb.g.test"));

        studio::ribbonBeginGroup();
        toggle("hset", Icon::Grid, "rb.settings", g_showGameSettings);
        if (studio::ribbonMenuButton("hgame", Icon::Package, L("rb.gamemenu"))) {
            ImGui::MenuItem(L("rb.teams"), nullptr, &g_showTeamsEditor);
            ImGui::MenuItem(L("rb.leaderstats"), nullptr, &g_showLeaderstatsEditor);
            ImGui::MenuItem(L("rb.spawns"), nullptr, &g_showSpawnEditor);
            ImGui::MenuItem(L("rb.badges"), nullptr, &g_showBadgePanel);
            ImGui::Separator();
            ImGui::MenuItem(L("rb.datastore"), nullptr, &g_showDataStoreBrowser);
            ImGui::MenuItem(L("rb.remotes"), nullptr, &g_showRemotesBrowser);
            ImGui::MenuItem(L("rb.http"), nullptr, &g_showHttpServicePanel);
            ImGui::Separator();
            ImGui::MenuItem(L("rb.placesettings"), nullptr, &g_showPlaceSettings);
            ImGui::MenuItem(L("rb.humdefaults"), nullptr, &g_showHumanoidDefaults);
            ImGui::MenuItem(L("rb.prompts"), nullptr, &g_showPromptPlacer);
            ImGui::MenuItem(L("rb.ctxactions"), nullptr, &g_showContextActions);
            ImGui::EndPopup();
        }
        studio::ribbonEndGroup(L("rb.g.settings"), true);
    } else if (tab == 1) {   // ---- MODELLO ----
        toolsGroup("m");

        studio::ribbonBeginGroup();
        if (studio::ribbonMenuButton("mpart", Icon::Cube, L("rb.part"))) {
            ribbonPartMenuItems();
            ImGui::EndPopup();
        }
        toggle("mcol", Icon::Light, "rb.color", g_showBrickColorPicker);
        toggle("mmat", Icon::Grid, "rb.material", g_showMaterialPalette);
        studio::ribbonEndGroup(L("rb.g.parts"));

        studio::ribbonBeginGroup();
        if (studio::ribbonMenuButton("malign", Icon::Grid, L("rb.align"))) {
            static const char* axes[3] = {"X", "Y", "Z"};
            for (int a = 0; a < 3; ++a) {
                char lab[96];
                std::snprintf(lab, sizeof(lab), "%s %s: %s", L("rb.align"), axes[a], L("rb.al.min"));
                if (ImGui::MenuItem(lab)) studioAlignSelection(a, StudioAlignMode::Min);
                std::snprintf(lab, sizeof(lab), "%s %s: %s", L("rb.align"), axes[a], L("rb.al.center"));
                if (ImGui::MenuItem(lab)) studioAlignSelection(a, StudioAlignMode::Center);
                std::snprintf(lab, sizeof(lab), "%s %s: %s", L("rb.align"), axes[a], L("rb.al.max"));
                if (ImGui::MenuItem(lab)) studioAlignSelection(a, StudioAlignMode::Max);
                if (a < 2) ImGui::Separator();
            }
            ImGui::Separator();
            for (int a = 0; a < 3; ++a) {
                char lab[96];
                std::snprintf(lab, sizeof(lab), "%s %s", L("rb.al.dist"), axes[a]);
                if (ImGui::MenuItem(lab)) studioDistributeSelection(a);
            }
            ImGui::EndPopup();
        }
        if (studio::ribbonMenuButton("mtrans", Icon::Transform, L("rb.transform"))) {
            if (ImGui::MenuItem(L("rb.tr.rot90y"))) studioRotateSelectionY(90.f);
            if (ImGui::MenuItem(L("rb.tr.rotm90y"))) studioRotateSelectionY(-90.f);
            if (ImGui::MenuItem(L("rb.tr.rot90x"))) studioRotateSelectionAxis(0, 90.f);
            if (ImGui::MenuItem(L("rb.tr.rot90z"))) studioRotateSelectionAxis(2, 90.f);
            ImGui::Separator();
            static const char* axes[3] = {"X", "Y", "Z"};
            for (int a = 0; a < 3; ++a) {
                char lab[96];
                std::snprintf(lab, sizeof(lab), "%s %s", L("rb.tr.mirror"), axes[a]);
                if (ImGui::MenuItem(lab)) studioMirrorSelection(a);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(L("rb.tr.scale2"))) studioUniformScaleSelection(2.f);
            if (ImGui::MenuItem(L("rb.tr.scale05"))) studioUniformScaleSelection(0.5f);
            if (ImGui::MenuItem(L("rb.tr.matchsize"))) studioMatchSizeToPrimary();
            if (ImGui::MenuItem(L("rb.tr.matchcolor"))) studioMatchColorToPrimary();
            ImGui::Separator();
            if (ImGui::MenuItem(L("rb.tr.typein"))) g_showTransformTypeIn = true;
            if (ImGui::MenuItem(L("rb.pivot"), nullptr, g_showPivotTools)) g_showPivotTools = !g_showPivotTools;
            ImGui::EndPopup();
        }
        if (studio::ribbonMenuButton("mgroup", Icon::Package, L("rb.group"))) {
            if (ImGui::MenuItem(L("rb.group"), "Ctrl+G")) studioGroupSelection();
            if (ImGui::MenuItem(L("rb.gm"))) studioGroupSelectionAsModel();
            if (ImGui::MenuItem(L("rb.ungroup"), nullptr, false, !studioSelectedWholeGroups().empty()))
                studioUngroupSelection();
            ImGui::EndPopup();
        }
        studio::ribbonEndGroup(L("rb.g.arrange"));

        // Snap come in Roblox Studio: passo di spostamento (stud) e di rotazione (gradi).
        studio::ribbonBeginGroup();
        studio::ribbonBeginStack();
        ImGui::Checkbox(L("rb.snap.on"), &g_studioSnapEnabled);
        ImGui::SameLine();
        if (ImGui::SmallButton(g_studioLocalSpace ? L("rb.space.local") : L("rb.space.world")))
            g_studioLocalSpace = !g_studioLocalSpace;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("rb.space.tip"));
        ImGui::SetNextItemWidth(110.f);
        if (ImGui::DragFloat("##snapmove", &g_studioSnapMove, 0.05f, 0.05f, 64.f, L("rb.snap.move")))
            g_studioSnapMove = std::max(0.05f, g_studioSnapMove);
        ImGui::SetNextItemWidth(110.f);
        if (ImGui::DragFloat("##snaprot", &g_studioSnapRotate, 0.5f, 1.f, 180.f, L("rb.snap.rot")))
            g_studioSnapRotate = std::clamp(g_studioSnapRotate, 1.f, 180.f);
        studio::ribbonEndStack();
        studio::ribbonEndGroup(L("rb.g.snap"));

        studio::ribbonBeginGroup();
        if (studio::ribbonMenuButton("msel", Icon::Cursor, L("rb.selmenu"))) {
            if (ImGui::MenuItem(L("rb.sel.all"), "Ctrl+A")) studioSelectAllParts();
            if (ImGui::MenuItem(L("rb.sel.invert"))) studioInvertSelection();
            ImGui::Separator();
            if (ImGui::MenuItem(L("rb.sel.color"), nullptr, false, g_selectedPart >= 0)) studioSelectSimilar(0);
            if (ImGui::MenuItem(L("rb.sel.material"), nullptr, false, g_selectedPart >= 0)) studioSelectSimilar(1);
            if (ImGui::MenuItem(L("rb.sel.size"), nullptr, false, g_selectedPart >= 0)) studioSelectSimilar(2);
            if (ImGui::MenuItem(L("rb.sel.unanchored"))) studioSelectUnanchored();
            if (ImGui::MenuItem(L("rb.sel.unlocked"))) studioSelectUnlocked();
            ImGui::Separator();
            ImGui::MenuItem(L("rb.findpart"), nullptr, &g_showFindPart);
            ImGui::MenuItem(L("rb.tags"), nullptr, &g_showTagsWindow);
            ImGui::MenuItem(L("rb.selsets"), nullptr, &g_showSelectionSets);
            ImGui::MenuItem(L("rb.selhistory"), nullptr, &g_showSelectionHistory);
            ImGui::EndPopup();
        }
        toggle("mmeas", Icon::Scale, "rb.measure", g_showMeasure);
        if (studio::ribbonMenuButton("mtools", Icon::Transform, L("rb.toolsmenu"))) {
            ImGui::MenuItem(L("rb.batchrename"), nullptr, &g_showBatchRename);
            ImGui::MenuItem(L("rb.copyprops"), nullptr, &g_showCopyPasteProps);
            ImGui::MenuItem(L("rb.batchtools"), nullptr, &g_showBatchTools);
            ImGui::MenuItem(L("rb.physbatch"), nullptr, &g_showPhysicsBatch);
            ImGui::MenuItem(L("rb.bulkscripts"), nullptr, &g_showBulkScripts);
            ImGui::Separator();
            ImGui::MenuItem(L("rb.prefabs"), nullptr, &g_showQuickPrefabs);
            ImGui::MenuItem(L("rb.generators"), nullptr, &g_showKitGenerators);
            ImGui::Separator();
            if (ImGui::MenuItem(L("rb.randcolors"), nullptr, false, hasSel)) studioRandomizeColors();
            if (ImGui::MenuItem(L("rb.spawnhere"), nullptr, false, g_selectedPart >= 0)) studioSetSpawnFromSelection();
            ImGui::EndPopup();
        }
        studio::ribbonEndGroup(L("rb.g.selection"));

        studio::ribbonBeginGroup();
        studio::ribbonBeginStack();
        if (small("manc", Icon::Anchor, "rb.anchor", hasSel)) studioSetAnchoredOnSelection(true);
        if (small("muanc", Icon::Anchor, "rb.unanchor", hasSel)) studioSetAnchoredOnSelection(false);
        if (small("mcoll", Icon::Cube, "rb.collide", hasSel)) studioSetCanCollideOnSelection(true);
        studio::ribbonEndStack();
        studio::ribbonBeginStack();
        if (small("mncoll", Icon::Cube, "rb.nocollide", hasSel)) studioSetCanCollideOnSelection(false);
        if (small("mcgrp", Icon::Package, "rb.colgroups")) g_showCollisionGroups = !g_showCollisionGroups;
        studio::ribbonEndStack();
        if (studio::ribbonMenuButton("mcons", Icon::Anchor, L("rb.constraints"))) {
            if (ImGui::MenuItem(L("rb.c.weld"))) studioCreateWeldFromSelection();
            if (ImGui::MenuItem(L("rb.c.hinge"))) studioCreateConstraintFromSelection(ConstraintKind::Hinge);
            if (ImGui::MenuItem(L("rb.c.rope"))) studioCreateConstraintFromSelection(ConstraintKind::Rope);
            if (ImGui::MenuItem(L("rb.c.spring"))) studioCreateConstraintFromSelection(ConstraintKind::Spring);
            if (ImGui::MenuItem(L("rb.c.rod"))) studioCreateConstraintFromSelection(ConstraintKind::Rod);
            ImGui::Separator();
            if (ImGui::MenuItem(L("rb.c.join"))) studioJoinSurfaces();
            if (ImGui::MenuItem(L("rb.c.list"), nullptr, g_showWeldsWindow)) g_showWeldsWindow = !g_showWeldsWindow;
            ImGui::EndPopup();
        }
        studio::ribbonEndGroup(L("rb.g.physics"));

        studio::ribbonBeginGroup();
        if (studio::ribbonBigButton("mterr", Icon::Grid, L("rb.terrain"), g_terrainToolActive || g_showTerrainEditor)) {
            g_showTerrainEditor = true;
            g_terrainToolActive = !g_terrainToolActive;
        }
        if (studio::ribbonBigButton("mspawn", Icon::Walk, L("rb.spawn"))) studioRunInsertKind(7);
        if (studio::ribbonMenuButton("mlight", Icon::Light, L("rb.lighting"))) {
            ImGui::TextDisabled("%s", L("rb.clock"));
            studioClockTimeSlider();
            ImGui::Separator();
            ImGui::MenuItem(L("rb.lightpresets"), nullptr, &g_showLightingPresets);
            ImGui::MenuItem(L("rb.atmosphere"), nullptr, &g_showAtmospherePanel);
            ImGui::EndPopup();
        }
        studio::ribbonEndGroup(L("rb.g.world"));

        studio::ribbonBeginGroup();
        if (studio::ribbonMenuButton("mscr", Icon::Script, L("rb.scriptmenu"))) {
            ribbonScriptMenuItems();
            ImGui::EndPopup();
        }
        if (studio::ribbonBigButton("mobj", Icon::NewFile, L("rb.object"))) g_showInsertObject = true;
        studio::ribbonEndGroup(L("rb.g.advanced"), true);
    } else if (tab == 2) {   // ---- AVATAR ----
        studio::ribbonBeginGroup();
        if (studio::ribbonBigButton("aavat", Icon::Torso, L("rb.avatar"))) enterAvatarPublisher();
        if (studio::ribbonBigButton("arig", Icon::ArmRight, L("rb.rig"))) beginRigPlayer();
        if (studio::ribbonBigButton("afriend", Icon::Head, L("rb.friend"))) insertFriendPlayer();
        studio::ribbonBeginStack();
        if (small("ariged", Icon::Torso, "rb.rigeditor")) g_showPlayerRigEditor = !g_showPlayerRigEditor;
        if (small("ahum", Icon::Head, "rb.humdefaults")) g_showHumanoidDefaults = !g_showHumanoidDefaults;
        if (small("aanimid", Icon::Walk, "rb.animids")) g_showAnimIdPanel = !g_showAnimIdPanel;
        studio::ribbonEndStack();
        studio::ribbonEndGroup(L("rb.g.character"));
        studio::ribbonBeginGroup();
        toggle("astd", Icon::Walk, "rb.animstudio", g_showAnimStudio);
        if (studio::ribbonBigButton("aanim", Icon::Walk, L("rb.animate"))) beginAnimateSelectedModel();
        studio::ribbonBeginStack();
        if (small("arigan", Icon::Walk, "rb.animator")) g_showRigAnimator = !g_showRigAnimator;
        if (small("apub", Icon::Save, "rb.publishanim")) g_showPlayerAnimPublisher = true;
        studio::ribbonEndStack();
        studio::ribbonEndGroup(L("rb.g.anim"), true);
    } else if (tab == 3) {   // ---- INTERFACCIA ----
        studio::ribbonBeginGroup();
        if (studio::ribbonBigButton("utl", Icon::NewFile, L("rb.textlabel"))) insertNewTextLabel();
        if (studio::ribbonBigButton("utb", Icon::NewFile, L("rb.textbutton"))) insertNewTextButton();
        if (studio::ribbonBigButton("ufr", Icon::NewFile, L("rb.frame"))) insertNewFrame();
        if (studio::ribbonBigButton("uim", Icon::NewFile, L("rb.image"))) insertNewImageLabel();
        studio::ribbonEndGroup(L("rb.g.gui"));
        studio::ribbonBeginGroup();
        if (studio::ribbonBigButton("ul3", Icon::NewFile, L("rb.label3d"))) insertNewLabel();
        if (studio::ribbonBigButton("usnd", Icon::Package, L("rb.sound"))) insertSound();
        studio::ribbonEndGroup(L("rb.g.media"), true);
    } else if (tab == 4) {   // ---- SCRIPT ----
        studio::ribbonBeginGroup();
        if (studio::ribbonBigButton("snew", Icon::Script, L("rb.newscript"))) addScriptToSelected();
        if (studio::ribbonBigButton("splace", Icon::Script, L("rb.placescript"))) g_showScript = true;
        studio::ribbonEndGroup(L("rb.g.scripts"));
        studio::ribbonBeginGroup();
        toggle("sout", Icon::Script, "rb.output", g_showLuaOutput);
        toggle("sana", Icon::Grid, "rb.analysis", g_showScriptAnalysis);
        toggle("scmd", Icon::Script, "rb.cmdbar", g_showCommandBar);
        studio::ribbonEndGroup(L("rb.g.debug"));
        studio::ribbonBeginGroup();
        toggle("sapi", Icon::Folder, "rb.api", g_showApiReference);
        toggle("sgui", Icon::Folder, "rb.guide", g_showLuaGuide);
        studio::ribbonEndGroup(L("rb.g.help"), true);
    } else if (tab == 5) {   // ---- TEST ----
        studio::ribbonBeginGroup();
        if (!g_studioTesting) {
            if (studio::ribbonBigButton("tplay", Icon::Play, L("rb.play"))) startStudioTest(win);
        } else if (studio::ribbonBigButton("tstop", Icon::Stop, L("rb.stop"), true)) {
            stopStudioTest(win);
        }
        if (studio::ribbonBigButton("there", Icon::Play, L("rb.playhere"), false, !g_studioTesting)) studioPlayHere(win);
        if (studio::ribbonBigButton("tpause", Icon::Stop, L(g_simPaused ? "rb.resume" : "rb.pause"), g_simPaused,
                                    g_studioTesting))
            g_simPaused = !g_simPaused;
        studio::ribbonEndGroup(L("rb.g.play"));
        studio::ribbonBeginGroup();
        toggle("tmp", Icon::Head, "rb.multi", g_showMpTest);
        if (studio::ribbonBigButton("tclose", Icon::Trash, L("rb.closeplayers"), false, !g_mpTestClients.empty()))
            studioStopMultiplayerTest();
        toggle("tdev", Icon::Camera, "rb.device", g_showDeviceEmulator);
        studio::ribbonEndGroup(L("rb.g.clients"));
        studio::ribbonBeginGroup();
        toggle("tcon", Icon::Script, "rb.console", g_showDevConsole);
        toggle("tperf", Icon::Grid, "rb.perf", g_showScriptPerf);
        toggle("twatch", Icon::Camera, "rb.watch", g_showWatch);
        if (studio::ribbonBigButton("tmute", Icon::Light, L(g_audioMuted ? "rb.audiooff" : "rb.audioon"), g_audioMuted))
            studioToggleMute();
        if (studio::ribbonMenuButton("tdbg", Icon::Grid, L("rb.debugmenu"))) {
            ImGui::MenuItem(L("rb.coldebug"), nullptr, &g_showCollisionDebug);
            ImGui::MenuItem(L("rb.pathdebug"), nullptr, &g_showPathDebug);
            ImGui::MenuItem(L("rb.physpanel"), nullptr, &g_showPhysicsPanel);
            ImGui::MenuItem(L("rb.perfoverlay"), nullptr, &g_showPerfOverlay);
            ImGui::MenuItem(L("rb.stats"), nullptr, &g_showPerfStats);
            ImGui::Separator();
            ImGui::MenuItem(L("rb.validator"), nullptr, &g_showPlaceValidator);
            ImGui::EndPopup();
        }
        studio::ribbonEndGroup(L("rb.g.debug"), true);
    } else {   // ---- VISUALIZZA ----
        studio::ribbonBeginGroup();
        toggle("vexp", Icon::Folder, "rb.explorer", g_showExplorer);
        toggle("vprp", Icon::Transform, "rb.properties", g_showProps);
        toggle("vtbx", Icon::Package, "rb.toolbox", g_showToolbox);
        toggle("vout", Icon::Script, "rb.output", g_showLuaOutput);
        studio::ribbonEndGroup(L("rb.g.windows"));
        studio::ribbonBeginGroup();
        toggle("vgrid", Icon::Grid, "rb.grid", g_studioGridVisible);
        toggle("vwire", Icon::Cube, "rb.wireframe", g_viewWireframe);
        toggle("vcol", Icon::Cube, "rb.collisions", g_viewCollision);
        toggle("vcon", Icon::Anchor, "rb.constraints", g_showConstraintLines);
        studio::ribbonEndGroup(L("rb.g.show"));
        studio::ribbonBeginGroup();
        if (studio::ribbonBigButton("vfoc", Icon::Camera, L("rb.focus"), false, hasSel)) studioFocusSelection();
        if (studio::ribbonBigButton("viso", Icon::Grid, L("rb.isolate"), g_viewIsolate)) studioIsolateToggle();
        if (studio::ribbonMenuButton("vviews", Icon::Camera, L("rb.views"))) {
            static const char* kViewKeys[6] = {"rb.v.top", "rb.v.front", "rb.v.back", "rb.v.left", "rb.v.right", "rb.v.iso"};
            for (int v = 0; v < 6; ++v)
                if (ImGui::MenuItem(L(kViewKeys[v]))) studioViewFrom(v);
            ImGui::Separator();
            if (ImGui::MenuItem(L("rb.zoomext"))) studioZoomExtents();
            ImGui::MenuItem(L("rb.cambookmarks"), nullptr, &g_showCameraBookmarks);
            ImGui::MenuItem(L("rb.campanel"), nullptr, &g_showCameraPanel);
            ImGui::Separator();
            ImGui::SetNextItemWidth(140);
            ImGui::SliderFloat(L("rb.fov"), &g_cam.fov, 30.f, 110.f, "%.0f");
            ImGui::EndPopup();
        }
        studio::ribbonEndGroup(L("rb.g.camera"));
        studio::ribbonBeginGroup();
        if (studio::ribbonBigButton("vshot", Icon::Camera, L("rb.screenshot"))) studioRequestScreenshot();
#ifndef __ANDROID__
        if (studio::ribbonBigButton("vfull", Icon::Grid, L("rb.fullscreen"), win && glfwGetWindowMonitor(win)))
            studioToggleFullscreen(win);
#endif
        studio::ribbonEndGroup(L("rb.g.screen"));
        studio::ribbonBeginGroup();
        studio::ribbonBeginStack();
        if (small("vtags", Icon::Package, "rb.tags")) g_showTagsWindow = !g_showTagsWindow;
        if (small("vstats", Icon::Grid, "rb.stats")) g_showPerfStats = !g_showPerfStats;
        if (small("vkeys", Icon::Script, "rb.hotkeys")) g_showHotkeySheet = !g_showHotkeySheet;
        studio::ribbonEndStack();
        toggle("vsearch", Icon::Folder, "rb.search", g_showCommandPalette);
        if (studio::ribbonMenuButton("vmore", Icon::Folder, L("rb.morewin"))) {
            ImGui::MenuItem(L("rb.versions"), nullptr, &g_showVersionHistory);
            ImGui::MenuItem(L("rb.profiler"), "F4", &g_showProfiler);
            ImGui::MenuItem(L("rb.settings"), nullptr, &g_showGameSettings);
            ImGui::MenuItem(L("rb.attributes"), nullptr, &g_showAttributesEditor);
            ImGui::MenuItem(L("rb.quick"), nullptr, &g_showQuickActions);
            ImGui::MenuItem(L("rb.colgroups"), nullptr, &g_showCollisionGroups);
            ImGui::MenuItem(L("rb.terrain"), nullptr, &g_showTerrainEditor);
            ImGui::MenuItem(L("rb.findreplace"), nullptr, &g_showFindReplace);
            ImGui::Separator();
            ImGui::MenuItem(L("rb.sounds"), nullptr, &g_showSoundBrowser);
            ImGui::MenuItem(L("rb.assets"), nullptr, &g_showAssets);
            ImGui::MenuItem(L("rb.partcounter"), nullptr, &g_showPartCounter);
            ImGui::MenuItem(L("rb.plugins"), nullptr, &g_showPluginBar);
            ImGui::MenuItem(L("rb.prefs"), nullptr, &g_showStudioPrefs);
            ImGui::EndPopup();
        }
        studio::ribbonEndGroup(L("rb.g.tools2"), true);
    }

    s_contentW[tab] = ImGui::GetItemRectMax().x - contentX0 + 8.f;
    if (!g_exportMsg.empty()) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored({1.f, 0.85f, 0.4f, 1.f}, "%s", g_exportMsg.c_str());
    }
    ImGui::Unindent(indent);

    // Frecce di scorrimento sui bordi quando c'e' altro da vedere.
    const ImVec2 wp = ImGui::GetWindowPos();
    const float by0 = wp.y + studio::kRibbonTabHeight, by1 = wp.y + studio::kRibbonHeight;
    auto arrow = [&](bool left) {
        const float x0 = left ? wp.x : wp.x + ImGui::GetWindowWidth() - 18.f;
        ImGui::SetCursorScreenPos({x0, by0});
        ImGui::PushID(left ? "rbl" : "rbr");
        if (ImGui::InvisibleButton("##sc", {18.f, by1 - by0})) scroll += left ? -240.f : 240.f;
        const bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled({x0, by0}, {x0 + 18.f, by1}, hov ? IM_COL32(70, 78, 92, 240) : IM_COL32(45, 48, 54, 220));
        const float cy = (by0 + by1) * 0.5f, cx = x0 + 9.f;
        if (left) dl->AddTriangleFilled({cx + 4, cy - 7}, {cx + 4, cy + 7}, {cx - 4, cy}, IM_COL32(220, 225, 235, 255));
        else dl->AddTriangleFilled({cx - 4, cy - 7}, {cx - 4, cy + 7}, {cx + 4, cy}, IM_COL32(220, 225, 235, 255));
    };
    if (scroll > 0.5f) arrow(true);
    if (scroll < maxScroll - 0.5f) arrow(false);
}

static void drawStudioUi(AnimatedModel& fbx, bool fbxOk) {
    g_studioUiTime = (float)glfwGetTime();

    if (g_studioTesting) {
        // Barra Stop SEMPRE cliccabile: prima spariva con tutta la BuildBar e
        // col cursore locked sembrava che Test "non funzionasse".
        {
            const float barW = ImGui::GetIO().DisplaySize.x;
            ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
            ImGui::SetNextWindowSize({barW, 44}, ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.82f);
            ImGui::Begin("##TestStopBar", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);
            ImGui::TextColored({0.3f, 1.f, 0.5f, 1.f}, "%s", L("test.playing"));
            ImGui::SameLine();
            ImGui::TextDisabled("%s", g_world.placeName.c_str());
            ImGui::SameLine(barW - 100.f);
            ImGui::PushStyleColor(ImGuiCol_Button, {0.82f, 0.22f, 0.18f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.32f, 0.25f, 1.f});
            if (ImGui::Button(LB("stop", "stopbtn"), {88, 28})) {
                if (g_playCursorLocked)
                    setPlayCursor(glfwGetCurrentContext(), false);
                stopStudioTest(glfwGetCurrentContext());
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Esc / F1 = stop");
            ImGui::End();
        }
        // Se il mouse e' sulla barra Stop, sblocca il cursore cosi' il click arriva.
        if (g_playCursorLocked && ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)
            && ImGui::GetIO().MousePos.y < 48.f) {
            setPlayCursor(glfwGetCurrentContext(), false);
        }
        drawHealthBarHud();
        drawGeneratedHud();
        drawPlayerHeadHealthBar();
        drawHamburgerMenuHud();
        drawChatBubbleHud();
        drawFpCrosshair();
        drawSpeechBubbles();
        drawInventoryHud();
        drawWorldLabels();
        if (g_showChat) {
            ImGui::SetNextWindowPos({12, 112}, ImGuiCond_Appearing);
            ImGui::SetNextWindowSize({360, 220}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowBgAlpha(0.92f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.08f, 0.22f, 0.48f, 0.95f});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
            ImGui::Begin("Chat##testchat", &g_showChat);
            ImGui::BeginChild("##tlog", {0, -36}, false);
            for (const auto& line : g_menu.chatLines)
                ImGui::TextWrapped("%s", line.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
                ImGui::SetScrollHereY(1.f);
            ImGui::EndChild();
            bool send = ImGui::InputText("##tmsg", g_studioChatBuf, sizeof(g_studioChatBuf),
                ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if ((ImGui::Button("Invia") || send) && g_studioChatBuf[0]) {
                sendPlayChat(g_studioChatBuf);
                g_studioChatBuf[0] = 0;
            }
            ImGui::End();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        drawSpeechBubbles();
        return;
    }

    if (ImGui::BeginMainMenuBar()) {
        // Menu stile Roblox Studio: brand discreto + File/Edit/View…
        ImGui::TextColored({0.55f, 0.58f, 0.62f, 1.f}, "Blocky Studio");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", g_world.placeName.c_str());
        ImGui::SameLine();
        ImGui::Spacing();
        if (ImGui::BeginMenu(L("file"))) {
            if (ImGui::MenuItem(L("menu.new_place"), "Ctrl+N")) {
                requestNav(glfwGetCurrentContext(), PendingNav::NewPlace);
            }
            if (ImGui::BeginMenu(L("menu.new_map"))) {
                if (ImGui::MenuItem(L("map.baseplate"))) applyMapTemplate(0);
                if (ImGui::MenuItem(L("map.garden"))) applyMapTemplate(1);
                if (ImGui::MenuItem(L("map.obby"))) applyMapTemplate(2);
                if (ImGui::MenuItem(L("map.arena"))) applyMapTemplate(3);
                if (ImGui::MenuItem(L("map.park"))) applyMapTemplate(4);
                if (ImGui::MenuItem(L("map.city"))) applyMapTemplate(5);
                ImGui::Separator();
                ImGui::TextDisabled("Game kits (script pronti)");
                if (ImGui::MenuItem("FPS Kit (hitscan)")) applyMapTemplate(6);
                if (ImGui::MenuItem("Tycoon Kit (Coins)")) applyMapTemplate(7);
                if (ImGui::MenuItem("Survival Kit (NPC)")) applyMapTemplate(8);
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(L("menu.open_place"), "Ctrl+O")) doOpenPlace();
            if (ImGui::MenuItem(L("menu.save_place"), "Ctrl+S")) doSavePlace();
            if (ImGui::MenuItem(L("menu.save_place_as"), "Ctrl+Shift+S")) doSavePlaceAs();
            if (ImGui::MenuItem("Salva per pubblicare", "Ctrl+Shift+U")) doSaveForPublish();
            if (ImGui::MenuItem(L("menu.save_project"))) doSaveProject();
            ImGui::Separator();
            if (ImGui::MenuItem(L("menu.export_place"), "Ctrl+E")) doExport();
            if (ImGui::MenuItem(L("menu.publish"), "Ctrl+Shift+P")) {
                if (publishCurrentGame()) {
                    refreshGamesList();
                    CrashHandler::note("Publish da menu File OK");
                }
            }
            ImGui::TextDisabled("Salva · Salva per pubblicare (bozza) · Pubblica");
            ImGui::TextDisabled("Max %d giochi / 2 mesi per account", kMaxPublishedGamesPerUser);
            if (g_publishDraftActive)
                ImGui::TextColored({0.35f, 0.9f, 0.55f, 1.f}, "Autosave bozza publish ON");
            if (!g_placeFilePath.empty())
                ImGui::TextDisabled("%s", g_placeFilePath.c_str());
            ImGui::Separator();
            if (kIsGameEngine) {
                if (ImGui::MenuItem("Home"))
                    requestNav(glfwGetCurrentContext(), PendingNav::GoHome);
                if (ImGui::MenuItem(L("menu.exit_engine")))
                    requestNav(glfwGetCurrentContext(), PendingNav::QuitApp);
            } else {
                if (ImGui::MenuItem(L("menu.back_blocky")))
                    returnToMenu(glfwGetCurrentContext());
                if (ImGui::MenuItem(L("menu.exit")))
                    glfwSetWindowShouldClose(glfwGetCurrentContext(), true);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("insert"))) {
            if (ImGui::MenuItem(L("menu.part"), "Ctrl+P")) insertNewPart();
            if (ImGui::MenuItem("Ball (sfera)")) insertShapedPart(PartShape::Ball);
            if (ImGui::MenuItem("Cylinder (cilindro)")) insertShapedPart(PartShape::Cylinder);
            if (ImGui::MenuItem("Wedge (triangolo/rampa)")) insertShapedPart(PartShape::Wedge);
            if (ImGui::MenuItem("Neon Part (lucente)")) insertNeonPart();
            if (ImGui::MenuItem("PointLight")) insertPointLightPart();
            if (ImGui::MenuItem("Explode Part")) insertExplodePart();
            ImGui::Separator();
            ImGui::MenuItem("Toolbox (oggetti pronti)...", nullptr, &g_showToolbox);
            if (ImGui::MenuItem("Player amico")) insertFriendPlayer();
            if (ImGui::MenuItem(L("menu.model"), "Ctrl+M")) insertImportedModel();
            if (ImGui::MenuItem(L("menu.textlabel"), "Ctrl+Shift+T")) insertNewTextLabel();
            if (ImGui::MenuItem(L("menu.label"), "Ctrl+T")) insertNewLabel();
            if (ImGui::MenuItem(L("menu.textbutton"), "Ctrl+B")) insertNewTextButton();
            if (ImGui::MenuItem(L("menu.sound"), "Ctrl+U")) insertSound();
            ImGui::Separator();
            if (ImGui::BeginMenu("Particles")) {
                glm::vec3 at = g_cam.pos + g_cam.front() * 8.f;
                if (g_selectedPart >= 0 && g_selectedPart < (int)g_world.parts.size())
                    at = g_world.parts[g_selectedPart].position + glm::vec3(0, 2, 0);
                if (ImGui::MenuItem("Sparks"))
                    g_particles.emitPreset(ParticlePreset::Sparks, at);
                if (ImGui::MenuItem("Smoke"))
                    g_particles.emitPreset(ParticlePreset::Smoke, at);
                if (ImGui::MenuItem("Coins"))
                    g_particles.emitPreset(ParticlePreset::Coins, at);
                if (ImGui::MenuItem("Burst"))
                    g_particles.emitPreset(ParticlePreset::Burst, at);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Backpack Item...")) {
                static int hue = 0;
                g_world.addBackpackItem("Item", hue++);
                g_exportMsg = "Aggiunto al Backpack — si vede in Test inventario";
            }
            if (ImGui::BeginMenu("Backpack presets")) {
                if (ImGui::MenuItem("Sword")) g_world.addBackpackItem("Sword", 3, BackpackTool::Sword);
                if (ImGui::MenuItem("Pistola")) g_world.addBackpackItem("Pistola", 4, BackpackTool::Pistol);
                if (ImGui::MenuItem("Potion")) g_world.addBackpackItem("Potion", 1);
                if (ImGui::MenuItem("Key")) g_world.addBackpackItem("Key", 2);
                if (ImGui::MenuItem("Shield")) g_world.addBackpackItem("Shield", 0);
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::BeginMenu(L("menu.props"))) {
                if (ImGui::MenuItem(L("menu.tree"))) insertPropPreset(0);
                if (ImGui::MenuItem(L("menu.house"))) insertPropPreset(1);
                if (ImGui::MenuItem(L("menu.ramp"))) insertPropPreset(2);
                if (ImGui::MenuItem(L("menu.tower"))) insertPropPreset(3);
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("edit"))) {
            if (ImGui::MenuItem("Annulla", "Ctrl+Z", false, g_undo.canUndo())) {
                g_undo.applyUndo(g_world, g_selectedPart);
                studioFollowRebaseline();
                g_exportMsg = "Annullato";
            }
            if (ImGui::MenuItem("Ripeti", "Ctrl+Y", false, g_undo.canRedo())) {
                g_undo.applyRedo(g_world, g_selectedPart);
                studioFollowRebaseline();
                g_exportMsg = "Ripetuto";
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Raggruppa in Model", "Ctrl+G")) studioGroupSelection();
            if (ImGui::MenuItem("Separa Model", "Ctrl+U", false, !studioSelectedWholeGroups().empty()))
                studioUngroupSelection();
            if (ImGui::MenuItem("Duplica selezione", "Ctrl+D")) studioDuplicateSelection();
            ImGui::Separator();
            if (ImGui::MenuItem(L("menu.dup"), "Ctrl+D")) studioDuplicateSelectionOffset();
            if (ImGui::MenuItem(L("menu.script_part"))) addScriptToSelected();
            if (ImGui::MenuItem(L("menu.delete"), "Del")) deleteSelected();
            drawStudioFeaturesEditMenu();
            drawStudioProEditMenu();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("player"))) {
            if (ImGui::MenuItem(L("menu.avatar_catalog"), "F8"))
                enterAvatarPublisher();
            if (ImGui::MenuItem(L("menu.rig_player"), "Ctrl+Shift+R"))
                beginRigPlayer();
            if (ImGui::MenuItem(L("menu.publish_anim"), "Ctrl+Shift+I"))
                g_showPlayerAnimPublisher = true;
            ImGui::Separator();
            if (ImGui::MenuItem(L("menu.play_idle"))) g_animator.play("Idle");
            if (ImGui::MenuItem(L("menu.play_walk"))) g_animator.play("Walk");
            if (ImGui::MenuItem(L("menu.stop_anim"))) g_animator.stop();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("animation"))) {
            ImGui::MenuItem("Blocky Animator (personaggio)...", nullptr, &g_showRigAnimator);
            ImGui::MenuItem("Animation Studio (rig, IK, curve)...", nullptr, &g_showAnimStudio);
            ImGui::Separator();
            if (ImGui::MenuItem(L("menu.animate_model"), "Ctrl+Shift+A"))
                beginAnimateSelectedModel();
            ImGui::Separator();
            ImGui::TextDisabled("Player (corpo R6):");
            if (ImGui::MenuItem(L("menu.play_idle"))) g_animator.play("Idle");
            if (ImGui::MenuItem(L("menu.play_walk"))) g_animator.play("Walk");
            if (ImGui::MenuItem(L("menu.stop_anim"))) g_animator.stop();
            ImGui::MenuItem("Dock clip Player", nullptr, &g_showAnimDock);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Script")) {
            ImGui::MenuItem("Riferimento API (Roblox-style)", nullptr, &g_showApiReference);
            ImGui::MenuItem("Output Lua (print)", nullptr, &g_showLuaOutput);
            ImGui::Separator();
            ImGui::MenuItem("Guida Lua Blocky", nullptr, &g_showLuaGuide);
            if (ImGui::MenuItem("Apri script globale place"))
                g_showScript = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Test")) {
            ImGui::MenuItem("Test multiplayer (2-4 giocatori)...", nullptr, &g_showMpTest);
            if (ImGui::MenuItem("Chiudi i giocatori di prova", nullptr, false, !g_mpTestClients.empty()))
                studioStopMultiplayerTest();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(L("view"))) {
            ImGui::MenuItem(L("menu.explorer"), nullptr, &g_showExplorer);
            ImGui::MenuItem(L("menu.properties"), nullptr, &g_showProps);
            ImGui::MenuItem(L("menu.create_panel"), nullptr, &g_showCreatePanel);
            ImGui::MenuItem(L("menu.global_script"), nullptr, &g_showScript);
            ImGui::MenuItem("Guida Lua Blocky", nullptr, &g_showLuaGuide);
            ImGui::MenuItem("Riferimento API", nullptr, &g_showApiReference);
            ImGui::MenuItem("Output Lua", nullptr, &g_showLuaOutput);
            ImGui::MenuItem(L("menu.engine_settings"), nullptr, &g_showPlatformPanel);
            ImGui::MenuItem(L("menu.chat"), nullptr, &g_showChat);
            ImGui::MenuItem("Statistiche prestazioni", "F4", &g_showProfiler);
            drawViewMenuPlatformItems();
            drawStudioFeaturesViewMenu();
            drawStudio2020ViewMenu();
            drawStudioProViewMenu();
            ImGui::Separator();
            ImGui::TextDisabled("%s", L("lang"));
            drawLanguageSelector("studiolang");
            ImGui::EndMenu();
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 320);
        drawLanguageButton("menulang");
        ImGui::SameLine();
        ImGui::TextDisabled("%.0f FPS", g_fps);
        ImGui::EndMainMenuBar();
    }

    const float menuH = ImGui::GetFrameHeight();

    // Shell Roblox Studio (come screenshot):
    //   SX = Toolbox · DX = Explorer (alto) + Properties (basso) · basso = Output
    constexpr float kToolboxW = 280.f;
    constexpr float kStudioPanelRight = 320.f;
    constexpr float kOutputH = 140.f;
    constexpr float kRibbonH = studio::kRibbonHeight;
    constexpr ImGuiWindowFlags kStudioPanelFlags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    const float ribbonY = menuH;
    const float panelY = menuH + kRibbonH;
    const bool bottomDockOpen = !g_studioTesting
        && (g_showLuaOutput || g_showAssets || g_showConsole);
    const float chromeReserve = studioBottomChromeReserve();
    const float dockH = bottomDockOpen ? kOutputH : 0.f;
    const float outputReserve = dockH + chromeReserve;
    const float panelH = (float)g_h - panelY - outputReserve;

    const float safePanelH = std::max(64.f, panelH);
    const float safeOutputH = dockH > 0.f ? std::max(80.f, dockH) : 0.f;
    const float leftW = (g_showToolbox && !g_studioTesting) ? kToolboxW : 0.f;
    const float rightW = ((g_showExplorer || g_showProps) && !g_studioTesting) ? kStudioPanelRight : 0.f;
    const float rightX = (float)g_w - rightW;
    // Explorer ~55% / Properties ~45% impilati a destra (come Studio).
    const float explorerH = g_showExplorer
        ? (g_showProps ? std::floor(safePanelH * 0.55f) : safePanelH)
        : 0.f;
    const float propsH = g_showProps
        ? (g_showExplorer ? std::max(64.f, safePanelH - explorerH) : safePanelH)
        : 0.f;
    const float propsY = panelY + (g_showExplorer ? explorerH : 0.f);

    ImGui::SetNextWindowPos({0.f, ribbonY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({std::max(1.f, (float)g_w), kRibbonH}, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.20f, 0.20f, 0.21f, 1.f});
    if (ImGui::Begin("##StudioRibbon", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        drawStudioRibbonBody(g_studioRibbonTab);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    drawPlayerRigWindows();
    drawModelAnimWindows();

    if (g_showCreatePanel && !g_studioTesting) {
        float appear = studioEaseOut(std::min(1.f, g_studioUiTime * 0.8f));
        ImGui::SetNextWindowPos({(float)g_w * 0.5f - 340.f, menuH + 72}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({680, 168}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92f * appear);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f);
        ImGui::Begin("Create##create", &g_showCreatePanel);
        ImGui::TextDisabled("Mappe e props — chiudi con X se non ti serve");
        ImGui::Separator();
        ImGui::Text("Mappe");
        if (ImGui::Button(LB("map.baseplate", "mbp"), {90, 28})) applyMapTemplate(0);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.garden", "mgd"), {80, 28})) applyMapTemplate(1);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.obby", "mob"), {70, 28})) applyMapTemplate(2);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.arena", "mar"), {70, 28})) applyMapTemplate(3);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.park", "mpk"), {70, 28})) applyMapTemplate(4);
        ImGui::SameLine();
        if (ImGui::Button(LB("map.city", "mct"), {70, 28})) applyMapTemplate(5);

        ImGui::Spacing();
        ImGui::Text("Props");
        if (ImGui::Button(LB("menu.tree", "ptree"), {70, 28})) insertPropPreset(0);
        ImGui::SameLine();
        if (ImGui::Button(LB("menu.house", "phouse"), {70, 28})) insertPropPreset(1);
        ImGui::SameLine();
        if (ImGui::Button(LB("menu.ramp", "pramp"), {70, 28})) insertPropPreset(2);
        ImGui::SameLine();
        if (ImGui::Button(LB("menu.tower", "ptower"), {70, 28})) insertPropPreset(3);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    if (g_showAnimDock && !g_studioTesting) {
        ImGui::SetNextWindowPos({(float)g_w - 280.f, (float)g_h - 160.f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({260, 130}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Clip rapide##animdock", &g_showAnimDock);
        if (ImGui::Button("Idle", {56, 26})) g_animator.play("Idle");
        ImGui::SameLine();
        if (ImGui::Button("Walk", {56, 26})) g_animator.play("Walk");
        ImGui::SameLine();
        if (ImGui::Button("Stop", {56, 26})) g_animator.stop();
        if (ImGui::Button("Pubblica AnimationId...", {-1, 28}))
            g_showPlayerAnimPublisher = true;
        ImGui::TextDisabled("%s", g_animator.lastPlayed.c_str());
        ImGui::End();
    }

    if (!g_studioTesting)
        drawStudioPlayerAnimPanel();

    if (g_showExplorer) {
        ImGui::SetNextWindowPos({rightX, panelY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({kStudioPanelRight, std::max(64.f, explorerH)}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.f);
        studio_chrome::pushDockPanelColors();
        ImGui::Begin("Explorer", &g_showExplorer, kStudioPanelFlags);

        drawRobloxExplorer();   // main_ui/StudioExplorer.h: albero stile Roblox Studio
        (void)fbxOk;
        ImGui::End();
        studio_chrome::popDockPanelColors();
    }
    drawViewportContextMenu();   // clic destro fermo sul viewport (StudioExplorer.h)

    if (g_showProps) {
        ImGui::SetNextWindowPos({rightX, propsY}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({kStudioPanelRight, std::max(64.f, propsH)}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.f);
        studio_chrome::pushDockPanelColors();
        ImGui::Begin("Properties", &g_showProps, kStudioPanelFlags);

        if (g_selectedService == 0) {
            studio_chrome::drawClassNameBanner("Lighting", "game.Lighting");
            studio_chrome::section("Grafica");
            ImGui::TextWrapped(
                "Performance / Studio Default / Beautiful — Quality + luce + post.");
            {
                if (ImGui::Button("Performance##studiogfx")) {
                    applyStudioGraphicsPreset(StudioGraphicsPreset::Performance,
                                             g_engine.quality, g_look, g_lighting);
                    g_lookPresetUi = 0;
                    g_exportMsg = "Grafica: Performance";
                    markPlaceDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button("Studio Default##studiogfx")) {
                    applyStudioGraphicsPreset(StudioGraphicsPreset::StudioDefault,
                                             g_engine.quality, g_look, g_lighting);
                    g_lookPresetUi = 0;
                    g_exportMsg = "Grafica: Studio Default";
                    markPlaceDirty();
                }
                ImGui::SameLine();
                if (ImGui::Button("Beautiful##studiogfx")) {
                    applyStudioGraphicsPreset(StudioGraphicsPreset::Beautiful,
                                             g_engine.quality, g_look, g_lighting);
                    g_lookPresetUi = (int)LookPreset::CleanAaa;
                    g_exportMsg = "Grafica: Beautiful";
                    markPlaceDirty();
                }
            }
            ImGui::Checkbox("Ombre (quality.shadows)", &g_engine.quality.shadows);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Deve essere ON insieme a Ombre sole/luna sotto.");

            ImGui::Separator();
            ImGui::TextColored({0.95f, 0.55f, 0.85f, 1.f}, "Look AAA (atmosfera)");
            ImGui::TextWrapped(
                "Preset atmosfera (fog/luce/grade). Preferisci Grafica Studio sopra "
                "per un look di base; questi sono varianti cinematiche.");
            {
                const char* lookNames[] = {
                    "Default (off)", "Forest (bosco)", "Cinematic", "Golden Hour",
                    "Night Neon", "Clean AAA", "Misty / Atmosphere"
                };
                if (ImGui::Combo("Preset Look##lookaaa", &g_lookPresetUi, lookNames,
                                 IM_ARRAYSIZE(lookNames))) {
                    applyLookPreset((LookPreset)g_lookPresetUi, g_look, g_lighting);
                    if (g_look.enabled) {
                        if (g_look.wantPostFx) g_engine.quality.postFx = true;
                        if (g_look.wantShadows) {
                            g_lighting.globalShadows = true;
                            g_engine.quality.shadows = true;
                            if (g_engine.quality.shadowOccluders < 4)
                                g_engine.quality.shadowOccluders = 8;
                        }
                        g_exportMsg = std::string("Look AAA: ") + g_look.presetName;
                        markPlaceDirty();
                    } else {
                        g_exportMsg = "Look AAA disattivato";
                    }
                }
            }
            if (g_look.enabled) {
                ImGui::TextDisabled("Attivo: %s", g_look.presetName);
                ImGui::SliderFloat("Exposure", &g_look.exposure, 0.5f, 1.8f, "%.2f");
                ImGui::SliderFloat("Contrast", &g_look.contrast, 0.7f, 1.6f, "%.2f");
                ImGui::SliderFloat("Saturation", &g_look.saturation, 0.5f, 1.6f, "%.2f");
                ImGui::SliderFloat("Vignette", &g_look.vignette, 0.f, 0.8f, "%.2f");
                ImGui::SliderFloat("Bloom", &g_look.bloomIntensity, 0.f, 3.5f, "%.2f");
            }
            ImGui::Separator();

            ImGui::TextColored({0.55f, 0.85f, 1.f, 1.f}, "Giorno / Notte");
            int mode = (int)g_lighting.dayNight;
            bool modeChanged = false;
            modeChanged |= ImGui::RadioButton("Ciclo giorno/notte", &mode, 0);
            modeChanged |= ImGui::RadioButton("Solo giorno", &mode, 1);
            ImGui::SameLine();
            modeChanged |= ImGui::RadioButton("Solo notte", &mode, 2);
            if (modeChanged) {
                g_lighting.dayNight = (DayNightMode)mode;
                markPlaceDirty();
            }
            if (g_lighting.dayNight == DayNightMode::Cycle) {
                ImGui::SliderFloat("Durata giornata", &g_lighting.dayLengthMinutes,
                                   0.25f, 60.f, "%.2f min");
                ImGui::TextDisabled("Un giro completo 0h → 24h in %.1f min",
                                    g_lighting.dayLengthMinutes);
                ImGui::TextColored({0.55f, 0.85f, 1.f, 1.f},
                    "Il tempo scorre solo in Test (F5) e in Play.");
                ImGui::TextDisabled("Qui in Studio resta fermo su ClockTime.");
            } else if (g_lighting.dayNight == DayNightMode::DayOnly) {
                ImGui::SliderFloat("Ora fissa", &g_lighting.fixedDayClock, 7.f, 17.f, "%.1f h");
            } else {
                ImGui::SliderFloat("Ora fissa", &g_lighting.fixedNightClock, 0.f, 24.f, "%.1f h");
                ImGui::TextDisabled("Sopra l'orizzonte c'è la luna: ombre più deboli.");
            }
            ImGui::Checkbox("Ombre da sole/luna", &g_lighting.globalShadows);
            if (g_engine.quality.shadowOccluders <= 0)
                ImGui::TextColored({1.f, 0.65f, 0.3f, 1.f},
                                   "Preset grafico Low: ombre disattivate");
            ImGui::Separator();

            ImGui::SliderFloat("ClockTime", &g_lighting.clockTime, 0.f, 24.f, "%.1f h");
            if (g_lighting.dayNight != DayNightMode::Cycle)
                ImGui::TextDisabled("(sovrascritto dall'ora fissa qui sopra)");
            ImGui::SliderFloat("Brightness", &g_lighting.brightness, 0.f, 10.f, "%.2f");
            ImGui::SliderFloat("Ambient", &g_lighting.ambient, 0.f, 1.f, "%.2f");
            ImGui::ColorEdit3("OutdoorAmbient", &g_lighting.outdoorAmbient.x);
            ImGui::ColorEdit3("FogColor", &g_lighting.fogColor.x);
            ImGui::DragFloat("FogStart", &g_lighting.fogStart, 1.f, 0.f, 5000.f);
            ImGui::DragFloat("FogEnd", &g_lighting.fogEnd, 1.f, 1.f, 8000.f);
            ImGui::Checkbox("Atmosphere", &g_lighting.atmosphereEnabled);
            if (g_lighting.atmosphereEnabled) {
                ImGui::SliderFloat("Atm Density", &g_lighting.atmosphereDensity, 0.f, 1.f);
                ImGui::SliderFloat("Atm Haze", &g_lighting.atmosphereHaze, 0.f, 1.f);
            }
            ImGui::Checkbox("SunRays", &g_lighting.sunRaysEnabled);
            if (g_lighting.sunRaysEnabled)
                ImGui::SliderFloat("SunRays Intensity", &g_lighting.sunRaysIntensity, 0.f, 2.f);
            if (ImGui::Button("Mezzogiorno (12)")) g_lighting.clockTime = 12.f;
            ImGui::SameLine();
            if (ImGui::Button("Tramonto (19)")) g_lighting.clockTime = 19.f;
            ImGui::SameLine();
            if (ImGui::Button("Notte (22)")) g_lighting.clockTime = 22.f;
            g_lighting.clampAll();
            ImGui::TextDisabled("Sole: %s · notte %.0f%%",
                                g_lighting.sunIsUp() ? "sopra l'orizzonte" : "tramontato",
                                g_lighting.nightAmount() * 100.f);
            ImGui::TextDisabled("Lua: game.Lighting:SetClockTime(14)");
        } else if (g_selectedService == 1) {
            ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "Services · RunService");
            ImGui::TextWrapped(
                "Heartbeat / Stepped / RenderStepped :Connect(fn(dt))\n"
                "IsStudio · IsClient · IsServer — come Roblox.");
            ImGui::TextDisabled("Attivo in Test (F5) e Play.");
        } else if (g_selectedService == 2) {
            ImGui::TextColored({0.75f, 0.55f, 1.f, 1.f}, "Services · TweenService");
            ImGui::TextWrapped("TweenService:Create(...):Play()");
        } else if (g_selectedService == 3) {
            ImGui::TextColored({0.55f, 0.95f, 0.65f, 1.f}, "Services · UserInputService");
            ImGui::TextWrapped("DisableJump / EnableJump da Lua.");
        } else if (g_selectedService == 4) {
            ImGui::TextColored({0.95f, 0.75f, 0.35f, 1.f}, "Services · Chat");
            ImGui::TextWrapped("Chat in-game (menu Chat). GetService(\"Chat\") disponibile.");
        } else if (g_selectedService == 5) {
            ImGui::TextColored({0.9f, 0.5f, 0.4f, 1.f}, "Services · Debris");
            ImGui::TextWrapped("Debris:AddItem(part, seconds) — rimozione parti.");
        } else if (g_selectedService == studio::platform::Svc_ReplicatedStorage
                   || g_selectedService == studio::platform::Svc_ServerStorage
                   || g_selectedService == studio::platform::Svc_ServerScriptService) {
            SharedService svc = SharedService::ReplicatedStorage;
            const char* title = "ReplicatedStorage";
            if (g_selectedService == studio::platform::Svc_ServerStorage) {
                svc = SharedService::ServerStorage;
                title = "ServerStorage";
            } else if (g_selectedService == studio::platform::Svc_ServerScriptService) {
                svc = SharedService::ServerScriptService;
                title = "ServerScriptService";
            }
            ImGui::TextColored({0.5f, 0.8f, 1.f, 1.f}, "Services · %s", title);
            ImGui::TextWrapped(
                "Container professionale: Folder, ModuleScript (require), RemoteEvent.\n"
                "Luau: local M = require(game.ReplicatedStorage.MyModule)");
            if (ImGui::Button("+ ModuleScript")) {
                auto& s = g_world.addShared(svc, "ModuleScript",
                    "Module" + std::to_string(g_world.instances.size() + 1));
                s.source = "local module = {}\n\nfunction module.hello()\n"
                           "    print(\"hello from\", script.Name)\nend\n\nreturn module\n";
                g_selectedSharedInstance = (int)g_world.instances.size() - 1;
                markPlaceDirty();
            }
            ImGui::SameLine();
            if (ImGui::Button("+ RemoteEvent")) {
                g_world.addShared(svc, "RemoteEvent",
                    "Remote" + std::to_string(g_world.instances.size() + 1));
                g_selectedSharedInstance = (int)g_world.instances.size() - 1;
                markPlaceDirty();
            }
            ImGui::SameLine();
            if (ImGui::Button("+ RemoteFunction")) {
                g_world.addShared(svc, "RemoteFunction",
                    "RemoteFn" + std::to_string(g_world.instances.size() + 1));
                g_selectedSharedInstance = (int)g_world.instances.size() - 1;
                markPlaceDirty();
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Folder")) {
                g_world.addShared(svc, "Folder",
                    "Folder" + std::to_string(g_world.instances.size() + 1));
                g_selectedSharedInstance = (int)g_world.instances.size() - 1;
                markPlaceDirty();
            }
            if (svc == SharedService::ServerScriptService) {
                ImGui::SameLine();
                if (ImGui::Button("+ Script")) {
                    auto& s = g_world.addShared(svc, "Script",
                        "Script" + std::to_string(g_world.instances.size() + 1));
                    s.source =
                        "-- Server Script (SSS)\n"
                        "print(script.Name, \"boot\")\n";
                    g_selectedSharedInstance = (int)g_world.instances.size() - 1;
                    markPlaceDirty();
                }
            }
            if (g_selectedSharedInstance >= 0
                && g_selectedSharedInstance < (int)g_world.instances.size()) {
                auto& s = g_world.instances[g_selectedSharedInstance];
                ImGui::Separator();
                ImGui::Text("%s · %s", s.className.c_str(), s.name.c_str());
                char nbuf[64];
                std::snprintf(nbuf, sizeof(nbuf), "%s", s.name.c_str());
                if (ImGui::InputText("Name##sh", nbuf, sizeof(nbuf))) {
                    s.name = nbuf;
                    markPlaceDirty();
                }
                if (s.className == "ModuleScript" || s.className == "Script"
                    || s.className == "LocalScript") {
                    static char srcBuf[8192];
                    static int synced = -1;
                    if (synced != g_selectedSharedInstance) {
                        synced = g_selectedSharedInstance;
                        std::snprintf(srcBuf, sizeof(srcBuf), "%s", s.source.c_str());
                    }
                    ImGui::TextDisabled(s.className == "ModuleScript"
                        ? "Source (return module)" : "Source (auto-run on Play)");
                    if (ImGui::InputTextMultiline("##modsrc", srcBuf, sizeof(srcBuf),
                            {-1.f, 180.f})) {
                        s.source = srcBuf;
                        markPlaceDirty();
                    }
                }
                if (ImGui::Button("Elimina Instance")) {
                    g_world.instances.erase(g_world.instances.begin() + g_selectedSharedInstance);
                    for (auto& o : g_world.instances)
                        if (o.parentIndex == g_selectedSharedInstance) o.parentIndex = -1;
                        else if (o.parentIndex > g_selectedSharedInstance) --o.parentIndex;
                    g_selectedSharedInstance = -1;
                    markPlaceDirty();
                }
            }
        } else if (g_selectedService == 7) {
            ImGui::TextColored({0.6f, 0.9f, 1.f, 1.f}, "Services · StarterGui");
            ImGui::TextWrapped("Contiene ScreenGui → TextLabel / Label / TextButton.");
        } else if (g_selectedService == 8) {
            ImGui::TextColored({0.55f, 0.75f, 1.f, 1.f}, "Services · DataStoreService");
            ImGui::TextWrapped("GetDataStore(name):GetAsync / SetAsync / IncrementAsync");
        } else if (g_selectedService == 9) {
            ImGui::TextColored({0.7f, 0.95f, 0.55f, 1.f}, "Services · CollectionService");
            ImGui::TextWrapped("AddTag · RemoveTag · HasTag · GetTagged · GetTags");
            ImGui::TextDisabled("Alias interno: global Tag (id numerici).");
        } else if (g_selectedService == 10) {
            ImGui::TextColored({0.95f, 0.7f, 0.45f, 1.f}, "Services · Players");
            ImGui::TextWrapped("game.Players.LocalPlayer · Humanoid · Character");
        } else if (g_selectedService >= 12) {
            drawServicePropertyFallback(g_selectedService);
        } else if (g_selectedLabel >= 0 && g_selectedLabel < (int)g_world.labels.size()) {
            auto& l = g_world.labels[g_selectedLabel];
            ImGui::TextColored({0.35f, 0.85f, 1.f, 1.f}, "ScreenGui · %s",
                               l.className.empty() ? "TextLabel" : l.className.c_str());
            if (ImGui::InputText("Name", g_labelNameBuf, sizeof(g_labelNameBuf)))
                l.name = g_labelNameBuf;
            if (ImGui::InputText("Text", g_labelTextBuf, sizeof(g_labelTextBuf)))
                l.text = g_labelTextBuf;
            {
                int ck = 0;
                if (l.className == "Label") ck = 1;
                else if (l.isButton()) ck = 2;
                const char* kinds[] = { "TextLabel", "Label", "TextButton" };
                if (ImGui::Combo("Class", &ck, kinds, 3)) {
                    l.className = kinds[ck];
                    if (l.isButton()) {
                        if (l.bgTransparency > 0.9f) l.bgTransparency = 0.f;
                        if (l.bgColor.x + l.bgColor.y + l.bgColor.z < 0.05f)
                            l.bgColor = {0.55f, 0.55f, 0.58f};
                        if (l.sizeW < 1.f) l.sizeW = 140.f;
                        if (l.sizeH < 1.f) l.sizeH = 40.f;
                    }
                }
            }
            ImGui::SliderFloat("Anchor X", &l.anchorX, 0.f, 1.f);
            ImGui::SliderFloat("Anchor Y", &l.anchorY, 0.f, 1.f);
            ImGui::DragFloat("Offset X (px)", &l.offsetX, 1.f);
            ImGui::DragFloat("Offset Y (px)", &l.offsetY, 1.f);
            ImGui::DragFloat("Size W (px)", &l.sizeW, 1.f, 0.f, 2000.f);
            ImGui::DragFloat("Size H (px)", &l.sizeH, 1.f, 0.f, 2000.f);
            if (ImGui::IsItemHovered() || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("0 = auto dal testo · trascina i vertici nel viewport");
            ImGui::SliderFloat("Font scale", &l.fontScale, 0.5f, 2.5f);
            ImGui::ColorEdit3("Text Color", &l.color.x);
            ImGui::ColorEdit3("Background Color", &l.bgColor.x);
            ImGui::SliderFloat("Background Transparency", &l.bgTransparency, 0.f, 1.f, "%.2f");
            ImGui::TextDisabled("0 = bg opaco · 1 = solo testo + outline");
            ImGui::Checkbox("Visible", &l.visible);
            if (l.isButton()) {
                ImGui::TextDisabled("TextButton: cliccabile in Test/Play (cursore libero)");
                ImGui::Separator();
                ImGui::TextColored({0.9f, 0.7f, 0.2f, 1.f}, "Script del bottone");
                if (l.script.empty()) {
                    ImGui::TextDisabled("Nessuno script — gira al click, in Test/Play.");
                    ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.65f, 0.12f, 1.f});
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.75f, 0.20f, 1.f});
                    ImGui::PushStyleColor(ImGuiCol_Text, {0.1f, 0.1f, 0.1f, 1.f});
                    if (ImGui::Button("Aggiungi Script", {-1, 28})) {
                        l.script = "-- script.Parent e' questo bottone\n"
                                   "script.Parent:Connect(function()\n"
                                   "    print(script.Parent.Name .. \" cliccato\")\n"
                                   "end)\n";
                        std::snprintf(g_labelScriptBuf, sizeof(g_labelScriptBuf), "%s", l.script.c_str());
                        markPlaceDirty();
                        g_showScriptCreatedGuide = true;
                    }
                    ImGui::PopStyleColor(3);
                } else {
                    drawLuaScriptEditor(g_labelScriptEditor, g_labelScriptBuf, sizeof(g_labelScriptBuf),
                        "##labelscript", 140.f);
                    l.script = g_labelScriptBuf;
                    {
                        static const char* kScriptTypeNames[] = {
                            "Legacy (compat)", "Script (Server)",
                            "LocalScript (Client)", "ModuleScript"
                        };
                        int st = (int)l.scriptType;
                        if (st < 0 || st > 3) st = 0;
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Combo("##labelscripttype", &st, kScriptTypeNames, 4))
                            l.scriptType = (ScriptType)st;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Apri guida"))
                        g_showLuaGuide = true;
                    if (ImGui::Button("Salva script", {-1, 0})) {
                        l.script = g_labelScriptBuf;
                        markPlaceDirty();
                    }
                    ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.90f, 0.28f, 0.20f, 1.f});
                    if (ImGui::Button("Rimuovi script", {-1, 0})) {
                        l.script.clear();
                        g_labelScriptBuf[0] = 0;
                        markPlaceDirty();
                    }
                    ImGui::PopStyleColor(2);
                }
            } else {
                ImGui::TextDisabled("Trascina per muovere · vertici per ridimensionare");
            }
            ImGui::Separator();
            if (studioAccentButton("Rimuovi", {-1, 32}, {0.75f, 0.2f, 0.18f, 1.f}))
                deleteSelected();
        } else if (g_selectedBackpack >= 0 && g_selectedBackpack < (int)g_world.backpack.size()) {
            auto& b = g_world.backpack[g_selectedBackpack];
            ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, "Backpack · Item");
            static char bpNameBuf[64] = {};
            static int bpEditIdx = -1;
            if (bpEditIdx != g_selectedBackpack) {
                bpEditIdx = g_selectedBackpack;
                std::snprintf(bpNameBuf, sizeof(bpNameBuf), "%s", b.name.c_str());
            }
            if (ImGui::InputText("Name", bpNameBuf, sizeof(bpNameBuf)))
                b.name = bpNameBuf;
            ImGui::SliderInt("Colore icona", &b.iconHue, 0, 5);
            {
                int tk = (int)b.tool;
                const char* toolNames[] = { "None", "Sword", "Pistol", "Torch" };
                if (ImGui::Combo("Tool", &tk, toolNames, 4))
                    b.tool = (BackpackTool)tk;
            }
            // Prima l'unico modo di dare una mesh in mano a un item era
            // scriverlo da script (Backpack.Give(nome, meshPath)) — niente
            // di scelto qui in Studio, a differenza di un Model normale.
            // Stesso file dialog gia' usato per importare i Model.
            if (studioAccentButton(
                    b.meshPath.empty() ? "Scegli modello (mesh in mano)..." : "Cambia modello...",
                    {-1, 30}, {0.18f, 0.42f, 0.72f, 1.f})) {
                // Si cattura l'INDICE, non il riferimento b: su Android la
                // risposta arriva anche mezzo minuto dopo, e nel frattempo il
                // vettore del Backpack puo' essersi riallocato. Un riferimento
                // catturato qui diventerebbe un puntatore a memoria libera.
                const int slot = g_selectedBackpack;
                plat::requestFile(
                    "Blocky — modello da tenere in mano", "*/*",
                    [slot](const std::string& path) {
                        if (path.empty()) return;   // annullato
                        if (slot < 0 || slot >= (int)g_world.backpack.size()) return;
                        auto& item = g_world.backpack[slot];
                        std::string lib = copyModelToLibrary(path, item.name);
                        item.meshPath = lib.empty() ? path : lib;
                        g_exportMsg = "Mesh assegnata a \"" + item.name + "\"";
                    });
            }
            if (!b.meshPath.empty()) {
                ImGui::TextDisabled("Mesh: %s", b.meshPath.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Togli mesh")) b.meshPath.clear();
            }

            // Anteprima in mano anche senza mesh (Sword/Torch/posa corpo).
            g_gripPreviewSlot = g_selectedBackpack;

            ImGui::SeparatorText("Come sta in mano");

            // Cosa si regola: oggetto O una parte R6 (braccia, torso, ...).
            {
                const char* targets[] = {
                    "Oggetto", "Braccio DX", "Braccio SX", "Torso",
                    "Testa", "Gamba DX", "Gamba SX"
                };
                ImGui::SetNextItemWidth(-1);
                if (ImGui::Combo("Cosa muovo##gripTarget", &g_gripEditTarget,
                                 targets, IM_ARRAYSIZE(targets))) {
                    // Entrando su una parte corpo, crea subito la posa cosi'
                    // si vede il braccio gia' in hold e non a riposo.
                    if (g_gripEditTarget >= 1) {
                        static const char* kPart[] = {
                            "Right Arm", "Left Arm", "Torso", "Head",
                            "Right Leg", "Left Leg"
                        };
                        ensureHoldPose(b.holdPose, kPart[g_gripEditTarget - 1],
                                       kEquippedArmHoldDeg);
                    }
                }
            }

            const float half = (ImGui::GetContentRegionAvail().x - 6.f) * 0.5f;
            auto modeButton = [&](const char* label, int mode, ImVec4 col) {
                const bool on = (g_gripEditMode == mode);
                ImGui::PushStyleColor(ImGuiCol_Button,
                    on ? ImVec4{col.x * 1.4f, col.y * 1.4f, col.z * 1.4f, 1.f} : col);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                    {col.x * 1.5f, col.y * 1.5f, col.z * 1.5f, 1.f});
                if (ImGui::Button(label, {half, 32}))
                    g_gripEditMode = on ? 0 : mode;
                ImGui::PopStyleColor(2);
            };
            modeButton(g_gripEditMode == 1 ? "Sposta: ATTIVO" : "Sposta col mouse",
                       1, {0.18f, 0.42f, 0.72f, 1.f});
            ImGui::SameLine(0, 6);
            modeButton(g_gripEditMode == 2 ? "Ruota: ATTIVO" : "Ruota col mouse",
                       2, {0.55f, 0.35f, 0.15f, 1.f});

            if (g_gripEditMode != 0) {
                ImGui::TextColored({0.4f, 0.9f, 0.5f, 1.f},
                    g_gripEditMode == 1
                        ? "Trascina: destra/sinistra e su/giu."
                        : "Trascina per ruotare.");
                ImGui::TextDisabled(g_gripEditMode == 1
                        ? "Shift + trascina = avanti / indietro."
                        : "Shift + trascina = inclina di lato.");
            } else {
                ImGui::TextDisabled(
                    g_gripEditTarget == 0
                        ? "Premi un modo, poi trascina l'oggetto in mano."
                        : "Premi un modo, poi trascina la parte del corpo.");
            }

            ImGui::Spacing();
            if (g_gripEditTarget == 0) {
                ImGui::TextDisabled("Scala: 1 = come la adatta il motore, 2 = il doppio");
                ImGui::SetNextItemWidth(-90);
                ImGui::DragFloat("Scala##grip", &b.gripScale, 0.02f, 0.05f, 20.f, "%.2f");
                ImGui::SetNextItemWidth(-90);
                ImGui::DragFloat3("Ruota##grip", &b.gripRotation.x, 1.f, -360.f, 360.f, "%.1f");
                ImGui::SetNextItemWidth(-90);
                ImGui::DragFloat3("Sposta##grip", &b.gripOffset.x, 0.02f, -10.f, 10.f, "%.3f");
                if (ImGui::SmallButton("Rimetti a zero##item")) {
                    b.gripScale = 1.f;
                    b.gripRotation = glm::vec3(0.f);
                    b.gripOffset = glm::vec3(0.f);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Metti orizzontale"))
                    b.gripRotation = glm::vec3(0.f, 90.f, 0.f);
            } else {
                static const char* kPart[] = {
                    "Right Arm", "Left Arm", "Torso", "Head",
                    "Right Leg", "Left Leg"
                };
                const char* partName = kPart[g_gripEditTarget - 1];
                HoldPartPose* hp = findHoldPose(b.holdPose, partName);
                ImGui::TextDisabled("Posa di \"%s\" mentre l'item e' tenuto", partName);
                if (!hp) {
                    ImGui::TextWrapped(
                        "Nessuna posa salvata: la parte segue camminata "
                        "(braccio DX = hold default).");
                    if (ImGui::Button("Crea posa per questa parte", {-1, 28}))
                        ensureHoldPose(b.holdPose, partName, kEquippedArmHoldDeg);
                } else {
                    ImGui::SetNextItemWidth(-90);
                    ImGui::DragFloat3("Ruota parte##hold", &hp->rot.x, 1.f, -360.f, 360.f, "%.1f");
                    ImGui::SetNextItemWidth(-90);
                    ImGui::DragFloat3("Sposta parte##hold", &hp->pos.x, 0.02f, -10.f, 10.f, "%.3f");
                    if (ImGui::SmallButton("Azzera questa parte")) {
                        hp->pos = glm::vec3(0.f);
                        hp->rot = (std::string(partName) == "Right Arm")
                            ? glm::vec3(kEquippedArmHoldDeg, 0.f, 0.f)
                            : glm::vec3(0.f);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Togli posa parte")) {
                        b.holdPose.erase(
                            std::remove_if(b.holdPose.begin(), b.holdPose.end(),
                                           [&](const HoldPartPose& h) {
                                               return h.name == partName;
                                           }),
                            b.holdPose.end());
                    }
                }
                if (!b.holdPose.empty()
                    && ImGui::SmallButton("Azzera tutte le parti corpo"))
                    b.holdPose.clear();
            }
            bool eq = (g_world.equippedBackpack == g_selectedBackpack);
            if (ImGui::Checkbox("Equipaggiato (default)", &eq)) {
                if (eq) g_world.equippedBackpack = g_selectedBackpack;
                else if (g_world.equippedBackpack == g_selectedBackpack)
                    g_world.equippedBackpack = -1;
            }
            ImGui::TextDisabled("Test: 1-9 equip · Sword/Pistol = tool in mano");
            if (studioAccentButton("Rimuovi dal Backpack", {-1, 32}, {0.75f, 0.2f, 0.18f, 1.f})) {
                int idx = g_selectedBackpack;
                g_world.backpack.erase(g_world.backpack.begin() + idx);
                if (g_world.equippedBackpack == idx) g_world.equippedBackpack = -1;
                else if (g_world.equippedBackpack > idx) g_world.equippedBackpack--;
                g_selectedBackpack = -1;
                bpEditIdx = -1;
            }
        } else if (g_selectedSpawn) {
            ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, "Workspace · SpawnLocation");
            ImGui::DragFloat3("Position", &g_world.spawn.position.x, 0.25f);
            ImGui::DragFloat("Yaw °", &g_world.spawn.yaw, 1.f);
            if (ImGui::Button("Metti spawn qui (camera)", {-1, 32})) {
                g_world.spawn.position = g_cam.pos;
                g_world.spawn.yaw = g_cam.yaw;
            }
            if (ImGui::Button("Metti spawn sul Player", {-1, 28})) {
                g_world.spawn.position = g_playerPos;
                g_world.spawn.yaw = g_playerYaw;
            }
        } else if (g_selectedPlayer) {
            auto& h = g_player.humanoid;
            studio_chrome::drawClassNameBanner("Player", "LocalPlayer");
            ImGui::TextDisabled(h.userRigged ? "Rig R6 attivo" : "Non riggato");
            ImGui::DragFloat3("Position (world)", &g_playerPos.x, 0.1f);
            ImGui::DragFloat("Yaw °", &g_playerYaw, 1.f);
            ImGui::SliderFloat("WalkSpeed", &h.walkSpeed, 4.f, 40.f);
            ImGui::SliderFloat("JumpPower", &h.jumpPower, 20.f, 90.f);
            if (g_selectedPlayerPart >= 0 && g_selectedPlayerPart < (int)h.parts.size()) {
                auto& p = h.parts[g_selectedPlayerPart];
                ImGui::Separator();
                ImGui::TextColored({0.95f, 0.75f, 0.3f, 1.f}, "Part: %s", p.name.c_str());
                ImGui::DragFloat3("Offset", &p.offset.x, 0.05f);
                ImGui::DragFloat3("Size", &p.size.x, 0.05f, 0.2f, 8.f);
                ImGui::DragFloat("Rot X", &p.rotX, 1.f);
                ImGui::DragFloat("Rot Z", &p.rotZ, 1.f);
                drawPartColorPicker(p.color);
            }
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
            if (ImGui::Button(h.userRigged ? "Apri Rigging Player..." : "Riggare il Player...", {-1, 40}))
                beginRigPlayer();
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.45f, 0.20f, 1.f});
            if (ImGui::Button("Play Wave", {-1, 28})) g_animator.play("Wave");
            ImGui::PopStyleColor();
        } else if (g_selectedModel >= 0 && g_selectedModel < (int)g_world.models.size()) {
            auto& m = g_world.models[g_selectedModel];

            static int syncedModel = -1;
            if (syncedModel != g_selectedModel) {
                syncedModel = g_selectedModel;
                std::snprintf(g_modelScriptBuf, sizeof(g_modelScriptBuf), "%s", m.script.c_str());
            }

            ImGui::TextColored({0.45f, 0.75f, 1.f, 1.f}, "Model");
            char nameBuf[64];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", m.name.c_str());
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) m.name = nameBuf;
            ImGui::TextWrapped("%s", m.sourcePath.c_str());
            if (m.rig.fileHasBones)
                ImGui::TextColored({0.3f, 0.85f, 0.45f, 1.f}, "Rig: scheletro nel file");
            else if (m.rig.simpleRig)
                ImGui::TextColored({0.3f, 0.75f, 0.95f, 1.f}, "Rig: Root (semplice)");
            else
                ImGui::TextColored({1.f, 0.55f, 0.25f, 1.f}, "Non riggato");
            if (g_selectedModel < (int)g_wsMeshes.size() && g_wsMeshes[g_selectedModel].loaded) {
                const auto& sm = g_wsMeshes[g_selectedModel];
                int withTex = 0, emissive = 0, twoSided = 0, transp = 0;
                for (const auto& s : sm.subs) {
                    if (s.texId) ++withTex;
                    if (s.emissiveTex || s.emissive != glm::vec3(0.f)) ++emissive;
                    if (s.twoSided) ++twoSided;
                    if (s.alpha < 0.99f) ++transp;
                }
                ImGui::Separator();
                ImGui::TextColored({0.55f, 0.85f, 1.f, 1.f}, "Materiali importati");
                ImGui::Text("%d submesh · %d con texture · %d verts",
                            (int)sm.subs.size(), withTex, sm.vertexCount);
                if (emissive) ImGui::TextDisabled("%d emissivi", emissive);
                if (twoSided) ImGui::TextDisabled("%d double-sided", twoSided);
                if (transp)   ImGui::TextDisabled("%d semi-trasparenti", transp);
                int missing = sm.missingTextureCount();
                if (missing > 0) {
                    ImGui::TextColored({1.f, 0.45f, 0.3f, 1.f},
                        "%d texture non trovate (es. %s)", missing,
                        sm.firstMissingTexture().c_str());
                    ImGui::TextDisabled("Mettile accanto al modello o in ./textures");
                }
                if (ImGui::TreeNode("Dettaglio submesh")) {
                    for (const auto& s : sm.subs) {
                        ImGui::BulletText("%s%s%s", s.name.c_str(),
                                          s.matName.empty() ? "" : " · mat: ",
                                          s.matName.c_str());
                        ImGui::SameLine();
                        if (s.texId) ImGui::TextColored({0.3f, 0.85f, 0.45f, 1.f}, "[tex]");
                        else if (!s.missingTex.empty())
                            ImGui::TextColored({1.f, 0.45f, 0.3f, 1.f}, "[tex mancante]");
                        else ImGui::TextDisabled("[solo colore]");
                    }
                    ImGui::TreePop();
                }
                ImGui::Separator();
            }
            bool xformEdited = false;
            xformEdited |= ImGui::DragFloat3("Position", &m.position.x, 0.25f);
            xformEdited |= ImGui::DragFloat3("Rotation °", &m.rotation.x, 1.f);
            xformEdited |= ImGui::DragFloat3("Scale", &m.scale.x, 0.05f, 0.01f, 100000.f);
            if (xformEdited) {
                m.anim.reseedIfStatic(m.position, m.rotation, m.scale);
                markPlaceDirty();
            }
            ImGui::ColorEdit3("Tint", &m.tint.x);
            if (ImGui::Checkbox("CanCollide", &m.canCollide))
                rebuildNavGrid();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Il player si ferma contro il volume del modello\n"
                                  "(un parallelepipedo, non la mesh esatta).");
            drawModelPrimaryPartProps(m);

            ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.45f, 0.20f, 1.f});
            if (ImGui::Button("Anima questo Model...", {-1, 40}))
                beginAnimateSelectedModel();
            ImGui::PopStyleColor();
            if (!m.rig.hasAnimRig()) {
                ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
                if (ImGui::Button("Crea Rig Root ora", {-1, 32}))
                    openModelAnimEditor(g_selectedModel);
                ImGui::PopStyleColor();
            }
            ImGui::TextDisabled("1) Anima  2) Move  3) Set Frame (K)  4) Play");

            ImGui::Separator();
            ImGui::TextColored({0.9f, 0.7f, 0.2f, 1.f}, "Script del Model");
            ImGui::TextWrapped("script.Parent e' questo Model: stessa API di FindFirstChild, "
                               "Animator, SetClipAlias, CanCollide.");
            if (m.script.empty()) {
                ImGui::TextDisabled("Nessuno script.");
                ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.65f, 0.12f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.75f, 0.20f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_Text, {0.1f, 0.1f, 0.1f, 1.f});
                if (ImGui::Button("Aggiungi Script", {-1, 28})) {
                    m.script = "-- script.Parent e' questo Model\n"
                              "local model = script.Parent\n\n"
                              "game:GetService(\"RunService\").Heartbeat:Connect(function(dt)\n"
                              "end)\n";
                    std::snprintf(g_modelScriptBuf, sizeof(g_modelScriptBuf), "%s", m.script.c_str());
                    markPlaceDirty();
                    g_showScriptCreatedGuide = true;
                }
                ImGui::PopStyleColor(3);
            } else {
                drawLuaScriptEditor(g_modelScriptEditor, g_modelScriptBuf, sizeof(g_modelScriptBuf),
                    "##modelscript", 140.f);
                if (ImGui::Button("Salva script", {-1, 0})) {
                    m.script = g_modelScriptBuf;
                    markPlaceDirty();
                }
                {
                    static const char* kScriptTypeNames[] = {
                        "Legacy (compat)", "Script (Server)",
                        "LocalScript (Client)", "ModuleScript"
                    };
                    int st = (int)m.scriptType;
                    if (st < 0 || st > 3) st = 0;
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##modelscripttype", &st, kScriptTypeNames, 4)) {
                        m.scriptType = (ScriptType)st;
                        markPlaceDirty();
                    }
                    if (m.scriptType == ScriptType::Server)
                        ImGui::TextDisabled("Non gira su chi si unisce come Client.");
                    else if (m.scriptType == ScriptType::Client)
                        ImGui::TextDisabled("Gira su ogni peer, incluso chi si unisce.");
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Apri guida"))
                    g_showLuaGuide = true;
                ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.90f, 0.28f, 0.20f, 1.f});
                if (ImGui::Button("Rimuovi script", {-1, 0})) {
                    m.script.clear();
                    g_modelScriptBuf[0] = 0;
                    markPlaceDirty();
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
            if (ImGui::Button("Rimuovi Model", {-1, 0})) deleteSelected();
            ImGui::PopStyleColor();
        } else if (g_selectedSound >= 0 && g_selectedSound < (int)g_sounds.sounds.size()) {
            auto& s = g_sounds.sounds[g_selectedSound];
            ImGui::TextColored({0.75f, 0.55f, 1.f, 1.f}, "SoundService · Sound");
            char nameBuf[64];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.name.c_str());
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) s.name = nameBuf;
            if (ImGui::InputText("SoundId (path)", g_soundPathBuf, sizeof(g_soundPathBuf)))
                s.soundId = g_soundPathBuf;
            ImGui::Checkbox("Looped", &s.looped);
            ImGui::SliderFloat("Volume", &s.volume, 0.f, 1.f);
            if (ImGui::Button("Play", {100, 0})) {
                s.soundId = g_soundPathBuf;
                if (!g_sounds.play(s))
                    g_exportMsg = g_sounds.lastError;
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop", {100, 0})) g_sounds.stop(s);
            ImGui::TextDisabled("WAV consigliato · %s", s.playing ? "in riproduzione" : "fermo");
            ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
            if (ImGui::Button("Rimuovi Sound", {-1, 0})) deleteSelected();
            ImGui::PopStyleColor();
        } else if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) {
            ImGui::TextColored({0.9f, 0.7f, 0.3f, 1.f}, "Animator");
            ImGui::InputText("Clip", g_animNameBuf, sizeof(g_animNameBuf));
            ImGui::TextDisabled("Idle · Walk · Wave · Dance · Jump");
            if (ImGui::Button("Play Clip", {-1, 32}))
                g_animator.play(g_animNameBuf[0] ? g_animNameBuf : "Wave");
            if (ImGui::Button("Stop / Idle", {-1, 0})) g_animator.stop();
            ImGui::Text("Attivo: %s", g_animator.lastPlayed.c_str());
            ImGui::Separator();
            ImGui::TextDisabled("Nessuna Part/Model selezionata.");
            ImGui::TextDisabled("Salva/Pubblica sono sempre in fondo a questo pannello.");
        } else {
            auto& p = g_world.parts[g_selectedPart];
            studio_chrome::drawClassNameBanner(exPartClass(p), p.name.c_str());
            drawMultiPartPropertiesSection();
            drawRobloxPartProperties(p, g_selectedPart);   // StudioPropertyGrid.h

            ImGui::PushStyleColor(ImGuiCol_Header, {0.22f, 0.23f, 0.26f, 1.f});
            const bool scriptOpen = ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleColor();
            if (scriptOpen) {
            ImGui::TextWrapped(
                "Non e' Lua vanilla: usi l'API Blocky (tipo Roblox). "
                "Esplosione = part:Explode(). Apri View → Guida Lua Blocky.");
            if (p.script.empty()) {
                ImGui::TextDisabled("Nessuno script — prova un preset.");
                ImGui::PushStyleColor(ImGuiCol_Button, {0.20f, 0.65f, 0.85f, 1.f});
                if (ImGui::Button(LB("btn.jump_block", "jmpblk"), {-1, 36})) {
                    p.script = makeJumpBlockScript(p);
                    syncPartEditors();
                    g_editingPartScript = true;
                    g_luaRunMsg = "Script salto — Test (F5)";
                    g_showScriptCreatedGuide = true;
                }
                ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.25f, 0.12f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.35f, 0.18f, 1.f});
                if (ImGui::Button(LB("btn.explode_block", "expblk"), {-1, 36})) {
                    p.script = makeExplodeBlockScript(p);
                    p.color = {0.85f, 0.2f, 0.12f};
                    p.boomOnTouch = false;
                    p.boomAutoSecs = 0.f;
                    p.canDestroy = true;
                    p.canCollide = false;
                    syncPartEditors();
                    g_editingPartScript = true;
                    g_luaRunMsg = "Script esplosione Lua (monete + boom) — Test (F5)";
                    g_showScriptCreatedGuide = true;
                }
                ImGui::PopStyleColor(2);
                ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.72f, 0.55f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.20f, 0.85f, 0.65f, 1.f});
                if (ImGui::Button("Script Neon (Material)", {-1, 36})) {
                    p.script = makeNeonBlockScript(p);
                    p.setMaterial(PartMaterial::Neon);
                    p.pointLight = true;
                    p.color = {0.25f, 1.f, 0.85f};
                    syncPartEditors();
                    g_editingPartScript = true;
                    g_luaRunMsg = "Script Neon — Test (F5) per pulse + luce";
                    g_showScriptCreatedGuide = true;
                }
                ImGui::PopStyleColor(2);
                ImGui::PushStyleColor(ImGuiCol_Button, {0.20f, 0.55f, 0.90f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f, 0.65f, 1.0f, 1.f});
                if (ImGui::Button("Script Stamina (sprint)", {-1, 36})) {
                    applyStaminaToSelectedPart();
                }
                ImGui::PopStyleColor(2);
                // Lo script dell'arma sta QUI, sulla Part, non sull'oggetto:
                // l'oggetto e' cosa tieni in mano, lo script decide cosa
                // succede quando lo usi. E' la cosa meno ovvia del motore, e
                // senza un bottone che la mostri non c'era modo di indovinarla.
                ImGui::PushStyleColor(ImGuiCol_Button, {0.55f, 0.25f, 0.70f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.68f, 0.35f, 0.85f, 1.f});
                if (ImGui::Button("Script Arma da fuoco", {-1, 36}))
                    addWeaponScriptToSelected();
                ImGui::PopStyleColor(2);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Fucile a pompa completo (mira, munizioni, rinculo),\n"
                        "gia' collegato all'oggetto \"%s\" del Backpack.",
                        weaponScriptTargetName().c_str());
                }

                ImGui::PushStyleColor(ImGuiCol_Button, {0.85f, 0.65f, 0.12f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.95f, 0.75f, 0.20f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_Text, {0.1f, 0.1f, 0.1f, 1.f});
                if (ImGui::Button(LB("btn.add_script", "addsc"), {-1, 28}))
                    addScriptToSelected();
                ImGui::PopStyleColor(3);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Script vuoto, da scrivere tu.");
            } else {
                g_editingPartScript = true;
                drawLuaScriptEditor(g_partScriptEditor, g_partScriptBuf, sizeof(g_partScriptBuf),
                    "##partscript", 140.f);
                p.script = g_partScriptBuf;
                {
                    static const char* kScriptTypeNames[] = {
                        "Legacy (compat)", "Script (Server)",
                        "LocalScript (Client)", "ModuleScript"
                    };
                    int st = (int)p.scriptType;
                    if (st < 0 || st > 3) st = 0;
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##scripttype", &st, kScriptTypeNames, 4))
                        p.scriptType = (ScriptType)st;
                    if (p.scriptType == ScriptType::Server)
                        ImGui::TextDisabled("Solo host — stesso place PC + Android.");
                    else if (p.scriptType == ScriptType::Client)
                        ImGui::TextDisabled("LocalScript: ogni client (input/UI/viewmodel).");
                    else if (p.scriptType == ScriptType::ModuleScript)
                        ImGui::TextDisabled("Non parte da solo — require da altri script.");
                }
                if (ImGui::SmallButton("Inserisci Explode")) {
                    std::string cur = g_partScriptBuf;
                    cur += "\npart:Explode()\n";
                    std::snprintf(g_partScriptBuf, sizeof(g_partScriptBuf), "%s", cur.c_str());
                    p.script = g_partScriptBuf;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Inserisci Touched→Explode")) {
                    std::snprintf(g_partScriptBuf, sizeof(g_partScriptBuf), "%s",
                        makeExplodeBlockScript(p).c_str());
                    p.script = g_partScriptBuf;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Apri guida"))
                    g_showLuaGuide = true;
                if (ImGui::SmallButton("Carica Stamina (sprint)"))
                    applyStaminaToSelectedPart();
                ImGui::TextDisabled("%s", L("hint.scripts_test"));
                if (ImGui::Button(LB("btn.save_script", "savesc")))
                    p.script = g_partScriptBuf;
                ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.90f, 0.28f, 0.20f, 1.f});
                if (ImGui::Button(LB("btn.remove_script", "rmsc"), {-1, 0}))
                    removeScriptFromSelected();
                ImGui::PopStyleColor(2);
            }
            if (!g_luaRunMsg.empty()) {
                ImGui::Separator();
                ImGui::TextWrapped("%s", g_luaRunMsg.c_str());
            }
            }   // Script
        }

        // Salva/Pubblica: SEMPRE visibile qui sotto, qualunque cosa sia
        // selezionata nell'Explorer — prima viveva solo dentro il ramo
        // "niente selezionato" del blocco sopra, quindi bastava avere una
        // Part/Model/Sound selezionato per non trovarlo piu' da nessuna
        // parte (il motivo di "non mi fa pubblicare i giochi").
        ImGui::Separator();
        {
            ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "Place / Pubblica");
            ImGui::TextDisabled("Salva · Salva per pubblicare · Pubblica");
            ImGui::InputText("Nome gioco", g_gameNameBuf, sizeof(g_gameNameBuf));
            g_world.placeName = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
            {
                // Il campo sopra parte sempre da "Il mio gioco" finche' non lo
                // cambi — se lo lasci com'e' (o lo riporti a un nome gia' usato)
                // Pubblica AGGIORNA quel gioco esistente invece di crearne uno
                // nuovo. Meglio dirlo qui prima di premere il bottone che dopo,
                // quando il vecchio contenuto e' gia' stato sovrascritto.
                std::string curId = sanitizeGameId(g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName);
                for (const auto& g : g_games) {
                    if (g.isPublished && g.id == curId) {
                        ImGui::TextColored({1.f, 0.75f, 0.3f, 1.f},
                            "Aggiornerai il gioco gia' pubblicato \"%s\" (stesso nome/ID) — "
                            "cambia \"Nome gioco\" se ne vuoi uno nuovo.", g.title.c_str());
                        break;
                    }
                }
            }
            if (ImGui::Button("Scegli icona...", { -1, 0 })) {
                std::string path;
                if (openImageFileDialog(path, publisherHwnd()))
                    g_pendingIconPath = path;
            }
            if (!g_pendingIconPath.empty()) {
                auto slash = g_pendingIconPath.find_last_of("\\/");
                std::string base = slash == std::string::npos
                    ? g_pendingIconPath : g_pendingIconPath.substr(slash + 1);
                ImGui::TextDisabled("Icona: %s", base.c_str());
            } else {
                ImGui::TextDisabled("Nessuna icona — mostra il colore accent");
            }
            if (ImGui::Button("Apri Place...", { -1, 0 })) doOpenPlace();
            if (ImGui::Button("Salva Place", { -1, 0 })) doSavePlace();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.22f, 0.48f, 0.78f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.30f, 0.58f, 0.90f, 1.f});
            if (ImGui::Button("Salva per pubblicare", { -1, 34 })) doSaveForPublish();
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Bozza + autosave ogni 30s — pubblichi dopo quando vuoi");
            if (g_publishDraftActive)
                ImGui::TextColored({0.35f, 0.9f, 0.55f, 1.f}, "Autosave bozza publish ON");
            if (ImGui::Button("Salva Progetto", { -1, 0 })) doSaveProject();
            ImGui::SliderInt("Max giocatori", &g_maxPlayers, 1, 32);
            ImGui::TextDisabled("Limite server multiplayer (1–32)");
            {
                int usedQ = 0;
                long long nowQ = (long long)std::time(nullptr);
                std::string cr = g_menu.username[0] ? g_menu.username : "Builder";
                std::string gid = sanitizeGameId(g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName);
                bool okSlot = canPublishGame(g_games, gid, cr, nowQ, &usedQ);
                ImGui::TextDisabled("Slot giochi: %d / %d (2 mesi)", usedQ, kMaxPublishedGamesPerUser);
                if (!okSlot)
                    ImGui::TextColored({1.f, 0.45f, 0.3f, 1.f}, "Limite raggiunto — aggiorna un gioco esistente");
            }
            ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.42f, 1.f});
            if (ImGui::Button("Pubblica nei Games", {-1, 40})) {
                if (publishCurrentGame())
                    refreshGamesList();
            }
            ImGui::PopStyleColor();
            if (ImGui::Button("Avvia Server dedicato", {-1, 32})) {
                if (publishCurrentGame())
                    refreshGamesList();
                // Il server carica questo place ed esegue i suoi Script server.
                const std::string serverPlace = blockyDataDir() + "/server_place.json";
                launchDedicatedServer(g_maxPlayers, g_gameNameBuf[0] ? g_gameNameBuf : "Blocky",
                                      writeCurrentPlaceTo(serverPlace) ? serverPlace : std::string());
            }
            if (!g_publishMsg.empty())
                ImGui::TextWrapped("%s", g_publishMsg.c_str());
            if (!g_exportMsg.empty())
                ImGui::TextWrapped("%s", g_exportMsg.c_str());
            ImGui::Separator();
            ImGui::Text("Place: %s", g_world.placeName.c_str());
            if (!g_placeFilePath.empty())
                ImGui::TextDisabled("%s", g_placeFilePath.c_str());
            if (g_publishDraftActive && !g_publishDraftPath.empty())
                ImGui::TextDisabled("Bozza: %s", g_publishDraftPath.c_str());
            ImGui::SliderFloat("FOV", &g_cam.fov, 40.f, 100.f);
            ImGui::Checkbox("VSync", &g_vsync);
        }
        ImGui::End();
        studio_chrome::popDockPanelColors();
    }

    if (g_showScript) {
        ImGui::SetNextWindowPos({12, (float)g_h - 280}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({480, 260}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Script (Safe Bytecode)", &g_showScript);
        ImGui::TextColored({0.45f, 0.9f, 0.7f, 1.f}, "Logica di gioco = Lua Blocky → .bbc sigillato");
        ImGui::TextWrapped(
            "Lua 5.4 da solo NON ha Explode / Particles / Touched. "
            "Quelle sono funzioni Blocky (come Roblox). Apri la Guida Lua.");
        if (ImGui::Button("Apri Guida Lua Blocky"))
            g_showLuaGuide = true;
        ImGui::SameLine();
        ImGui::TextDisabled("OpenGL = solo grafica");
        ImGui::TextWrapped("Place: %s", g_world.placeName.c_str());
        drawLuaScriptEditor(g_luaEditEditor, g_luaEditBuf, sizeof(g_luaEditBuf),
            "##luaedit", 100.f);
        ImGui::TextDisabled("%s", L("hint.scripts_test"));
        if (ImGui::Button(LB("btn.compile_bbc", "bbc"))) {
            std::string id = sanitizeGameId(g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName);
            std::string dir = BlockyProject::joinPath(BlockyProject::projectsDir(), id);
            BlockyProject::mkDir(dir);
            BlockyProject::mkDir(BlockyProject::joinPath(dir, "scripts"));
            std::string out = BlockyProject::joinPath(dir, "scripts\\main.bbc");
            std::string err;
            if (LuaRuntime::compileSealedFile(g_luaEditBuf, id, out, err)) {
                BlockyProject::writeTextFile(
                    BlockyProject::joinPath(dir, "scripts\\main.lua"), g_luaEditBuf);
                g_luaRunMsg = "Safe Bytecode OK: " + out;
                g_exportMsg = g_luaRunMsg;
            } else {
                g_luaRunMsg = "Compile fallita: " + err;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Esempio AnimateFrame")) {
            std::string path = resolveScriptPath("scripts/animate_example.lua");
            std::string src = readTextFile(path);
            if (src.empty()) {
                src =
                    "-- AnimateFrame(frame, position)\n"
                    "AnimateFrame(0, Vector3.new(0, 2, 0))\n"
                    "AnimateFrame(15, Vector3.new(4, 5, 0))\n"
                    "AnimateFrame(30, Vector3.new(0, 2, 0))\n"
                    "print(\"AnimateFrame impostato — apri Animator e Play\")\n";
            }
            std::snprintf(g_luaEditBuf, sizeof(g_luaEditBuf), "%s", src.c_str());
            g_luaRunMsg = "Caricato esempio AnimateFrame (serve un Model)";
        }
        ImGui::SameLine();
        if (ImGui::Checkbox("API completa (Test)", &g_loadApiCompleta)) {
            if (g_loadApiCompleta)
                g_luaRunMsg = "Al Test (F5) carica lua_api_completo.lua — pad effetti a z=12";
        }
        ImGui::SameLine();
        if (ImGui::Button("Carica stamina.lua")) {
            std::string src = loadStaminaScriptSource();
            if (!src.empty()) {
                std::snprintf(g_luaEditBuf, sizeof(g_luaEditBuf), "%s", src.c_str());
                g_loadStamina = true;
                g_luaRunMsg = "Stamina caricata — premi Test (F5), poi CTRL+WASD";
            } else {
                g_loadStamina = false;
                g_luaRunMsg = "File stamina.lua non trovato in assets/scripts";
            }
        }
        if (g_loadStamina) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Rimuovi stamina")) {
                g_loadStamina = false;
                g_luaRunMsg = "Stamina disattivata";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Carica cart_lontano.lua")) {
            std::string path = resolveScriptPath("scripts/cart_lontano.lua");
            std::string src = readTextFile(path);
            if (!src.empty()) {
                std::snprintf(g_luaEditBuf, sizeof(g_luaEditBuf), "%s", src.c_str());
                g_loadCartLontano = true;
                g_luaRunMsg = "Carrello caricato — Test (F5), poi clicca VIA!";
            } else {
                g_loadCartLontano = false;
                g_luaRunMsg = "File cart_lontano.lua non trovato in assets/scripts";
            }
        }
        if (g_loadCartLontano) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Rimuovi carrello")) {
                g_loadCartLontano = false;
                g_luaRunMsg = "Carrello disattivato";
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("part:Explode · Particles · Humanoid · barre UI · raycast...");
        if (!g_luaRunMsg.empty()) ImGui::TextWrapped("%s", g_luaRunMsg.c_str());
        if (!g_lua.lastError.empty())
            ImGui::TextColored({1,.4f,.4f,1}, "%s", g_lua.lastError.c_str());
        ImGui::End();
    }

    if (g_showLuaGuide) {
        ImGui::SetNextWindowSize({520, 420}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({(float)g_w - 540.f, 90.f}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Guida Lua Blocky", &g_showLuaGuide);
        ImGui::TextColored({1.f, 0.85f, 0.35f, 1.f}, "Non e' Lua standard");
        ImGui::TextWrapped(
            "Il linguaggio e' Lua 5.4, ma Explode / Particles / Touched / Humanoid "
            "sono API Blocky (stile Roblox). Senza di esse non puoi far esplodere nulla.");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Esplosione (1 riga)");
        ImGui::BulletText("part:Explode()     — boom + sparisce");
        ImGui::BulletText("part:Explode(60)   — boom piu' grosso");
        ImGui::BulletText("part:Destroy()     — rimuove (se CanDestroy)");
        ImGui::BulletText("part.Material      — Neon/Metal/Glass/Wood/...");
        ImGui::BulletText("part.CanDestroy    — true/false");
        ImGui::BulletText("game.Particles:Explode(pos) — solo effetti");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Part (script sul blocco)");
        ImGui::BulletText("local part = script.Parent");
        ImGui::BulletText("part.Position / Size / Orientation");
        ImGui::BulletText("part.Touched:Connect(function() ... end)");
        ImGui::BulletText("part:Destroy()  ·  part:Explode()");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Effetti");
        ImGui::BulletText("game.Particles:Sparks(pos, n)");
        ImGui::BulletText("game.Particles:Smoke(pos, n)");
        ImGui::BulletText("game.Particles:Emit(pos, n, Color3.new(r,g,b))");
        ImGui::BulletText("game.Particles:Coins(pos, n)");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Tempo / loop");
        ImGui::BulletText("task.wait(1)  ·  task.spawn(fn)  ·  task.delay(t, fn)");
        ImGui::BulletText("game.RunService.Heartbeat:Connect(function(dt) end)");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Player");
        ImGui::BulletText("Humanoid.SetWalkSpeed(16)");
        ImGui::BulletText("Humanoid.SetJumpPower(50)");
        ImGui::BulletText("Humanoid.SetJumpEnabled(false)");
        ImGui::Separator();
        ImGui::TextColored({0.45f, 0.9f, 1.f, 1.f}, "Mondo / Terrain");
        ImGui::BulletText("workspace.Terrain:FillBlock(cframe, size, material)");
        ImGui::BulletText("workspace:Raycast(origin, direction)");
        ImGui::BulletText("workspace:GetDescendants() · FindFirstChild");
        ImGui::BulletText("game:GetService(\"CollectionService\")");
        ImGui::BulletText("Vector3.new(x,y,z)  ·  Color3.new(r,g,b)");
        ImGui::Separator();
        ImGui::TextDisabled("Preset Properties: Blocco esplosivo / Blocco che salta");
        ImGui::TextDisabled("Gli script girano solo in Test (F5) o Play.");
        if (ImGui::Button("Chiudi", {-1, 28}))
            g_showLuaGuide = false;
        ImGui::End();
    }

    if (g_showPlatformPanel) {
        ImGui::SetNextWindowPos({(float)g_w * 0.5f - 200.f, (float)g_h - 250}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({400, 230}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Blocky Game Engine", &g_showPlatformPanel);
        ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "%s", kAppBrand);
        ImGui::TextDisabled("Editor di giochi — parti, Lua, publish su Blocky");
        ImGui::Separator();
        ImGui::Text("Piattaforma: %s",
                    g_engine.platform == TargetPlatform::Unknown
                        ? "PC" : targetPlatformName(g_engine.platform));
        ImGui::Text("%s: %s", L("lang"), locCurrentName());
        drawLanguageSelector("enginepanel_lang");
        ImGui::Text("Graphics:   %s", gfxApiName(g_engine.gfxApi));
        ImGui::Text("Profilo:    %s", g_engine.quality.name);
        ImGui::Separator();
        ImGui::TextDisabled("Grafica Studio (Quality + Lighting + post):");
        if (ImGui::Button("Performance##engfx")) {
            applyStudioGraphicsPreset(StudioGraphicsPreset::Performance,
                                     g_engine.quality, g_look, g_lighting);
        }
        ImGui::SameLine();
        if (ImGui::Button("Studio Default##engfx")) {
            applyStudioGraphicsPreset(StudioGraphicsPreset::StudioDefault,
                                     g_engine.quality, g_look, g_lighting);
        }
        ImGui::SameLine();
        if (ImGui::Button("Beautiful##engfx")) {
            applyStudioGraphicsPreset(StudioGraphicsPreset::Beautiful,
                                     g_engine.quality, g_look, g_lighting);
            g_lookPresetUi = (int)LookPreset::CleanAaa;
        }
        ImGui::TextDisabled("Preset tecnici:");
        if (ImGui::Button("Low"))    g_engine.setPreset(QualityPreset::Low);
        ImGui::SameLine();
        if (ImGui::Button("Medium")) g_engine.setPreset(QualityPreset::Medium);
        ImGui::SameLine();
        if (ImGui::Button("High"))   g_engine.setPreset(QualityPreset::High);
        ImGui::SameLine();
        if (ImGui::Button("Ultra"))  g_engine.setPreset(QualityPreset::Ultra);
        ImGui::SameLine();
        if (ImGui::Button("Default")) g_engine.setPreset(QualityPreset::PlatformDefault);
        ImGui::Checkbox("Shadows (quality)", &g_engine.quality.shadows);
        ImGui::SliderFloat("Render scale", &g_engine.quality.renderScale, 0.5f, 1.25f, "%.2f");
        ImGui::SliderFloat("Draw distance", &g_engine.quality.maxDrawDistance, 50.f, 2500.f, "%.0f");
        ImGui::SliderInt("Max parts", &g_engine.quality.maxVisibleParts, 100, 50000);
        ImGui::SliderInt("Max modelli", &g_engine.quality.maxVisibleModels, 8, 512);
        ImGui::SliderInt("Max vertici modelli", &g_engine.quality.maxModelVerts, 100000, 12000000);
        ImGui::Checkbox("Cull part a priorita'", &g_engine.quality.priorityPartCull);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Sotto overload tiene part grandi/vicine, scarta le piccole lontane.");
        ImGui::SliderFloat("Physics Hz", &g_engine.quality.physicsHz, 20.f, 120.f, "%.0f");
        ImGui::SliderInt("Lua budget us", &g_engine.quality.luaBudgetUs, 500, 16000);
        ImGui::Checkbox("Auto LOD + culling", &g_engine.quality.autoLod);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Attiva insieme: LOD distanza, frustum e occlusion culling.");
        ImGui::Checkbox("Frustum culling", &g_engine.quality.frustumCull);
        ImGui::Checkbox("Occlusion culling", &g_engine.quality.occlusionCull);
        ImGui::Checkbox("LOD part (stud/mesh)", &g_engine.quality.partLod);
        ImGui::Checkbox("Culling part piccole", &g_engine.quality.cullSmallParts);
        if (g_engine.quality.autoLod) {
            g_engine.quality.frustumCull = true;
            g_engine.quality.occlusionCull = true;
            g_engine.quality.partLod = true;
            g_engine.quality.cullSmallParts = true;
        }
        ImGui::Checkbox("Auto difesa lag", &g_engine.quality.autoLagDefense);
        ImGui::Checkbox("Guardia mondo (NaN)", &g_engine.quality.runtimeWorldGuard);
        ImGui::Checkbox("Stud textures", &g_engine.quality.studTextures);
        ImGui::Checkbox("Post FX / Bloom", &g_engine.quality.postFx);
        ImGui::Checkbox("VSync", &g_engine.quality.vsync);
        ImGui::Separator();
        ImGui::TextColored({0.95f, 0.55f, 0.85f, 1.f}, "Look AAA rapido");
        if (ImGui::Button("Forest")) {
            g_lookPresetUi = (int)LookPreset::Forest;
            applyLookPreset(LookPreset::Forest, g_look, g_lighting);
            g_engine.quality.postFx = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cinematic")) {
            g_lookPresetUi = (int)LookPreset::Cinematic;
            applyLookPreset(LookPreset::Cinematic, g_look, g_lighting);
            g_engine.quality.postFx = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clean AAA")) {
            g_lookPresetUi = (int)LookPreset::CleanAaa;
            applyLookPreset(LookPreset::CleanAaa, g_look, g_lighting);
            g_engine.quality.postFx = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Off")) {
            g_lookPresetUi = 0;
            applyLookPreset(LookPreset::Default, g_look, g_lighting);
        }
        g_vsync = g_engine.quality.vsync;
        ImGui::Separator();
        ImGui::TextColored({1.f, 0.55f, 0.35f, 1.f}, "AntiCheat / AntiTamper");
        ImGui::Checkbox("Enabled", &antiCheat().cfg.enabled);
        ImGui::Checkbox("Anti-tamper", &antiCheat().cfg.antiTamper);
        ImGui::Checkbox("Anti-debug", &antiCheat().cfg.antiDebug);
        ImGui::Checkbox("Kill tool cheat", &antiCheat().cfg.killHostileProcs);
        if (ImGui::IsItemDeactivatedAfterEdit() || ImGui::IsItemEdited())
            acSealConfig();
        ImGui::Text("Strikes: %d  ·  Tamper: %d  ·  Kill: %d",
                    antiCheat().strikes, antiCheat().tamperHits, antiCheat().hostileKills);
        ImGui::TextDisabled("Seal %s · HB %u",
            antiCheat().integrityOk ? "OK" : "FAIL",
            (unsigned)antiCheat().heartbeat);
        if (!antiCheat().lastReason.empty())
            ImGui::TextDisabled("%s", antiCheat().lastReason.c_str());
        if (antiCheat().debuggerSeen)
            ImGui::TextColored({1.f, 0.25f, 0.2f, 1.f}, "DEBUGGER RILEVATO");
        if (antiCheat().softKick || (float)glfwGetTime() < antiCheat().hardMuteUntil)
            ImGui::TextColored({1.f, 0.3f, 0.3f, 1.f}, "MUTE / SOFT-KICK attivo");
        if (ImGui::SmallButton("Reset AntiCheat")) { acReset(); acSealConfig(); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Re-seal")) acSealConfig();
        ImGui::TextDisabled("Moderazione: odio/swear/spam/mute · Lua sandbox");
        ImGui::TextDisabled("PC · Mobile · iOS · PS5 — Blocky su ogni piattaforma");
        ImGui::End();
    }

    if (g_showChat) {
        ImGui::SetNextWindowPos({(float)g_w - 340, (float)g_h - 280}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({320, 250}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Chat", &g_showChat);
        ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "Blocky Chat");
        ImGui::TextDisabled("Parolacce → ****  ·  Report nel tab Chat del menu");
        ImGui::Separator();
        ImGui::BeginChild("##chatlog", {0, -36}, true);
        for (const auto& line : g_menu.chatLines)
            ImGui::TextWrapped("%s", line.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.f)
            ImGui::SetScrollHereY(1.f);
        ImGui::EndChild();
        bool send = ImGui::InputText("##msg", g_studioChatBuf, sizeof(g_studioChatBuf),
            ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Invia") || send) && g_studioChatBuf[0]) {
            sendPlayChat(g_studioChatBuf);
            g_studioChatBuf[0] = 0;
        }
        ImGui::End();
    }

    if (g_showApiReference) {
        ImGui::SetNextWindowSize({560, 520}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos({24.f, 90.f}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Riferimento API Blocky (Roblox-style)", &g_showApiReference);
        if (ImGui::BeginTabBar("##apiref")) {
            if (ImGui::BeginTabItem("Servizi")) {
                ImGui::TextWrapped(
                    "game:GetService(\"RunService|Lighting|Workspace|Players|"
                    "ReplicatedStorage|TweenService|Debris|DataStoreService|"
                    "CollectionService|UserInputService|StarterGui|SoundService\")");
                ImGui::Separator();
                ImGui::BulletText("RunService: Heartbeat, Stepped, RenderStepped, IsStudio");
                ImGui::BulletText("Lighting: ClockTime, Fog, OutdoorAmbient, Brightness");
                ImGui::BulletText("CollectionService: tag sulle Part live");
                ImGui::BulletText("ServerScriptService / ServerStorage / StarterPlayer (contenitori)");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Workspace")) {
                ImGui::BulletText("workspace:FindFirstChild(name) · GetChildren()");
                ImGui::BulletText("workspace:GetDescendants() · FindFirstChildOfClass(\"Part\")");
                ImGui::BulletText("workspace:Raycast(origin, dir, maxDist?)");
                ImGui::BulletText("workspace.Terrain: Clear, FillBlock, FillBall, Read/WriteVoxels");
                ImGui::BulletText("workspace.CurrentCamera:ScreenPointToRay(x,y)");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Instance")) {
                ImGui::BulletText("Part: Name, Position, Touched, Explode, Destroy");
                ImGui::BulletText("Humanoid: WalkSpeed, JumpPower, TakeDamage");
                ImGui::BulletText("task.wait · task.spawn · task.delay");
                ImGui::BulletText("math.noise · math.clamp · Enum.Material");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Blocky way")) {
                ImGui::TextWrapped(
                    "Un solo modo sensato (niente doppioni):");
                ImGui::BulletText("UI gioco → StarterGui / ScreenGui (non ImGui da C++)");
                ImGui::BulletText("Tool → StarterPack + Equipped/Unequipped/Activated");
                ImGui::BulletText("Server logic → Script (Server) sullo stesso place");
                ImGui::BulletText("Client (camera/UI) → LocalScript (Client)");
                ImGui::BulletText("Codice condiviso → ModuleScript (require)");
                ImGui::BulletText("Grafica → Lighting → Performance / Studio Default / Beautiful");
                ImGui::Separator();
                ImGui::TextDisabled("PC Test e Android Hub usano lo stesso place.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
    }

    // Bottom dock: Output · Assets · Console (sopra command/status bar).
    if (safeOutputH > 0.f) {
        const float outX = leftW;
        const float outW = std::max(64.f, (float)g_w - leftW - rightW);
        const float outY = (float)g_h - chromeReserve - safeOutputH;
        drawBottomDockTabs(outX, outY, outW, safeOutputH, kStudioPanelFlags);
    }

    drawWindowSearchPopup();
    drawStudioFeaturesOverlays();
    drawStudio2020Overlays();
    drawStudioProOverlays();
    studioProPollHotkeys();
    drawScriptCreatedGuidePopup();
    drawSelectedPartLabel();
}
