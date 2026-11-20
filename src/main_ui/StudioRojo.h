#pragma once

/**
 * Rojo nello Studio: collega un progetto (default.project.json), tiene gli
 * script allineati ai file ogni mezzo secondo, esporta il place come progetto.
 *
 * Il lavoro vero e' in engine/RojoProject.h. Qui: menu, messaggi in Output,
 * ultimo progetto ricordato, e BLOCKY_ROJO=<default.project.json> per
 * collegarlo da solo all'avvio (comodo per chi sviluppa con VS Code o un'AI).
 *
 * Durante il Test non si sincronizza: i cambiamenti arrivano quando si torna
 * allo Studio e partono al Play successivo, come con Rojo in Roblox Studio.
 */

#include "engine/RojoProject.h"
#include "platform/FilePick.h"

#include <cstdlib>
#include <fstream>

static blk::rojo::Watcher g_rojo;
static float g_rojoPollAcc = 0.f;
static std::string g_rojoStatus = "Nessun progetto Rojo collegato";

static void rojoLog(const std::string& line) {
    g_lua.printLog.push_back("[Rojo] " + line);
    ++g_lua.printCount;
    std::printf("[Rojo] %s\n", line.c_str());
}

static std::string rojoLastPathFile() { return blockyDataDir() + "/rojo_ultimo_progetto.txt"; }

static std::string rojoLastPath() {
    std::ifstream f(rojoLastPathFile());
    std::string p;
    std::getline(f, p);
    return p;
}

static bool studioRojoConnect(const std::string& projectFile) {
    std::string err;
    if (!g_rojo.connect(projectFile, g_world, err)) {
        g_rojoStatus = "Rojo: " + err;
        rojoLog("non collegato: " + err);
        return false;
    }
    const auto& p = g_rojo.project();
    int scripts = 0;
    for (const auto& e : p.entries) scripts += !e.file.empty();
    g_rojoStatus = "Rojo: \"" + p.name + "\" collegato (" + std::to_string(scripts) + " script)";
    rojoLog("collegato \"" + p.name + "\": " + std::to_string(p.entries.size()) + " instance, "
            + std::to_string(scripts) + " script da " + p.dir);
    for (const auto& w : p.warnings) rojoLog("avviso: " + w);
    for (const auto& e : g_rojo.errors()) rojoLog("errore: " + e);
    std::ofstream(rojoLastPathFile()) << projectFile << "\n";
    markPlaceDirty();
    return true;
}

static void studioRojoDisconnect() {
    if (!g_rojo.connected()) return;
    const std::string name = g_rojo.project().name;
    // Gli script restano nel place (si salvano col place): si smette solo di aggiornarli.
    g_rojo.disconnect(nullptr);
    g_rojoStatus = "Nessun progetto Rojo collegato";
    rojoLog("scollegato \"" + name + "\" (gli script restano nel place)");
}

static void studioRojoExport() {
    const std::string name = g_world.placeName.empty() ? std::string("place") : g_world.placeName;
    std::string folder;
    for (char c : name) folder += (std::isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
    const std::string dir = blockyDataDir() + "/rojo/" + folder;
    std::string err;
    const int n = blk::rojo::exportPlace(g_world, dir, name, err);
    if (n < 0) { rojoLog("esportazione non riuscita: " + err); return; }
    rojoLog("esportati " + std::to_string(n) + " script in " + dir + " (apri default.project.json e collegalo)");
    g_rojoStatus = "Rojo: esportato in " + dir;
}

/** Ogni frame dello Studio: collegamento automatico all'avvio e controllo dei file. */
static void studioRojoTick(float dt) {
    static bool autoTried = false;
    if (!autoTried) {
        autoTried = true;
        // (gia' collegato dalla prova automatica BLOCKY_SHOT: non una seconda volta)
        if (const char* env = std::getenv("BLOCKY_ROJO"); env && *env && !g_rojo.connected()) studioRojoConnect(env);
    }
    if (!g_rojo.connected() || g_studioTesting) return;
    g_rojoPollAcc += dt;
    if (g_rojoPollAcc < 0.5f) return;
    g_rojoPollAcc = 0.f;
    std::vector<std::string> changed;
    const bool structure = g_rojo.poll(g_world, changed);
    if (changed.empty()) return;
    if (structure) {
        rojoLog("struttura del progetto cambiata: place aggiornato (" + std::to_string(g_rojo.project().entries.size()) + " instance)");
        for (const auto& w : g_rojo.project().warnings) rojoLog("avviso: " + w);
    } else {
        for (const auto& n : changed) rojoLog("aggiornato " + n);
    }
    for (const auto& e : g_rojo.errors()) rojoLog("errore: " + e);
    markPlaceDirty();
}

/** Voci del menu Script. */
static void drawStudioRojoMenu() {
    if (!ImGui::BeginMenu("Rojo (VS Code / AI)")) return;
    ImGui::TextDisabled("%s", g_rojoStatus.c_str());
    ImGui::Separator();
    if (ImGui::MenuItem("Collega progetto (default.project.json)...")) {
        plat::requestFile("Progetto Rojo (default.project.json)", "application/json",
                          [](const std::string& path) { if (!path.empty()) studioRojoConnect(path); });
    }
    const std::string last = rojoLastPath();
    if (!last.empty() && ImGui::MenuItem(("Ricollega " + last).c_str(), nullptr, false, !g_rojo.connected()))
        studioRojoConnect(last);
    if (ImGui::MenuItem("Scollega", nullptr, false, g_rojo.connected()))
        studioRojoDisconnect();
    ImGui::Separator();
    if (ImGui::MenuItem("Esporta gli script del place come progetto Rojo"))
        studioRojoExport();
    ImGui::EndMenu();
}
