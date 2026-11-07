#pragma once

/**
 * Il catalogo giochi online (backend/, rotte in backend/src/games.ts):
 * dove sta e con che token si pubblica.
 *
 *   indirizzo  BLOCKY_GAMES_URL, poi <dati>/cloud_url.txt, poi kGamesUrl
 *              (vuoto finche' il server non e' online: allora niente catalogo)
 *   token      BLOCKY_PUBLISH_TOKEN, poi <dati>/publish_token.txt
 *              (lo da' l'admin del backend: POST /v1/admin/publishers)
 *
 * Il token non va mai nei log, nei pacchetti dei giochi o nei messaggi.
 */

#include "engine/GameHub.h"   // blockyDataDir, readTextFile

#include <cstdlib>
#include <string>

namespace blk {
namespace cloud {

// L'indirizzo del server Blocky online, quando c'e' (es. "https://api.blocky.example").
inline const char* kGamesUrl = "";

inline std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) ++a;
    return s.substr(a);
}

inline std::string catalogUrl() {
    if (const char* env = std::getenv("BLOCKY_GAMES_URL"))
        if (env[0]) return trimmed(env);
    const std::string fromFile = trimmed(readTextFile(blockyDataDir() + "/cloud_url.txt"));
    if (!fromFile.empty()) return fromFile;
    return kGamesUrl;
}

inline std::string publishToken() {
    if (const char* env = std::getenv("BLOCKY_PUBLISH_TOKEN"))
        if (env[0]) return trimmed(env);
    return trimmed(readTextFile(blockyDataDir() + "/publish_token.txt"));
}

} // namespace cloud
} // namespace blk
