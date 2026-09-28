
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
#include "engine/ServerToken.h"
#include "platform/SystemApi.h"
#include "engine/HttpMini.h"

namespace fs = std::filesystem;

namespace {

std::string g_root = "./hosted";

// Segreto condiviso per le rotte che SCRIVONO. Le GET restano aperte: i
// giocatori devono poter vedere e scaricare i giochi.
std::string g_token;
int g_port = 8080;
std::mutex g_publishMutex;

bool safeId(const std::string& id) {
    if (id.empty() || id.size() > 128) return false;
    for (char c : id) {
        if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-')) return false;
    }
    return true;
}

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
         << "PUT  /publish/<id>             body = archivio .zip\n"
         << "GET  /catalog                  item del catalogo in JSON\n"
         << "GET  /catalog/<file>           mesh di un item\n"
         << "PUT  /catalog                  body = archivio .zip del catalogo\n";
    HttpMini::sendText(s, 200, "OK", body.str());
}

/** Estrae "chiave": "valore" da un JSON semplice. Piccolo apposta: qui i file
 *  sono scritti da noi, non arrivano da Internet. */
std::string jsonStr(const std::string& json, const char* key) {
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

std::string jsonEsc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else if ((unsigned char)c < 0x20) continue;
        else o += c;
    }
    return o;
}

/** Il primo file che nella cartella del gioco somiglia a un'anteprima. */
std::string findThumbnail(const std::filesystem::path& dir) {
    std::error_code ec;
    const char* preferred[] = { "thumbnail.png", "thumb.png", "icon.png",
                                "thumbnail.jpg", "icon.jpg" };
    for (const char* name : preferred) {
        if (fs::exists(dir / name, ec)) return name;
    }
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        std::string ext = e.path().extension().string();
        for (char& c : ext) c = (char)std::tolower((unsigned char)c);
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
            return e.path().filename().string();
    }
    return {};
}

/** Il file del mondo, con i nomi che il client puo' aspettarsi. */
std::string findPlaceFileName(const std::filesystem::path& dir) {
    std::error_code ec;
    const char* names[] = { "place.blockyplace", "place.blocky", "place.json" };
    for (const char* n : names) {
        if (fs::exists(dir / n, ec)) return n;
    }
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        const std::string ext = e.path().extension().string();
        if (ext == ".blockyplace" || ext == ".blocky")
            return e.path().filename().string();
    }
    return {};
}

/**
 * GET /games — l elenco dei giochi.
 *
 * Prima qui si incollavano uno dietro l'altro i game_index.json interi: file
 * grossi, pieni di collider e checksum, che al client per disegnare una card
 * non servono a niente. Ora esce un elenco pulito con solo cio' che la pagina
 * Games mostra davvero — nome, descrizione, anteprima — piu' il nome del file
 * del mondo da scaricare quando si preme PLAY.
 *
 * I dati vengono da info.json se il gioco ce l'ha; altrimenti si ricavano dal
 * game_index.json, cosi' i giochi pubblicati prima di questo formato
 * continuano a comparire senza doverli ripubblicare.
 */
void handleGamesIndex(SOCKET s) {
    std::error_code ec;
    std::ostringstream j;
    j << "[";
    bool first = true;

    if (fs::exists(g_root, ec)) {
        for (const auto& entry : fs::directory_iterator(g_root, ec)) {
            if (!entry.is_directory(ec)) continue;
            const std::string name = entry.path().filename().string();
            if (name.empty() || name[0] == '.') continue;

            const std::string info = GamePublish::readTextFile((entry.path() / "info.json").string());
            const std::string index = GamePublish::readTextFile((entry.path() / "game_index.json").string());
            if (info.empty() && index.empty()) continue;

            std::string id = jsonStr(info, "id");
            if (id.empty()) id = jsonStr(index, "gameId");
            if (id.empty()) id = name;

            std::string title = jsonStr(info, "name");
            if (title.empty()) title = jsonStr(info, "title");
            if (title.empty()) title = jsonStr(index, "title");
            if (title.empty()) title = id;

            std::string desc = jsonStr(info, "description");
            if (desc.empty()) desc = jsonStr(info, "subtitle");

            std::string thumb = jsonStr(info, "thumbnail");
            if (thumb.empty()) thumb = findThumbnail(entry.path());

            std::string place = jsonStr(info, "place");
            if (place.empty()) place = findPlaceFileName(entry.path());

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
    HttpMini::sendJson(s, 200, "OK", j.str());
}

void handleGameFile(SOCKET s, const std::string& path) {
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


// ---- Catalogo degli item ----------------------------------------------
//
// Stessa idea dei giochi: il catalogo che un giocatore costruisce sul PC vive
// solo su quel computer, quindi aprendo Blocky dal telefono i suoi item non
// esistono. Qui il server ne tiene una copia e la serve a chiunque.
//
// Vive in una cartella col punto davanti (".catalog") cosi' non compare
// nell'elenco dei giochi: quel ciclo salta di proposito le cartelle che
// iniziano con un punto.

std::string catalogDir() { return (fs::path(g_root) / ".catalog").string(); }

void handleCatalogIndex(SOCKET s) {
    const std::string json = GamePublish::readTextFile((fs::path(catalogDir()) / "catalog.json").string());
    if (json.empty()) {
        // Nessun catalogo pubblicato non e' un errore: e' un elenco vuoto, e il
        // client lo tratta come "niente da aggiungere" senza doversene accorgere.
        HttpMini::sendJson(s, 200, "OK", "{\"items\":[]}");
        return;
    }
    HttpMini::sendJson(s, 200, "OK", json);
}

void handleCatalogFile(SOCKET s, const std::string& relFile) {
    fs::path filePath;
    if (relFile.empty() || !safeJoin(catalogDir(), relFile, filePath)) {
        HttpMini::sendText(s, 400, "Bad Request", "percorso non valido");
        return;
    }
    std::error_code ec;
    if (!fs::is_regular_file(filePath, ec)) {
        HttpMini::sendText(s, 404, "Not Found", "file non trovato: " + relFile);
        return;
    }
    if (!HttpMini::sendFile(s, filePath.string()))
        HttpMini::sendText(s, 500, "Internal Server Error", "lettura fallita");
}

void handleCatalogPublish(SOCKET s, const std::vector<std::uint8_t>& body) {
    if (body.empty()) {
        HttpMini::sendJson(s, 400, "Bad Request", "{\"ok\":false,\"error\":\"body vuoto\"}");
        return;
    }

    std::lock_guard<std::mutex> lock(g_publishMutex);
    std::error_code ec;
    const fs::path dir = catalogDir();

    // Si sostituisce in blocco invece di fondere: il catalogo e' uno solo, e
    // lasciare in giro i file di una versione precedente significherebbe
    // servire mesh che nessun item nomina piu'.
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    if (ec) {
        HttpMini::sendJson(s, 500, "Internal Server Error",
                           "{\"ok\":false,\"error\":\"cartella non creata\"}");
        return;
    }

    const ZipArchive::ExtractResult unz = ZipArchive::extractFromMemory(body, dir.string());
    if (!unz.ok || unz.entries == 0) {
        HttpMini::sendJson(s, 422, "Unprocessable Entity",
                           "{\"ok\":false,\"error\":\"archivio non valido\"}");
        return;
    }

    std::printf("[catalogo] aggiornato: %d file\n", unz.entries);
    std::ostringstream j;
    j << "{\"ok\":true,\"files\":" << unz.entries << "}";
    HttpMini::sendJson(s, 200, "OK", j.str());
}


/**
 * Chi scrive deve conoscere il segreto.
 *
 * Solo le rotte che modificano il server: le GET restano aperte, perche'
 * servono ai giocatori per vedere e scaricare i giochi.
 *
 * Il messaggio di errore non dice MAI se il token era assente, corto o
 * sbagliato: a chi prova a indovinare non si regalano indizi.
 */
bool writeAllowed(SOCKET s, const HttpMini::Request& req) {
    const std::string given = req.header("x-blocky-token");
    if (!g_token.empty() && blk::servertoken::equalsConstantTime(given, g_token))
        return true;

    HttpMini::sendJson(s, 401, "Unauthorized",
                       "{\"ok\":false,\"error\":\"token mancante o non valido\"}");
    std::printf("[hostd] scrittura RIFIUTATA su %s\n", req.path.c_str());
    return false;
}

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
    const DWORD timeoutMs = 20000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

    HttpMini::Request req;
    std::string err;
    if (!HttpMini::parseRequest(client, req, err)) {
        closesocket(client);
        return;
    }

    if (req.method == "GET" && req.path == "/") {
        handleRoot(client);
    } else if (req.method == "GET" && req.path == "/games") {
        handleGamesIndex(client);
    } else if (req.method == "GET" && req.path.rfind("/games/", 0) == 0) {
        handleGameFile(client, req.path.substr(7));
    } else if (req.method == "GET" && req.path == "/catalog") {
        handleCatalogIndex(client);
    } else if (req.method == "GET" && req.path.rfind("/catalog/", 0) == 0) {
        handleCatalogFile(client, req.path.substr(9));
    } else if ((req.method == "PUT" || req.method == "POST") && req.path == "/catalog") {
        // Niente return anticipato: in fondo alla funzione c'e' la
        // closesocket, e saltarla perderebbe un socket a ogni tentativo
        // respinto — cioe' un modo per mettere in ginocchio il server
        // proprio sbagliando il token.
        if (writeAllowed(client, req)) handleCatalogPublish(client, req.body);
    } else if ((req.method == "PUT" || req.method == "POST") &&
              req.path.rfind("/publish/", 0) == 0) {
        if (writeAllowed(client, req)) handlePublish(client, req.path.substr(9), req.body);
    } else {
        HttpMini::sendText(client, 404, "Not Found", "rotta sconosciuta: " + req.path);
    }

    closesocket(client);
}

}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) g_port = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--root") == 0 && i + 1 < argc) g_root = argv[++i];
        else if (std::strcmp(argv[i], "--token") == 0 && i + 1 < argc) g_token = argv[++i];
        else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("blocky_hostd [--port 8080] [--root ./hosted] [--token X]\n"
                        "  Le rotte PUT richiedono l'intestazione X-Blocky-Token.\n"
                        "  Senza --token si usa BLOCKY_TOKEN, poi <root>/.token,\n"
                        "  e se non esiste ne viene creato uno al primo avvio.\n");
            return 0;
        }
    }

    std::error_code ec;
    fs::create_directories(g_root, ec);

    // Il token si carica PRIMA di aprire la porta: il server non deve mai
    // restare in ascolto senza protezione, nemmeno per un istante.
    {
        bool created = false;
        g_token = blk::servertoken::loadOrCreate(g_root, g_token, created);
        if (created) {
            std::printf(
                "\n  ATTENZIONE - creato un nuovo token di pubblicazione:\n\n"
                "    %s\n\n"
                "  Scrivilo in Studio (Server giochi) per poter pubblicare.\n"
                "  E' salvato in %s/.token - chi lo legge puo' pubblicare.\n\n",
                g_token.c_str(), g_root.c_str());
        } else {
            std::printf("[hostd] token di pubblicazione caricato (%d caratteri)\n",
                        (int)g_token.size());
        }
    }

    // Era WSAStartup diretta, SENZA #ifdef: questo file non compilava proprio
    // su Linux, cioe' sul sistema di qualunque VPS. plat::sys::netInit fa la
    // cosa giusta su Windows e non fa nulla altrove.
    if (!plat::sys::netInit()) {
        std::fprintf(stderr, "rete di sistema non inizializzabile\n");
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        std::fprintf(stderr, "socket() fallita\n");
        WSACleanup();
        return 1;
    }

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
