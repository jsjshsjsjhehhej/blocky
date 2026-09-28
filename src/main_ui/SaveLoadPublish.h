// Salvataggio, autosave, chiusura pulita, apertura place e pubblicazione gioco.
// Non e' un header riusabile: usa direttamente le globali di main.cpp
// (g_world, g_menu, g_placeFilePath, ecc.). Compila solo se incluso da
// main.cpp esattamente in questo punto, mai da altri translation unit.

static void doExport() {
    syncFriendsToWorld();
    std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos, g_playerYaw, &g_lighting);
    std::string path = PlaceExporter::makeExportPath(g_world.placeName);
    if (PlaceExporter::saveToFile(path, json))
        g_exportMsg = "Esportato: " + path;
    else
        g_exportMsg = "Export fallito!";
}

static void refreshGamesList() {
    g_games = loadGameManifest();
    auto packaged = BlockyProject::listInstalledGames();
    std::vector<GameEntry> extras;
    for (const auto& g : packaged)
        extras.push_back(BlockyProject::toGameEntry(g));
    mergeGamePackages(g_games, extras);
}

static bool writeCurrentPlaceTo(const std::string& path) {
    if (path.empty()) return false;
    syncFriendsToWorld();
    std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos, g_playerYaw, &g_lighting);
    return PlaceExporter::saveToFile(path, json);
}

static std::string placeAutosavePath() {
    return blockyDataDir() + "/autosave_place.json";
}

static bool g_persistBusy = false;
static bool g_didPersistExit = false;
static float g_autosaveAcc = 0.f;

static void persistAutosavePlaceOnly() {
    if (g_persistBusy || !kIsGameEngine) return;
    if (!(g_screen == AppScreen::Studio || g_screen == AppScreen::AvatarPublisher
          || !g_world.parts.empty()))
        return;
    g_persistBusy = true;
    writeCurrentPlaceTo(placeAutosavePath());
    g_persistBusy = false;
}

static void persistFastExit() {
    if (g_didPersistExit || g_persistBusy) return;
    g_didPersistExit = true;
    g_persistBusy = true;
    if (g_menu.username[0])
        saveUserProfile(g_menu);
    saveShopItems(g_menu);
    saveEquippedAvatar(g_catalog);
    if (kIsGameEngine) {
        if (!g_placeFilePath.empty())
            writeCurrentPlaceTo(g_placeFilePath);
        else if (g_screen == AppScreen::Studio || !g_world.parts.empty())
            writeCurrentPlaceTo(placeAutosavePath());
    }
    g_persistBusy = false;
    std::fprintf(stderr, "[Blocky] Salvato (fast-exit)\n");
}

static void persistAllUserData(const char* reason) {
    if (g_persistBusy) return;
    bool isExit = reason && (std::strcmp(reason, "shutdown") == 0
                          || std::strcmp(reason, "exit") == 0
                          || std::strcmp(reason, "window-close") == 0
                          || std::strcmp(reason, "console-ctrl") == 0
                          || std::strcmp(reason, "signal") == 0);
    if (isExit) {
        persistFastExit();
        return;
    }
    g_persistBusy = true;

    for (const auto& a : g_catalog.items)
        upsertOwnedAvatarInventory(a);
    saveEquippedAvatar(g_catalog);
    saveShopItems(g_menu);
    if (g_menu.username[0])
        saveUserProfile(g_menu);

    r6SaveLibrary(g_catalog.walkClips, g_catalog.equippedWalkId);
    animLibrary().persistIndex();

    if (kIsGameEngine) {
        if (!g_placeFilePath.empty())
            writeCurrentPlaceTo(g_placeFilePath);
        if (g_screen == AppScreen::Studio || g_screen == AppScreen::AvatarPublisher
            || !g_world.parts.empty()) {
            writeCurrentPlaceTo(placeAutosavePath());
        }
    }
    g_persistBusy = false;
    if (reason && reason[0])
        std::fprintf(stderr, "[Blocky] Salvato (%s)\n", reason);
}

static void persistAtExit() {
}

static void onWindowCloseRequest(GLFWwindow* win) {
    if (kIsGameEngine && g_placeDirty) {
        glfwSetWindowShouldClose(win, 0);
        g_pendingNav = PendingNav::QuitApp;
        g_showSavePrompt = true;
    }
}

static void onFatalSignal(int) {
    persistAllUserData("signal");
    std::_Exit(128);
}

#ifdef _WIN32
static BOOL WINAPI onConsoleCtrl(DWORD type) {
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        persistAllUserData("console-ctrl");
        if (g_mainWin) glfwSetWindowShouldClose(g_mainWin, 1);
        return TRUE;
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        persistAllUserData("console-ctrl");
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

static std::string suggestPlaceFileName() {
    std::string suggest = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
    if (suggest.empty()) suggest = "place";
    for (char& c : suggest) if (c == ' ') c = '_';
    auto dot = suggest.find_last_of('.');
    if (dot != std::string::npos) {
        std::string ext = suggest.substr(dot);
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".json" || ext == ".blocky")
            suggest = suggest.substr(0, dot);
    }
    return suggest + ".blocky";
}

static void doSavePlace() {
    if (g_placeFilePath.empty()) {
        std::string path;
        if (!savePlaceFileDialog(path, publisherHwnd(), suggestPlaceFileName().c_str())) {
            g_exportMsg = "Salvataggio annullato";
            return;
        }
        ensureBlockyPlaceExt(path);
        g_placeFilePath = path;
    }
    if (writeCurrentPlaceTo(g_placeFilePath)) {
        g_exportMsg = "Salvato: " + g_placeFilePath;
        clearPlaceDirty();
    } else
        g_exportMsg = "Salvataggio fallito";
}

static void doSavePlaceAs() {
    std::string path;
    if (!savePlaceFileDialog(path, publisherHwnd(), suggestPlaceFileName().c_str())) {
        g_exportMsg = "Salvataggio annullato";
        return;
    }
    ensureBlockyPlaceExt(path);
    g_placeFilePath = path;
    if (writeCurrentPlaceTo(g_placeFilePath)) {
        g_exportMsg = "Salvato come: " + g_placeFilePath;
        clearPlaceDirty();
    } else
        g_exportMsg = "Salvataggio fallito";
}

static std::string publishDraftDirForUser() {
    std::string creator = g_menu.username[0] ? g_menu.username : "Builder";
    std::string dir = blockyDataDir() + "/publish_draft/" + normalizeCreatorKey(creator);
#ifdef _WIN32
    _mkdir((blockyDataDir() + "/publish_draft").c_str());
    _mkdir(dir.c_str());
#else
    _mkdir((blockyDataDir() + "/publish_draft").c_str());
    _mkdir(dir.c_str());
#endif
    return dir;
}

static std::string currentPublishDraftPath() {
    std::string title = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
    if (title.empty()) title = "Il_mio_gioco";
    std::string id = sanitizeGameId(title);
    return publishDraftDirForUser() + "/" + id + ".json";
}

static void doSaveForPublish() {
    std::string path = currentPublishDraftPath();
    if (!writeCurrentPlaceTo(path)) {
        g_exportMsg = "Salva per pubblicare fallito";
        g_publishMsg = g_exportMsg;
        return;
    }
    g_publishDraftPath = path;
    g_publishDraftActive = true;
    g_publishDraftAcc = 0.f;
    {
        std::string meta = publishDraftDirForUser() + "/draft_meta.txt";
        FILE* f = std::fopen(meta.c_str(), "wb");
        if (f) {
            std::fprintf(f, "%s\n%d\n%s\n",
                g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName.c_str(),
                g_maxPlayers, path.c_str());
            std::fclose(f);
        }
    }
    int used = 0;
    long long now = (long long)std::time(nullptr);
    std::string creator = g_menu.username[0] ? g_menu.username : "Builder";
    std::string id = sanitizeGameId(g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName);
    canPublishGame(g_games, id, creator, now, &used);
    clearPlaceDirty();
    g_exportMsg = "Salvato per pubblicare (bozza) — autosave attivo";
    g_publishMsg = "Bozza OK · slot " + std::to_string(used) + "/"
        + std::to_string(kMaxPublishedGamesPerUser)
        + " in 2 mesi · Pubblica quando vuoi";
}

static void persistPublishDraftOnly() {
    if (!g_publishDraftActive || g_persistBusy) return;
    if (!(g_screen == AppScreen::Studio || g_screen == AppScreen::AvatarPublisher))
        return;
    if (g_publishDraftPath.empty())
        g_publishDraftPath = currentPublishDraftPath();
    g_persistBusy = true;
    writeCurrentPlaceTo(g_publishDraftPath);
    g_persistBusy = false;
}

static bool autosaveFileExists() {
    std::string p = placeAutosavePath();
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

static void loadAutosaveIntoEditor() {
    std::string p = placeAutosavePath();
    if (!loadPlaceFromJson(p, g_world, &g_lighting)) {
        g_exportMsg = "Autosave non leggibile";
        return;
    }
    g_placeFilePath.clear();
    g_wsMeshes.clear();
    syncFriendsFromWorld();
    selectPart(g_world.parts.empty() ? -1 : 0);
    markPlaceDirty();
    g_exportMsg = "Ripristinato place non salvato (autosave)";
}

static void executePendingNav(GLFWwindow* win) {
    PendingNav a = g_pendingNav;
    g_pendingNav = PendingNav::None;
    g_showSavePrompt = false;
    switch (a) {
    case PendingNav::GoHome:
        goGameEngineHome(win);
        break;
    case PendingNav::QuitApp:
        glfwSetWindowShouldClose(win, true);
        break;
    case PendingNav::NewPlace:
        applyMapTemplate(0);
        clearPlaceDirty();
        g_placeFilePath.clear();
        returnToStudioEditor(win);
        g_exportMsg = "Nuovo place Baseplate";
        break;
    case PendingNav::OpenPlace:
        doOpenPlace();
        returnToStudioEditor(win);
        break;
    default:
        break;
    }
}

static void requestNav(GLFWwindow* win, PendingNav a) {
    if (!kIsGameEngine) {
        g_pendingNav = a;
        executePendingNav(win);
        return;
    }
    if (g_placeDirty) {
        g_pendingNav = a;
        g_showSavePrompt = true;
        return;
    }
    g_pendingNav = a;
    executePendingNav(win);
}

static void drawSavePlacePrompt(GLFWwindow* win) {
    if (!g_showSavePrompt) return;
    ImGui::OpenPopup("Salvare il place?##saveprompt");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Salvare il place?##saveprompt", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Il place ha modifiche non salvate.\nVuoi salvarlo prima di continuare?");
        ImGui::Spacing();
        if (ImGui::Button("Salva", {120, 36})) {
            doSavePlace();
            if (!g_placeDirty)
                executePendingNav(win);
            else
                g_showSavePrompt = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Non salvare", {120, 36}))
            executePendingNav(win);
        ImGui::SameLine();
        if (ImGui::Button("Annulla", {120, 36})) {
            g_showSavePrompt = false;
            g_pendingNav = PendingNav::None;
        }
        ImGui::EndPopup();
    }
}

static void drawAutosavePrompt(GLFWwindow* win) {
    if (!g_showAutosavePrompt) return;
    ImGui::OpenPopup("Place non salvato##autosprompt");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Place non salvato##autosprompt", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped(
            "C'e' un place non salvato dall'ultima sessione.\n"
            "Vuoi aprirlo nell'editor?");
        ImGui::Spacing();
        if (ImGui::Button("Si, apri", {140, 36})) {
            loadAutosaveIntoEditor();
            g_showAutosavePrompt = false;
            returnToStudioEditor(win);
        }
        ImGui::SameLine();
        if (ImGui::Button("No, Home", {140, 36}))
            g_showAutosavePrompt = false;
        ImGui::EndPopup();
    }
}

static void drawGameEngineHome(GLFWwindow* win, float ) {
    const float W = (float)g_w, H = (float)g_h;
    const float cardW = std::min(720.f, W * 0.72f);
    const float cardH = std::min(640.f, H * 0.78f);
    const float cardX = (W - cardW) * 0.5f;
    const float cardY = (H - cardH) * 0.5f;

    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({W, H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.05f, 0.08f, 0.14f, 1.f});
    ImGui::Begin("##GEHome", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* bg = ImGui::GetWindowDrawList();
    bg->AddRectFilledMultiColor({0, 0}, {W, H},
        IM_COL32(12, 22, 42, 255), IM_COL32(18, 36, 64, 255),
        IM_COL32(10, 18, 34, 255), IM_COL32(8, 14, 28, 255));

    ImGui::SetCursorPos({cardX, cardY});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.09f, 0.13f, 0.22f, 1.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 18.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {36.f, 32.f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {14.f, 14.f});
    ImGui::BeginChild("##gehomebox", {cardW, cardH}, true,
        ImGuiWindowFlags_NoScrollbar);

    ImGui::SetWindowFontScale(1.55f);
    ImGui::TextColored({0.40f, 0.88f, 1.f, 1.f}, "BLOCKY");
    ImGui::SetWindowFontScale(1.25f);
    ImGui::TextColored({1.f, 0.82f, 0.30f, 1.f}, "Game Engine");
    ImGui::SetWindowFontScale(1.f);
    ImGui::Spacing();
    ImGui::TextDisabled("Home — crea e modifica place");
    if (g_placeDirty)
        ImGui::TextColored({1.f, 0.55f, 0.25f, 1.f}, "Place non salvato");
    else if (!g_placeFilePath.empty())
        ImGui::TextWrapped("%s", g_placeFilePath.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f);
    ImGui::PushStyleColor(ImGuiCol_Button, {0.14f, 0.52f, 0.92f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.22f, 0.62f, 1.f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 1.f, 1.f, 1.f});
    if (ImGui::Button("Continua editor", {-1, 64}))
        returnToStudioEditor(win);
    ImGui::PopStyleColor(3);

    ImGui::PushStyleColor(ImGuiCol_Button, {0.16f, 0.28f, 0.46f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.22f, 0.38f, 0.62f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 1.f, 1.f, 1.f});
    if (ImGui::Button("Nuovo place", {-1, 56}))
        requestNav(win, PendingNav::NewPlace);
    if (ImGui::Button("Apri place...", {-1, 56}))
        requestNav(win, PendingNav::OpenPlace);
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, {0.42f, 0.18f, 0.18f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.58f, 0.24f, 0.24f, 1.f});
    ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 1.f, 1.f, 1.f});
    if (ImGui::Button("Esci", {-1, 52}))
        requestNav(win, PendingNav::QuitApp);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndChild();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleColor();

    drawAutosavePrompt(win);
    drawSavePlacePrompt(win);
}

static bool openPlacePath(const std::string& path) {
    if (!loadPlaceFromJson(path, g_world, &g_lighting)) {
        g_exportMsg = "Impossibile aprire: " + path;
        return false;
    }
    g_placeFilePath = path;
    g_wsMeshes.clear();
    syncFriendsFromWorld();
    g_selectedPart = -1;
    g_selectedModel = -1;
    g_selectedSound = -1;
    g_selectedLabel = -1;
    g_scaleDragging = g_moveDragging = g_rotateDragging = false;
    clearPlaceDirty();
    std::snprintf(g_gameNameBuf, sizeof(g_gameNameBuf), "%s", g_world.placeName.c_str());
    if (!g_world.parts.empty())
        selectPart(0);
    g_exportMsg = "Aperto: " + path + " (" + std::to_string(g_world.parts.size()) + " parti"
        + (g_friends.empty() ? "" : (", " + std::to_string(g_friends.size()) + " amici")) + ")";
    glfwSetWindowTitle(glfwGetCurrentContext(),
        (std::string(kAppBrand) + " - " + g_world.placeName).c_str());
    return true;
}

static void doOpenPlace() {
    std::string path;
    if (!openPlaceFileDialog(path, publisherHwnd())) {
        g_exportMsg = "Apertura annullata";
        return;
    }
    openPlacePath(path);
}

static void doSaveProject() {
    std::string title = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
    if (title.empty()) title = "Il mio gioco";
    g_world.placeName = title;
    std::string id = sanitizeGameId(title);
    std::string root = BlockyProject::joinPath(BlockyProject::projectsDir(), id);
    syncFriendsToWorld();
    std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos, g_playerYaw, &g_lighting);

    BlockyProject::ProjectManifest m;
    m.name = title;
    m.id = id;
    m.creator = g_menu.username[0] ? g_menu.username : "Builder";
    m.maxPlayers = std::clamp(g_maxPlayers, 1, 32);

    std::string placePath = BlockyProject::joinPath(root, "places\\main.place.json");
    std::string manifestPath = BlockyProject::joinPath(root, "project.blocky.json");
    if (!fileExistsPath(manifestPath)) {
        if (!BlockyProject::createNewProject(root, m, json)) {
            g_exportMsg = "Salvataggio progetto fallito";
            return;
        }
    } else {
        BlockyProject::mkDir(root);
        BlockyProject::mkDir(BlockyProject::joinPath(root, "places"));
        if (!BlockyProject::writeTextFile(placePath, json)) {
            g_exportMsg = "Salvataggio place progetto fallito";
            return;
        }
        BlockyProject::writeTextFile(manifestPath, BlockyProject::writeProjectJson(m));
    }
    g_placeFilePath = placePath;
    g_exportMsg = "Progetto salvato: " + root;
}

static bool publishCurrentGame() {
    std::string title = g_gameNameBuf[0] ? g_gameNameBuf : g_world.placeName;
    if (title.empty()) title = "Il mio gioco";

    auto mod = Moderation::checkGameTitle(title);
    if (mod.verdict != Moderation::Verdict::Ok) {
        g_publishMsg = mod.reason;
        g_exportMsg = g_publishMsg;
        g_menu.chatLines.push_back(mod.reason);
        return false;
    }

    g_world.placeName = title;

    std::string id = sanitizeGameId(title);
    std::string creator = g_menu.username[0] ? g_menu.username : "Builder";
    long long now = (long long)std::time(nullptr);
    int used = 0;
    if (!canPublishGame(g_games, id, creator, now, &used)) {
        g_publishMsg = "Limite raggiunto: max "
            + std::to_string(kMaxPublishedGamesPerUser)
            + " giochi ogni 2 mesi (" + std::to_string(used) + "/"
            + std::to_string(kMaxPublishedGamesPerUser)
            + "). Aggiorna un gioco esistente o aspetta.";
        g_exportMsg = g_publishMsg;
        g_menu.chatLines.push_back("[Games] " + g_publishMsg);
        return false;
    }

    if (g_publishDraftActive) {
        if (g_publishDraftPath.empty())
            g_publishDraftPath = currentPublishDraftPath();
        writeCurrentPlaceTo(g_publishDraftPath);
    }

    std::string placePath = blockyDataDir() + "/published/" + id + ".json";

    for (char& c : placePath)
        if (c == '\\') c = '/';

    std::string iconPath;
    if (!g_pendingIconPath.empty()) {
        auto dot = g_pendingIconPath.find_last_of('.');
        std::string ext = (dot != std::string::npos) ? g_pendingIconPath.substr(dot) : ".png";
        iconPath = blockyDataDir() + "/published/" + id + "_icon" + ext;
        for (char& c : iconPath)
            if (c == '\\') c = '/';
        if (BlockyProject::copyFileBinary(g_pendingIconPath, iconPath))
            g_pendingIconPath.clear();
        else
            iconPath.clear();
    } else {
        for (const auto& g : g_games)
            if (g.id == id) { iconPath = g.iconPath; break; }
    }

    syncFriendsToWorld();
    std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos, g_playerYaw, &g_lighting);
    if (!PlaceExporter::saveToFile(placePath, json)) {
        g_publishMsg = "Pubblicazione fallita (salvataggio file)";
        g_exportMsg = g_publishMsg;
        return false;
    }

    GameEntry e;
    e.id = id;
    e.title = title;
    e.subtitle = "di " + creator;
    e.script = "scripts/baseplate.lua";
    e.playersOnline = 1;
    e.maxPlayers = std::clamp(g_maxPlayers, 1, 32);
    e.accent = {0.15f, 0.48f, 0.92f};
    e.isPublished = true;
    e.placePath = placePath;
    e.iconPath = iconPath;
    e.creator = creator;
    e.publishedAt = now;

    bool updated = false;
    for (auto& g : g_games) {
        if (g.id == id) {
            if (g.publishedAt > 0) e.publishedAt = g.publishedAt;
            g = e;
            updated = true;
            break;
        }
    }
    if (!updated) g_games.push_back(e);
    savePublishedGamesList(g_games);

    BlockyProject::GamePackage pkg;
    pkg.id = id;
    pkg.title = title;
    pkg.creator = creator;
    pkg.description = "Pubblicato con Blocky Game Engine";
    pkg.maxPlayers = e.maxPlayers;
    std::string lua =
        "-- " + title + " (pubblicato da Blocky Game Engine)\n"
        "print(\"[Blocky] Gioco: " + title + "\")\n"
        "print(\"[Blocky] Safe Bytecode — logica sigillata, OpenGL solo rendering\")\n";
    std::string pkgDir = BlockyProject::publishGamePackage(pkg, json, lua);
    if (!pkgDir.empty()) {
        e.placePath = BlockyProject::joinPath(pkgDir, "place.json");
        std::string bbc = BlockyProject::joinPath(pkgDir, "scripts\\main.bbc");
        std::string luaPath = BlockyProject::joinPath(pkgDir, "scripts\\main.lua");
        e.script = fileExistsPath(bbc) ? bbc : luaPath;
        for (auto& g : g_games) {
            if (g.id == id) { g = e; break; }
        }
        savePublishedGamesList(g_games);
        refreshGamesList();
        // Se e' configurato un server dei giochi, il gioco appena pubblicato
        // gli viene spedito: da li' lo vedranno anche gli altri dispositivi,
        // invece di restare chiuso dentro questo computer.
        if (g_menu.gameServer[0])
            blk::remotegames::uploadGame(g_menu.gameServer, id, pkgDir);

        if (fileExistsPath(bbc))
            g_publishMsg += " · Safe Bytecode (.bbc) sigillato";
    }

    int usedAfter = countUserPublishedInWindow(g_games, creator, now);
    g_publishMsg = "Pubblicato \"" + title + "\" — max " + std::to_string(e.maxPlayers)
        + " giocatori · slot " + std::to_string(usedAfter) + "/"
        + std::to_string(kMaxPublishedGamesPerUser) + " (2 mesi)";
    g_exportMsg = g_publishMsg;
    g_menu.chatLines.push_back("[Games] Pubblicato: " + title
        + " (max " + std::to_string(e.maxPlayers) + " player) · "
        + std::to_string(usedAfter) + "/" + std::to_string(kMaxPublishedGamesPerUser));
    g_menu.tab = HubTab::Games;
    ecoGrant(g_menu.username, 25, "publish game");
    ecoSyncMenu(g_menu);
    saveUserProfile(g_menu);
    CrashHandler::note(("Pubblicato gioco: " + title).c_str());
    return true;
}
