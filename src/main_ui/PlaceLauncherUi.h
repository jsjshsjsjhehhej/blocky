// Place Launcher e JobId nel motore: collega blk::placelauncher (la scelta del
// server) a menu, schermata di caricamento, TeleportService e game.JobId.
// Come gli altri main_ui: usa le globali di main.cpp, incluso dopo
// ScreenTransitions.h (startLoadingGame, returnToMenu).

#include "engine/PlaceLauncher.h"

static blk::placelauncher::Launcher g_placeLauncher;

// Da dove e' partita la richiesta: dal menu si aspetta sulla schermata di
// caricamento; da un teleport si resta in partita finche' il server non e'
// trovato, e se non c'e' non si esce affatto.
enum class LaunchOrigin { None, Menu, Teleport };
static LaunchOrigin g_launchOrigin = LaunchOrigin::None;
static int g_launchGameIdx = -1;
// JobId del server scelto, finche' il Welcome non lo conferma (e per sempre
// con un server di una versione vecchia, che nel Welcome non lo manda).
static std::string g_expectedJobId;

static int findGameIndexById(const std::string& id) {
    for (int i = 0; i < (int)g_games.size(); ++i)
        if (g_games[(size_t)i].id == id) return i;
    return -1;
}

static void startPlaceLaunch(int gameIdx, const blk::placeinvite::Target& t, LaunchOrigin origin) {
    blk::placelauncher::Request r;
    r.gameIndex = gameIdx;
    r.placeId = g_games[(size_t)gameIdx].id;
    r.jobId = t.jobId;
    r.poolCode = t.poolCode;
    r.port = g_menu.joinPort >= 1024 ? g_menu.joinPort : 27015;
    r.serverOnly = gateServerOnly();
    g_placeLauncher.reset();
    g_placeLauncher.start(r);
    g_launchOrigin = origin;
    g_launchGameIdx = gameIdx;
    g_expectedJobId.clear();
}

/** "Gioca" dal menu: il launcher sceglie il server, intanto si carica. */
static void placeLaunchPlay(int gameIdx) {
    if (gameIdx < 0 || gameIdx >= (int)g_games.size()) return;
    startPlaceLaunch(gameIdx, {}, LaunchOrigin::Menu);
    startLoadingGame(gameIdx);
}

/**
 * Il campo "Unisciti" del menu: JobId, invito blocky://place/... o codice
 * blk://. Falso (con il motivo in g_menu.netStatus) se il testo non e' valido
 * o il gioco dell'invito non e' installato qui.
 */
static bool placeLaunchJoinText(const char* text, int selectedGame) {
    blk::placeinvite::Target t;
    if (!blk::placeinvite::parse(text ? text : "", t)) {
        std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus),
                      "Non e' un Job ID, un invito blocky:// o un codice blk:// valido");
        return false;
    }
    int idx = selectedGame;
    if (!t.placeId.empty()) {
        idx = findGameIndexById(t.placeId);
        if (idx < 0) {
            std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus),
                          "Il gioco \"%s\" non e' tra i tuoi giochi: scaricalo prima", t.placeId.c_str());
            return false;
        }
    }
    if (idx < 0 || idx >= (int)g_games.size()) {
        std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus), "Scegli prima un gioco");
        return false;
    }
    if (!t.poolCode.empty() && !blk::PoolConfig::isConfigured()) {
        std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus), "Pool online non configurato");
        return false;
    }
    g_menu.netStatus[0] = '\0';
    startPlaceLaunch(idx, t, LaunchOrigin::Menu);
    startLoadingGame(idx);
    return true;
}

// Il risultato del launcher diventa la modalita' di ingresso che finishLoading
// conosce gia' (IP, codice, o ricerca automatica che apre un server).
static void applyLaunchDecision() {
    using blk::placelauncher::Status;
    g_joinByIp = false;
    g_joinByCode = false;
    if (g_placeLauncher.status() == Status::Join) {
        const auto* s = g_placeLauncher.chosen();
        std::snprintf(g_menu.joinIp, sizeof(g_menu.joinIp), "%s", s->ip.c_str());
        g_menu.joinPort = s->port;
        g_expectedJobId = s->jobId;
        g_joinByIp = true;
    } else if (g_placeLauncher.status() == Status::JoinPool) {
        std::snprintf(g_menu.joinCode, sizeof(g_menu.joinCode), "%s",
                      g_placeLauncher.request().poolCode.c_str());
        g_joinByCode = true;
    }
    // HostNew: niente, finishLoading fa la ricerca automatica e apre un server.
}

/**
 * Durante la schermata di caricamento. Vero = si puo' finire di caricare;
 * falso = il launcher sta ancora cercando, oppure ha fallito e si e' tornati
 * al menu con il motivo.
 */
// Nessun server per il gioco: su PC se ne avvia uno qui (BlockyServer con il
// place del gioco e i suoi Script server), come Roblox che crea un server
// quando entri in un gioco vuoto, e lo si cerca di nuovo finche' compare.
static double g_autoServerUntil = 0.0;   // >0: avviato da noi, si cerca fino a qui
static double g_autoServerNextScan = 0.0;

static bool tryAutoStartServer() {
    if (!plat::has(plat::Service::LaunchProcess)) return false;
    if (g_launchGameIdx < 0 || g_launchGameIdx >= (int)g_games.size()) return false;
    const GameEntry& g = g_games[(size_t)g_launchGameIdx];
    if (g.placePath.empty() || !plat::fs::exists(g.placePath)) return false;
    if (!launchDedicatedServer(g.maxPlayers > 0 ? g.maxPlayers : 8, g.title.c_str(), g.placePath)) return false;
    LOG_INFO("launcher", "nessun server per '%s': ne avvio uno con il suo place", g.id.c_str());
    return true;
}

static bool placeLauncherLoadingReady() {
    if (g_launchOrigin != LaunchOrigin::Menu) return true;
    if (g_autoServerUntil > 0.0 && !g_placeLauncher.busy()
        && g_placeLauncher.status() == blk::placelauncher::Status::NoServer) {
        // Server avviato da noi: si riprova a cercarlo ogni secondo.
        if (glfwGetTime() < g_autoServerUntil) {
            if (glfwGetTime() >= g_autoServerNextScan) {
                g_autoServerNextScan = glfwGetTime() + 1.0;
                const blk::placelauncher::Request r = g_placeLauncher.request();
                g_placeLauncher.reset();
                g_placeLauncher.start(r);
                g_placeLauncher.showMessage("Avvio un server per questo gioco...");
            }
            return false;
        }
    }
    g_placeLauncher.tick();
    if (g_placeLauncher.busy()) return false;
    if (g_placeLauncher.status() == blk::placelauncher::Status::NoServer && g_autoServerUntil == 0.0
        && g_placeLauncher.request().jobId.empty() && tryAutoStartServer()) {
        g_autoServerUntil = glfwGetTime() + 15.0;
        g_autoServerNextScan = glfwGetTime() + 1.5;   // il tempo di partire e caricare il place
        g_placeLauncher.showMessage("Avvio un server per questo gioco...");
        return false;
    }
    g_autoServerUntil = 0.0;
    g_launchOrigin = LaunchOrigin::None;
    if (blk::placelauncher::isFailure(g_placeLauncher.status())) {
        std::snprintf(g_menu.netStatus, sizeof(g_menu.netStatus), "%s", g_placeLauncher.message().c_str());
        g_menu.launchGame = -1;
        g_screen = AppScreen::MainMenu;
        glfwSetWindowTitle(glfwGetCurrentContext(), "Blocky");
        return false;
    }
    applyLaunchDecision();
    return true;
}

/** Riga di stato del launcher sopra la schermata di caricamento. */
static void drawPlaceLauncherLoadingStatus() {
    if (g_launchOrigin != LaunchOrigin::Menu || g_placeLauncher.message().empty()) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const char* msg = g_placeLauncher.message().c_str();
    const ImVec2 sz = ImGui::CalcTextSize(msg);
    const ImVec2 p{(float)g_w * 0.5f - sz.x * 0.5f, (float)g_h * 0.48f + 96.f};
    dl->AddRectFilled({p.x - 12, p.y - 6}, {p.x + sz.x + 12, p.y + sz.y + 6}, IM_COL32(10, 30, 70, 190), 8.f);
    dl->AddText(p, IM_COL32(220, 235, 255, 255), msg);
}

/**
 * In partita: i teleport chiesti dagli script. Si cerca il server restando
 * in gioco; solo quando c'e' si esce e si carica la nuova partita. Se il
 * server non c'e' si resta e scatta TeleportInitFailed.
 */
static void placeLauncherPlayingTick(GLFWwindow* win) {
    using blk::placelauncher::Status;
    auto& q = blk::teleport::queue();
    if (g_launchOrigin == LaunchOrigin::None && !q.empty()) {
        const blk::teleport::Request req = q.front();
        q.clear();
        const int idx = findGameIndexById(req.placeId);
        if (idx < 0) {
            const std::string msg = "Il gioco \"" + req.placeId + "\" non e' installato";
            g_lua.fireTeleportFailed("Failure", msg.c_str(), req.placeId.c_str());
            g_menu.chatLines.push_back("[Teleport] " + msg);
            return;
        }
        blk::placeinvite::Target t;
        t.jobId = req.jobId;
        startPlaceLaunch(idx, t, LaunchOrigin::Teleport);
        g_menu.chatLines.push_back("[Teleport] " + g_placeLauncher.message());
        return;
    }
    if (g_launchOrigin != LaunchOrigin::Teleport) return;
    g_placeLauncher.tick();
    if (g_placeLauncher.busy()) return;
    g_launchOrigin = LaunchOrigin::None;

    const Status st = g_placeLauncher.status();
    if (blk::placelauncher::isFailure(st)) {
        const char* reason = st == Status::GameFull ? "GameFull" : "GameEnded";
        g_lua.fireTeleportFailed(reason, g_placeLauncher.message().c_str(),
                                 g_placeLauncher.request().placeId.c_str());
        g_menu.chatLines.push_back("[Teleport] " + g_placeLauncher.message());
        return;
    }
    // returnToMenu ricarica l'elenco giochi: l'indice si ricerca per id.
    const std::string placeId = g_placeLauncher.request().placeId;
    returnToMenu(win);
    const int idx = findGameIndexById(placeId);
    if (idx < 0) return;
    applyLaunchDecision();
    startLoadingGame(idx);
}

/** Nel test di Studio non si cambia partita, come in Roblox Studio. */
static void placeLauncherRejectTeleportsInStudio() {
    auto& q = blk::teleport::queue();
    if (q.empty()) return;
    const std::string place = q.front().placeId;
    q.clear();
    g_lua.fireTeleportFailed("Failure", "Il teleport non funziona nel test di Studio", place.c_str());
    LOG_INFO("teleport", "ignorato nel test di Studio (place %s)", place.c_str());
}

/** Tiene game.PlaceId / game.JobId uguali alla partita in cui si e'. */
static void syncGameIdentity() {
    if (!g_lua.ok) return;
    const bool inGame = g_screen == AppScreen::Playing;
    const std::string place = inGame ? g_activeGame.id : std::string();
    std::string job;
    if (inGame && g_net.role != NetRole::Offline)
        job = g_net.jobId[0] ? std::string(g_net.jobId) : g_expectedJobId;
    if (place != g_lua.placeId || job != g_lua.jobId) g_lua.setGameIdentity(place, job);
}

/** Il JobId attuale (o vuoto), da mostrare e copiare. */
static std::string currentJobId() {
    if (g_net.role == NetRole::Offline) return {};
    return g_net.jobId[0] ? std::string(g_net.jobId) : g_expectedJobId;
}

/** Sezione "Server" del menu di gioco: JobId, copia, invito. */
static void drawJobIdSection() {
    const std::string job = currentJobId();
    ImGui::TextDisabled("Server");
    if (job.empty()) {
        ImGui::TextDisabled(g_net.role == NetRole::Offline ? "Partita offline: nessun Job ID"
                                                           : "Job ID in arrivo dal server...");
        return;
    }
    ImGui::TextColored({0.80f, 0.90f, 1.f, 1.f}, "Job ID %s", blk::jobid::shortForm(job).c_str());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", job.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Copia##jobid")) ImGui::SetClipboardText(job.c_str());
    if (ImGui::Button("Copia invito per un amico", {-1, 30})) {
        const std::string link = blk::placeinvite::make(g_activeGame.id, job);
        ImGui::SetClipboardText(link.c_str());
        g_menu.chatLines.push_back("[Server] Invito copiato: " + link);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("L'amico lo incolla in \"Unisciti\" nel menu ed entra in QUESTO server.");
}

/**
 * Avvio con un invito: Blocky.exe "blocky://place/<id>?job=<jobId>" (o con la
 * variabile BLOCKY_JOIN, che il launcher imposta dall'argomento). Il menu si
 * apre comunque: l'ingresso parte appena l'elenco giochi e' pronto.
 */
static std::string g_pendingInvite;

static void captureInviteFromEnv() {
    const char* inv = std::getenv("BLOCKY_JOIN");
    if (inv && inv[0]) g_pendingInvite = inv;
}

static void placeLauncherMenuTick() {
    if (g_pendingInvite.empty() || g_games.empty() || g_screen != AppScreen::MainMenu) return;
    if (!g_menu.loggedIn) return; // prima il login: il nome in partita e' quello dell'account
    const std::string inv = std::move(g_pendingInvite);
    g_pendingInvite.clear();
    g_menu.launchGame = g_menu.selectedGame;
    if (placeLaunchJoinText(inv.c_str(), g_menu.selectedGame)) g_menu.launchGame = g_launchGameIdx;
}

/** Una volta per frame, da main.cpp. */
static void placeLauncherFrame(GLFWwindow* win) {
    if (g_screen == AppScreen::MainMenu) placeLauncherMenuTick();
    if (g_studioTesting) {
        placeLauncherRejectTeleportsInStudio();
    } else if (g_screen == AppScreen::Playing) {
        placeLauncherPlayingTick(win);
    } else if (g_launchOrigin == LaunchOrigin::Teleport) {
        // Uscito dalla partita mentre il teleport cercava: si annulla.
        g_launchOrigin = LaunchOrigin::None;
        g_placeLauncher.reset();
        blk::teleport::queue().clear();
    }
    syncGameIdentity();
}
