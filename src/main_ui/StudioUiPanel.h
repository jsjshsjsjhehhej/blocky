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
            if (ImGui::MenuItem(L("menu.script_part"), "Ctrl+L")) addScriptToSelected();
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
            ImGui::MenuItem("Guida Lua Blocky", "F2", &g_showLuaGuide);
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
            ImGui::MenuItem("Guida Lua Blocky", "F2", &g_showLuaGuide);
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
        // Tab come Roblox Studio: Home · Avatar · UI · Script · Model · Test
        static const char* kTabs[] = { "Home", "Avatar", "UI", "Script", "Model", "Test" };
        if (g_studioRibbonTab < 0 || g_studioRibbonTab > 5) g_studioRibbonTab = 0;
        studio::ribbonTabs(kTabs, 6, g_studioRibbonTab);
        ImGui::Dummy({0.f, 3.f});
        ImGui::Indent(8.f);

        auto clearGizmoDrag = []() {
            g_scaleDragging = false;
            g_moveDragging = false;
            g_rotateDragging = false;
        };
        auto sep = []() {
            ImGui::SameLine(0.f, 8.f);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddLine({p.x, p.y + 4.f}, {p.x, p.y + 70.f}, IM_COL32(62, 70, 84, 255));
            ImGui::Dummy({8.f, 1.f});
            ImGui::SameLine(0.f, 0.f);
        };

        // ---- HOME: tools + Part + Play (come ribbon Studio) ----
        if (g_studioRibbonTab == 0) {
            if (studio::ribbonBigButton("sel", studio::Icon::Cursor, "Select",
                                        g_tool == StudioTool::Select)) {
                g_tool = StudioTool::Select; clearGizmoDrag();
            }
            if (studio::ribbonBigButton("mov", studio::Icon::Move, "Move",
                                        g_tool == StudioTool::Move)) {
                g_tool = StudioTool::Move; clearGizmoDrag();
            }
            if (studio::ribbonBigButton("scl", studio::Icon::Scale, "Scale",
                                        g_tool == StudioTool::Scale)) {
                g_tool = StudioTool::Scale; clearGizmoDrag();
            }
            if (studio::ribbonBigButton("rot", studio::Icon::Rotate, "Rotate",
                                        g_tool == StudioTool::Rotate)) {
                g_tool = StudioTool::Rotate; clearGizmoDrag();
            }
            sep();
            if (studio::ribbonBigButton("part", studio::Icon::Cube, "Part"))
                insertNewPart();
            if (studio::ribbonBigButton("mdl", studio::Icon::Package, "Model"))
                insertImportedModel();
            if (studio::ribbonBigButton("tbx", studio::Icon::Folder, "Toolbox", g_showToolbox))
                g_showToolbox = !g_showToolbox;
            if (studio::ribbonBigButton("ast", studio::Icon::Package, "Assets", g_showAssets)) {
                g_showAssets = !g_showAssets;
                if (g_showAssets) g_bottomDockTab = 1;
            }
            sep();
            drawStudioSnapViewRibbonExtras();
            drawStudio2020RibbonExtras();
            drawStudioProRibbonExtras();
            sep();
            if (studio::ribbonBigButton("scrH", studio::Icon::Script, "Script"))
                addScriptToSelected();
            if (studio::ribbonBigButton("playH", studio::Icon::Play, "Play",
                                       false, !g_studioTesting))
                startStudioTest(glfwGetCurrentContext());
        }
        // ---- AVATAR ----
        else if (g_studioRibbonTab == 1) {
            if (studio::ribbonBigButton("avat", studio::Icon::Torso, "Avatar"))
                enterAvatarPublisher();
            if (studio::ribbonBigButton("rig", studio::Icon::ArmRight, "Rig"))
                beginRigPlayer();
            if (studio::ribbonBigButton("anid", studio::Icon::Walk, "AnimId"))
                g_showPlayerAnimPublisher = true;
            if (studio::ribbonBigButton("friend", studio::Icon::Head, "Friend"))
                insertFriendPlayer();
        }
        // ---- UI ----
        else if (g_studioRibbonTab == 2) {
            if (studio::ribbonBigButton("tlbl", studio::Icon::NewFile, "TextLabel"))
                insertNewTextLabel();
            if (studio::ribbonBigButton("tbtn", studio::Icon::NewFile, "TextButton"))
                insertNewTextButton();
            if (studio::ribbonBigButton("frm", studio::Icon::NewFile, "Frame"))
                insertNewFrame();
            if (studio::ribbonBigButton("img", studio::Icon::NewFile, "ImageLabel"))
                insertNewImageLabel();
            if (studio::ribbonBigButton("lbl", studio::Icon::NewFile, "Label 3D"))
                insertNewLabel();
            if (studio::ribbonBigButton("snd", studio::Icon::Package, "Sound"))
                insertSound();
        }
        // ---- SCRIPT ----
        else if (g_studioRibbonTab == 3) {
            if (studio::ribbonBigButton("scr", studio::Icon::Script, "Script"))
                addScriptToSelected();
            if (studio::ribbonBigButton("anim", studio::Icon::Walk, "Animate"))
                beginAnimateSelectedModel();
            sep();
            if (studio::ribbonBigButton("out", studio::Icon::Script, "Output", g_showLuaOutput))
                g_showLuaOutput = !g_showLuaOutput;
            if (studio::ribbonBigButton("guide", studio::Icon::Script, "Guida Lua", g_showLuaGuide))
                g_showLuaGuide = !g_showLuaGuide;
        }
        // ---- MODEL ----
        else if (g_studioRibbonTab == 4) {
            if (studio::ribbonBigButton("blk", studio::Icon::Cube, "Block"))
                insertNewPart();
            if (studio::ribbonBigButton("ball", studio::Icon::Sphere, "Ball"))
                insertShapedPart(PartShape::Ball);
            if (studio::ribbonBigButton("cyl", studio::Icon::Cylinder, "Cylinder"))
                insertShapedPart(PartShape::Cylinder);
            if (studio::ribbonBigButton("wed", studio::Icon::Wedge, "Wedge"))
                insertShapedPart(PartShape::Wedge);
            sep();
            if (studio::ribbonBigButton("neon", studio::Icon::Light, "Neon"))
                insertNeonPart();
            if (studio::ribbonBigButton("pl", studio::Icon::Light, "PointLight"))
                insertPointLightPart();
            if (studio::ribbonBigButton("boom", studio::Icon::Trash, "Explode"))
                insertExplodePart();
            sep();
            if (studio::ribbonBigButton("dup", studio::Icon::Duplicate, "Duplicate"))
                duplicateSelected();
            if (studio::ribbonBigButton("del", studio::Icon::Trash, "Delete"))
                deleteSelected();
            sep();
            if (studio::ribbonBigButton("anc", studio::Icon::Anchor, "Anchor"))
                studioSetAnchoredOnSelection(true);
            if (studio::ribbonBigButton("uanc", studio::Icon::Anchor, "Unanchor"))
                studioSetAnchoredOnSelection(false);
            if (studio::ribbonBigButton("col", studio::Icon::Cube, "Collide"))
                studioSetCanCollideOnSelection(true);
            if (studio::ribbonBigButton("ncol", studio::Icon::Cube, "NoCollide"))
                studioSetCanCollideOnSelection(false);
            sep();
            drawStudioModelTabExtras();
            drawStudioTransformRibbonExtras();
        }
        // ---- TEST ----
        else if (g_studioRibbonTab == 5) {
            if (!g_studioTesting) {
                if (studio::ribbonBigButton("play", studio::Icon::Play, "Play"))
                    startStudioTest(glfwGetCurrentContext());
            } else {
                if (studio::ribbonBigButton("stop", studio::Icon::Stop, "Stop", true))
                    stopStudioTest(glfwGetCurrentContext());
            }
            sep();
            if (studio::ribbonBigButton("exp", studio::Icon::Folder, "Explorer", g_showExplorer))
                g_showExplorer = !g_showExplorer;
            if (studio::ribbonBigButton("prp", studio::Icon::Transform, "Properties", g_showProps))
                g_showProps = !g_showProps;
            if (studio::ribbonBigButton("tbx2", studio::Icon::Package, "Toolbox", g_showToolbox))
                g_showToolbox = !g_showToolbox;
            if (studio::ribbonBigButton("cam", studio::Icon::Camera, "Camera"))
                g_mode = CamMode::Studio;
        }

        if (!g_exportMsg.empty()) {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored({1.f, 0.85f, 0.4f, 1.f}, "%s", g_exportMsg.c_str());
        }
        ImGui::Unindent(8.f);
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

        drawExplorerToolbar();
        ImGui::Separator();
        // Albero game → Workspace / Services (come Studio a destra).
        ImGui::TextDisabled("game");
        if (ImGui::TreeNodeEx("Workspace", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Selectable("SpawnLocation##sp", g_selectedSpawn))
                selectSpawn();
            {
                char tbuf[96];
                std::snprintf(tbuf, sizeof(tbuf), "Terrain (%d voxel)##terrain", g_terrain.blockCount());
                ImGui::Selectable(tbuf, false);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "workspace.Terrain — FillBlock, ReadVoxels, WriteVoxels\n"
                        "Solo via script (Test/Play), stile Roblox Studio.");
            }

            if (ImGui::TreeNodeEx("Parts", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawExplorerPartsTree();
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Models", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (int i = 0; i < (int)g_world.models.size(); ++i) {
                    auto& m = g_world.models[i];
                    std::string label = m.name + "##model" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), i == g_selectedModel)) {
                        selectModel(i);
                        g_showProps = true;
                    }
                }
                if (ImGui::SmallButton("+ Import Model")) insertImportedModel();
                ImGui::TreePop();
            }

            ImGui::TreePop(); // Workspace
        }

        ImGui::Separator();
        if (ImGui::TreeNodeEx("Services", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Full DataModel service catalog (scriptable via GetService).
            if (ImGui::TreeNodeEx("All Services", ImGuiTreeNodeFlags_DefaultOpen)) {
                drawExplorerServiceTree();
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("StarterGui", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Selectable("StarterGui (service)##svc7", g_selectedService == 7))
                    toggleService(7);
                if (ImGui::TreeNodeEx("ScreenGui", ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (int i = 0; i < (int)g_world.labels.size(); ++i) {
                        auto& l = g_world.labels[i];
                        std::string label = l.className + " · " + l.name + "##lab" + std::to_string(i);
                        if (ImGui::Selectable(label.c_str(), i == g_selectedLabel))
                            selectLabel(i);
                    }
                    if (ImGui::SmallButton("+ TextLabel")) insertNewTextLabel();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("+ Label")) insertNewLabel();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("+ TextButton")) insertNewTextButton();
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("StarterPack", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Selectable("StarterPack (service)##svc15",
                        g_selectedService == studio::platform::Svc_StarterPack))
                    toggleService(studio::platform::Svc_StarterPack);
                ImGui::TextDisabled("Tool → hotbar in Test/Play (un solo percorso)");
                for (int i = 0; i < (int)g_world.backpack.size(); ++i) {
                    auto& b = g_world.backpack[i];
                    bool eq = (g_world.equippedBackpack == i);
                    std::string label = (eq ? "* " : "") + b.name + "##bp" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), i == g_selectedBackpack)) {
                        g_selectedBackpack = i;
                        g_showProps = true;
                    }
                }
                if (ImGui::SmallButton("+ Item")) {
                    g_world.addBackpackItem("Item", (int)g_world.backpack.size());
                    g_selectedBackpack = (int)g_world.backpack.size() - 1;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Sword"))
                    g_world.addBackpackItem("Sword", 3, BackpackTool::Sword);
                ImGui::SameLine();
                if (ImGui::SmallButton("+ Pistola"))
                    g_world.addBackpackItem("Pistola", 4, BackpackTool::Pistol);
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("Players", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Selectable("Players (service)##svc10", g_selectedService == 10))
                    toggleService(10);
                bool playerOpen = ImGui::TreeNodeEx("LocalPlayer",
                    ImGuiTreeNodeFlags_DefaultOpen
                    | (g_selectedPlayer && g_selectedPlayerPart < 0 ? ImGuiTreeNodeFlags_Selected : 0));
                if (ImGui::IsItemClicked())
                    selectPlayer(-1);
                if (playerOpen) {
                    ImGui::TextDisabled(g_player.humanoid.userRigged ? "Rig: OK" : "Rig: mancante");
                    if (ImGui::SmallButton("Riggare..."))
                        beginRigPlayer();
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Anim ID..."))
                        g_showPlayerAnimPublisher = true;

                    if (ImGui::TreeNodeEx("Humanoid", ImGuiTreeNodeFlags_DefaultOpen)) {
                        if (ImGui::IsItemClicked())
                            selectPlayer(-1);
                        for (int i = 0; i < (int)g_player.humanoid.parts.size(); ++i) {
                            auto& bp = g_player.humanoid.parts[i];
                            bool sel = g_selectedPlayer && g_selectedPlayerPart == i;
                            if (ImGui::Selectable(bp.name.c_str(), sel))
                                selectPlayer(i);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("StarterPlayer")) {
                if (ImGui::Selectable("StarterPlayer##svc13",
                        g_selectedService == studio::platform::Svc_StarterPlayer))
                    toggleService(studio::platform::Svc_StarterPlayer);
                if (ImGui::Selectable("StarterCharacter##svc14",
                        g_selectedService == studio::platform::Svc_StarterCharacter))
                    toggleService(studio::platform::Svc_StarterCharacter);
                ImGui::TreePop();
            }
            if (ImGui::TreeNodeEx("SoundService", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::Selectable("SoundService (service)##svc30",
                        g_selectedService == studio::platform::Svc_SoundService))
                    toggleService(studio::platform::Svc_SoundService);
                for (int i = 0; i < (int)g_sounds.sounds.size(); ++i) {
                    auto& s = g_sounds.sounds[i];
                    std::string label = s.name + (s.playing ? " [playing]" : "") + "##snd" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), i == g_selectedSound)) {
                        selectSound(i);
                        g_showProps = true;
                    }
                }
                if (ImGui::SmallButton("+ Sound")) insertSound();
                ImGui::TreePop();
            }
            bool animSel = g_selectedService < 0 && !g_selectedPlayer && !g_selectedSpawn
                && g_selectedPart < 0 && g_selectedModel < 0 && g_selectedSound < 0
                && g_selectedLabel < 0;
            if (ImGui::Selectable("Animator##svc", animSel)) {
                clearAllSelection();
                g_showProps = true;
            }
            ImGui::TextDisabled("clip: %s", g_animator.lastPlayed.c_str());
            ImGui::TreePop();
        }

        ImGui::Separator();
        if (ImGui::BeginMenu("Create object")) {
            if (ImGui::MenuItem("Part")) insertNewPart();
            if (ImGui::MenuItem("Ball")) insertShapedPart(PartShape::Ball);
            if (ImGui::MenuItem("Cylinder")) insertShapedPart(PartShape::Cylinder);
            if (ImGui::MenuItem("Wedge")) insertShapedPart(PartShape::Wedge);
            if (ImGui::MenuItem("Model (import)")) insertImportedModel();
            ImGui::Separator();
            if (ImGui::MenuItem("TextLabel")) insertNewTextLabel();
            if (ImGui::MenuItem("TextButton")) insertNewTextButton();
            if (ImGui::MenuItem("Sound")) insertSound();
            ImGui::Separator();
            if (ImGui::MenuItem("Script on selection")) addScriptToSelected();
            if (ImGui::MenuItem("Tool (StarterPack)"))
                g_world.addBackpackItem("Tool", (int)g_world.backpack.size());
            ImGui::EndMenu();
        }
        if (ImGui::Button("Aggiungi Script al blocco", {-1, 0}))
            addScriptToSelected();
        if (g_showFbx && fbxOk) ImGui::BulletText("FBX (legacy)");
        ImGui::End();
        studio_chrome::popDockPanelColors();
    }

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
            studio_chrome::drawClassNameBanner("Part", p.name.c_str());
            drawMultiPartPropertiesSection();
            ImGui::TextDisabled("Shape: %s", partShapeName(p.shape));
            if (ImGui::InputText("Name", g_partNameBuf, sizeof(g_partNameBuf)))
                p.name = g_partNameBuf;
            {
                int sh = (int)p.shape;
                if (sh < 0 || sh >= (int)PartShape::Count) sh = 0;
                const char* shapes[] = { "Block", "Ball", "Cylinder", "Wedge" };
                if (ImGui::Combo("Shape", &sh, shapes, IM_ARRAYSIZE(shapes))) {
                    p.shape = (PartShape)sh;
                    if (p.shape != PartShape::Block) p.useStud = false;
                    markPlaceDirty();
                }
            }

            ImGui::Separator();
            ImGui::Text("Transform");
            ImGui::DragFloat3("Position", &p.position.x, 0.25f);
            ImGui::DragFloat3("Size", &p.size.x, 0.25f, 0.05f, 100000.f);
            ImGui::DragFloat3("Rotation °", &p.rotation.x, 1.f);
            if (g_tool == StudioTool::Move)
                ImGui::TextWrapped("Muovi: frecce RGB. Snap 0.25 · Shift=0.05 · Ctrl=libero.");
            else if (g_tool == StudioTool::Scale)
                ImGui::TextWrapped("Scala: cubetti sulle facce (lento · Shift=piu' veloce · Ctrl=libero).");
            else if (g_tool == StudioTool::Rotate)
                ImGui::TextWrapped("Ruota: anelli RGB + tick 15°. Snap 5° · Shift=1° · Ctrl=libero.");
            else
                ImGui::TextWrapped("Seleziona: scegli Muovi / Scala / Ruota dalla toolbar.");
            if (ImGui::Button("Allarga X")) p.size.x = std::min(100000.f, p.size.x + 1.f);
            ImGui::SameLine();
            if (ImGui::Button("Allarga Z")) p.size.z = std::min(100000.f, p.size.z + 1.f);
            ImGui::SameLine();
            if (ImGui::Button("Alza Y")) p.size.y = std::min(100000.f, p.size.y + 1.f);
            if (ImGui::Button("Ingrandisci x1.25")) studioScaleSelection(1.25f);
            ImGui::SameLine();
            if (ImGui::Button("Riduci x0.8")) studioScaleSelection(0.8f);

            ImGui::Separator();
            drawPartColorPicker(p.color);
            ImGui::Separator();
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help) \
            partprops::editUi(luaName, p.field, mn, mx);          \
            partprops::helpTooltip(help);
#include "engine/PartProps.def"
#undef PART_PROP
            if (!p.anchored)
                ImGui::TextColored({0.45f, 0.8f, 1.f, 1.f}, "Non ancorata: in Play cade, rotola e si spinge");
            ImGui::Checkbox("Stud texture", &p.useStud);

            ImGui::Separator();
            ImGui::TextColored({0.85f, 0.75f, 0.35f, 1.f}, "Material");
            {
                int matIdx = (int)p.material;
                if (matIdx < 0 || matIdx >= (int)PartMaterial::Count) matIdx = 0;
                const char* mats[] = {
                    "Plastic", "SmoothPlastic", "Neon", "Metal", "Wood",
                    "Concrete", "Grass", "Glass", "Ice", "Brick", "ForceField",
                    "Fabric", "Marble", "Sand", "Slate", "DiamondPlate",
                    "CorrodedMetal", "Granite", "Asphalt"
                };
                if (ImGui::Combo("Material##mat", &matIdx, mats, IM_ARRAYSIZE(mats)))
                    p.setMaterial((PartMaterial)matIdx);
            }
            drawPartStudioExtras(p);
            ImGui::SliderFloat("Emissive", &p.emissive, 0.f, 2.f, "%.2f");
            ImGui::Checkbox("PointLight (illumina intorno)", &p.pointLight);
            if (p.pointLight) {
                ImGui::SliderFloat("Light Range", &p.lightRange, 4.f, 60.f, "%.0f");
                ImGui::SliderFloat("Light Brightness", &p.lightBrightness, 0.2f, 4.f, "%.2f");
                ImGui::TextDisabled("Colore luce = colore Part · max 8 luci attive");
            }

            ImGui::Separator();
            ImGui::TextColored({1.f, 0.45f, 0.3f, 1.f}, "Distruzione");
            ImGui::Checkbox("CanDestroy", &p.canDestroy);
            ImGui::Checkbox("BoomOnTouch", &p.boomOnTouch);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("In Test: 1 avvicinamento = boom");
            ImGui::SliderFloat("BoomAutoSecs", &p.boomAutoSecs, 0.f, 15.f, "%.1f s");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("0 = solo tocco; >0 = esplode da solo dopo N secondi in Test");
            if (!p.canDestroy)
                ImGui::TextDisabled("Destroy / Explode / spada non la rimuovono");
            else {
                ImGui::SliderFloat("Health", &p.health, 1.f, 500.f, "%.0f");
                p.maxHealth = std::max(p.maxHealth, p.health);
                if (ImGui::Button("Explodi ora", {180, 0})) {
                    glm::vec3 boomPos = p.position;
                    if (g_studioTesting || g_screen == AppScreen::Playing)
                        doBoomAt(boomPos, 14.f, 55.f, 24);
                    else
                        g_particles.spawnExplosion(boomPos, 14.f, 24);
                    g_exportMsg = "BOOM!";
                    if ((g_studioTesting || g_screen == AppScreen::Playing) && p.canDestroy) {
                        int idx = g_selectedPart;
                        selectPart(-1);
                        if (idx >= 0 && idx < (int)g_world.parts.size())
                            g_world.parts.erase(g_world.parts.begin() + idx);
                    } else if (!g_studioTesting && g_screen == AppScreen::Studio) {
                        g_exportMsg += " (solo FX in editor — Test per danno/distruggi)";
                    }
                }
            }

            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.75f, 0.18f, 0.15f, 1.f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.90f, 0.28f, 0.20f, 1.f});
            if (ImGui::Button("Rimuovi Part", {-1, 0})) deleteSelected();
            ImGui::PopStyleColor(2);

            ImGui::Separator();
            ImGui::TextColored({0.9f, 0.7f, 0.2f, 1.f}, "Script del blocco");
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
                launchDedicatedServer(g_maxPlayers, g_gameNameBuf[0] ? g_gameNameBuf : "Blocky");
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
        ImGui::Text("%s: %s", L("lang"), locLangLabel(loc().lang));
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
