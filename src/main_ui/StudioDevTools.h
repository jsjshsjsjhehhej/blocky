// Strumenti da sviluppatore di Roblox Studio per Blocky, e i bottoni della
// barra che li aprono:
//   - Developer Console (F9): log, memoria, rete, fisica, script, audio
//   - Script Performance: tempo di CPU di ogni script (LuaRuntime::scriptCosts)
//   - Watch: variabili degli script ed espressioni, in tempo reale nel Test
//   - Cronologia versioni del place, con ripristino reversibile
//   - Play Here, pausa della fisica, audio muto, Join Surfaces (salda le
//     part che si toccano)
//
// Non e' un header riusabile: usa le globali di main.cpp. Va incluso dopo
// StudioSelectionUi.h.

#include "platform/SystemStats.h"

#include <ctime>
#include <cstring>
#include <vector>
#include "stb_image_write.h"
#include <filesystem>

static bool g_showDevConsole = false;
static bool g_showScriptPerf = false;
static bool g_showWatch = false;
static bool g_showVersionHistory = false;
static bool g_simPaused = false;      // Test: fisica ferma (gli script girano)
static bool g_audioMuted = false;

// ---- Script Performance ---------------------------------------------------------

static void drawScriptPerfTable(bool compact) {
    if (!g_lua.ok || g_lua.scriptCosts.empty()) {
        ImGui::TextDisabled("Nessuno script in esecuzione (avvia il Test o una partita).");
        return;
    }
    std::vector<std::pair<std::string, LuaRuntime::ScriptCost>> rows(g_lua.scriptCosts.begin(),
                                                                    g_lua.scriptCosts.end());
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return a.second.lastSecMs > b.second.lastSecMs;
    });
    double totalSec = 0.0;
    for (const auto& r : rows) totalSec += r.second.lastSecMs;
    // In Roblox "Activity" e' la percentuale di un secondo spesa nello script.
    ImGui::Text("Script: %d   Totale: %.2f ms/s (%.2f%%)", (int)rows.size(), totalSec, totalSec / 10.0);
    ImGui::SameLine();
    if (ImGui::SmallButton("Azzera")) g_lua.scriptCosts.clear();
    const ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
                             | ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("##sperf", compact ? 4 : 6, tf, ImVec2(0, compact ? 160.f : 0.f))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthStretch, 3.f);
    ImGui::TableSetupColumn("Attivita'");
    ImGui::TableSetupColumn("Chiamate/s");
    ImGui::TableSetupColumn("Picco");
    if (!compact) {
        ImGui::TableSetupColumn("Totale");
        ImGui::TableSetupColumn("Chiamate");
    }
    ImGui::TableHeadersRow();
    for (const auto& r : rows) {
        const auto& c = r.second;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r.first.c_str());
        ImGui::TableNextColumn();
        const double pct = c.lastSecMs / 10.0;
        const ImVec4 col = pct > 3.0 ? ImVec4{1.f, 0.45f, 0.35f, 1.f}
                         : pct > 1.0 ? ImVec4{1.f, 0.8f, 0.35f, 1.f} : ImVec4{0.75f, 0.9f, 0.75f, 1.f};
        ImGui::TextColored(col, "%.2f%%", pct);
        ImGui::TableNextColumn();
        ImGui::Text("%ld", c.lastSecCalls);
        ImGui::TableNextColumn();
        ImGui::Text("%.2f ms", c.peakMs);
        if (!compact) {
            ImGui::TableNextColumn();
            ImGui::Text("%.1f ms", c.totalMs);
            ImGui::TableNextColumn();
            ImGui::Text("%ld", c.calls);
        }
    }
    ImGui::EndTable();
}

static void drawScriptPerfWindow() {
    if (!g_showScriptPerf) return;
    ImGui::SetNextWindowSize({560.f, 300.f}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Script Performance", &g_showScriptPerf))
        drawScriptPerfTable(false);
    ImGui::End();
}

// ---- Watch ------------------------------------------------------------------------

struct WatchExpr {
    char text[128] = {};
    std::string value;
};
static std::vector<WatchExpr> g_watchExprs(1);
static std::vector<std::pair<std::string, std::string>> g_watchGlobals;

static void watchRefresh() {
    for (auto& w : g_watchExprs) w.value = w.text[0] ? g_lua.evalWatch(w.text) : "";
    g_watchGlobals = g_lua.userGlobals();
}

static void drawWatchWindow() {
    if (!g_showWatch) return;
    ImGui::SetNextWindowSize({460.f, 360.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Watch", &g_showWatch)) {
        ImGui::End();
        return;
    }
    // Solo nel Test di Studio: in una partita pubblicata un giocatore non deve
    // poter eseguire espressioni negli script del gioco.
    if (!(g_screen == AppScreen::Studio && g_studioTesting)) {
        ImGui::TextWrapped("Watch funziona durante il Test di Studio (F5): mostra le variabili degli "
                           "script mentre il gioco gira.");
        ImGui::End();
        return;
    }
    static double next = 0.0;
    if (ImGui::GetTime() >= next) {
        watchRefresh();
        next = ImGui::GetTime() + 0.25;
    }
    ImGui::TextDisabled("Espressioni (aggiornate 4 volte al secondo)");
    for (size_t k = 0; k < g_watchExprs.size(); ++k) {
        ImGui::PushID((int)k);
        ImGui::SetNextItemWidth(170.f);
        ImGui::InputTextWithHint("##e", "es. punti, Classifica.Get('Monete')", g_watchExprs[k].text,
                                 sizeof(g_watchExprs[k].text));
        ImGui::SameLine();
        const bool err = g_watchExprs[k].value.rfind("errore", 0) == 0;
        ImGui::TextColored(err ? ImVec4{1.f, 0.5f, 0.4f, 1.f} : ImVec4{0.7f, 0.9f, 1.f, 1.f}, "%s",
                           g_watchExprs[k].value.c_str());
        ImGui::SameLine();
        if (g_watchExprs.size() > 1 && ImGui::SmallButton("x")) {
            g_watchExprs.erase(g_watchExprs.begin() + (long)k);
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (g_watchExprs.size() < 16 && ImGui::SmallButton("+ espressione")) g_watchExprs.emplace_back();
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Variabili globali degli script", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (g_watchGlobals.empty()) ImGui::TextDisabled("Nessuna (le 'local' non sono globali).");
        for (const auto& g : g_watchGlobals) ImGui::BulletText("%s = %s", g.first.c_str(), g.second.c_str());
    }
    ImGui::End();
}

// ---- Developer Console (F9) ------------------------------------------------------

static void drawDevConsoleLogTab() {
    static bool showErr = true, showWarn = true, showInfo = true, showLua = true, follow = true;
    static char filter[64] = {};
    ImGui::Checkbox("Errori", &showErr);
    ImGui::SameLine();
    ImGui::Checkbox("Avvisi", &showWarn);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &showInfo);
    ImGui::SameLine();
    ImGui::Checkbox("print() script", &showLua);
    ImGui::SameLine();
    ImGui::Checkbox("Segui", &follow);
    ImGui::SetNextItemWidth(200.f);
    ImGui::InputTextWithHint("##flt", "filtra...", filter, sizeof(filter));
    ImGui::SameLine();
    const bool copy = ImGui::SmallButton("Copia tutto");

    std::string clip;
    ImGui::BeginChild("##log", ImVec2(0, 0), ImGuiChildFlags_Borders);
    auto match = [&](const std::string& s) { return !filter[0] || s.find(filter) != std::string::npos; };
    for (const blog::Entry& e : blog::recentOrdered()) {
        if ((e.level == blog::Err && !showErr) || (e.level == blog::Warn && !showWarn)
            || (e.level >= blog::Info && !showInfo))
            continue;
        const std::string line = "[" + e.tag + "] " + e.text;
        if (!match(line)) continue;
        const ImVec4 col = e.level == blog::Err ? ImVec4{1.f, 0.45f, 0.4f, 1.f}
                         : e.level == blog::Warn ? ImVec4{1.f, 0.8f, 0.35f, 1.f} : ImVec4{0.82f, 0.85f, 0.9f, 1.f};
        ImGui::TextColored(col, "%7.1fs %s", e.time, line.c_str());
        if (copy) clip += line + "\n";
    }
    if (showLua)
        for (const std::string& s : g_lua.printLog) {
            if (!match(s)) continue;
            ImGui::TextColored({0.6f, 0.85f, 1.f, 1.f}, "   [Lua] %s", s.c_str());
            if (copy) clip += "[Lua] " + s + "\n";
        }
    if (follow && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 40.f) ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();
    if (copy) ImGui::SetClipboardText(clip.c_str());
}

static void drawDevConsoleMemoryTab() {
    plat::sysstats::update(ImGui::GetIO().DeltaTime);
    const plat::sysstats::Snapshot& s = plat::sysstats::snap();
    char a[32], b[32];
    plat::sysstats::formatMb(a, sizeof(a), s.memWorkingSetBytes);
    plat::sysstats::formatMb(b, sizeof(b), s.memPrivateBytes);
    ImGui::Text("Processo: %s in uso, %s privati, CPU %.0f%%", a, b, s.cpuProcessPct);
    ImGui::Text("Lua: %d KB", g_lua.memoryKb());
    if (s.vramKnown) {
        plat::sysstats::formatMb(a, sizeof(a), s.vramTotalBytes - s.vramFreeBytes);
        plat::sysstats::formatMb(b, sizeof(b), s.vramTotalBytes);
        ImGui::Text("GPU: %s / %s", a, b);
    }
    if (s.gpuQueried) ImGui::TextDisabled("%s", s.gpuRenderer);
    ImGui::Separator();
    ImGui::Text("Part: %d   Model: %d   Vincoli: %d   Mesh importate: %d",
                (int)g_world.parts.size(), (int)g_world.groups.size(), (int)g_world.welds.size(),
                (int)g_world.models.size());
    int scripts = 0;
    for (const auto& p : g_world.parts) scripts += p.script.empty() ? 0 : 1;
    ImGui::Text("Script nelle part: %d   Etichette UI: %d", scripts, (int)g_world.labels.size());
}

static void drawDevConsoleNetworkTab() {
    const char* role = g_net.role == NetRole::Host ? "Host" : g_net.role == NetRole::Client ? "Client" : "Offline";
    ImGui::Text("Ruolo: %s   Nome: %s", role, g_net.localName);
    if (g_net.jobId[0]) {
        ImGui::Text("Job ID: %s", g_net.jobId);
        ImGui::SameLine();
        if (ImGui::SmallButton("Copia##devjob")) ImGui::SetClipboardText(g_net.jobId);
    }
    ImGui::TextWrapped("Stato: %s", g_net.status);
    ImGui::Text("Giocatori: %d / %d", (int)g_net.remotes.size() + 1, g_net.maxPlayers);
    if (g_net.remotes.empty()) return;
    if (ImGui::BeginTable("##net", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Giocatore");
        ImGui::TableSetupColumn("Posizione");
        ImGui::TableSetupColumn("Distanza");
        ImGui::TableSetupColumn("Stato");
        ImGui::TableHeadersRow();
        for (const auto& r : g_net.remotes) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s", r.name);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f, %.0f, %.0f", r.pos.x, r.pos.y, r.pos.z);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", glm::length(r.pos - g_playerPos));
            ImGui::TableNextColumn();
            ImGui::Text("%s", r.airborne ? "in aria" : r.walking ? "cammina" : "fermo");
        }
        ImGui::EndTable();
    }
}

static void drawDevConsolePhysicsTab() {
    const bphys::Stats& s = g_bphys.stats();
    ImGui::Text("Profilo: %s   Passo: %.0f Hz", s.tier, g_engine.quality.physicsHz);
    ImGui::Text("Corpi: %d   Attivi: %d / %d (dormono %d)", s.bodies, s.active, g_bphys.budget().maxActive, s.sleeping);
    ImGui::Text("Congelati (LOD): %d   Lontani: %d   Statici: %d", s.frozen, s.culled, s.statics);
    ImGui::Text("Auto: %d   Vincoli attivi: %d", s.vehicles, g_bphys.jointCount());
    const plat::prof::PhaseStat& ph = plat::prof::state().phase[(int)plat::prof::Phase::Physics];
    ImGui::Text("Tempo fisica: %.2f ms per frame (picco %.2f)", ph.avgMs, ph.peakMs);
    if (g_screen == AppScreen::Studio && g_studioTesting)
        ImGui::Checkbox("Pausa fisica", &g_simPaused);
}

static void drawDevConsoleAudioTab() {
    if (!g_audioMixer.ready()) {
        ImGui::TextDisabled("Mixer spento: audio a voce singola.");
        return;
    }
    ImGui::Text("Suoni attivi: %d", g_audioMixer.activeVoices());
    float vol = g_audioMixer.masterVolume();
    if (ImGui::SliderFloat("Volume generale", &vol, 0.f, 1.f)) g_audioMixer.setMasterVolume(vol);
}

static void drawDevConsoleWindow() {
    if (!g_showDevConsole) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f},
                            ImGuiCond_FirstUseEver, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({720.f, 420.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Developer Console (F9)", &g_showDevConsole)) {
        ImGui::End();
        return;
    }
    if (ImGui::BeginTabBar("##dc")) {
        if (ImGui::BeginTabItem("Log")) { drawDevConsoleLogTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Memoria")) { drawDevConsoleMemoryTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Rete")) { drawDevConsoleNetworkTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Fisica")) { drawDevConsolePhysicsTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Script")) { drawScriptPerfTable(true); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Audio")) { drawDevConsoleAudioTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Server")) { drawServerControlDevTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

// ---- Cronologia versioni ---------------------------------------------------------
//
// Il salvataggio automatico tiene UN file e lo sovrascrive. Qui si tengono
// le ultime 30 versioni diverse del place, con data, per tornare indietro.

static constexpr size_t kMaxVersions = 30;
static uint64_t g_lastVersionHash = 0;

static std::string versionHistoryDir() {
    std::string name = g_world.placeName.empty() ? "place" : g_world.placeName;
    for (char& c : name)
        if (!std::isalnum((unsigned char)c) && c != '-' && c != '_') c = '_';
    return blockyDataDir() + "/history/" + name;
}

struct VersionFile {
    std::string path;
    std::string label;   // "2026-11-04 15:32:10"
    uintmax_t bytes = 0;
};

static std::vector<VersionFile> listVersions() {
    std::vector<VersionFile> out;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(versionHistoryDir(), ec)) {
        if (!e.is_regular_file(ec) || e.path().extension() != ".blocky") continue;
        const std::string stem = e.path().stem().string();   // 20261104-153210
        VersionFile v;
        v.path = e.path().string();
        v.bytes = e.file_size(ec);
        v.label = stem.size() == 15
            ? stem.substr(0, 4) + "-" + stem.substr(4, 2) + "-" + stem.substr(6, 2) + " "
              + stem.substr(9, 2) + ":" + stem.substr(11, 2) + ":" + stem.substr(13, 2)
            : stem;
        out.push_back(v);
    }
    std::sort(out.begin(), out.end(), [](const VersionFile& a, const VersionFile& b) { return a.path > b.path; });
    return out;
}

// Salva una versione se il place e' cambiato dall'ultima. Ritorna true se l'ha salvata.
static bool versionHistorySnapshot() {
    const std::string json = PlaceExporter::exportPlace(g_world, g_player.humanoid, g_playerPos,
                                                        g_playerYaw, &g_lighting);
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : json) { h ^= c; h *= 1099511628211ull; }
    if (h == g_lastVersionHash) return false;
    const std::string dir = versionHistoryDir();
    plat::makeDirs(dir);
    char stamp[32];
    const std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
    if (!PlaceExporter::saveToFile(dir + "/" + stamp + ".blocky", json)) return false;
    g_lastVersionHash = h;
    std::vector<VersionFile> all = listVersions();
    std::error_code ec;
    for (size_t k = kMaxVersions; k < all.size(); ++k) std::filesystem::remove(all[k].path, ec);
    return true;
}

// In Studio: una versione ogni 2 minuti, solo se qualcosa e' cambiato.
static void versionHistoryTick(float dt) {
    static float acc = 0.f;
    if (g_screen != AppScreen::Studio || g_studioTesting) return;
    acc += dt;
    const float every = (float)std::clamp<long long>(
        blk::clientsettings::getInt("FIntVersionHistorySeconds", 120), 30, 3600);
    if (acc < every) return;
    acc = 0.f;
    versionHistorySnapshot();
}

static void drawVersionHistoryWindow() {
    if (!g_showVersionHistory) return;
    ImGui::SetNextWindowSize({420.f, 360.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Cronologia versioni", &g_showVersionHistory)) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("Ogni 2 minuti, se il place e' cambiato, ne viene tenuta una copia (le ultime %d).",
                       (int)kMaxVersions);
    if (ImGui::Button("Salva versione adesso"))
        g_exportMsg = versionHistorySnapshot() ? "Versione salvata" : "Nessuna modifica dall'ultima versione";
    ImGui::SameLine();
    if (ImGui::Button("Apri cartella")) {
        plat::makeDirs(versionHistoryDir());
        plat::sys::revealFolder(versionHistoryDir());
    }
    ImGui::Separator();
    static std::string confirm;
    for (const VersionFile& v : listVersions()) {
        ImGui::PushID(v.path.c_str());
        ImGui::Text("%s   %.0f KB", v.label.c_str(), (double)v.bytes / 1024.0);
        ImGui::SameLine(300.f);
        if (ImGui::SmallButton("Ripristina")) confirm = v.path;
        ImGui::PopID();
    }
    if (!confirm.empty()) ImGui::OpenPopup("Ripristinare?");
    if (ImGui::BeginPopupModal("Ripristinare?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Il place attuale viene prima salvato come versione,\n"
                           "quindi puoi sempre tornare indietro.");
        if (ImGui::Button("Ripristina", {120, 0})) {
            versionHistorySnapshot();
            if (loadPlaceFromJson(confirm, g_world, &g_lighting)) {
                g_wsMeshes.clear();
                syncFriendsFromWorld();
                studioMultiClear();
                selectPart(g_world.parts.empty() ? -1 : 0);
                g_undo.clear();
                g_worldReplaced = true;
                markPlaceDirty();
                g_exportMsg = "Versione ripristinata";
            } else {
                g_exportMsg = "Versione non leggibile";
            }
            confirm.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Annulla", {120, 0})) {
            confirm.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::End();
}

// ---- Azioni dei bottoni ----------------------------------------------------------

// Play Here: il Test parte con il personaggio davanti alla camera, appoggiato
// sulla prima superficie sotto di lui, invece che allo SpawnLocation.
static void studioPlayHere(GLFWwindow* win) {
    const glm::vec3 f = g_cam.front();
    glm::vec3 flat(f.x, 0.f, f.z);
    flat = glm::dot(flat, flat) > 1e-4f ? glm::normalize(flat) : glm::vec3(0, 0, -1);
    const glm::vec3 at = g_cam.pos + flat * 6.f;
    float ground = 0.f;
    bool found = false;
    for (const WorkspacePart& p : g_world.parts) {
        if (!p.canCollide) continue;
        glm::vec3 mn, mx;
        partWorldAabb(p, mn, mx);
        if (at.x < mn.x || at.x > mx.x || at.z < mn.z || at.z > mx.z || mx.y > g_cam.pos.y) continue;
        if (!found || mx.y > ground) { ground = mx.y; found = true; }
    }
    startStudioTest(win);
    if (!g_studioTesting) return;
    g_playerPos = {at.x, ground + 0.05f, at.z};
    g_playerYaw = yawFromDir(flat);
    acOnLegitimateMove();
}

static void studioToggleMute() {
    g_audioMuted = !g_audioMuted;
    static float before = 1.f;
    if (g_audioMuted) {
        before = std::max(0.05f, g_audioMixer.masterVolume());
        g_audioMixer.setMasterVolume(0.f);
    } else {
        g_audioMixer.setMasterVolume(before);
    }
}

// Join Surfaces di Roblox: salda ogni part selezionata a quelle che tocca.
static void studioJoinSurfaces() {
    const std::vector<int> sel = studioActivePartIndices();
    if (sel.empty()) {
        g_exportMsg = "Join Surfaces: seleziona le part da saldare";
        return;
    }
    auto welded = [](int a, int b) {
        for (const auto& w : g_world.welds)
            if ((w.part0 == a && w.part1 == b) || (w.part0 == b && w.part1 == a)) return true;
        return false;
    };
    const float gap = 0.05f;
    int made = 0;
    for (int i : sel) {
        glm::vec3 amn, amx;
        partWorldAabb(g_world.parts[(size_t)i], amn, amx);
        for (int j = 0; j < (int)g_world.parts.size() && made < 500; ++j) {
            if (j == i || welded(i, j)) continue;
            glm::vec3 bmn, bmx;
            partWorldAabb(g_world.parts[(size_t)j], bmn, bmx);
            const bool touch = amn.x <= bmx.x + gap && amx.x >= bmn.x - gap && amn.y <= bmx.y + gap
                            && amx.y >= bmn.y - gap && amn.z <= bmx.z + gap && amx.z >= bmn.z - gap;
            if (touch && partconstraints::create(g_world, ConstraintKind::Weld, j, i) >= 0) ++made;
        }
    }
    if (made) markPlaceDirty();
    g_exportMsg = "Join Surfaces: " + std::to_string(made) + " saldature create";
}

// ---- Ogni frame -------------------------------------------------------------------

static void devToolsTick(float dt) {
    const ImGuiIO& io = ImGui::GetIO();
    // Fuori da Studio la console si puo spegnere dal server (FFlagDeveloperConsole).
    const bool consoleAllowed = g_screen == AppScreen::Studio || g_studioTesting
                             || blk::clientsettings::getBool("FFlagDeveloperConsole", true);
    if (!consoleAllowed) g_showDevConsole = false;
    else if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F9, false)) g_showDevConsole = !g_showDevConsole;
    static float roll = 0.f;
    roll += dt;
    if (roll >= 1.f) {
        roll = 0.f;
        g_lua.rollScriptCosts();
    }
    versionHistoryTick(dt);
    if (!g_studioTesting) g_simPaused = false;
}

static void drawStudioMeasureOverlay();
static void drawDevToolsWindows() {
    drawDevConsoleWindow();
    if (g_screen != AppScreen::Studio) return;
    drawScriptPerfWindow();
    drawWatchWindow();
    drawVersionHistoryWindow();
    drawStudioMeasureOverlay();
}

// ---- Bottoni della barra (ribbon) ---------------------------------------------------
// Azioni della barra dei comandi (drawStudioRibbonBody in StudioUiPanel.h).

static void studioDoUndo() {
    if (!g_undo.canUndo()) return;
    g_undo.applyUndo(g_world, g_selectedPart);
    studioFollowRebaseline();
    g_exportMsg = "Annullato";
}

static void studioDoRedo() {
    if (!g_undo.canRedo()) return;
    g_undo.applyRedo(g_world, g_selectedPart);
    studioFollowRebaseline();
    g_exportMsg = "Ripetuto";
}

/** Script del tipo scelto sulla part selezionata (Script = server, LocalScript, ModuleScript). */
static void studioAddScriptOfType(ScriptType type) {
    if (g_selectedPart < 0 || g_selectedPart >= (int)g_world.parts.size()) {
        g_exportMsg = "Seleziona prima una Part";
        return;
    }
    addScriptToSelected();
    g_world.parts[(size_t)g_selectedPart].scriptType = type;
    markPlaceDirty();
}

// Schermo intero (F11 di Roblox Studio). Su Android l'app e' gia' a tutto schermo.
static void studioToggleFullscreen(GLFWwindow* win) {
#ifndef __ANDROID__
    static int wx = 100, wy = 100, ww = 1280, wh = 720;
    if (!win) return;
    if (glfwGetWindowMonitor(win)) {
        glfwSetWindowMonitor(win, nullptr, wx, wy, ww, wh, 0);
    } else {
        glfwGetWindowPos(win, &wx, &wy);
        glfwGetWindowSize(win, &ww, &wh);
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = mon ? glfwGetVideoMode(mon) : nullptr;
        if (mode) glfwSetWindowMonitor(win, mon, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
#else
    (void)win;
#endif
}

// Screenshot: si chiede col pulsante, si scatta a fine frame (prima dello
// swap, cosi' c'e' l'immagine intera) in <dati>/screenshots/.
static bool g_screenshotRequested = false;
static void studioRequestScreenshot() { g_screenshotRequested = true; }

static void studioSaveScreenshotIfRequested(int w, int h) {
    if (!g_screenshotRequested || w <= 0 || h <= 0) return;
    g_screenshotRequested = false;
    std::vector<unsigned char> px((size_t)w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    std::vector<unsigned char> flipped(px.size());
    for (int y = 0; y < h; ++y)   // OpenGL parte dal basso
        std::memcpy(&flipped[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
    const std::string dir = blockyDataDir() + "/screenshots";
    plat::makeDirs(dir);
    char name[64];
    const std::time_t t = std::time(nullptr);
    std::strftime(name, sizeof(name), "blocky_%Y%m%d_%H%M%S.png", std::localtime(&t));
    const std::string path = dir + "/" + name;
    if (stbi_write_png(path.c_str(), w, h, 4, flipped.data(), w * 4))
        g_exportMsg = "Screenshot salvato: " + path;
    else
        g_exportMsg = "Screenshot non salvato";
}

// ---- Viste della camera (View Selector di Roblox Studio) -------------------------------
// 0 Sopra, 1 Davanti, 2 Dietro, 3 Sinistra, 4 Destra, 5 Isometrica: la camera
// guarda il centro della selezione (o il punto davanti) dalla direzione scelta.
static void studioViewFrom(int view) {
    glm::vec3 c = g_cam.pos + g_cam.front() * 20.f;
    float radius = 8.f;
    const std::vector<int> sel = studioActivePartIndices();
    if (!sel.empty()) {
        glm::vec3 lo(1e9f), hi(-1e9f);
        for (int i : sel) {
            const WorkspacePart& p = g_world.parts[(size_t)i];
            lo = glm::min(lo, p.position - p.size * 0.5f);
            hi = glm::max(hi, p.position + p.size * 0.5f);
        }
        c = (lo + hi) * 0.5f;
        radius = std::max(4.f, glm::length(hi - lo) * 0.5f);
    }
    static const float kYaw[6] = {-90.f, -90.f, 90.f, 0.f, 180.f, -45.f};
    static const float kPitch[6] = {-89.f, 0.f, 0.f, 0.f, 0.f, -35.f};
    static const char* kName[6] = {"Sopra", "Davanti", "Dietro", "Sinistra", "Destra", "Isometrica"};
    view = std::clamp(view, 0, 5);
    g_cam.yaw = kYaw[view];
    g_cam.pitch = kPitch[view];
    const float dist = radius / std::tan(glm::radians(g_cam.fov * 0.5f)) + radius;
    g_cam.pos = c - g_cam.front() * dist;
    g_exportMsg = std::string("Vista: ") + kName[view];
}

// ---- Selezione ------------------------------------------------------------------------

static void studioInvertSelection() {
    std::vector<char> was(g_world.parts.size(), 0);
    for (int i : studioActivePartIndices()) was[(size_t)i] = 1;
    studioMultiClear();
    for (int i = 0; i < (int)g_world.parts.size(); ++i)
        if (!was[(size_t)i] && !g_world.parts[(size_t)i].locked) studioMultiAdd(i);
    if (!g_multiParts.empty()) selectPart(g_multiParts.back());
    else selectPart(-1);
    g_exportMsg = "Selezione invertita: " + std::to_string(g_multiParts.size()) + " parti";
}

/** Tutte le parti col colore (o il materiale, o la misura) della part selezionata. */
static void studioSelectSimilar(int what) {
    if (!partIndexValid(g_selectedPart)) { g_exportMsg = "Seleziona prima una Part"; return; }
    const WorkspacePart ref = g_world.parts[(size_t)g_selectedPart];
    studioMultiClear();
    for (int i = 0; i < (int)g_world.parts.size(); ++i) {
        const WorkspacePart& p = g_world.parts[(size_t)i];
        const bool same = what == 0 ? glm::length(p.color - ref.color) < 0.02f
                        : what == 1 ? p.material == ref.material
                                    : glm::length(p.size - ref.size) < 0.01f;
        if (same) studioMultiAdd(i);
    }
    if (!g_multiParts.empty()) selectPart(g_multiParts.back());
    static const char* kWhat[3] = {"stesso colore", "stesso materiale", "stessa misura"};
    g_exportMsg = std::to_string(g_multiParts.size()) + " parti con " + kWhat[std::clamp(what, 0, 2)];
}

// ---- Metro: distanze nel viewport --------------------------------------------------------
// Con 2+ parti selezionate: linee tra i centri con la distanza in stud; con una
// sola, le sue misure. Come il righello dei plugin di Roblox Studio.
static bool g_showMeasure = false;

static void drawStudioMeasureOverlay() {
    if (!g_showMeasure || g_screen != AppScreen::Studio || g_studioTesting) return;
    const std::vector<int> sel = studioActivePartIndices();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    auto label = [&](float x, float y, const char* txt) {
        const ImVec2 ts = ImGui::CalcTextSize(txt);
        dl->AddRectFilled({x - ts.x * 0.5f - 4, y - ts.y * 0.5f - 2}, {x + ts.x * 0.5f + 4, y + ts.y * 0.5f + 2},
                          IM_COL32(20, 24, 32, 220), 4.f);
        dl->AddText({x - ts.x * 0.5f, y - ts.y * 0.5f}, IM_COL32(255, 220, 90, 255), txt);
    };
    char buf[96];
    if (sel.size() == 1) {
        const WorkspacePart& p = g_world.parts[(size_t)sel[0]];
        float x, y;
        if (worldToScreen(p.position + glm::vec3(0, p.size.y * 0.5f + 0.6f, 0), x, y)) {
            std::snprintf(buf, sizeof(buf), "%.2f x %.2f x %.2f stud", p.size.x, p.size.y, p.size.z);
            label(x, y, buf);
        }
        return;
    }
    const size_t n = std::min<size_t>(sel.size(), 8);   // oltre diventa illeggibile
    for (size_t k = 1; k < n; ++k) {
        const glm::vec3 a = g_world.parts[(size_t)sel[k - 1]].position, b = g_world.parts[(size_t)sel[k]].position;
        float ax, ay, bx, by;
        if (!worldToScreen(a, ax, ay) || !worldToScreen(b, bx, by)) continue;
        dl->AddLine({ax, ay}, {bx, by}, IM_COL32(255, 220, 90, 230), 2.f);
        dl->AddCircleFilled({ax, ay}, 3.5f, IM_COL32(255, 220, 90, 255));
        dl->AddCircleFilled({bx, by}, 3.5f, IM_COL32(255, 220, 90, 255));
        const glm::vec3 d = b - a;
        std::snprintf(buf, sizeof(buf), "%.2f stud  (X %.1f  Y %.1f  Z %.1f)", glm::length(d), d.x, d.y, d.z);
        label((ax + bx) * 0.5f, (ay + by) * 0.5f, buf);
    }
}

// ---- Ora del giorno (Lighting.ClockTime) ------------------------------------------------
static void studioClockTimeSlider() {
    float t = g_lighting.clockTime;
    ImGui::SetNextItemWidth(160);
    if (ImGui::SliderFloat("##clock", &t, 0.f, 24.f, "%.1f h")) {
        g_lighting.clockTime = t;
        markPlaceDirty();
    }
}
