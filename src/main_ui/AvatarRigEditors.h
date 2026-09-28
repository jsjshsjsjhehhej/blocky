// Finestre editor per il rig del player e per l'animazione dei modelli,
// import di accessori avatar da file, e l'intera UI dell'Avatar Publisher
// (wizard di posizionamento, gizmo, pubblicazione nello shop). Non e' un
// header riusabile: usa direttamente le globali di main.cpp (g_catalog,
// g_showRigPrompt, g_animEditModel, ecc.). Compila solo se incluso da
// main.cpp esattamente in questo punto.

static void drawPlayerRigWindows() {
    if (g_showBackpackOffer) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({460, 240}, ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        bool open = true;
        if (ImGui::Begin("Mettere nel Backpack?##bpoffer", &open, ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextColored({1.f, 0.75f, 0.3f, 1.f}, "Import da Blender · arma rilevata");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Il file \"%s\" sembra una pistola/arma.\n"
                "Vuoi aggiungerla al Backpack come item (hotbar 1-9 in Test)?",
                g_backpackOfferName.c_str());
            ImGui::Spacing();
            ImGui::TextDisabled("Si = item Pistol nel Backpack  ·  No = solo modello nel mondo");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
            if (ImGui::Button("Si, mettila nel Backpack", {-1, 42}))
                confirmBackpackOffer(true);
            ImGui::PopStyleColor();
            if (ImGui::Button("No, solo modello", {-1, 0}))
                confirmBackpackOffer(false);
        }
        ImGui::End();
        if (!open) confirmBackpackOffer(false);
    }

    if (g_showPlayerRigPrompt) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({440, 220}, ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        bool open = true;
        if (ImGui::Begin("Rig Player##playerrigask", &open, ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextColored({1.f, 0.7f, 0.3f, 1.f}, "Workspace → Players → LocalPlayer");
            ImGui::Spacing();
            ImGui::TextWrapped("Il Player non e' ancora riggato.\nVuoi creare il Rig R6 (Head, Torso, Arm, Leg)?");
            ImGui::Spacing();
            ImGui::TextDisabled("Potrai spostare, ruotare e ridimensionare ogni parte.");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
            if (ImGui::Button("Si, rigga il Player", {-1, 42}))
                confirmPlayerRig(true);
            ImGui::PopStyleColor();
            if (ImGui::Button("No, annulla", {-1, 0}))
                confirmPlayerRig(false);
        }
        ImGui::End();
        if (!open) confirmPlayerRig(false);
    }

    if (!g_showPlayerRigEditor) return;

    if (g_playerRigEditorNeedsPlace) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos({center.x, center.y + 40.f}, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({520, 380}, ImGuiCond_Always);
        g_playerRigEditorNeedsPlace = false;
    }
    if (ImGui::Begin("Rigging Player R6##playerrigedit", &g_showPlayerRigEditor)) {
        auto& h = g_player.humanoid;
        ImGui::TextColored({0.45f, 0.85f, 1.f, 1.f}, "LocalPlayer · Humanoid");
        ImGui::SameLine();
        ImGui::TextDisabled(h.userRigged ? "| Rig attivo" : "| non riggato");
        ImGui::Checkbox("RigEnabled", &h.rigEnabled);
        ImGui::SliderFloat("WalkSpeed", &h.walkSpeed, 4.f, 40.f);
        ImGui::SliderFloat("JumpPower", &h.jumpPower, 20.f, 90.f);
        ImGui::SliderFloat("Health", &h.health, 0.f, h.maxHealth);

        ImGui::Separator();
        ImGui::Text("Parti del Rig (seleziona e modifica)");
        ImGui::BeginChild("##rigparts", {160, 220}, true);
        if (ImGui::Selectable("LocalPlayer##root", g_selectedPlayer && g_selectedPlayerPart < 0))
            selectPlayer(-1);
        for (int i = 0; i < (int)h.parts.size(); ++i) {
            bool sel = g_selectedPlayer && g_selectedPlayerPart == i;
            if (ImGui::Selectable(h.parts[i].name.c_str(), sel))
                selectPlayer(i);
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##rigprops", {0, 220}, true);
        if (g_selectedPlayerPart >= 0 && g_selectedPlayerPart < (int)h.parts.size()) {
            auto& p = h.parts[g_selectedPlayerPart];
            ImGui::TextColored({0.95f, 0.75f, 0.3f, 1.f}, "%s", p.name.c_str());
            ImGui::DragFloat3("Offset (spostamento)", &p.offset.x, 0.05f);
            ImGui::DragFloat3("Size (ingrandimento)", &p.size.x, 0.05f, 0.2f, 8.f);
            ImGui::DragFloat3("Pivot", &p.pivot.x, 0.05f);
            ImGui::DragFloat("Rot X °", &p.rotX, 1.f, -180.f, 180.f);
            ImGui::DragFloat("Rot Z °", &p.rotZ, 1.f, -180.f, 180.f);
            ImGui::ColorEdit3("Colore", &p.color.x);
            ImGui::Checkbox("IsLimb", &p.isLimb);
        } else {
            ImGui::TextWrapped("Seleziona una parte a sinistra (es. Left Arm) per fare il rigging: spostamento, rotazione, scala.");
            ImGui::Spacing();
            if (ImGui::Button("Reset Rig R6 default", {-1, 32})) {
                bool was = h.userRigged;
                float ws = h.walkSpeed, jp = h.jumpPower, hp = h.health;
                h = Humanoid::createR6Blocky();
                h.userRigged = was;
                h.walkSpeed = ws;
                h.jumpPower = jp;
                h.health = hp;
                g_exportMsg = "Rig Player ripristinato (R6 default)";
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Play Wave (prova)")) g_animator.play("Wave");
        ImGui::SameLine();
        if (ImGui::Button("Play Dance")) g_animator.play("Dance");
        ImGui::SameLine();
        if (ImGui::Button("Idle")) g_animator.stop();
        ImGui::TextDisabled("In Studio vedi il Player aggiornato subito nel viewport.");
    }
    ImGui::End();
    if (!g_showPlayerRigEditor) {
    }
}

static void drawModelAnimWindows() {
    if (g_showRigPrompt) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({420, 210}, ImGuiCond_Always);
        ImGui::SetNextWindowFocus();
        bool open = true;
        if (ImGui::Begin("Rig richiesto##rigask", &open, ImGuiWindowFlags_NoCollapse)) {
            const char* mname = "?";
            if (g_animEditModel >= 0 && g_animEditModel < (int)g_world.models.size())
                mname = g_world.models[g_animEditModel].name.c_str();
            ImGui::TextColored({1.f, 0.7f, 0.3f, 1.f}, "Crea Rig Root");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "\"%s\" non ha ancora un Rig per l'editor.\n"
                "Crea un Rig Root: poi sposti, ruoti e scali con i keyframe.",
                mname);
            ImGui::Spacing();
            ImGui::TextDisabled("Workflow: Move → Set Frame (K) → Play");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.65f, 0.40f, 1.f});
            if (ImGui::Button("Crea Rig e anima", {-1, 40}))
                confirmModelRig(true);
            ImGui::PopStyleColor();
            if (ImGui::Button("Apri comunque", {-1, 0}))
                confirmModelRig(false);
        }
        ImGui::End();
        if (!open) confirmModelRig(false);
    }

    if (!g_showAnimEditor || g_animEditModel < 0 || g_animEditModel >= (int)g_world.models.size())
        return;

    auto& m = g_world.models[g_animEditModel];
    if (m.anim.fps < 1) m.anim.fps = 30;
    if (g_animEditorNeedsPlace) {
        ImGui::SetNextWindowPos({(float)g_w * 0.5f, (float)g_h - 8.f}, ImGuiCond_Always, {0.5f, 1.f});
        ImGui::SetNextWindowSize({std::min(720.f, (float)g_w - 24.f), 280.f}, ImGuiCond_Always);
        g_animEditorNeedsPlace = false;
    }

    if (g_animLiveCapture && !g_animPlaying && g_animSelKey >= 0 && g_animSelKey < (int)m.anim.keys.size()) {
        auto& k = m.anim.keys[g_animSelKey];
        float d = glm::length(k.position - m.position)
                + glm::length(k.rotation - m.rotation) * 0.01f
                + glm::length(k.scale - m.scale);
        if (d > 0.001f) {
            k.position = m.position;
            k.rotation = m.rotation;
            k.scale = m.scale;
        }
    }

    auto gotoFrame = [&](int fr) {
        fr = std::max(0, fr);
        g_animPlaying = false;
        g_animScrub = m.anim.frameToTime(fr);
        m.anim.duration = std::max(m.anim.duration, g_animScrub);
        applyModelAnimSample(m, g_animScrub);
        g_animSelKey = -1;
        for (int i = 0; i < (int)m.anim.keys.size(); ++i) {
            int kf = m.anim.keys[i].frame > 0 ? m.anim.keys[i].frame : m.anim.timeToFrame(m.anim.keys[i].time);
            if (kf == fr) { g_animSelKey = i; break; }
        }
    };

    auto setAnimateFrameHere = [&](bool advance) {
        int fr = m.anim.timeToFrame(g_animScrub);
        g_animSelKey = m.anim.animateFrame(fr, m.position, m.rotation, m.scale);
        m.anim.duration = std::max(m.anim.duration, m.anim.frameToTime(fr));
        g_exportMsg = "AnimateFrame(" + std::to_string(fr) + ", "
            + std::to_string((int)m.position.x) + "," + std::to_string((int)m.position.y)
            + "," + std::to_string((int)m.position.z) + ")";
        if (advance && g_animStepFrames > 0)
            gotoFrame(fr + g_animStepFrames);
    };

    if (ImGui::Begin("AnimateFrame Studio##animedit", &g_showAnimEditor)) {
        ImGui::TextColored({0.25f, 0.75f, 1.f, 1.f}, "AnimateFrame");
        ImGui::SameLine();
        ImGui::TextDisabled("· %s · Rig %s", m.name.c_str(), m.rig.rootName.c_str());

        char clipName[64];
        std::snprintf(clipName, sizeof(clipName), "%s", m.anim.name.c_str());
        ImGui::SetNextItemWidth(160);
        if (ImGui::InputText("Clip", clipName, sizeof(clipName)))
            m.anim.name = clipName;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        ImGui::DragInt("FPS", &m.anim.fps, 1, 1, 120);
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &m.anim.looped);
        ImGui::SameLine();
        ImGui::Checkbox("Live", &g_animLiveCapture);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Sposta il modello nel viewport: aggiorna il frame selezionato");

        int scrubFrame = m.anim.timeToFrame(g_animScrub);
        int maxFrame = std::max(30, m.anim.lastFrame() + m.anim.fps);
        for (const auto& k : m.anim.keys)
            maxFrame = std::max(maxFrame, (k.frame > 0 ? k.frame : m.anim.timeToFrame(k.time)) + 10);
        m.anim.duration = std::max(m.anim.duration, m.anim.frameToTime(maxFrame));

        if (ImGui::Button("|<")) gotoFrame(0);
        ImGui::SameLine();
        if (ImGui::Button("<<")) gotoFrame(scrubFrame - g_animStepFrames);
        ImGui::SameLine();
        if (ImGui::Button("<")) gotoFrame(scrubFrame - 1);
        ImGui::SameLine();
        if (ImGui::Button(g_animPlaying ? "Pause" : "Play", {70, 0}))
            g_animPlaying = !g_animPlaying;
        ImGui::SameLine();
        if (ImGui::Button(">")) gotoFrame(scrubFrame + 1);
        ImGui::SameLine();
        if (ImGui::Button(">>")) gotoFrame(scrubFrame + g_animStepFrames);
        ImGui::SameLine();
        if (ImGui::Button(">|")) gotoFrame(m.anim.lastFrame());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        ImGui::DragInt("Step", &g_animStepFrames, 1, 1, 60);

        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##timeline", &scrubFrame, 0, maxFrame, "Frame %d"))
            gotoFrame(scrubFrame);

        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            float h = 36.f;
            ImGui::InvisibleButton("##dope", {w, h});
            ImVec2 p1 = {p0.x + w, p0.y + h};
            dl->AddRectFilled(p0, p1, IM_COL32(18, 32, 52, 255), 6.f);
            for (int f = 0; f <= maxFrame; f += 5) {
                float x = p0.x + (w - 4.f) * ((float)f / (float)maxFrame) + 2.f;
                dl->AddLine({x, p0.y + 4}, {x, p1.y - 4},
                    f % 10 == 0 ? IM_COL32(120, 160, 210, 180) : IM_COL32(70, 100, 140, 120));
            }
            for (const auto& k : m.anim.keys) {
                int f = k.frame > 0 ? k.frame : m.anim.timeToFrame(k.time);
                float x = p0.x + (w - 4.f) * ((float)f / (float)maxFrame) + 2.f;
                dl->AddCircleFilled({x, p0.y + h * 0.5f}, 5.f, IM_COL32(255, 190, 60, 255));
            }
            float px = p0.x + (w - 4.f) * ((float)scrubFrame / (float)maxFrame) + 2.f;
            dl->AddLine({px, p0.y + 2}, {px, p1.y - 2}, IM_COL32(80, 220, 120, 255), 2.f);
            if (ImGui::IsItemActive() || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))) {
                float t = std::clamp((ImGui::GetIO().MousePos.x - p0.x) / std::max(1.f, w), 0.f, 1.f);
                gotoFrame((int)std::lround(t * maxFrame));
            }
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.38f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.75f, 0.48f, 1.f});
        if (ImGui::Button("Set AnimateFrame (K)", {-1, 40}))
            setAnimateFrameHere(true);
        ImGui::PopStyleColor(2);

        ImGui::TextDisabled("Scorciatoie: ← → frame  |  K set+avanza  |  Space play  |  C/V copia/incolla");

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) gotoFrame(scrubFrame - 1);
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) gotoFrame(scrubFrame + 1);
            if (ImGui::IsKeyPressed(ImGuiKey_K)) setAnimateFrameHere(true);
            if (ImGui::IsKeyPressed(ImGuiKey_Space)) g_animPlaying = !g_animPlaying;
            if (ImGui::IsKeyPressed(ImGuiKey_C) && g_animSelKey >= 0 && g_animSelKey < (int)m.anim.keys.size()) {
                const auto& k = m.anim.keys[g_animSelKey];
                g_animClipboard = {k.frame > 0 ? k.frame : m.anim.timeToFrame(k.time), k.position, k.rotation, k.scale};
                g_animHaveClipboard = true;
                g_exportMsg = "Frame copiato";
            }
            if (ImGui::IsKeyPressed(ImGuiKey_V) && g_animHaveClipboard) {
                g_animSelKey = m.anim.animateFrame(scrubFrame, g_animClipboard.position,
                    g_animClipboard.rotation, g_animClipboard.scale);
                applyModelAnimSample(m, g_animScrub);
                g_exportMsg = "Frame incollato su F" + std::to_string(scrubFrame);
            }
        }

        ImGui::Separator();
        ImGui::BeginChild("##keys", {0, 120}, true);
        for (int i = 0; i < (int)m.anim.keys.size(); ++i) {
            auto& k = m.anim.keys[i];
            if (k.frame <= 0) k.frame = m.anim.timeToFrame(k.time);
            char lab[128];
            std::snprintf(lab, sizeof(lab), "F%-4d  pos (%.1f, %.1f, %.1f)##k%d",
                k.frame, k.position.x, k.position.y, k.position.z, i);
            if (ImGui::Selectable(lab, i == g_animSelKey))
                gotoFrame(k.frame);
        }
        ImGui::EndChild();

        if (g_animSelKey >= 0 && g_animSelKey < (int)m.anim.keys.size()) {
            auto& k = m.anim.keys[g_animSelKey];
            int keyFr = k.frame > 0 ? k.frame : m.anim.timeToFrame(k.time);
            ImGui::Text("Key F%d", keyFr);
            if (ImGui::DragInt("Frame##kf", &keyFr, 1, 0, 9999)) {
                k.frame = keyFr;
                k.time = m.anim.frameToTime(keyFr);
                m.anim.duration = std::max(m.anim.duration, k.time);
                m.anim.sortKeys();
                gotoFrame(keyFr);
            }
            if (ImGui::DragFloat3("Position", &k.position.x, 0.1f))
                m.position = k.position;
            if (ImGui::DragFloat3("Rotation", &k.rotation.x, 1.f))
                m.rotation = k.rotation;
            if (ImGui::DragFloat3("Scale", &k.scale.x, 0.05f, 0.05f, 80.f))
                m.scale = k.scale;

            if (ImGui::Button("Copia")) {
                g_animClipboard = {keyFr, k.position, k.rotation, k.scale};
                g_animHaveClipboard = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Incolla qui") && g_animHaveClipboard) {
                g_animSelKey = m.anim.animateFrame(scrubFrame, g_animClipboard.position,
                    g_animClipboard.rotation, g_animClipboard.scale);
            }
            ImGui::SameLine();
            if (ImGui::Button("Elimina") && m.anim.keys.size() > 1) {
                m.anim.keys.erase(m.anim.keys.begin() + g_animSelKey);
                g_animSelKey = std::clamp(g_animSelKey, 0, (int)m.anim.keys.size() - 1);
            }
        }

        ImGui::Separator();
        ImGui::TextWrapped(
            "Flusso rapido: posiziona il modello → Set AnimateFrame → si avanza di Step frame → ripeti → Play.");
        ImGui::TextDisabled("Lua: AnimateFrame(frame, Vector3.new(x,y,z))");
    }
    ImGui::End();
    if (!g_showAnimEditor) {
        g_animPlaying = false;
        g_animEditModel = -1;
    }
}

#include "main_ui/StudioUiPanel.h"

static bool importAccessoryFromPath(const std::string& path) {
    if (path.empty()) {
        g_catalog.status = "Percorso vuoto";
        return false;
    }
    auto mod = Moderation::checkAvatarOrModelName(path);
    if (mod.verdict != Moderation::Verdict::Ok) {
        g_catalog.status = mod.reason;
        g_menu.chatLines.push_back(mod.reason);
        return false;
    }
    AvatarAccessory acc;
    acc.slot = g_catalog.pendingSlot;
    acc.replacesBody = g_catalog.pendingAsBodyReplace;
    acc.offset = acc.replacesBody ? bodyReplaceOffsetForSlot(acc.slot)
                                  : defaultOffsetForSlot(acc.slot);
    acc.scale = {1, 1, 1};
    auto slash = path.find_last_of("\\/");
    acc.name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    std::string lib = copyModelToLibrary(path, acc.name);
    acc.sourcePath = lib.empty() ? path : lib;
    if (!acc.mesh.load(acc.sourcePath)) {
        if (acc.sourcePath != path && acc.mesh.load(path))
            acc.sourcePath = path;
        else {
            g_catalog.status = "Import fallito (formato o file): " + path;
            return false;
        }
    }
    if (acc.replacesBody) {
        for (int i = (int)g_catalog.items.size() - 1; i >= 0; --i) {
            if (g_catalog.items[i].slot == acc.slot && g_catalog.items[i].replacesBody)
                g_catalog.items.erase(g_catalog.items.begin() + i);
        }
        g_catalog.hideDefaultPart[(int)acc.slot] = true;
    }
    g_catalog.items.push_back(std::move(acc));
    g_catalog.selected = (int)g_catalog.items.size() - 1;
    const auto& back = g_catalog.items.back();
    upsertOwnedAvatarInventory(back);
    saveShopItems(g_menu);
    saveEquippedAvatar(g_catalog);
    syncEquippedToMenu();
    g_catalog.status = back.replacesBody
        ? (std::string("Equipaggiata — ") + back.name + " sostituisce " + avatarSlotName(back.slot)
           + " (anche senza Pubblica)")
        : (std::string("Equipaggiato — ") + back.name + " su " + avatarSlotName(back.slot)
           + " (" + std::to_string(back.mesh.vertexCount) + " vertici)");
    g_catalog.camDist = 10.f;
    g_catalog.camPitch = -12.f;
    offerBackpackForGunImport(path.empty() ? back.name : path);
    return true;
}

static void* publisherHwnd() {
#ifdef _WIN32
    GLFWwindow* w = glfwGetCurrentContext();
    return w ? (void*)glfwGetWin32Window(w) : nullptr;
#else
    return nullptr;
#endif
}

// Flusso completo "importa e pubblica un'animazione", senza creare finestre:
// lo chiamano sia il pannello fluttuante di Studio (drawStudioPlayerAnimPanel)
// sia la categoria Animazioni dell'Avatar Editor, che prima ne tenevano due
// copie quasi identiche e leggermente divergenti.
static void drawWalkAnimWorkflow() {
    if (ImGui::Button("Apri guida web (HTML)", {-1, 30}))
        openWalkingAnimWebGuide();
    ImGui::TextWrapped(
        "1) Scarica R6 e anima in Blender/Maya  2) Importa FBX/GLB\n"
        "3) Scegli AnimationId e pubblica  4) Usalo: game.Animator:Play(\"Id\")\n"
        "Per una pistola basta animare Right Arm (un solo arto va bene).");

    ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.48f, 0.88f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.25f, 0.58f, 0.98f, 1.f});
    if (ImGui::Button("1. Scarica modello Player R6 (default)", {-1, 40}))
        downloadDefaultR6PlayerModel();
    ImGui::PopStyleColor(2);
    ImGui::TextDisabled("OBJ + MTL · Head / Torso / Left_Arm / Right_Arm / Left_Leg / Right_Leg");

    ImGui::InputText("Action/clip in Blender-Maya", g_catalog.walkClipPrefBuf,
                     sizeof(g_catalog.walkClipPrefBuf));
    ImGui::TextDisabled("Es. WalkingAnimation, FancyWalk, Shoot, Reload...");

    ImGui::PushStyleColor(ImGuiCol_Button, {0.55f, 0.28f, 0.85f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.68f, 0.38f, 0.95f, 1.f});
    if (ImGui::Button("2. Importa animazione (FBX / GLB)", {-1, 40}))
        importWalkingAnimationFromFile();
    ImGui::PopStyleColor(2);

    if (g_catalog.hasPendingWalk && g_catalog.pendingWalk.valid()) {
        ImGui::TextColored({0.4f, 1.f, 0.55f, 1.f}, "Clip: %s · %d keyframe",
                           g_catalog.pendingWalk.clipName.c_str(),
                           (int)g_catalog.pendingWalk.keys.size());
        if (!g_catalog.pendingClipNames.empty()) {
            ImGui::TextDisabled("Clip nel file:");
            for (const auto& n : g_catalog.pendingClipNames)
                ImGui::BulletText("%s", n.c_str());
        }
        ImGui::InputText("AnimationId (scegli tu)", g_catalog.walkNameBuf,
                         sizeof(g_catalog.walkNameBuf));
        ImGui::Checkbox("Usa come walk di default", &g_catalog.pendingAsWalkDefault);
        if (g_catalog.pendingAsWalkDefault && Animator::looksLikeToolAnim(g_catalog.pendingWalk))
            ImGui::TextColored({1.f, 0.75f, 0.3f, 1.f},
                "Attenzione: sembra un'anim pistola/tool — meglio NON usarla come walk");
        ImGui::Checkbox("Anteprima sul player", &g_catalog.previewWalk);
        ImGui::TextWrapped("Dopo Pubblica: game.Animator:Play(\"%s\")",
                           g_catalog.walkNameBuf[0] ? g_catalog.walkNameBuf : "Id");

        ImGui::PushStyleColor(ImGuiCol_Button, {0.12f, 0.62f, 0.42f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.75f, 0.52f, 1.f});
        if (ImGui::Button("3. Pubblica e ottieni AnimationId", {-1, 46}))
            releaseWalkingAnimation();
        ImGui::PopStyleColor(2);
    }

    if (g_lastPublishedAnimId[0]) {
        ImGui::Separator();
        ImGui::TextColored({0.35f, 1.f, 0.55f, 1.f}, "AnimationId pubblicato");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.12f, 0.18f, 0.14f, 1.f});
        ImGui::BeginChild("##pubIdBox", {0, 68}, true);
        ImGui::SetWindowFontScale(1.30f);
        ImGui::Text("%s", g_lastPublishedAnimId);
        ImGui::SetWindowFontScale(1.f);
        ImGui::TextDisabled("Incolla questo ID nello script");
        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (ImGui::Button("Copia AnimationId", {-1, 30}))
            ImGui::SetClipboardText(g_lastPublishedAnimId);
        if (ImGui::Button("Copia: Play(\"...\")", {-1, 26})) {
            std::string snip = std::string("game.Animator:Play(\"")
                + g_lastPublishedAnimId + "\")";
            ImGui::SetClipboardText(snip.c_str());
        }
        if (ImGui::Button("Copia: SetWalkAnim(\"...\")", {-1, 26})) {
            std::string snip = std::string("game.Animator:SetWalkAnim(\"")
                + g_lastPublishedAnimId + "\")";
            ImGui::SetClipboardText(snip.c_str());
        }
        if (ImGui::Button("Copia: SetJumpAnim(\"...\")", {-1, 26})) {
            std::string snip = std::string("game.Animator:SetJumpAnim(\"")
                + g_lastPublishedAnimId + "\")";
            ImGui::SetClipboardText(snip.c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Prova Play"))
            g_animator.play(g_lastPublishedAnimId);
    }

    if (!g_catalog.walkClips.empty()) {
        ImGui::Separator();
        ImGui::Text("Animazioni pubblicate (ID)");
        ImGui::BeginChild("##pubList", {0, 150}, true);
        for (int i = 0; i < (int)g_catalog.walkClips.size(); ++i) {
            const auto& c = g_catalog.walkClips[i];
            ImGui::PushID(3100 + i);
            const bool eq = (c.id == g_catalog.equippedWalkId);
            const char* showId = c.displayName.empty() ? c.id.c_str() : c.displayName.c_str();
            if (eq) ImGui::TextColored({0.3f, 0.9f, 0.5f, 1.f}, "* %s", showId);
            else    ImGui::Text("%s", showId);
            ImGui::SameLine();
            if (ImGui::SmallButton("Copia"))
                ImGui::SetClipboardText(showId);
            ImGui::SameLine();
            if (ImGui::SmallButton("Play"))
                g_animator.play(showId);
            ImGui::SameLine();
            if (!eq) {
                if (ImGui::SmallButton("Walk default")) {
                    g_catalog.equippedWalkId = c.id;
                    syncEquippedWalkToAnimator();
                    r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
                    animLibrary().setDefaultWalk(showId);
                    g_catalog.status = "Walk default = " + std::string(showId);
                }
            } else {
                if (ImGui::SmallButton("Classic")) {
                    g_catalog.equippedWalkId.clear();
                    syncEquippedWalkToAnimator();
                    r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
                    g_catalog.status = "Walk: Classic (procedurale)";
                }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Jump")) {
                animLibrary().setDefaultJump(c.id);
                g_animator.setJumpAnimId(c.id);
                g_catalog.status = "Jump = " + std::string(showId);
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Esempio script:");
    ImGui::TextWrapped(
        "game.Animator:Play(\"Shoot\")\n"
        "game.Animator:SetWalkAnim(\"FancyWalk\")\n"
        "game.Animator:SetJumpAnim(\"ClassicJump\")");
}

static void drawStudioPlayerAnimPanel() {
    if (!g_showPlayerAnimPublisher) return;
    ImGui::SetNextWindowPos({(float)g_w * 0.5f - 220.f, 90.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({440, 520}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Animazioni Player — Pubblica ID##playerAnimPub",
                      &g_showPlayerAnimPublisher)) {
        ImGui::End();
        return;
    }

    ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, "Flusso Studio");
    if (!g_catalog.status.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, {0.55f, 0.9f, 1.f, 1.f});
        ImGui::TextWrapped("%s", g_catalog.status.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Separator();
    drawWalkAnimWorkflow();
    ImGui::End();
}

// ---- Stato di sola presentazione dell'Avatar Editor -------------------------
// (categoria mostrata nel browser, buffer di rinomina). Niente qui influenza
// il modello dati: g_catalog resta l'unica fonte di verita'.
enum {
    AVCAT_ALL = 0, AVCAT_HEAD, AVCAT_TORSO, AVCAT_ARMS, AVCAT_LEGS,
    AVCAT_ANIMS, AVCAT_PACKS
};

static struct AvatarPublisherUi {
    int  cat = AVCAT_ALL;
    int  renameSel = -1;
    char renameBuf[128] = {};
} g_avpub;

static studio::Icon avatarSlotIcon(AvatarSlot s) {
    switch (s) {
    case AvatarSlot::Head:     return studio::Icon::Head;
    case AvatarSlot::Torso:    return studio::Icon::Torso;
    case AvatarSlot::LeftArm:  return studio::Icon::ArmLeft;
    case AvatarSlot::RightArm: return studio::Icon::ArmRight;
    case AvatarSlot::LeftLeg:  return studio::Icon::LegLeft;
    case AvatarSlot::RightLeg: return studio::Icon::LegRight;
    default:                   return studio::Icon::Cube;
    }
}

static bool avatarCatMatches(int cat, AvatarSlot s) {
    switch (cat) {
    case AVCAT_ALL:   return true;
    case AVCAT_HEAD:  return s == AvatarSlot::Head;
    case AVCAT_TORSO: return s == AvatarSlot::Torso;
    case AVCAT_ARMS:  return s == AvatarSlot::LeftArm || s == AvatarSlot::RightArm;
    case AVCAT_LEGS:  return s == AvatarSlot::LeftLeg || s == AvatarSlot::RightLeg;
    default:          return false;
    }
}

// Pillola di categoria: stesso linguaggio di hubTabButton, ma con larghezza
// libera e palette scura (la schermata usa applyStudioTheme, non il tema
// chiaro dell'hub).
static bool avatarPubTab(const char* label, bool active, float w) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.20f, 0.52f, 0.95f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.28f, 0.60f, 1.00f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text,          {1.f, 1.f, 1.f, 1.f});
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.13f, 0.18f, 0.28f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.18f, 0.26f, 0.40f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_Text,          {0.72f, 0.80f, 0.92f, 1.f});
    }
    const bool p = ImGui::Button(label, {w, 30});
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    return p;
}

static void drawAvatarBodyVisibility() {
    bool changed = false;
    const char* hideLabs[] = {
        "Nascondi Head", "Nascondi Torso",
        "Nascondi Braccio SX", "Nascondi Braccio DX",
        "Nascondi Gamba SX", "Nascondi Gamba DX"
    };
    for (int i = 0; i < (int)AvatarSlot::Count; ++i) {
        if (ImGui::Checkbox(hideLabs[i], &g_catalog.hideDefaultPart[i]))
            changed = true;
    }
    const float halfW = (ImGui::GetContentRegionAvail().x - 8.f) * 0.5f;
    if (ImGui::Button("Nascondi tutte", {halfW, 26})) {
        for (int i = 0; i < (int)AvatarSlot::Count; ++i)
            g_catalog.hideDefaultPart[i] = true;
        changed = true;
    }
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Mostra tutte##showbody", {halfW, 26})) {
        for (int i = 0; i < (int)AvatarSlot::Count; ++i)
            g_catalog.hideDefaultPart[i] = false;
        changed = true;
    }
    if (changed) {
        saveEquippedAvatar(g_catalog);
        g_catalog.status = "Parti corpo aggiornate";
    }
}

// Riquadri-slot con icona (3 per riga) al posto dei sei bottoni a tutta
// larghezza: il comportamento del click e' identico a prima.
static void drawAvatarSlotPicker() {
    if (!g_catalog.pendingImportPath.empty()) {
        ImGui::TextColored({0.4f, 1.f, 0.55f, 1.f},
            g_catalog.pendingAsBodyReplace ? "Parte custom — quale limb?" : "Modello — dove lo metti?");
        auto slash = g_catalog.pendingImportPath.find_last_of("\\/");
        std::string name = (slash == std::string::npos)
            ? g_catalog.pendingImportPath
            : g_catalog.pendingImportPath.substr(slash + 1);
        ImGui::TextWrapped("%s", name.c_str());
    } else {
        ImGui::TextDisabled(g_catalog.pendingAsBodyReplace
            ? "Scegli il limb da sostituire" : "Scegli dove va il prossimo modello");
    }
    ImGui::Spacing();

    const float availW = ImGui::GetContentRegionAvail().x;
    const float tileW = (availW - 2 * 6.f) / 3.f;
    int col = 0;
    auto slotTile = [&](AvatarSlot s, const char* label, const char* hint) {
        if (col > 0) ImGui::SameLine(0, 6.f);
        ImGui::PushID((int)s);
        ImGui::PushStyleColor(ImGuiCol_Button,        {0.16f, 0.34f, 0.66f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.26f, 0.48f, 0.86f, 1.f});
        const bool pressed = ImGui::Button("##slotTile", {tileW, 62});
        ImGui::PopStyleColor(2);
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        studio::drawIcon(dl, {(mn.x + mx.x) * 0.5f, mn.y + 22.f}, 26.f,
                         avatarSlotIcon(s), IM_COL32(255, 255, 255, 240));
        const ImVec2 ts = ImGui::CalcTextSize(label);
        dl->AddText({(mn.x + mx.x) * 0.5f - ts.x * 0.5f, mx.y - 17.f},
                    IM_COL32(232, 240, 255, 255), label);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hint);
        ImGui::PopID();
        col = (col + 1) % 3;

        if (pressed) {
            g_catalog.pendingSlot = s;
            g_catalog.wizardStep = 1;
            if (!g_catalog.pendingImportPath.empty()) {
                std::string path = g_catalog.pendingImportPath;
                g_catalog.pendingImportPath.clear();
                if (!importAccessoryFromPath(path)) {
                    g_catalog.status = std::string("Slot: ") + avatarSlotName(s)
                        + " — import fallito, riprova";
                }
            } else {
                g_catalog.status = std::string("Slot: ") + avatarSlotName(s) + " — importa un modello";
            }
        }
    };
    slotTile(AvatarSlot::Head,     "Testa",   "Parte custom testa / cappelli");
    slotTile(AvatarSlot::Torso,    "Torso",   "Torso custom / corazza");
    slotTile(AvatarSlot::LeftArm,  "Bracc.SX", "Braccio custom sinistro");
    slotTile(AvatarSlot::RightArm, "Bracc.DX", "Braccio custom destro");
    slotTile(AvatarSlot::LeftLeg,  "Gamba SX", "Gamba custom sinistra");
    slotTile(AvatarSlot::RightLeg, "Gamba DX", "Gamba custom destra");
}

// Griglia di card degli accessori, filtrata dalla categoria scelta.
static void drawAvatarAssetGrid() {
    const float avail = ImGui::GetContentRegionAvail().x;
    const int cols = std::max(1, (int)((avail + 14.f) / (122.f + 14.f)));
    const float cardW = (avail - 14.f * (cols - 1)) / (float)cols;
    const float cardH = 152.f;

    int shown = 0;
    int removeIdx = -1;
    for (int i = 0; i < (int)g_catalog.items.size(); ++i) {
        auto& a = g_catalog.items[i];
        if (!avatarCatMatches(g_avpub.cat, a.slot)) continue;
        if (shown > 0 && (shown % cols) != 0) ImGui::SameLine(0, 14.f);
        shown++;

        const bool sel = (i == g_catalog.selected);
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
            sel ? ImVec4{0.16f, 0.30f, 0.50f, 1.f} : ImVec4{0.11f, 0.16f, 0.26f, 1.f});
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 14.f);
        ImGui::BeginChild("acard", {cardW, cardH}, true);

        ImDrawList* cdl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float thumbW = cardW - 18.f;
        const bool hovered = ImGui::IsWindowHovered();

        // Niente thumbnail reali nel motore: la tinta dell'oggetto fa da
        // "copertina", cosi' ogni card e' comunque distinguibile a colpo d'occhio.
        const ImU32 thumbCol = IM_COL32((int)(a.tint.x * 140), (int)(a.tint.y * 140),
                                        (int)(a.tint.z * 140), 255);
        cdl->AddRectFilled({p.x, p.y}, {p.x + thumbW, p.y + 70.f}, thumbCol, 10.f);
        cdl->AddRectFilledMultiColor({p.x, p.y}, {p.x + thumbW, p.y + 70.f},
            IM_COL32(255, 255, 255, 45), IM_COL32(255, 255, 255, 10),
            IM_COL32(0, 0, 0, 45), IM_COL32(0, 0, 0, 45));
        studio::drawIcon(cdl, {p.x + thumbW * 0.5f, p.y + 35.f}, 34.f,
                         avatarSlotIcon(a.slot), IM_COL32(255, 255, 255, 240));
        if (hovered || sel) {
            cdl->AddRect({p.x - 7, p.y - 7}, {p.x + cardW - 5, p.y + cardH - 5},
                         sel ? IM_COL32(60, 150, 255, 190) : IM_COL32(100, 170, 255, 95),
                         14.f, 0, sel ? 2.2f : 1.4f);
        }
        if (a.replacesBody) {
            cdl->AddRectFilled({p.x + 6, p.y + 6}, {p.x + 62, p.y + 24},
                               IM_COL32(210, 150, 30, 235), 7.f);
            cdl->AddText({p.x + 12, p.y + 8}, IM_COL32(255, 255, 255, 255), "PARTE");
        }
        const bool forSale = accessoryIsForSale(a);
        cdl->AddRectFilled({p.x + 6, p.y + 46}, {p.x + (forSale ? 86.f : 74.f), p.y + 64},
                           forSale ? IM_COL32(12, 160, 100, 235) : IM_COL32(70, 80, 96, 225), 7.f);
        cdl->AddText({p.x + 12, p.y + 48}, IM_COL32(255, 255, 255, 255),
                     forSale ? "IN VENDITA" : "PRIVATO");

        ImGui::Dummy({0, 76});
        ImGui::TextWrapped("%s", a.name.c_str());
        ImGui::TextDisabled("%s", avatarSlotName(a.slot));
        if (hovered && ImGui::IsMouseClicked(0)) g_catalog.selected = i;

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        if (ImGui::BeginPopupContextItem("cardmenu")) {
            if (ImGui::MenuItem("Togli dall'avatar")) removeIdx = i;
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    if (shown == 0) {
        ImGui::Spacing();
        ImGui::TextDisabled(g_catalog.items.empty()
            ? "Nessun pezzo ancora. Usa \"Importa modello\" qui sopra."
            : "Niente in questa categoria.");
    }
    if (removeIdx >= 0) {
        const AvatarSlot removedSlot = g_catalog.items[(size_t)removeIdx].slot;
        const bool wasReplace = g_catalog.items[(size_t)removeIdx].replacesBody;
        unequipEquippedIndex(removeIdx);
        if (wasReplace) {
            bool still = false;
            for (const auto& x : g_catalog.items)
                if (x.slot == removedSlot && x.replacesBody) { still = true; break; }
            if (!still) g_catalog.hideDefaultPart[(int)removedSlot] = false;
        }
        g_avpub.renameSel = -1;
        g_catalog.status = "Accessorio rimosso";
        saveEquippedAvatar(g_catalog);
    }
}

// Contenuto dell'ispettore: proprieta' dell'accessorio selezionato oppure,
// nelle categorie Animazioni/Pacchetti, il rispettivo flusso.
static void drawAvatarInspector() {
    if (g_avpub.cat == AVCAT_ANIMS) {
        drawWalkAnimWorkflow();
        return;
    }
    if (g_avpub.cat == AVCAT_PACKS) {
        drawAnimPackPublisher(g_animPackUi);
        return;
    }
    if (g_catalog.selected < 0 || g_catalog.selected >= (int)g_catalog.items.size()) {
        ImGui::TextDisabled("Seleziona un pezzo dalla griglia per modificarlo.");
        return;
    }

    auto& a = g_catalog.items[g_catalog.selected];
    if (g_avpub.renameSel != g_catalog.selected) {
        g_avpub.renameSel = g_catalog.selected;
        std::snprintf(g_avpub.renameBuf, sizeof(g_avpub.renameBuf), "%s", a.name.c_str());
    }

    ImGui::TextColored({0.35f, 0.80f, 1.f, 1.f}, "%s", a.name.c_str());
    ImGui::TextDisabled("Attaccato a %s — trascina col mouse nella vista 3D",
                        avatarSlotName(a.slot));

    auto applyRename = [&]() {
        if (!g_avpub.renameBuf[0]) return;
        auto mod = Moderation::checkAvatarOrModelName(g_avpub.renameBuf);
        if (mod.verdict == Moderation::Verdict::Ok) {
            a.name = g_avpub.renameBuf;
            saveEquippedAvatar(g_catalog);
            g_catalog.status = "Rinominato: " + a.name;
        } else {
            g_catalog.status = mod.reason;
            std::snprintf(g_avpub.renameBuf, sizeof(g_avpub.renameBuf), "%s", a.name.c_str());
        }
    };
    studioSectionLabel("Nome");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##accRename", g_avpub.renameBuf, sizeof(g_avpub.renameBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue))
        applyRename();
    else if (ImGui::IsItemDeactivatedAfterEdit())
        applyRename();

    studioSectionLabel("Aspetto");
    if (ImGui::Checkbox("Sostituisce la parte del corpo", &a.replacesBody)) {
        if (a.replacesBody) {
            a.offset = bodyReplaceOffsetForSlot(a.slot);
            g_catalog.hideDefaultPart[(int)a.slot] = true;
            for (int j = 0; j < (int)g_catalog.items.size(); ++j) {
                if (j == g_catalog.selected) continue;
                if (g_catalog.items[j].slot == a.slot && g_catalog.items[j].replacesBody)
                    g_catalog.items[j].replacesBody = false;
            }
        }
        saveEquippedAvatar(g_catalog);
        g_catalog.status = a.replacesBody
            ? (std::string("Ora sostituisce ") + avatarSlotName(a.slot))
            : "Ora e' un accessorio sopra";
    }
    ImGui::ColorEdit3("Tinta", &a.tint.x);

    studioSectionLabel("Trasformazione");
    auto toolBtn = [&](const char* id, studio::Icon ic, AccelTool tl, const char* tip) {
        const bool on = (g_catalog.tool == tl);
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button,
            on ? ImVec4{0.20f, 0.52f, 0.95f, 1.f} : ImVec4{0.13f, 0.18f, 0.28f, 1.f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            on ? ImVec4{0.28f, 0.60f, 1.00f, 1.f} : ImVec4{0.18f, 0.26f, 0.40f, 1.f});
        if (ImGui::Button("##tool", {52, 34})) g_catalog.tool = tl;
        ImGui::PopStyleColor(2);
        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        studio::drawIcon(ImGui::GetWindowDrawList(),
                         {(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f}, 20.f, ic,
                         IM_COL32(255, 255, 255, 235));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        ImGui::PopID();
    };
    toolBtn("tmove", studio::Icon::Move, AccelTool::Move, "Muovi (1)");
    ImGui::SameLine(0, 6);
    toolBtn("trot", studio::Icon::Rotate, AccelTool::Rotate, "Ruota (2)");
    ImGui::SameLine(0, 6);
    toolBtn("tscale", studio::Icon::Scale, AccelTool::Scale, "Scala (3)");

    ImGui::DragFloat3("Offset", &a.offset.x, 0.02f);
    ImGui::DragFloat3("Rotazione", &a.rotation.x, 1.f);
    ImGui::DragFloat3("Scala", &a.scale.x, 0.02f, 0.05f, 8.f);
    const float halfW = (ImGui::GetContentRegionAvail().x - 8.f) * 0.5f;
    if (ImGui::Button("Ingrandisci", {halfW, 26})) a.scale *= 1.15f;
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Rimpicciolisci", {halfW, 26})) a.scale *= 0.87f;

    studioSectionLabel("Pubblicazione");
    if (accessoryIsForSale(a)) {
        const int si = findShopIndexForAccessory(a);
        ImGui::TextColored({0.35f, 0.95f, 0.55f, 1.f}, "IN VENDITA — %d B$",
                           si >= 0 ? g_menu.shopItems[(size_t)si].price : 0);
        ImGui::TextDisabled("Altri giocatori possono comprarlo dallo Shop.");
        if (ImGui::Button("Ritira dal negozio", {-1, 30}))
            unpublishAccessory(g_catalog.selected);
    } else {
        ImGui::TextDisabled("Solo tuo: e' sull'avatar, ma non nel negozio.");
        char lbl[96];
        std::snprintf(lbl, sizeof(lbl), "Metti in vendita  ·  %d B$", avatarItemPrice(a));
        if (studioAccentButton(lbl, {-1, 38}, {0.12f, 0.62f, 0.42f, 1.f}))
            publishSingleAccessory(g_catalog.selected);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.62f, 0.20f, 0.18f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.78f, 0.26f, 0.22f, 1.f});
    if (ImGui::Button("Togli dall'avatar", {-1, 30})) {
        const AvatarSlot removedSlot = a.slot;
        const bool wasReplace = a.replacesBody;
        unequipEquippedIndex(g_catalog.selected);
        if (wasReplace) {
            bool still = false;
            for (const auto& x : g_catalog.items)
                if (x.slot == removedSlot && x.replacesBody) { still = true; break; }
            if (!still) g_catalog.hideDefaultPart[(int)removedSlot] = false;
        }
        g_avpub.renameSel = -1;
        g_catalog.status = "Pezzo rimosso";
        saveEquippedAvatar(g_catalog);
    }
    ImGui::PopStyleColor(2);
}

static void drawAvatarPublisherUi() {
    // Il flag che apre il pannello animazioni di Studio qui diventa
    // "vai alla categoria Animazioni", cosi' quella finestra non resta
    // aperta alle spalle dell'editor.
    if (g_showPlayerAnimPublisher) {
        g_avpub.cat = AVCAT_ANIMS;
        g_showPlayerAnimPublisher = false;
    }

    const float TOP_H = 56.f, PAD = 12.f, LEFT_W = 304.f, RIGHT_W = 344.f;
    const float bodyY = TOP_H + PAD;
    const float bodyH = std::max(120.f, (float)g_h - bodyY - PAD);
    const bool wide = ((float)g_w >= 1000.f);

    // ATTENZIONE: niente finestra a tutto schermo. I gizmo e lo zoom sono
    // condizionati a !io.WantCaptureMouse, quindi ogni pannello copre solo il
    // proprio rettangolo e il centro resta libero per la vista 3D.
    const ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;

    ImGui::GetBackgroundDrawList()->AddRectFilledMultiColor(
        {0, 0}, {(float)g_w, TOP_H + 26.f},
        IM_COL32(6, 10, 18, 225), IM_COL32(6, 10, 18, 225),
        IM_COL32(6, 10, 18, 0), IM_COL32(6, 10, 18, 0));

    // ---- Barra superiore ----
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({(float)g_w, TOP_H});
    ImGui::Begin("##avpubTop", nullptr, kFlags | ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored({0.25f, 0.55f, 0.95f, 1.f}, "BLOCKY");
    ImGui::SameLine();
    ImGui::TextColored({0.95f, 0.75f, 0.25f, 1.f}, "Avatar Editor");
    if (!g_catalog.status.empty()) {
        ImGui::SameLine(0, 24);
        ImGui::PushStyleColor(ImGuiCol_Text, {0.55f, 0.85f, 1.f, 1.f});
        ImGui::TextUnformatted(g_catalog.status.c_str());
        ImGui::PopStyleColor();
    }
    {
        const float blockW = 430.f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                      ImGui::GetWindowWidth() - blockW - 14.f));
        ImGui::TextColored({1.f, 0.85f, 0.25f, 1.f}, "%d B$", g_menu.shopCoins);
        ImGui::SameLine(0, 12);
        if (ImGui::Button("Pubblica tutto", {126, 30})) {
            const int n = publishAccessoriesToShop();
            if (n > 0) {
                g_catalog.status = "Pubblicati " + std::to_string(n) + " pezzi nello Shop";
            } else if (g_catalog.status.find("moderazione") == std::string::npos) {
                g_catalog.status = g_catalog.items.empty()
                    ? "Niente da pubblicare — importa prima un modello"
                    : "Nessun nuovo pezzo da mettere in vendita";
            }
        }
        ImGui::SameLine(0, 8);
        if (studioAccentButton("Salva", {92, 30}, {0.15f, 0.55f, 0.95f, 1.f})) {
            g_catalog.open = false;
            syncCatalogItemsToInventory();
            syncEquippedToMenu();
            g_catalog.status = "Salvato sul personaggio ("
                + std::to_string(g_catalog.items.size()) + " item)";
            saveEquippedAvatar(g_catalog);
            returnToStudioEditor(glfwGetCurrentContext());
        }
        ImGui::SameLine(0, 8);
        if (ImGui::Button("Chiudi", {84, 30})) {
            g_catalog.open = false;
            if (kIsGameEngine) returnToStudioEditor(glfwGetCurrentContext());
            else               returnToMenu(glfwGetCurrentContext());
        }
    }
    ImGui::End();

    // ---- Browser a sinistra ----
    ImGui::SetNextWindowPos({PAD, bodyY});
    ImGui::SetNextWindowSize({LEFT_W, bodyH});
    ImGui::Begin("##avpubLeft", nullptr, kFlags);
    studioPanelHeader(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), LEFT_W,
                      "I miei asset", (float)glfwGetTime());

    if (studioAccentButton(g_catalog.pendingAsBodyReplace
                               ? "+  Aggiungi parte custom" : "+  Importa modello",
                           {-1, 46}, {0.12f, 0.62f, 0.42f, 1.f})) {
        plat::requestFile("Blocky — importa modello avatar", "*/*",
                          [](const std::string& path) {
            if (path.empty()) {
                g_catalog.status = "Import annullato";
                return;
            }
            g_catalog.pendingImportPath = path;
            g_catalog.wizardStep = 0;
            if (path.size() < sizeof(g_importPathBuf))
                std::snprintf(g_importPathBuf, sizeof(g_importPathBuf), "%s", path.c_str());
            auto slash = path.find_last_of("\\/");
            std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
            g_catalog.status = g_catalog.pendingAsBodyReplace
                ? ("Parte custom: " + name + " — scegli quale limb sostituire")
                : ("File: " + name + " — scegli dove metterlo");
        });
    }
    ImGui::Checkbox("Sostituisce la parte del corpo", &g_catalog.pendingAsBodyReplace);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("ON: nasconde il cubo R6 e mette la mesh al posto del limb.\nOFF: accessorio sopra (cappello, arma...).");

    ImGui::Spacing();
    drawAvatarSlotPicker();

    if (!g_catalog.pendingImportPath.empty() || g_catalog.wizardStep != 0) {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##importPath", g_importPathBuf, sizeof(g_importPathBuf));
        if (ImGui::Button("Carica da percorso", {-1, 26}))
            importAccessoryFromPath(g_importPathBuf);
    }

    ImGui::Spacing();
    ImGui::Separator();
    {
        struct { const char* label; int cat; } tabs[] = {
            {"Tutti", AVCAT_ALL}, {"Testa", AVCAT_HEAD}, {"Torso", AVCAT_TORSO},
            {"Braccia", AVCAT_ARMS}, {"Gambe", AVCAT_LEGS},
            {"Animazioni", AVCAT_ANIMS}, {"Pacchetti", AVCAT_PACKS},
        };
        const float pillW = (ImGui::GetContentRegionAvail().x - 2 * 6.f) / 3.f;
        for (int i = 0; i < (int)(sizeof(tabs) / sizeof(tabs[0])); ++i) {
            if (i % 3 != 0) ImGui::SameLine(0, 6.f);
            if (avatarPubTab(tabs[i].label, g_avpub.cat == tabs[i].cat, pillW))
                g_avpub.cat = tabs[i].cat;
        }
    }
    ImGui::Spacing();

    if (g_avpub.cat == AVCAT_ANIMS || g_avpub.cat == AVCAT_PACKS) {
        ImGui::TextDisabled(g_avpub.cat == AVCAT_ANIMS
            ? "Le animazioni si gestiscono nel pannello a destra."
            : "I pacchetti si gestiscono nel pannello a destra.");
    } else {
        ImGui::BeginChild("##assetgrid", {0, wide ? 0.f : bodyH * 0.30f}, false);
        drawAvatarAssetGrid();
        ImGui::EndChild();
    }

    if (!wide) {
        ImGui::Separator();
        drawAvatarInspector();
        ImGui::Separator();
        studioSectionLabel("Parti del corpo R6");
        drawAvatarBodyVisibility();
    }
    ImGui::End();

    // ---- Ispettore a destra (solo se c'e' spazio) ----
    if (wide) {
        ImGui::SetNextWindowPos({(float)g_w - RIGHT_W - PAD, bodyY});
        ImGui::SetNextWindowSize({RIGHT_W, bodyH});
        ImGui::Begin("##avpubRight", nullptr, kFlags);
        const char* title = (g_avpub.cat == AVCAT_ANIMS)  ? "Animazioni"
                          : (g_avpub.cat == AVCAT_PACKS)  ? "Pacchetti animazioni"
                                                          : "Proprieta'";
        studioPanelHeader(ImGui::GetWindowDrawList(), ImGui::GetWindowPos(), RIGHT_W,
                          title, (float)glfwGetTime());
        drawAvatarInspector();
        if (g_avpub.cat != AVCAT_ANIMS && g_avpub.cat != AVCAT_PACKS) {
            ImGui::Spacing();
            studioSectionLabel("Parti del corpo R6");
            drawAvatarBodyVisibility();
        }
        ImGui::End();
    }
}
