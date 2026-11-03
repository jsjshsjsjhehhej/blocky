// Test multiplayer da Studio, come "Clients and Servers" di Roblox Studio:
// salva il place com'e' adesso e apre N finestre di Blocky.exe gia'
// collegate fra loro sul PC (la prima fa da host, le altre entrano su
// 127.0.0.1). Si chiudono tutte con un clic.
//
// Le istruzioni al giocatore di prova passano per variabili d'ambiente date
// SOLO a quel processo (plat::sys::launchTracked): il motore non legge la
// riga di comando, e cosi' niente problemi di virgolette nei percorsi.
//
// Non e' un header riusabile: usa le globali di main.cpp e le funzioni di
// ScreenTransitions.h. Va incluso dopo quel file.

#include "engine/PlaceExporter.h"

#include <cstdlib>

struct MpTestClient {
    uint32_t pid = 0;
    std::string name;
    bool host = false;
    double launchAt = 0.0;   // i client partono dopo l'host
    bool launched = false;
};

static bool g_showMpTest = false;
static int g_mpTestPlayers = 2;
static int g_mpTestPort = 27100;   // non la 27015 dei server veri
static std::vector<MpTestClient> g_mpTestClients;
static std::string g_mpTestStatus;
static std::string g_mpTestPlace;
static std::string g_mpTestScript;

static constexpr const char* kMpEnvPlace = "BLOCKY_MPTEST_PLACE";
static constexpr const char* kMpEnvRole = "BLOCKY_MPTEST_ROLE";
static constexpr const char* kMpEnvPort = "BLOCKY_MPTEST_PORT";
static constexpr const char* kMpEnvName = "BLOCKY_MPTEST_NAME";
static constexpr const char* kMpEnvIndex = "BLOCKY_MPTEST_INDEX";
static constexpr const char* kMpEnvScript = "BLOCKY_MPTEST_SCRIPT";

static void studioStopMultiplayerTest() {
    for (const MpTestClient& c : g_mpTestClients)
        if (c.launched) plat::sys::terminateTracked(c.pid);
    g_mpTestClients.clear();
    g_mpTestStatus = "Test multiplayer chiuso";
}

static void studioStartMultiplayerTest() {
    studioStopMultiplayerTest();
    if (!plat::has(plat::Service::LaunchProcess)) {
        g_mpTestStatus = "Questa piattaforma non puo' aprire altre finestre: usa un PC.";
        return;
    }
    const std::string exe = siblingExePath("Blocky");
    if (exe.empty() || !plat::fs::exists(exe)) {
        g_mpTestStatus = "Blocky.exe non trovato accanto a BlockyGameEngine: ricompila il target Blocky";
        return;
    }
    g_mpTestPlace = blockyDataDir() + "/studio_mptest.blocky";
    const std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos,
                                                        g_playerYaw, &g_lighting);
    if (!PlaceExporter::saveToFile(g_mpTestPlace, json)) {
        g_mpTestStatus = "Impossibile salvare il place di prova in " + g_mpTestPlace;
        return;
    }
    g_mpTestScript = g_activeGame.script;
    const double now = glfwGetTime();
    for (int i = 0; i < g_mpTestPlayers; ++i) {
        MpTestClient c;
        c.name = "Player" + std::to_string(i + 1);
        c.host = i == 0;
        // Il tempo che l'host carichi il place e apra la porta.
        c.launchAt = now + (c.host ? 0.0 : 3.0 + 0.5 * (i - 1));
        g_mpTestClients.push_back(c);
    }
    g_mpTestStatus = "Avvio di " + std::to_string(g_mpTestPlayers) + " giocatori...";
}

// Ogni frame in Studio: lancia chi e' in attesa, nota chi e' stato chiuso.
static void studioMultiplayerTestTick() {
    if (g_mpTestClients.empty()) return;
    const double now = glfwGetTime();
    int alive = 0, failed = 0;
    for (size_t i = 0; i < g_mpTestClients.size(); ++i) {
        MpTestClient& c = g_mpTestClients[i];
        if (!c.launched && now >= c.launchAt) {
            c.launched = true;
            c.pid = plat::sys::launchTracked(siblingExePath("Blocky"), "", plat::selfDir(), {
                {kMpEnvPlace, g_mpTestPlace},
                {kMpEnvRole, c.host ? "host" : "client"},
                {kMpEnvPort, std::to_string(g_mpTestPort)},
                {kMpEnvName, c.name},
                {kMpEnvIndex, std::to_string(i)},
                {kMpEnvScript, g_mpTestScript},
            });
        }
        if (!c.launched) continue;
        if (c.pid == 0) ++failed;
        else if (plat::sys::isTrackedAlive(c.pid)) ++alive;
    }
    const bool allLaunched = std::all_of(g_mpTestClients.begin(), g_mpTestClients.end(),
                                         [](const MpTestClient& c) { return c.launched; });
    if (failed > 0)
        g_mpTestStatus = std::to_string(failed) + " finestre non si sono aperte";
    else if (allLaunched && alive == 0)
        g_mpTestStatus = "Tutte le finestre di prova sono state chiuse";
    else if (allLaunched)
        g_mpTestStatus = std::to_string(alive) + " giocatori di prova aperti (host: Player1, porta "
                       + std::to_string(g_mpTestPort) + ")";
}

static void drawMultiplayerTestWindow() {
    if (!g_showMpTest) return;
    ImGui::SetNextWindowSize({380.f, 0.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Test multiplayer", &g_showMpTest)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Apre piu' finestre di Blocky collegate fra loro su questo PC, con il "
                       "place com'e' adesso (anche se non l'hai salvato). Player1 fa da host.");
    ImGui::SliderInt("Giocatori", &g_mpTestPlayers, 2, 4);
    ImGui::InputInt("Porta", &g_mpTestPort, 0, 0);
    g_mpTestPort = std::clamp(g_mpTestPort, 1024, 65000);
    if (g_mpTestPort == 27015) ImGui::TextColored({1.f, 0.6f, 0.3f, 1.f}, "27015 e' la porta dei server veri");

    const bool running = !g_mpTestClients.empty();
    if (ImGui::Button(running ? "Riavvia" : "Avvia", {120, 32})) studioStartMultiplayerTest();
    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    if (ImGui::Button("Chiudi tutti", {120, 32})) studioStopMultiplayerTest();
    ImGui::EndDisabled();

    if (!g_mpTestStatus.empty()) ImGui::TextWrapped("%s", g_mpTestStatus.c_str());
    for (const MpTestClient& c : g_mpTestClients) {
        const char* state = !c.launched ? "in attesa"
                          : c.pid == 0 ? "non avviato"
                          : plat::sys::isTrackedAlive(c.pid) ? "aperto" : "chiuso";
        ImGui::BulletText("%s%s — %s", c.name.c_str(), c.host ? " (host)" : "", state);
    }
    ImGui::End();
}

// ---- Lato giocatore di prova ------------------------------------------------

// Ogni cambio di stato della rete di un giocatore di prova finisce in
// blocky.log: se il test non si collega, li' si legge perche'.
static void multiplayerTestStatusTick() {
    if (g_mpTestName.empty()) return;
    static std::string last;
    const std::string now = g_net.status;
    if (now == last) return;
    last = now;
    LOG_INFO("mptest", "%s: %s", g_mpTestName.c_str(), now.c_str());
}

// All'avvio di Blocky.exe: se e' un giocatore di prova lanciato da Studio,
// entra subito nella partita di test invece di mostrare il menu.
static bool maybeStartMultiplayerTestFromEnv(GLFWwindow* win) {
    const char* place = std::getenv(kMpEnvPlace);
    if (!place || !place[0] || !plat::fs::exists(place)) return false;
    const char* role = std::getenv(kMpEnvRole);
    const char* port = std::getenv(kMpEnvPort);
    const char* name = std::getenv(kMpEnvName);
    const char* index = std::getenv(kMpEnvIndex);
    const char* script = std::getenv(kMpEnvScript);
    const int p = port ? std::clamp(std::atoi(port), 1024, 65000) : 27100;
    const int idx = index ? std::clamp(std::atoi(index), 0, 15) : 0;

    g_mpTestName = name && name[0] ? name : ("Player" + std::to_string(idx + 1));
    GameEntry e;
    e.id = "studio-mptest";
    e.title = "Test multiplayer — " + g_mpTestName;
    e.placePath = place;
    e.script = script ? script : "";
    e.maxPlayers = 8;
    g_activeGame = e;

    if (role && std::strcmp(role, "client") == 0) {
        g_joinByIp = true;
        std::snprintf(g_menu.joinIp, sizeof(g_menu.joinIp), "127.0.0.1");
        g_menu.joinPort = p;
    } else {
        g_mpTestHostPort = p;
    }

#ifndef __ANDROID__
    // Finestre piu' piccole e sfalsate, per vederle tutte insieme (solo PC:
    // su Android non ci sono finestre, e Studio non vi lancia giocatori).
    glfwSetWindowSize(win, 960, 540);
    glfwSetWindowPos(win, 40 + idx * 480, 60 + (idx % 2) * 300);
#endif
    glfwSetWindowTitle(win, ("Blocky — " + e.title).c_str());

    g_loadTimer = 0.f;
    g_screen = AppScreen::Loading;
    return true;
}
