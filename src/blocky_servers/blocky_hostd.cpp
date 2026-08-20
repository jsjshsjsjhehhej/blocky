/**
 * blocky_hostd — server di hosting per i giochi pubblicati.
 *
 *   blocky_hostd.exe [--port 8080] [--root ./hosted]
 *
 * Due responsabilità, non di più:
 *   - GET  serve i file statici di un gioco già pubblicato (game_index.json,
 *     place.json, assets/...)
 *   - PUT /publish/<id>  riceve un .zip, lo estrae, lo valida con
 *     GamePublish e lo rende visibile solo se passa l'audit
 *
 * Non esegue Lua, non simula fisica, non sincronizza giocatori — quello resta
 * lavoro di BlockyServer.exe. Questo è un magazzino con un controllo qualità
 * all'ingresso, non un motore di gioco.
 *
 * Solo LAN/uso da studio: niente TLS, thread per connessione senza limite di
 * concorrenza. Non va esposto direttamente su internet senza un reverse
 * proxy davanti.
 */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "engine/GamePublish.h"
#include "engine/HttpMini.h"

namespace fs = std::filesystem;

namespace {

std::string g_root = "./hosted";
int g_port = 8080;
std::mutex g_publishMutex;   // un publish alla volta: e' un'operazione rara, non serve di piu'

/** True se `id` è sicuro da usare come nome di cartella diretto. */
bool safeId(const std::string& id) {
    if (id.empty() || id.size() > 128) return false;
    for (char c : id) {
        if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-')) return false;
    }
    return true;
}

/**
 * Risolve `relPath` dentro `root` e verifica che il risultato resti sotto
 * `root` una volta canonicalizzato. Un ".." nell'URL o un symlink non deve
 * poter far uscire dalla cartella ospitata.
 */
bool safeJoin(const std::string& root, const std::string& relPath, fs::path& outPath) {
    std::error_code ec;
    const fs::path rootCanon = fs::weakly_canonical(root, ec);
    if (ec) return false;

    fs::path candidate = fs::path(root) / relPath;
    const fs::path candidateCanon = fs::weakly_canonical(candidate, ec);
    if (ec) return false;

    const std::string rootStr = rootCanon.generic_string();
    const std::string candStr = candidateCanon.generic_string();
    if (candStr.size() < rootStr.size()) return false;
    if (candStr.compare(0, rootStr.size(), rootStr) != 0) return false;
    // Deve essere un confine di cartella vero, non "rootX" che combacia come prefisso di "root".
    if (candStr.size() > rootStr.size() && candStr[rootStr.size()] != '/') return false;

    outPath = candidateCanon;
    return true;
}

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

/** GET / — stato in testo semplice, utile con un browser o curl a mano. */
void handleRoot(SOCKET s) {
    std::error_code ec;
    int count = 0;
    if (fs::exists(g_root, ec)) {
        for (const auto& e : fs::directory_iterator(g_root, ec)) {
            if (e.is_directory(ec) && e.path().filename().string()[0] != '.') ++count;
        }
    }
    std::ostringstream body;
    body << "blocky_hostd attivo\n"
         << "porta        " << g_port << "\n"
         << "cartella     " << fs::absolute(g_root, ec).string() << "\n"
         << "giochi ospitati  " << count << "\n\n"
         << "GET  /games                    elenco in JSON\n"
         << "GET  /games/<id>/game_index.json\n"
         << "PUT  /publish/<id>             body = archivio .zip\n";
    HttpMini::sendText(s, 200, "OK", body.str());
}

/** GET /games — catalogo: legge il game_index.json di ognuno, non li ricostruisce. */
void handleCatalog(SOCKET s) {
    std::error_code ec;
    std::ostringstream j;
    j << "[";
    bool first = true;

    if (fs::exists(g_root, ec)) {
        for (const auto& entry : fs::directory_iterator(g_root, ec)) {
            if (!entry.is_directory(ec)) continue;
            const std::string name = entry.path().filename().string();
            if (name.empty() || name[0] == '.') continue;   // salta .staging, .old_*

            const fs::path idxPath = entry.path() / "game_index.json";
            if (!fs::exists(idxPath, ec)) continue;

            const std::string content = GamePublish::readTextFile(idxPath.string());
            if (content.empty()) continue;

            if (!first) j << ",";
            first = false;
            // Non ri-parsiamo il JSON già scritto da GameIndex: lo passiamo
            // così com'è, come oggetto della lista.
            j << content;
        }
    }
    j << "]";
    HttpMini::sendJson(s, 200, "OK", j.str());
}

/** GET /games/<id>/<resto...> — file statico di un gioco pubblicato. */
void handleGameFile(SOCKET s, const std::string& path) {
    // path è già senza il prefisso "/games/"
    const size_t slash = path.find('/');
    const std::string id = slash == std::string::npos ? path : path.substr(0, slash);
    const std::string rest = slash == std::string::npos ? "" : path.substr(slash + 1);

    if (!safeId(id)) {
        HttpMini::sendText(s, 400, "Bad Request", "id non valido");
        return;
    }

    const fs::path gameDir = fs::path(g_root) / id;
    std::error_code ec;
    if (!fs::exists(gameDir, ec)) {
        HttpMini::sendText(s, 404, "Not Found", "gioco non trovato: " + id);
        return;
    }

    const std::string relFile = rest.empty() ? "game_index.json" : rest;
    fs::path filePath;
    if (!safeJoin(gameDir.string(), relFile, filePath)) {
        HttpMini::sendText(s, 400, "Bad Request", "percorso non valido");
        return;
    }

    if (!fs::exists(filePath, ec) || !fs::is_regular_file(filePath, ec)) {
        HttpMini::sendText(s, 404, "Not Found", "file non trovato: " + relFile);
        return;
    }

    if (!HttpMini::sendFile(s, filePath.string())) {
        HttpMini::sendText(s, 500, "Internal Server Error", "invio del file fallito");
    }
}

/** PUT /publish/<id> — riceve un .zip, valida, pubblica solo se pulito. */
void handlePublish(SOCKET s, const std::string& id, const std::vector<std::uint8_t>& body) {
    if (!safeId(id)) {
        HttpMini::sendJson(s, 400, "Bad Request", "{\"ok\":false,\"error\":\"id non valido\"}");
        return;
    }
    if (body.empty()) {
        HttpMini::sendJson(s, 400, "Bad Request", "{\"ok\":false,\"error\":\"body vuoto\"}");
        return;
    }

    std::lock_guard<std::mutex> lock(g_publishMutex);
    const GamePublish::PublishResult r = GamePublish::publishZipBytes(body, id, g_root);

    std::ostringstream j;
    j << "{\"ok\":" << (r.ok ? "true" : "false")
      << ",\"gameId\":\"" << jsonEscape(r.gameId) << "\""
      << ",\"message\":\"" << jsonEscape(r.message) << "\"";
    if (r.ok) {
        j << ",\"contentHash\":\"" << r.build.index.contentHash << "\""
          << ",\"assets\":" << r.build.index.assets.size()
          << ",\"colliders\":" << r.build.index.colliders.size();
    }
    j << "}";

    HttpMini::sendJson(s, r.ok ? 200 : 422, r.ok ? "OK" : "Unprocessable Entity", j.str());

    std::printf("[publish] %s -> %s%s\n", r.gameId.c_str(), r.ok ? "ok" : "rifiutato: ",
               r.ok ? "" : r.message.c_str());
}

void handleConnection(SOCKET client) {
    // Timeout: un client lento o silenzioso non deve bloccare il thread per
    // sempre. 20s è generoso per un upload di un pacchetto di gioco su LAN.
    const DWORD timeoutMs = 20000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

    HttpMini::Request req;
    std::string err;
    if (!HttpMini::parseRequest(client, req, err)) {
        // Niente risposta: se la connessione è già a pezzi, provarci
        // peggiorerebbe soltanto le cose.
        closesocket(client);
        return;
    }

    if (req.method == "GET" && req.path == "/") {
        handleRoot(client);
    } else if (req.method == "GET" && req.path == "/games") {
        handleCatalog(client);
    } else if (req.method == "GET" && req.path.rfind("/games/", 0) == 0) {
        handleGameFile(client, req.path.substr(7));
    } else if ((req.method == "PUT" || req.method == "POST") &&
              req.path.rfind("/publish/", 0) == 0) {
        handlePublish(client, req.path.substr(9), req.body);
    } else {
        HttpMini::sendText(client, 404, "Not Found", "rotta sconosciuta: " + req.path);
    }

    closesocket(client);
}

} // namespace

int main(int argc, char** argv) {
    // Senza, l'output resta nel buffer finché il processo non esce: un
    // demone a lunga vita con stdout rediretto su file sembrerebbe muto.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) g_port = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--root") == 0 && i + 1 < argc) g_root = argv[++i];
        else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("blocky_hostd [--port 8080] [--root ./hosted]\n");
            return 0;
        }
    }

    std::error_code ec;
    fs::create_directories(g_root, ec);

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::fprintf(stderr, "WSAStartup fallita\n");
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        std::fprintf(stderr, "socket() fallita\n");
        WSACleanup();
        return 1;
    }

    // Riuso immediato della porta dopo un riavvio: senza, resta in TIME_WAIT
    // e il server non ripartirebbe per un minuto o due.
    BOOL reuse = TRUE;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)g_port);

    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) != 0) {
        std::fprintf(stderr, "bind() fallita sulla porta %d — occupata?\n", g_port);
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    if (listen(listener, 32) != 0) {
        std::fprintf(stderr, "listen() fallita\n");
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    std::printf("========================================\n");
    std::printf("  blocky_hostd ATTIVO\n");
    std::printf("  Porta:    %d\n", g_port);
    std::printf("  Cartella: %s\n", fs::absolute(g_root, ec).string().c_str());
    std::printf("  Ctrl+C per chiudere\n");
    std::printf("========================================\n");

    for (;;) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        SOCKET client = accept(listener, (sockaddr*)&clientAddr, &clientLen);
        if (client == INVALID_SOCKET) continue;

        std::thread(handleConnection, client).detach();
    }

    closesocket(listener);
    WSACleanup();
    return 0;
}
