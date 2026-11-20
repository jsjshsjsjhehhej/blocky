#pragma once

/**
 * Prelude Luau di Blocky: le API Roblox scritte in Luau invece che in C++,
 * caricate prima degli script del gioco. Il codice sta in src/engine/lua/:
 *
 *   prelude.luau     la base (Instance Luau, CFrame, segnali, strato "API presente"...)
 *   api/*.luau       un file per API (es. api/rete.luau): aggiungere un'API Luau
 *                    = aggiungere un file. Vedi src/engine/lua/README.md.
 *   core/*.luau      le CoreScripts: l'interfaccia di base (barra della vita...)
 *
 * Da dove si leggono:
 *   - in sviluppo (programma avviato da .../build/bin, o variabile BLOCKY_LUA_DIR)
 *     dal disco, a ogni avvio di uno stato Lua (ogni Play): si modifica il .luau,
 *     si preme Play, si prova. Niente ricompilazione.
 *   - altrimenti (release, APK) dalla copia incorporata nel programma, che CMake
 *     genera da quei file (cmake/BlockyEmbedLua.cmake -> blocky_lua_embedded.h).
 */

#include "blocky_lua_embedded.h"
#include "platform/Platform.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#if !defined(__ANDROID__)
#include <filesystem>
#endif

namespace blk {
namespace luaprelude {

struct File {
    std::string name;   // "prelude.luau", "api/rete.luau"
    std::string code;
};

inline std::string readWhole(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/** La cartella src/engine/lua da cui leggere in sviluppo, o "" (usa la copia incorporata). */
inline std::string devDir() {
#if defined(__ANDROID__)
    return {};
#else
    static const std::string dir = [] {
        if (const char* env = std::getenv("BLOCKY_LUA_DIR"); env && *env) return std::string(env);
        // Avviato da <repo>/build/bin (o build-pc/bin): il sorgente e' due cartelle su.
        std::string self = plat::selfDir();
        std::replace(self.begin(), self.end(), '\\', '/');
        while (!self.empty() && self.back() == '/') self.pop_back();
        const bool fromBuild = self.size() > 4 && self.compare(self.size() - 4, 4, "/bin") == 0
            && self.find("/build") != std::string::npos;
        if (!fromBuild) return std::string();
        const std::string cand = self + "/../../src/engine/lua";
        std::ifstream probe(cand + "/prelude.luau");
        return probe ? cand : std::string();
    }();
    return dir;
#endif
}

/** Tutti i file Luau del motore: prelude.luau e api/*.luau (in ordine di nome). */
inline std::vector<File> files() {
    std::vector<File> out;
#if !defined(__ANDROID__)
    const std::string dir = devDir();
    if (!dir.empty()) {
        static bool detto = false;
        if (!detto) {
            detto = true;
            std::fprintf(stderr, "[Lua] API Luau lette dal disco (sviluppo): %s\n", dir.c_str());
        }
        out.push_back({"prelude.luau", readWhole(dir + "/prelude.luau")});
        for (const char* sub : {"api", "core"}) {
            std::vector<std::string> names;
            std::error_code ec;
            for (const auto& e : std::filesystem::directory_iterator(dir + "/" + sub, ec))
                if (e.is_regular_file() && e.path().extension() == ".luau") names.push_back(e.path().filename().string());
            std::sort(names.begin(), names.end());
            for (const auto& n : names) out.push_back({std::string(sub) + "/" + n, readWhole(dir + "/" + sub + "/" + n)});
        }
        if (!out.front().code.empty()) return out;
        out.clear();   // file illeggibile: meglio la copia incorporata
    }
#endif
    int n = 0;
    const luaembedded::File* f = luaembedded::files(n);
    for (int i = 0; i < n; ++i) out.push_back({f[i].name, f[i].code});
    return out;
}

/** Il prelude (prelude.luau). */
inline std::string source() {
    for (auto& f : files())
        if (f.name == "prelude.luau") return f.code;
    return {};
}

} // namespace luaprelude
} // namespace blk
