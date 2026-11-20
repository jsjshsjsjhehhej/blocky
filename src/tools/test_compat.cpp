// Banco di prova della compatibilita' con gli script Roblox.
//
// Ogni file in tests/compat/ e' un pezzo di gioco scritto come su Roblox: si
// controlla da solo con assert e alla fine scrive COMPAT_OK = true (anche
// dopo dei task.wait). Qui ognuno gira in un mondo pulito con 2 secondi di
// gioco simulato; alla fine: quanti funzionano e, per gli altri, l'errore
// (di solito l'API che manca). E' il numero da far salire verso il 100%.
//
// Uso: test_compat [cartella]   (default: tests/compat del sorgente)
//      test_compat --min N       fallisce se passano meno di N script (default: tutti)

#include "engine/DataModel.h"
#include "engine/GameHub.h"
#include "engine/LuaRuntime.h"
#include "systems/Terrain.h"
#include "systems/TerrainLua.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef BLOCKY_SOURCE_DIR
#define BLOCKY_SOURCE_DIR "."
#endif

int main(int argc, char** argv) {
    std::string dir = std::string(BLOCKY_SOURCE_DIR) + "/tests/compat";
    int minPass = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--min") == 0 && i + 1 < argc) minPass = std::atoi(argv[++i]);
        else dir = argv[i];
    }

    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec))
        if (e.path().extension() == ".lua") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        std::printf("[compat] nessuno script in %s\n", dir.c_str());
        return 1;
    }

    DataModel dm;
    LuaRuntime lua;
    // Gli stessi servizi che il gioco collega a Lua (bindLuaServices), tranne
    // quelli che vogliono la grafica: senza, gli script falliscono per colpa
    // del banco di prova e non della compatibilita'.
    SoundService sounds;
    LightingService lighting;
    Animator animator;
    LanNet net;
    Humanoid hum = Humanoid::createR6Blocky();
    glm::vec3 playerPos{0.f, 3.f, 0.f};
    float playerYaw = 0.f;
    NavGrid nav;
    Tag tags;
    Classifica classifica;
    Prossimita proximity;
    lua.soundService = &sounds;
    lua.lighting = &lighting;
    lua.animator = &animator;
    lua.net = &net;
    lua.playerHumanoid = &hum;
    lua.playerPos = &playerPos;
    lua.playerYaw = &playerYaw;
    lua.nav = &nav;
    lua.tags = &tags;
    lua.classifica = &classifica;
    lua.proximity = &proximity;
    lua.runIsClient = true;
    lua.runIsServer = true;
    Terrain terrain;
    lua.extensions.push_back({"TerrainLuaInternal", &terrainlua::registerLua, &terrain});
    int pass = 0;
    std::vector<std::string> failures;
    for (const auto& f : files) {
        std::ifstream in(f, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        dm.clear();
        dm.loadBaseplatePlace();
        if (!lua.init(&dm)) {
            failures.push_back(f.filename().string() + ": init Lua fallita");
            continue;
        }
        lua.lastError.clear();
        const bool ran = lua.runString(ss.str());
        std::string err = ran ? std::string() : lua.lastError;
        for (int i = 0; i < 120; ++i) lua.fireHeartbeat(1.f / 60.f);
        const bool ok = ran && lua.evalWatch("COMPAT_OK") == "true";
        if (ok) {
            ++pass;
            std::printf("[compat] OK    %s\n", f.filename().string().c_str());
        } else {
            if (err.empty()) err = lua.lastError.empty() ? "non ha finito (COMPAT_OK mai scritto)" : lua.lastError;
            std::printf("[compat] FAIL  %s\n        %s\n", f.filename().string().c_str(), err.c_str());
            failures.push_back(f.filename().string());
        }
    }
    const int total = (int)files.size();
    std::printf("[compat] %d / %d script funzionano (%.0f%%)\n", pass, total, 100.0 * pass / total);
    // Di default devono passare tutti: uno che smette di funzionare e' una regressione.
    if (minPass < 0) minPass = total;
    if (pass < minPass) {
        std::printf("[compat] sotto il minimo richiesto (%d)\n", minPass);
        return 1;
    }
    return 0;
}
