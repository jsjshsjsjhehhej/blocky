// Il client comandato dal server (engine/ClientSettings.h): all'avvio carica
// le ultime impostazioni e chiede quelle nuove al backend; nel menu mostra il
// messaggio del giorno e gli aggiornamenti, e blocca il gioco se la versione
// installata non e' piu' ammessa. Usa le globali di main.cpp.
//
// Flag lette qui e nel resto del motore (default tra parentesi):
//   FFlagPlaceLauncherPlay (true)  "Gioca" passa dal Place Launcher
//   FFlagDeveloperConsole  (true)  F9 apre la Developer Console fuori da Studio
//   FStringHubMotd         ("")    messaggio in alto nel menu
//   FIntVersionHistorySeconds (120) ogni quanto Studio salva una versione

#include "engine/BackendConfig.h"
#include "engine/ClientSettings.h"
#include "engine/PlaceInstance.h"

namespace srvctl = blk::clientsettings;

static std::string clientSettingsCachePath() { return blockyDataDir() + "/client_settings.json"; }

/**
 * Id casuale del dispositivo, solo per i rollout a percentuale (lo stesso
 * dispositivo resta nello stesso gruppo). Non contiene niente dell'utente.
 */
static std::string clientDeviceId() {
    const std::string path = blockyDataDir() + "/device_id.txt";
    std::string id = readTextFile(path);
    while (!id.empty() && (id.back() == '\n' || id.back() == '\r' || id.back() == ' ')) id.pop_back();
    if (blk::jobid::isValid(id)) return id;
    id = blk::jobid::generate();
    std::ofstream(path, std::ios::trunc) << id << "\n";
    return id;
}

static void serverControlBoot() {
    if (srvctl::loadCache(clientSettingsCachePath()))
        LOG_INFO("server", "impostazioni: ultime salvate (%d flag)", (int)srvctl::snapshot().flags.size());
    const auto addr = blk::BackendConfig::address();
    if (!addr.configured()) {
        LOG_INFO("server", "backend non configurato: impostazioni %s",
                 srvctl::sourceName(srvctl::snapshot().source));
        return;
    }
    LOG_INFO("server", "chiedo le impostazioni a %s:%d", addr.host.c_str(), addr.port);
    srvctl::startFetch(addr.host, addr.port, clientDeviceId(), clientSettingsCachePath());
}

static bool g_updateBannerClosed = false;

/** Messaggio del giorno, aggiornamenti e blocco per versione troppo vecchia. */
static void drawServerControlOverlay() {
    if (g_screen != AppScreen::MainMenu) return;
    const srvctl::Snapshot snap = srvctl::snapshot();
    const auto& ver = snap.version;

    if (ver.known && ver.mustUpdate) {
        // Come Roblox: con una versione non piu' ammessa non si gioca. Si
        // copre il menu intero, cosi' Gioca non e' raggiungibile.
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({(float)g_w, (float)g_h});
        ImGui::SetNextWindowBgAlpha(0.88f);
        ImGui::Begin("##mustupdate", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetCursorPos({(float)g_w * 0.5f - 220.f, (float)g_h * 0.5f - 90.f});
        ImGui::BeginGroup();
        {
            blk::ui::FontScope bold(blk::ui::fontBold());
            ImGui::TextUnformatted("Aggiornamento obbligatorio");
        }
        ImGui::Spacing();
        ImGui::TextWrapped("Questa versione di Blocky (%s) non e' piu' supportata. "
                           "Installa la %s per continuare a giocare.",
                           srvctl::kClientVersion, ver.latest.c_str());
        if (!ver.notes.empty()) ImGui::TextDisabled("%s", ver.notes.c_str());
        ImGui::Spacing();
        if (!ver.url.empty() && ImGui::Button("Scarica l'aggiornamento", {240, 38})) plat::sys::openUrl(ver.url);
        ImGui::EndGroup();
        ImGui::End();
        return;
    }

    const std::string motd = srvctl::getString("FStringHubMotd", "");
    const bool showUpdate = ver.known && ver.updateAvailable && !g_updateBannerClosed;
    if (motd.empty() && !showUpdate) return;

    ImGui::SetNextWindowPos({(float)g_w * 0.5f, 104.f}, ImGuiCond_Always, {0.5f, 0.f});
    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::Begin("##motd", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove
                 | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    if (!motd.empty()) ImGui::TextUnformatted(motd.c_str());
    if (showUpdate) {
        ImGui::Text("Disponibile Blocky %s", ver.latest.c_str());
        if (!ver.url.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Scarica")) plat::sys::openUrl(ver.url);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Dopo")) g_updateBannerClosed = true;
    }
    ImGui::End();
}

/** Scheda "Server" della Developer Console: cosa ha deciso il backend. */
static void drawServerControlDevTab() {
    const srvctl::Snapshot snap = srvctl::snapshot();
    const auto addr = blk::BackendConfig::address();
    ImGui::Text("Backend: %s", addr.configured() ? (addr.host + ":" + std::to_string(addr.port)).c_str()
                                                 : "non configurato");
    ImGui::Text("Client %s (%s)   impostazioni: %s%s", srvctl::kClientVersion, srvctl::platformName(),
                srvctl::sourceName(snap.source), srvctl::isFetching() ? "   (aggiorno...)" : "");
    if (!snap.lastError.empty()) ImGui::TextColored({1.f, 0.6f, 0.3f, 1.f}, "Ultimo errore: %s", snap.lastError.c_str());
    if (snap.version.known)
        ImGui::Text("Ultima versione: %s   minima: %s%s", snap.version.latest.empty() ? "-" : snap.version.latest.c_str(),
                    snap.version.minSupported.empty() ? "-" : snap.version.minSupported.c_str(),
                    snap.version.mustUpdate ? "   DA AGGIORNARE" : "");
    if (addr.configured() && !srvctl::isFetching() && ImGui::SmallButton("Richiedi di nuovo"))
        srvctl::startFetch(addr.host, addr.port, clientDeviceId(), clientSettingsCachePath());
    ImGui::Separator();
    if (snap.flags.empty()) {
        ImGui::TextDisabled("Nessuna flag dal server: valgono i default nel codice.");
        return;
    }
    if (ImGui::BeginTable("flags", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY,
                          {0, 220})) {
        ImGui::TableSetupColumn("Flag");
        ImGui::TableSetupColumn("Valore");
        ImGui::TableHeadersRow();
        for (const auto& kv : snap.flags) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(kv.first.c_str());
            ImGui::TableNextColumn();
            switch (kv.second.kind) {
            case srvctl::Flag::Kind::Bool: ImGui::TextUnformatted(kv.second.b ? "true" : "false"); break;
            case srvctl::Flag::Kind::Int: ImGui::Text("%lld", kv.second.i); break;
            case srvctl::Flag::Kind::String: ImGui::TextUnformatted(kv.second.s.c_str()); break;
            }
        }
        ImGui::EndTable();
    }
}
