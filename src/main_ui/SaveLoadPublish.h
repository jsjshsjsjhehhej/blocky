// Salvataggio, autosave, chiusura pulita, apertura place e pubblicazione gioco.
// Non e' un header riusabile: usa direttamente le globali di main.cpp
// (g_world, g_menu, g_placeFilePath, ecc.). Compila solo se incluso da
// main.cpp esattamente in questo punto, mai da altri translation unit.

#include "engine/SceneIntegrity.h"
#include "engine/RuntimeScale.h"

#include <atomic>
#include <mutex>
#include <thread>

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
    // Giochi gia' scaricati dal PC (telefono): restano in menu e giocabili.
    mergeGamePackages(g_games, blk::remotegames::listDownloadedEntries());
    blk::remotegames::applyDownloadedToList(g_games);
}

static bool writeCurrentPlaceTo(const std::string& path) {
    if (path.empty()) return false;
    // Durante il Test il mondo contiene quello che gli script hanno creato giocando
    // (GUI, part, NPC...): si salva il place di PRIMA del Test, come Roblox Studio.
    // Prima chiudere lo Studio a meta' di un Test sporcava il place.
    if (g_studioTesting && g_hasTestBackup) {
        std::string json = PlaceExporter::exportPlace(g_worldTestBackup, g_player.humanoid, g_playerPosTestBackup,
                                                      g_playerYawTestBackup, &g_lighting);
        saveTerrainBeside(path, true);
        return PlaceExporter::saveToFile(path, json);
    }
    syncFriendsToWorld();
    std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos, g_playerYaw, &g_lighting);
    g_world.placeFile = path;
    saveTerrainBeside(path, false);
    return PlaceExporter::saveToFile(path, json);
}

static std::string placeAutosavePath() {
    return blockyDataDir() + "/autosave_place.json";
}

static bool g_persistBusy = false;
static bool g_didPersistExit = false;
static float g_autosaveAcc = 0.f;

static void persistAutosavePlaceOnly() {
    if (g_persistBusy || !kIsGameEngine || g_studioTesting) return;
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

// StudioDevTools.h (incluso dopo): ogni salvataggio e' anche una versione.
static bool versionHistorySnapshot();

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
        versionHistorySnapshot();
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
    g_undo.clear();   // le voci del place di prima non valgono qui
    g_worldReplaced = true;
    g_placeFilePath.clear();
    g_wsMeshes.clear();
    syncFriendsFromWorld();
    selectPart(g_world.parts.empty() ? -1 : 0);
    markPlaceDirty();
    g_exportMsg = "Ripristinato place non salvato (autosave)";
}

static int g_pendingTemplate = -1;

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
    case PendingNav::NewPlace: {
        const int kind = g_pendingTemplate >= 0 ? g_pendingTemplate : 0;
        g_pendingTemplate = -1;
        applyMapTemplate(kind);
        clearPlaceDirty();
        g_placeFilePath.clear();
        returnToStudioEditor(win);
        g_exportMsg = std::string("Nuovo place: ") + g_world.placeName;
        break;
    }
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

static void startPlaceTemplate(GLFWwindow* win, int kind) {
    g_pendingTemplate = kind;
    requestNav(win, PendingNav::NewPlace);
}

static void drawGameEngineHome(GLFWwindow* win, float ) {
    const float W = (float)g_w, H = (float)g_h;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({W, H});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.11f, 0.11f, 0.12f, 1.f});
    ImGui::Begin("##GEHome", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, {W, 52.f}, IM_COL32(24, 24, 26, 255));
    dl->AddRectFilled({0, 52.f}, {0.f + 3.f, 52.f}, IM_COL32(0, 120, 212, 255));
    ImGui::SetCursorPos({18.f, 14.f});
    ImGui::TextUnformatted("Blocky Studio");
    ImGui::SameLine();
    ImGui::TextDisabled("  place");
    if (g_placeDirty) {
        ImGui::SameLine();
        ImGui::TextColored({1.f, 0.55f, 0.28f, 1.f}, "  non salvato");
    }

    const float railW = 220.f;
    ImGui::SetCursorPos({0.f, 52.f});
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.15f, 0.15f, 0.16f, 1.f});
    ImGui::BeginChild("##homerail", {railW, H - 52.f}, false);
    ImGui::Dummy({0, 12});
    ImGui::Indent(12.f);
    auto voce = [railW](const char* etichetta, bool acceso) {
        ImGui::PushStyleColor(ImGuiCol_Button, acceso ? ImVec4(0.f, 0.47f, 0.83f, 1.f) : ImVec4(0.20f, 0.20f, 0.22f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, acceso ? ImVec4(0.10f, 0.55f, 0.92f, 1.f) : ImVec4(0.26f, 0.26f, 0.28f, 1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        const bool premuto = ImGui::Button(etichetta, ImVec2(railW - 28.f, 36.f));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        return premuto;
    };
    if (voce("Continua", true)) returnToStudioEditor(win);
    ImGui::Dummy({0, 6});
    if (voce("Apri place...", false)) requestNav(win, PendingNav::OpenPlace);
    ImGui::Dummy({0, 18});
    ImGui::TextDisabled("SESSIONE");
    if (voce("Esci", false)) requestNav(win, PendingNav::QuitApp);
    ImGui::Unindent(12.f);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SetCursorPos({railW + 28.f, 72.f});
    ImGui::BeginChild("##hometemplates", {W - railW - 40.f, H - 96.f}, false);
    ImGui::TextUnformatted("Nuovo");
    ImGui::TextDisabled("Scegli un modello, come in Roblox Studio.");
    ImGui::Dummy({0, 8});

    struct Modello { const char* nome; const char* cosa; int kind; ImU32 fascia; };
    const Modello modelli[] = {
        {"Baseplate", "Pavimento e spawn", 0, IM_COL32(90, 170, 90, 255)},
        {"Obby", "Piattaforme e salti", 2, IM_COL32(230, 150, 40, 255)},
        {"Arena", "Spazio per uno scontro", 3, IM_COL32(190, 70, 70, 255)},
        {"Citta'", "Strade e isolati", 5, IM_COL32(120, 130, 145, 255)},
        {"Giardino", "Erba, alberi, luce", 1, IM_COL32(70, 160, 90, 255)},
        {"Parco", "Aperto, poco costruito", 4, IM_COL32(60, 140, 110, 255)},
        {"FPS", "Kit di sparatoria", 6, IM_COL32(70, 110, 180, 255)},
        {"Tycoon", "Pad e monete", 7, IM_COL32(200, 170, 50, 255)},
        {"Survival", "Base e un NPC", 8, IM_COL32(90, 80, 140, 255)},
    };
    const float cardW = 188.f, cardH = 96.f, gap = 12.f;
    const float avail = ImGui::GetContentRegionAvail().x;
    int col = 0;
    const int cols = std::max(1, (int)((avail + gap) / (cardW + gap)));
    for (const Modello& m : modelli) {
        if (col > 0) ImGui::SameLine(0.f, gap);
        ImGui::PushID(m.kind);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.17f, 0.18f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.26f, 0.32f, 1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        const std::string et = std::string(m.nome) + "\n" + m.cosa;
        if (ImGui::Button(et.c_str(), {cardW, cardH}))
            startPlaceTemplate(win, m.kind);
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(a, {b.x, a.y + 5.f}, m.fascia);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::PopID();
        if (++col >= cols) col = 0;
    }
    if (!g_placeFilePath.empty()) {
        ImGui::Dummy({0, 16});
        ImGui::TextDisabled("Ultimo file");
        ImGui::TextWrapped("%s", g_placeFilePath.c_str());
    }
    ImGui::EndChild();

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
    g_undo.clear();   // come Roblox: aprire un altro place azzera la cronologia
    g_worldReplaced = true;
    int integrityFixes = SceneIntegrity::repair(g_world);
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
        + (g_friends.empty() ? "" : (", " + std::to_string(g_friends.size()) + " amici"))
        + (integrityFixes > 0 ? (", " + std::to_string(integrityFixes) + " fix integrita'") : "")
        + ")";
    glfwSetWindowTitle(glfwGetCurrentContext(),
        (std::string(kAppBrand) + " - " + g_world.placeName).c_str());
    return true;
}

struct AsyncPlaceJob {
    std::mutex mu;
    std::thread thread;
    std::atomic<int> phase{0};
    std::string path;
    DataModel world;
    LightingService lighting;
};

static AsyncPlaceJob& asyncPlaceJob() {
    static AsyncPlaceJob job;
    return job;
}

static void requestAsyncPlace(const std::string& path) {
    AsyncPlaceJob& job = asyncPlaceJob();
    if (job.phase.load() == 1) {
        g_exportMsg = "Caricamento gia' in corso";
        return;
    }
    if (job.thread.joinable()) job.thread.join();
    job.phase.store(1);
    blk::scale::asyncLoad().phase.store(1);
    blk::scale::asyncLoad().path = path;
    blk::scale::asyncLoad().message = "Lettura " + path;
    g_exportMsg = "Caricamento in background: " + path;
    job.thread = std::thread([path] {
        DataModel loaded;
        LightingService lights;
        const bool ok = loadPlaceFromJson(path, loaded, &lights);
        AsyncPlaceJob& job = asyncPlaceJob();
        if (ok) {
            std::lock_guard<std::mutex> lock(job.mu);
            job.world = std::move(loaded);
            job.lighting = lights;
            job.path = path;
        }
        job.phase.store(ok ? 2 : 3);
        blk::scale::asyncLoad().phase.store(ok ? 2 : 3);
        blk::scale::asyncLoad().message = ok ? "Pronto" : "Lettura fallita";
    });
}

static void pollAsyncPlaceOpen() {
    AsyncPlaceJob& job = asyncPlaceJob();
    const int phase = job.phase.load();
    if (phase != 2 && phase != 3) return;
    if (job.thread.joinable()) job.thread.join();
    if (phase == 3) {
        g_exportMsg = "Impossibile aprire: " + blk::scale::asyncLoad().path;
        job.phase.store(0);
        blk::scale::asyncLoad().phase.store(0);
        return;
    }
    DataModel loaded;
    LightingService lights;
    std::string path;
    {
        std::lock_guard<std::mutex> lock(job.mu);
        loaded = std::move(job.world);
        lights = job.lighting;
        path = job.path;
    }
    job.phase.store(0);
    blk::scale::asyncLoad().phase.store(0);
    g_world = std::move(loaded);
    g_lighting = lights;
    g_undo.clear();
    g_worldReplaced = true;
    const int integrityFixes = SceneIntegrity::repair(g_world);
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
        + (g_friends.empty() ? "" : (", " + std::to_string(g_friends.size()) + " amici"))
        + (integrityFixes > 0 ? (", " + std::to_string(integrityFixes) + " fix integrita'") : "")
        + ")";
    if (GLFWwindow* win = glfwGetCurrentContext())
        glfwSetWindowTitle(win, (std::string(kAppBrand) + " - " + g_world.placeName).c_str());
}

static void doOpenPlace() {
    std::string path;
    if (!openPlaceFileDialog(path, publisherHwnd())) {
        g_exportMsg = "Apertura annullata";
        return;
    }
    requestAsyncPlace(path);
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
        // Con GamesHost locale il pacchetto e' gia' su HTTP (cartella games/).
        // PUT serve solo verso un host esterno (blocky_hostd su un altro PC).
        {
            const std::string gs = g_menu.gameServer;
            const bool selfHost = gs.empty()
                || gs.rfind("127.0.0.1", 0) == 0
                || gs.rfind("localhost", 0) == 0
                || blk::remotegames::isCloud(gs);
            if (!selfHost)
                blk::remotegames::uploadGame(gs, id, pkgDir);
        }
        // Catalogo online: il gioco va sul server Blocky e lo vedono tutti,
        // anche con questo PC spento. L'esito arriva nella chat del menu.
        {
            const std::string cloud = blk::cloud::catalogUrl();
            if (!cloud.empty())
                blk::remotegames::publishToCloud(cloud, blk::cloud::publishToken(), id, pkgDir,
                                                 title, pkg.description, e.maxPlayers);
        }

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
