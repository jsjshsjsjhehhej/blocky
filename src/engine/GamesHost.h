#pragma once

#ifdef _WIN32

/**
 * Serve i giochi pubblicati su QUESTO PC, in rete locale.
 *
 * Prima bisognava avviare blocky_hostd e premere "Sincronizza". Adesso
 * Blocky stesso, mentre e' aperto, risponde a GET /games dalla cartella
 * dove pubblica gia' (APPDATA/Blocky/games). Il telefono li vede da solo.
 */

#include "engine/BlockyProject.h"
#include "engine/HttpMini.h"
#include "engine/GameHub.h"
#include "platform/Log.h"

#include <atomic>
#include <cctype>
#include <filesystem>
#include <initializer_list>
#include <sstream>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

namespace blk {
namespace gameshost {

namespace fs = std::filesystem;

inline std::atomic<bool>& running() { static std::atomic<bool> b{false}; return b; }
inline std::atomic<int>& boundPort() { static std::atomic<int> p{0}; return p; }
inline SOCKET& listenSock() { static SOCKET s = INVALID_SOCKET; return s; }

inline int port() { return boundPort().load(); }

inline bool safeId(const std::string& id) {
    if (id.empty() || id.size() > 128) return false;
    for (char c : id) {
        if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-')) return false;
    }
    return true;
}

inline std::string jsonEsc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else if ((unsigned char)c < 0x20) continue;
        else o += c;
    }
    return o;
}

inline std::string jsonStr(const std::string& json, const char* key) {
    const std::string pat = std::string("\"") + key + "\"";
    size_t k = json.find(pat);
    if (k == std::string::npos) return {};
    k = json.find(':', k + pat.size());
    if (k == std::string::npos) return {};
    const size_t open = json.find('"', k);
    if (open == std::string::npos) return {};
    std::string out;
    for (size_t i = open + 1; i < json.size(); ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) { out += json[i + 1]; ++i; continue; }
        if (json[i] == '"') break;
        out += json[i];
    }
    return out;
}

inline std::string findNamed(const fs::path& dir, std::initializer_list<const char*> names) {
    std::error_code ec;
    for (const char* n : names) {
        if (fs::exists(dir / n, ec)) return n;
    }
    return {};
}

inline std::string gamesJson() {
    std::error_code ec;
    std::ostringstream j;
    j << "[";
    bool first = true;
    const fs::path root = BlockyProject::gamesInstallDir();
    if (fs::exists(root, ec)) {
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (!entry.is_directory(ec)) continue;
            const std::string name = entry.path().filename().string();
            if (name.empty() || name[0] == '.') continue;

            const std::string info = readTextFile((entry.path() / "info.json").string());
            const std::string pkg = readTextFile((entry.path() / "game.blocky.json").string());
            const std::string index = readTextFile((entry.path() / "game_index.json").string());
            if (info.empty() && pkg.empty() && index.empty()) continue;
            if (!fs::exists(entry.path() / "place.json", ec)
                && findNamed(entry.path(), {"place.blockyplace", "place.blocky"}).empty())
                continue;

            // Pack mesh nel pacchetto PRIMA che il telefono scarichi place.json
            // (altrimenti restano path assoluti del PC).
            BlockyProject::ensurePackageAssets(entry.path().string());

            // L'id HTTP e' il nome cartella: cosi' GET /games/<id>/place.json
            // trova sempre i file (publish scrive in games/<id>/).
            const std::string id = name;

            std::string title = jsonStr(info, "title");
            if (title.empty()) title = jsonStr(info, "name");
            if (title.empty()) title = jsonStr(pkg, "title");
            if (title.empty()) title = id;

            std::string desc = jsonStr(info, "description");
            if (desc.empty()) desc = jsonStr(pkg, "description");
            if (desc.empty()) {
                const std::string cr = jsonStr(pkg, "creator");
                desc = cr.empty() ? "Pubblicato su questo PC" : ("di " + cr);
            }

            std::string thumb = jsonStr(info, "thumbnail");
            if (thumb.empty())
                thumb = findNamed(entry.path(), {"thumbnail.png", "thumbnail.jpg", "icon.png"});

            std::string place = jsonStr(info, "place");
            if (place.empty())
                place = findNamed(entry.path(), {"place.json", "place.blockyplace", "place.blocky"});
            if (place.empty()) place = "place.json";

            if (!first) j << ",";
            first = false;
            j << "{\"id\":\"" << jsonEsc(id) << "\""
              << ",\"title\":\"" << jsonEsc(title) << "\""
              << ",\"description\":\"" << jsonEsc(desc) << "\""
              << ",\"thumbnail\":\"" << jsonEsc(thumb) << "\""
              << ",\"place\":\"" << jsonEsc(place) << "\"}";
        }
    }
    j << "]";
    return j.str();
}

inline bool safeRelFile(const std::string& rel) {
    if (rel.empty() || rel.size() > 240) return false;
    if (rel.find("..") != std::string::npos) return false;
    for (char c : rel) {
        if (c == ':' || c == '\\' || (unsigned char)c < 32) return false;
    }
    return true;
}

inline void handleClient(SOCKET client) {
    HttpMini::Request req;
    std::string err;
    if (!HttpMini::parseRequest(client, req, err, 8ull * 1024 * 1024)) {
        HttpMini::sendText(client, 400, "Bad Request", err);
        return;
    }

    if (req.method == "GET" && (req.path == "/" || req.path == "/status")) {
        HttpMini::sendText(client, 200, "OK", "blocky games host\n");
        return;
    }
    if (req.method == "GET" && req.path == "/games") {
        HttpMini::sendJson(client, 200, "OK", gamesJson());
        return;
    }
    if (req.method == "GET" && req.path.rfind("/games/", 0) == 0) {
        const std::string rest = req.path.substr(7);
        const size_t slash = rest.find('/');
        const std::string id = slash == std::string::npos ? rest : rest.substr(0, slash);
        const std::string file = slash == std::string::npos ? "place.json" : rest.substr(slash + 1);
        if (!safeId(id)) {
            HttpMini::sendText(client, 400, "Bad Request", "id non valido");
            return;
        }
        const fs::path gameDir = fs::path(BlockyProject::gamesInstallDir()) / id;
        // Elenco di TUTTI i file del pacchetto (mesh incluse): il telefono
        // scarica place-first, poi questo elenco.
        if (file == "files" || file == "files.json") {
            BlockyProject::ensurePackageAssets(gameDir.string());
            const std::string manifest = BlockyProject::writeFilesManifest(gameDir.string());
            HttpMini::sendJson(client, 200, "OK", manifest);
            return;
        }
        if (!safeRelFile(file)) {
            HttpMini::sendText(client, 400, "Bad Request", "percorso non valido");
            return;
        }
        // Prima di servire il place: assicurati che i path siano relativi.
        if (file == "place.json" || file == "place.blocky" || file == "place.blockyplace")
            BlockyProject::ensurePackageAssets(gameDir.string());
        const fs::path path = gameDir / file;
        std::error_code ec;
        if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) {
            HttpMini::sendText(client, 404, "Not Found", "file non trovato");
            return;
        }
        if (!HttpMini::sendFile(client, path.string()))
            HttpMini::sendText(client, 500, "Internal Server Error", "lettura fallita");
        return;
    }
    HttpMini::sendText(client, 404, "Not Found", "rotta sconosciuta");
}

inline bool tryListen(int port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((u_short)port);
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(s);
        return false;
    }
    if (listen(s, 8) != 0) {
        closesocket(s);
        return false;
    }
    DWORD tv = 500;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    listenSock() = s;
    boundPort().store(port);
    return true;
}

inline void acceptLoop() {
    while (running().load()) {
        SOCKET ls = listenSock();
        if (ls == INVALID_SOCKET) break;
        sockaddr_in from{};
        int fromLen = sizeof(from);
        SOCKET client = accept(ls, (sockaddr*)&from, &fromLen);
        if (client == INVALID_SOCKET) continue;
        handleClient(client);
        closesocket(client);
    }
}

inline bool start() {
    if (!netEnsureSockets()) return false;
    bool expected = false;
    if (!running().compare_exchange_strong(expected, true))
        return boundPort().load() > 0;

    const int candidates[] = {8080, 18080, 27080};
    bool ok = false;
    for (int p : candidates) {
        if (tryListen(p)) { ok = true; break; }
    }
    if (!ok) {
        running().store(false);
        LOG_INFO("games", "host locale: nessuna porta libera");
        return false;
    }
    std::thread(acceptLoop).detach();
    LOG_INFO("games", "host locale sui giochi pubblicati, porta %d", boundPort().load());
    return true;
}

inline void stop() {
    running().store(false);
    SOCKET s = listenSock();
    listenSock() = INVALID_SOCKET;
    if (s != INVALID_SOCKET) closesocket(s);
    boundPort().store(0);
}

} // namespace gameshost
} // namespace blk

#endif
