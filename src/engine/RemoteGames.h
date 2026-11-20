#pragma once

/**
 * Giochi pubblicati su un altro computer.
 *
 * Finora l'elenco dei giochi veniva solo da file locali: il manifest negli
 * asset piu' i pacchetti installati su questa macchina. Su un tablet appena
 * installato quelle cartelle sono vuote, quindi i giochi pubblicati dal PC non
 * si vedevano — non perche' mancasse il server (blocky_hostd li serve gia' su
 * GET /games), ma perche' nessuno glieli chiedeva.
 *
 * Qui c'e' la parte che mancava: si interroga il server e si traducono i suoi
 * game_index.json in voci della lista giochi.
 */

#include <algorithm>
#include <atomic>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/GameHub.h"
#include "engine/HttpClient.h"
#include "engine/WebRequest.h"
#include "engine/ZipArchive.h"
#include "platform/Log.h"

namespace blk {
namespace remotegames {

inline constexpr int kDefaultPort = 8080;

// Definita in fondo, accanto al resto dello stato di contatto col server.
inline void noteServerContact();

/** Dove sta il server dei giochi, ricordato tra un avvio e l'altro. */
inline std::string configPath() { return blockyDataDir() + "/game_server.txt"; }

inline std::string loadHost() {
    std::string s = readTextFile(configPath());
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

inline void saveHost(const std::string& host) {
    std::ofstream f(configPath(), std::ios::binary);
    if (f) f << host;
}

/**
 * Separa "192.168.1.5:8080" in indirizzo e porta. Senza porta si usa l'8080,
 * che e' quella predefinita di blocky_hostd — cosi' basta scrivere l'IP.
 */
inline void splitHostPort(const std::string& text, std::string& host, int& port) {
    port = kDefaultPort;
    host = text;
    const size_t colon = text.rfind(':');
    if (colon != std::string::npos && colon + 1 < text.size()) {
        const std::string p = text.substr(colon + 1);
        bool digits = !p.empty();
        for (char c : p) if (c < '0' || c > '9') digits = false;
        if (digits) {
            host = text.substr(0, colon);
            port = std::atoi(p.c_str());
        }
    }
}

/**
 * Chiede al server l'elenco e lo traduce in voci di gioco.
 *
 * La risposta e' un array di game_index.json completi (con asset, collider,
 * script): qui interessano solo i due campi che servono a mostrare una card,
 * gameId e title. Il resto del contenuto si scarica al momento di giocare, non
 * per riempire una lista.
 */
/**
 * Il server dei giochi e' il catalogo online ("https://..." o "http://...")
 * e non un PC in rete locale ("192.168.1.5:8080"). I percorsi sono gli
 * stessi (/games, /games/<id>/files, /games/<id>/<file>), cambia il trasporto.
 */
inline bool isCloud(const std::string& server) { return blk::web::hasScheme(server); }

/** Nome breve del server per i messaggi (l'host, senza http://). */
inline std::string serverLabel(const std::string& server) {
    blk::web::Url u;
    if (isCloud(server) && blk::web::parseUrl(server, u)) return u.host;
    std::string host;
    int port = kDefaultPort;
    splitHostPort(server, host, port);
    return host;
}

/** GET di un percorso sul server dei giochi, in rete locale o online. */
inline blk::http::GetResult serverGet(const std::string& server, const std::string& path,
                                      int timeoutMs, int attempts = 1, long long knownLocalBytes = -1) {
    if (!isCloud(server)) {
        std::string host;
        int port = kDefaultPort;
        splitHostPort(server, host, port);
        if (attempts > 1) return blk::http::getRetryEx(host, port, path, timeoutMs, attempts, knownLocalBytes);
        return blk::http::getEx(host, port, path, timeoutMs, 32u * 1024 * 1024, knownLocalBytes);
    }
    blk::http::GetResult out;
    for (int i = 0; i < std::max(1, attempts); ++i) {
        const blk::web::Response r = blk::web::get(blk::web::join(server, path), timeoutMs);
        out.status = r.status;
        if (r.ok()) {
            out.ok = true;
            out.body = r.body;
            out.contentLength = (long long)r.body.size();
            return out;
        }
        if (r.transport && r.status >= 400 && r.status < 500) break;   // non c'e': inutile riprovare
        if (i + 1 < attempts) blk::http::sleepMs(400 * (i + 1));
    }
    return out;
}

/** Nome dei file che il server ha per un gioco, imparato leggendo il catalogo. */
struct RemoteFiles {
    std::string thumbnail;   // es. "thumbnail.png"
    std::string place;       // es. "place.blockyplace"
    int version = 0;         // catalogo online: sale a ogni ripubblicazione (0 = non si sa)
};

inline std::mutex& filesMutex() { static std::mutex m; return m; }
inline std::map<std::string, RemoteFiles>& filesMap() {
    static std::map<std::string, RemoteFiles> m;
    return m;
}

inline RemoteFiles filesFor(const std::string& id) {
    std::lock_guard<std::mutex> lock(filesMutex());
    auto it = filesMap().find(id);
    return it == filesMap().end() ? RemoteFiles{} : it->second;
}

inline std::string thumbnailPath(const std::string& id) {
    return blockyDataDir() + "/thumbs/" + id + ".img";
}

/**
 * Scarica l'anteprima di un gioco, se non l'abbiamo gia'.
 *
 * L'immagine viene salvata con un nome nostro invece che con quello del
 * server: cosi' il resto del motore la ritrova sempre allo stesso posto,
 * qualunque estensione avesse in origine (stb_image riconosce il formato dal
 * contenuto, non dal nome).
 */
inline std::string ensureThumbnail(const std::string& server,
                                   const std::string& id, const std::string& remoteName) {
    if (remoteName.empty()) return {};
    const std::string local = thumbnailPath(id);
    if (plat::fileExists(local) && plat::fileSize(local) > 0) return local;

    std::string body;
    const blk::http::GetResult tr = serverGet(server, "/games/" + id + "/" + remoteName, 8000);
    body = tr.body;
    if (!tr.ok || body.empty())
        return {};

    plat::makeDirs(blockyDataDir() + "/thumbs");
    std::ofstream f(local, std::ios::binary);
    if (!f) return {};
    f.write(body.data(), (std::streamsize)body.size());
    return local;
}

/**
 * Chiede al server il catalogo e lo traduce in card della pagina Games.
 *
 * Il server manda nome, descrizione, anteprima e nome del file del mondo: da
 * qui in poi la pagina Games e' davvero dinamica — se il server aggiunge o
 * cambia un gioco, il client lo vede al prossimo avvio senza che nessuno
 * tocchi il codice.
 */
inline std::vector<GameEntry> fetch(const std::string& hostPort) {
    std::vector<GameEntry> out;
    if (hostPort.empty()) return out;

    const std::string host = serverLabel(hostPort);
    const blk::http::GetResult lr = serverGet(hostPort, "/games", isCloud(hostPort) ? 8000 : 2500);
    const std::string body = lr.body;
    if (!lr.ok || body.empty()) {
        LOG_INFO("games", "nessuna risposta da %s", hostPort.c_str());
        return out;
    }

    size_t pos = 0;
    while ((pos = body.find("\"id\"", pos)) != std::string::npos) {
        GameEntry e;
        e.id = jsonFindStr(body, "id", pos);
        e.title = jsonFindStr(body, "title", pos);
        const std::string desc = jsonFindStr(body, "description", pos);
        const std::string thumb = jsonFindStr(body, "thumbnail", pos);
        const std::string place = jsonFindStr(body, "place", pos);
        // "version" c'e' solo nel catalogo online: cercata solo dentro questo gioco.
        int version = 0;
        {
            const size_t next = body.find("\"id\"", pos + 4);
            const size_t vk = body.find("\"version\"", pos);
            if (vk != std::string::npos && (next == std::string::npos || vk < next))
                version = std::max(0, jsonFindInt(body, "version", pos));
        }
        pos += 4;
        if (e.id.empty()) continue;

        if (e.title.empty()) e.title = e.id;
        e.subtitle = desc.empty() ? ("Dal server " + host) : desc;
        e.isPublished = true;
        e.remote = true;
        e.accent = glm::vec3(0.18f, 0.45f, 0.78f);   // blu: viene dalla rete

        {
            std::lock_guard<std::mutex> lock(filesMutex());
            filesMap()[e.id] = RemoteFiles{thumb, place, version};
        }
        e.iconPath = ensureThumbnail(hostPort, e.id, thumb);

        out.push_back(e);
    }

    noteServerContact();
    LOG_INFO("games", "%d giochi dal server %s", (int)out.size(), host.c_str());
    return out;
}


// ---- Recupero in secondo piano ----------------------------------------
//
// Interrogare il server puo' richiedere secondi (rete lenta, PC spento fino al
// timeout). Farlo dentro il frame significherebbe bloccare il gioco per tutto
// quel tempo, quindi la richiesta parte su un thread a parte e il risultato
// viene raccolto quando e' pronto.

inline std::mutex& resultMutex() { static std::mutex m; return m; }
inline std::vector<GameEntry>& resultSlot() { static std::vector<GameEntry> v; return v; }
inline std::atomic<bool>& resultReady() { static std::atomic<bool> b{false}; return b; }
inline std::atomic<bool>& fetchBusy() { static std::atomic<bool> b{false}; return b; }

/** Avvia una richiesta, se non ce n'e' gia' una in corso. */
inline void startFetch(const std::string& hostPort) {
    if (hostPort.empty()) return;
    bool expected = false;
    if (!fetchBusy().compare_exchange_strong(expected, true)) return;

    std::thread([hostPort]() {
        std::vector<GameEntry> games = fetch(hostPort);
        {
            std::lock_guard<std::mutex> lock(resultMutex());
            resultSlot() = std::move(games);
        }
        resultReady().store(true);
        fetchBusy().store(false);
    }).detach();
}

/** true una sola volta, quando un risultato e' arrivato. */
inline bool takeResult(std::vector<GameEntry>& out) {
    if (!resultReady().exchange(false)) return false;
    std::lock_guard<std::mutex> lock(resultMutex());
    out = std::move(resultSlot());
    resultSlot().clear();
    return true;
}

inline bool isFetching() { return fetchBusy().load(); }


// ---- Invio al server ---------------------------------------------------

inline std::mutex& uploadMsgMutex() { static std::mutex m; return m; }
inline std::string& uploadMsgSlot() { static std::string s; return s; }
inline std::atomic<bool>& uploadBusy() { static std::atomic<bool> b{false}; return b; }

/** Ultimo esito dell'invio, da mostrare nell'interfaccia. Si svuota leggendolo. */
inline std::string takeUploadMessage() {
    std::lock_guard<std::mutex> lock(uploadMsgMutex());
    std::string s = std::move(uploadMsgSlot());
    uploadMsgSlot().clear();
    return s;
}

inline void setUploadMessage(const std::string& s) {
    std::lock_guard<std::mutex> lock(uploadMsgMutex());
    uploadMsgSlot() = s;
}

inline bool isUploading() { return uploadBusy().load(); }

/**
 * Manda al server un gioco appena pubblicato, cosi' che lo conservi lui e lo
 * mostri a chiunque altro apra Blocky.
 *
 * Tutto in un thread a parte: impacchettare la cartella e spedire qualche
 * megabyte richiede tempo, e chi ha appena premuto "Pubblica" non deve vedere
 * il gioco bloccarsi. Finita la spedizione si richiede l'elenco aggiornato, in
 * modo che il gioco compaia da solo nella pagina principale senza che nessuno
 * debba premere "Aggiorna".
 */
inline void uploadGame(const std::string& hostPort, const std::string& gameId,
                       const std::string& packageDir) {
    if (hostPort.empty() || gameId.empty() || packageDir.empty()) return;
    if (isCloud(hostPort)) return;   // il catalogo online passa da publishToCloud

    bool expected = false;
    if (!uploadBusy().compare_exchange_strong(expected, true)) {
        setUploadMessage("Invio gia' in corso, riprova tra poco");
        return;
    }

    std::thread([hostPort, gameId, packageDir]() {
        std::vector<std::uint8_t> zipBytes;
        std::string error;
        if (!ZipArchive::createFromFolder(packageDir, zipBytes, error)) {
            setUploadMessage("Invio fallito: " + error);
            uploadBusy().store(false);
            return;
        }

        std::string host;
        int port = kDefaultPort;
        splitHostPort(hostPort, host, port);

        std::string reply;
        const bool ok = blk::http::put(host, port, "/publish/" + gameId, zipBytes, reply);
        if (ok) {
            const double mb = (double)zipBytes.size() / (1024.0 * 1024.0);
            char msg[160];
            std::snprintf(msg, sizeof(msg), "Inviato al server %s (%.2f MB)", host.c_str(), mb);
            setUploadMessage(msg);
            LOG_INFO("games", "%s inviato a %s:%d", gameId.c_str(), host.c_str(), port);
        } else {
            setUploadMessage("Il server " + host + " non ha accettato l'invio"
                             + (reply.empty() ? "" : (": " + reply)));
            LOG_INFO("games", "invio di %s a %s:%d fallito", gameId.c_str(), host.c_str(), port);
        }
        uploadBusy().store(false);

        // L'elenco e' cambiato: lo si richiede subito, cosi' la card compare
        // nella pagina principale da sola.
        if (ok) startFetch(hostPort);
    }).detach();
}


// ---- Pubblicare sul catalogo online ------------------------------------

/** Un nome di file che il catalogo accetta (backend/src/games.ts, safePath). */
inline bool cloudSafePath(const std::string& p) {
    if (p.empty() || p.size() > 200) return false;
    size_t start = 0;
    int parts = 0;
    while (start <= p.size()) {
        const size_t slash = p.find('/', start);
        const std::string seg = p.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (seg.empty() || seg.size() > 80 || seg == "." || seg == "..") return false;
        for (char c : seg)
            if (!(std::isalnum((unsigned char)c) || c == '_' || c == '.' || c == '-')) return false;
        if (++parts > 6) return false;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

inline std::string jsonQuote(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o + "\"";
}

/**
 * Corpo JSON di una pubblicazione: scheda del gioco + tutti i file del
 * pacchetto in base64. Vuoto + motivo in `error` se non si puo'.
 */
inline std::string buildCloudPublishBody(const std::string& packageDir, const std::string& title,
                                         const std::string& description, int maxPlayers,
                                         std::string& error, int& fileCount) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root(packageDir);
    std::vector<std::string> rels;
    for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::string rel = fs::relative(entry.path(), root, ec).generic_string();
        if (ec || rel.empty() || rel[0] == '.') continue;
        if (rel == "files.json") continue;   // lo ricostruisce il server
        rels.push_back(rel);
    }
    std::sort(rels.begin(), rels.end());
    std::string place;
    for (const char* n : {"place.json", "place.blockyplace", "place.blocky"})
        if (std::find(rels.begin(), rels.end(), n) != rels.end()) { place = n; break; }
    if (place.empty()) { error = "nel pacchetto manca il place"; return {}; }
    std::string thumb;
    for (const char* n : {"thumbnail.png", "thumbnail.jpg", "icon.png"})
        if (std::find(rels.begin(), rels.end(), n) != rels.end()) { thumb = n; break; }

    std::string j = "{\"title\":" + jsonQuote(title.substr(0, 60))
                  + ",\"description\":" + jsonQuote(description.substr(0, 500))
                  + ",\"maxPlayers\":" + std::to_string(std::clamp(maxPlayers, 1, 32))
                  + ",\"place\":" + jsonQuote(place)
                  + ",\"thumbnail\":" + jsonQuote(thumb)
                  + ",\"files\":[";
    fileCount = 0;
    for (const std::string& rel : rels) {
        if (!cloudSafePath(rel)) {
            error = "nome di file non accettato online: " + rel + " (solo lettere, cifre, _ . -)";
            return {};
        }
        std::string data;
        {
            // binario: in modo testo Windows rovinerebbe immagini e mesh
            std::ifstream in((root / rel).string(), std::ios::binary);
            data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        }
        if (fileCount) j += ",";
        j += "{\"path\":" + jsonQuote(rel) + ",\"data\":\"" + blk::web::base64(data) + "\"}";
        ++fileCount;
    }
    j += "]}";
    return j;
}

/**
 * Pubblica il pacchetto sul catalogo online (PUT /v1/games/<id>), in un
 * thread a parte. Il risultato arriva in takeUploadMessage(); se va bene si
 * richiede subito l'elenco, cosi' la card compare da sola.
 */
inline void publishToCloud(const std::string& url, const std::string& token, const std::string& gameId,
                           const std::string& packageDir, const std::string& title,
                           const std::string& description, int maxPlayers) {
    if (url.empty() || gameId.empty() || packageDir.empty()) return;
    if (token.empty()) {
        setUploadMessage("Online: manca il token del creatore (publish_token.txt)");
        return;
    }
    bool expected = false;
    if (!uploadBusy().compare_exchange_strong(expected, true)) {
        setUploadMessage("Invio gia' in corso, riprova tra poco");
        return;
    }
    std::thread([=]() {
        std::string error;
        int files = 0;
        const std::string body = buildCloudPublishBody(packageDir, title, description, maxPlayers, error, files);
        if (body.empty()) {
            setUploadMessage("Online: non pubblicato, " + error);
            uploadBusy().store(false);
            return;
        }
        blk::web::Request rq;
        rq.method = "PUT";
        rq.url = blk::web::join(url, "/v1/games/" + gameId);
        rq.body = body;
        rq.contentType = "application/json";
        rq.bearer = token;
        rq.timeoutMs = 60000;
        const blk::web::Response r = blk::web::send(rq);
        if (r.ok()) {
            const int version = jsonFindInt(r.body, "version");
            setUploadMessage("Pubblicato online su " + serverLabel(url) + " (versione "
                             + std::to_string(version) + ", " + std::to_string(files) + " file)");
            LOG_INFO("games", "%s pubblicato online, versione %d", gameId.c_str(), version);
        } else if (!r.transport) {
            setUploadMessage("Online: server non raggiungibile (" + r.error + ")");
        } else {
            std::string why = jsonFindStr(r.body, "error");
            if (why.empty()) why = "errore " + std::to_string(r.status);
            setUploadMessage("Online: non pubblicato, " + why);
            LOG_INFO("games", "pubblicazione online di %s rifiutata: %d", gameId.c_str(), r.status);
        }
        uploadBusy().store(false);
        if (r.ok()) startFetch(url);
    }).detach();
}


// ---- Scaricare un gioco dal server -------------------------------------

inline std::string localGameDir(const std::string& id) {
    return blockyDataDir() + "/downloaded/" + id;
}

/** Il file del mondo scaricato, con qualunque nome sia arrivato. */
inline std::string localPlacePath(const std::string& id) {
    const std::string dir = localGameDir(id);
    for (const char* n : {"place.blockyplace", "place.blocky", "place.json"}) {
        const std::string p = dir + "/" + n;
        if (plat::fileExists(p)) return p;
    }
    return {};
}

/** Un gioco e' giocabile solo se ne abbiamo il mondo. */
inline bool isDownloaded(const std::string& id) {
    return !localPlacePath(id).empty();
}

/** Place c'e' ma mancano mesh/asset referenziati (download vecchio incompleto). */
inline bool packageLooksIncomplete(const std::string& id) {
    if (!isDownloaded(id)) return true;
    const std::string place = localPlacePath(id);
    const std::string dir = localGameDir(id);
    const std::string json = readTextFile(place);
    if (json.empty()) return true;
    size_t pos = 0;
    while ((pos = json.find("\"meshPath\"", pos)) != std::string::npos) {
        const std::string mp = jsonFindStr(json, "meshPath", pos);
        pos += 10;
        if (mp.empty()) continue;
        // Path assoluti del PC: sul telefono non esistono → va ri-scaricato
        // dopo che il server ha fatto ensurePackageAssets.
        if (mp.size() >= 2 && std::isalpha((unsigned char)mp[0]) && mp[1] == ':')
            return true;
        if (mp.rfind("assets/", 0) == 0 || mp.rfind("assets\\", 0) == 0) {
            std::string local = dir + "/" + mp;
            for (char& c : local) if (c == '\\') c = '/';
            if (!plat::fileExists(local)) return true;
        }
    }
    return false;
}

/** Collega una card ai file gia' scaricati: titolo dal server, mondo locale. */
inline void bindLocalPlayable(GameEntry& e) {
    if (e.id.empty() || !isDownloaded(e.id)) return;
    const std::string dir = localGameDir(e.id);
    e.remote = false;
    e.isPublished = true;
    e.placePath = localPlacePath(e.id);
    const std::string thumb = thumbnailPath(e.id);
    if (plat::fileExists(thumb) && plat::fileSize(thumb) > 0)
        e.iconPath = thumb;
    const std::string bbc = dir + "/scripts/main.bbc";
    const std::string lua = dir + "/scripts/main.lua";
    if (plat::fileExists(bbc)) e.script = bbc;
    else if (plat::fileExists(lua)) e.script = lua;
    // Titolo/descrizione da info.json locale se la card e' ancora grezza.
    if (e.title.empty() || e.title == e.id) {
        const std::string info = readTextFile(dir + "/info.json");
        if (!info.empty()) {
            const std::string t = jsonFindStr(info, "title");
            if (!t.empty()) e.title = t;
            else {
                const std::string n = jsonFindStr(info, "name");
                if (!n.empty()) e.title = n;
            }
            const std::string d = jsonFindStr(info, "description");
            if (!d.empty() && e.subtitle.empty()) e.subtitle = d;
        }
    }
    if (e.title.empty()) e.title = e.id;
    if (e.subtitle.empty()) e.subtitle = "Scaricato";
    e.accent = glm::vec3(0.18f, 0.45f, 0.78f);
}

/**
 * Tutti i mondi gia' in downloaded/ — restano nel menu anche offline.
 */
inline std::vector<GameEntry> listDownloadedEntries() {
    std::vector<GameEntry> out;
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = blockyDataDir() + "/downloaded";
    if (!fs::exists(root, ec)) return out;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (!entry.is_directory(ec)) continue;
        const std::string id = entry.path().filename().string();
        if (id.empty() || id[0] == '.') continue;
        if (!isDownloaded(id)) continue;
        GameEntry e;
        e.id = id;
        bindLocalPlayable(e);
        out.push_back(std::move(e));
    }
    return out;
}

/** Dopo un fetch: se un remoto e' gia' in cache, diventa giocabile subito. */
inline void applyDownloadedToList(std::vector<GameEntry>& games) {
    for (GameEntry& e : games) {
        if (isDownloaded(e.id))
            bindLocalPlayable(e);
    }
    // Aggiungi scaricati che il server non ha (piu') in elenco.
    for (GameEntry& d : listDownloadedEntries()) {
        bool found = false;
        for (const GameEntry& g : games) {
            if (g.id == d.id) { found = true; break; }
        }
        if (!found) games.push_back(std::move(d));
    }
}

inline std::mutex& dlMutex() { static std::mutex m; return m; }
inline std::string& dlDoneSlot() { static std::string s; return s; }
inline std::string& dlToastSlot() { static std::string s; return s; }   // one-shot chat
inline std::string& dlLiveSlot() { static std::string s; return s; }    // barra UI
inline std::string& dlCurrentIdSlot() { static std::string s; return s; }
inline std::atomic<bool>& dlBusy() { static std::atomic<bool> b{false}; return b; }
inline std::atomic<float>& dlProgressAtom() { static std::atomic<float> p{0.f}; return p; }

inline bool isDownloading() { return dlBusy().load(); }
inline float downloadProgress() { return dlProgressAtom().load(); }

inline std::string downloadingId() {
    std::lock_guard<std::mutex> lock(dlMutex());
    return dlCurrentIdSlot();
}

/** Stato attuale (non consuma): per barra progresso UI. */
inline std::string peekDownloadStatus() {
    std::lock_guard<std::mutex> lock(dlMutex());
    return dlLiveSlot();
}

inline void setDownloadProgress(float t01, const std::string& status) {
    dlProgressAtom().store(std::max(0.f, std::min(1.f, t01)));
    std::lock_guard<std::mutex> lock(dlMutex());
    dlLiveSlot() = status;
}

inline void setDownloadToast(const std::string& s) {
    std::lock_guard<std::mutex> lock(dlMutex());
    dlToastSlot() = s;
    dlLiveSlot() = s;
}

/** Id del gioco appena scaricato, una volta sola. Vuoto se non e' pronto. */
inline std::string takeDownloadedId() {
    std::lock_guard<std::mutex> lock(dlMutex());
    std::string s = std::move(dlDoneSlot());
    dlDoneSlot().clear();
    return s;
}

inline std::string takeDownloadStatus() {
    std::lock_guard<std::mutex> lock(dlMutex());
    std::string s = std::move(dlToastSlot());
    dlToastSlot().clear();
    return s;
}

/**
 * Scarica dal server i file di un gioco e li mette in una cartella locale.
 *
 * Ordine tipo Roblox: prima il place (mondo giocabile), poi asset secondari.
 * Cache: se Content-Length del server coincide col file locale si salta il corpo.
 * Retry su rete domestica instabile. Progresso esposto ad atom + status.
 */
inline void downloadGame(const std::string& hostPort, const std::string& gameId, bool launchWhenDone);

inline std::mutex& dlQueueMutex() { static std::mutex m; return m; }
inline std::vector<std::pair<std::string, std::string>>& dlQueue() {
    static std::vector<std::pair<std::string, std::string>> q;
    return q;
}
inline std::atomic<bool>& dlLaunchWhenDone() { static std::atomic<bool> b{false}; return b; }
inline std::atomic<bool>& dlLastFinishedLaunch() { static std::atomic<bool> b{false}; return b; }

inline void pumpDownloadQueue() {
    if (dlBusy().load()) return;
    std::string hostPort, gameId;
    {
        std::lock_guard<std::mutex> lock(dlQueueMutex());
        if (dlQueue().empty()) return;
        hostPort = dlQueue().front().first;
        gameId = dlQueue().front().second;
        dlQueue().erase(dlQueue().begin());
    }
    dlLaunchWhenDone().store(false);
    downloadGame(hostPort, gameId, false);
}

/** Accoda i giochi remoti non ancora presenti in locale (es. Android appena connesso al PC). */
inline void enqueueMissingDownloads(const std::string& hostPort,
                                    const std::vector<GameEntry>& games) {
    if (hostPort.empty()) return;
    int added = 0;
    {
        std::lock_guard<std::mutex> lock(dlQueueMutex());
        for (const GameEntry& e : games) {
            if (e.id.empty()) continue;
            // Remoti nuovi, oppure locali incompleti (place senza .glb).
            const bool need = (e.remote && !isDownloaded(e.id))
                || packageLooksIncomplete(e.id);
            if (!need) continue;
            bool dup = false;
            for (const auto& p : dlQueue())
                if (p.second == e.id) { dup = true; break; }
            if (dup) continue;
            dlQueue().push_back({hostPort, e.id});
            ++added;
        }
    }
    if (added > 0) {
        LOG_INFO("games", "accodati %d download da %s", added, hostPort.c_str());
        if (!dlBusy().load()) pumpDownloadQueue();
    }
}

/** Scrive un file scaricato; crea sottocartelle se serve (scripts/). */
inline bool writeDownloadedFile(const std::string& dir, const std::string& rel,
                                const std::string& body) {
    if (body.empty()) return false;
    const size_t slash = rel.rfind('/');
    if (slash != std::string::npos)
        plat::makeDirs(dir + "/" + rel.substr(0, slash));
    std::ofstream f(dir + "/" + rel, std::ios::binary);
    if (!f) return false;
    f.write(body.data(), (std::streamsize)body.size());
    return (bool)f;
}

/**
 * Scarica un singolo path relativo con cache Content-Length + retry.
 * Ritorna: 1 = scritto/nuovo, 0 = cache skip, -1 = assente/fallito.
 */
inline int fetchGameFile(const std::string& server, const std::string& gameId,
                         const std::string& dir, const std::string& rel) {
    const std::string local = dir + "/" + rel;
    long long localBytes = -1;
    if (plat::fileExists(local)) {
        const long long sz = plat::fileSize(local);
        if (sz > 0) localBytes = sz;
    }
    blk::http::GetResult r = serverGet(server, "/games/" + gameId + "/" + rel, 12000, 3, localBytes);
    if (!r.ok) return -1;
    if (r.skipped) return 0;
    if (r.body.empty()) return -1;
    return writeDownloadedFile(dir, rel, r.body) ? 1 : -1;
}

inline void downloadGame(const std::string& hostPort, const std::string& gameId,
                         bool launchWhenDone = true) {
    if (hostPort.empty() || gameId.empty()) return;

    // Gia' scaricato: controlla se il PC ha ripubblicato il mondo (stessa
    // dimensione = non e' cambiato). Senza questo il telefono teneva per
    // sempre la prima copia e il gioco nuovo non compariva.
    if (isDownloaded(gameId) && !packageLooksIncomplete(gameId) && launchWhenDone) {
        const std::string dir = localGameDir(gameId);
        const std::string place = localPlacePath(gameId);
        std::string rel = "place.json";
        if (place.size() > dir.size() + 1)
            rel = place.substr(dir.size() + 1);
        const int fresh = fetchGameFile(hostPort, gameId, dir, rel);
        if (fresh <= 0) {
            {
                std::lock_guard<std::mutex> lock(dlMutex());
                dlDoneSlot() = gameId;
            }
            setDownloadToast("In cache: \"" + gameId + "\"");
            dlLastFinishedLaunch().store(true);
            dlProgressAtom().store(1.f);
            return;
        }
        setDownloadToast("Aggiorno \"" + gameId + "\"...");
    }

    bool expected = false;
    if (!dlBusy().compare_exchange_strong(expected, true)) {
        if (!launchWhenDone) {
            std::lock_guard<std::mutex> lock(dlQueueMutex());
            bool dup = false;
            for (const auto& p : dlQueue())
                if (p.second == gameId) { dup = true; break; }
            if (!dup) dlQueue().push_back({hostPort, gameId});
        } else if (isDownloaded(gameId)) {
            // Busy su altro gioco ma questo e' gia' locale → avvia comunque.
            {
                std::lock_guard<std::mutex> lock(dlMutex());
                dlDoneSlot() = gameId;
            }
            dlLastFinishedLaunch().store(true);
            setDownloadToast("In cache: \"" + gameId + "\"");
        }
        return;
    }
    dlLaunchWhenDone().store(launchWhenDone);
    {
        std::lock_guard<std::mutex> lock(dlMutex());
        dlCurrentIdSlot() = gameId;
    }
    setDownloadProgress(0.02f, "Connessione...");

    std::thread([hostPort, gameId]() {
        const std::string dir = localGameDir(gameId);
        plat::makeDirs(dir);
        plat::makeDirs(dir + "/scripts");

        const RemoteFiles known = filesFor(gameId);

        // 1) PLACE FIRST — senza mondo non si gioca; tutto il resto e' optional.
        std::vector<std::string> placeCandidates;
        if (!known.place.empty()) placeCandidates.push_back(known.place);
        for (const char* n : {"place.blockyplace", "place.blocky", "place.json"}) {
            bool dup = false;
            for (const auto& c : placeCandidates) if (c == n) { dup = true; break; }
            if (!dup) placeCandidates.push_back(n);
        }

        setDownloadProgress(0.08f, "Scarico il mondo...");
        // Catalogo online: una versione nuova del gioco = si riscarica il place.
        const std::string versionFile = dir + "/.version";
        const bool newVersion = known.version > 0
            && readTextFile(versionFile) != std::to_string(known.version);
        const bool incomplete = packageLooksIncomplete(gameId) || newVersion;
        bool gotPlace = isDownloaded(gameId);
        // Place incompleto (path Windows / mesh mancanti): forza riscarico
        // ignorando la cache Content-Length.
        if (!gotPlace || incomplete) {
            for (const std::string& rel : placeCandidates) {
                // Cancella place locale obsoleto cosi' getCached non lo salta.
                if (incomplete) {
                    const std::string local = dir + "/" + rel;
                    if (plat::fileExists(local))
                        std::remove(local.c_str());
                }
                const int r = fetchGameFile(hostPort, gameId, dir, rel);
                if (r >= 0 && isDownloaded(gameId)) {
                    gotPlace = true;
                    if (!incomplete) break;
                    // Con incomplete continua: prende il place aggiornato e basta.
                    break;
                }
            }
        } else {
            const std::string existing = localPlacePath(gameId);
            if (!existing.empty()) {
                const std::string rel = existing.substr(dir.size() + 1);
                fetchGameFile(hostPort, gameId, dir, rel);
            }
        }

        if (!gotPlace) {
            setDownloadToast("Download di \"" + gameId + "\" fallito (manca il mondo)");
            {
                std::lock_guard<std::mutex> lock(dlMutex());
                dlCurrentIdSlot().clear();
            }
            dlProgressAtom().store(0.f);
            LOG_INFO("games", "download %s: place mancante", gameId.c_str());
            dlBusy().store(false);
            pumpDownloadQueue();
            return;
        }

        // 2) Elenco completo del pacchetto (mesh/texture/audio/script).
        std::vector<std::string> extras;
        {
            std::string listBody;
            const blk::http::GetResult fl = serverGet(hostPort, "/games/" + gameId + "/files", 12000, 3);
            listBody = fl.ok ? fl.body : std::string();
            if (!listBody.empty()) {
                size_t q = 0;
                while ((q = listBody.find('"', q)) != std::string::npos) {
                    size_t e = listBody.find('"', q + 1);
                    if (e == std::string::npos) break;
                    std::string rel = listBody.substr(q + 1, e - q - 1);
                    q = e + 1;
                    if (rel.empty() || rel == "files" || rel == "files.json") continue;
                    // Evita escape JSON grezzi
                    if (rel.find('\\') != std::string::npos) {
                        std::string clean;
                        for (size_t i = 0; i < rel.size(); ++i) {
                            if (rel[i] == '\\' && i + 1 < rel.size()) {
                                clean += rel[++i];
                                continue;
                            }
                            clean += rel[i];
                        }
                        rel = clean;
                    }
                    bool dup = false;
                    for (const auto& x : extras) if (x == rel) { dup = true; break; }
                    if (!dup) extras.push_back(rel);
                }
            }
        }
        // Fallback se il server e' vecchio senza /files.
        if (extras.empty()) {
            for (const char* n : {"info.json", "game.blocky.json", "game_index.json",
                                  "files.json", "scripts/main.bbc", "scripts/main.lua"})
                extras.push_back(n);
            if (!known.thumbnail.empty()) extras.push_back(known.thumbnail);
        }

        // Dal place: qualunque meshPath → prova assets/meshes/<basename>
        // (anche se il JSON ha ancora C:\Users\...\foo.glb del PC).
        {
            const std::string place = localPlacePath(gameId);
            const std::string pj = place.empty() ? std::string{} : readTextFile(place);
            size_t pos = 0;
            while (!pj.empty() && (pos = pj.find("\"meshPath\"", pos)) != std::string::npos) {
                std::string mp = jsonFindEscapedStr(pj, "meshPath", pos);
                if (mp.empty()) mp = jsonFindStr(pj, "meshPath", pos);
                pos += 10;
                if (mp.empty()) continue;
                size_t b = mp.find_last_of("/\\");
                const std::string base = (b == std::string::npos) ? mp : mp.substr(b + 1);
                if (base.empty() || base.find('.') == std::string::npos) continue;
                const std::string rel = "assets/meshes/" + base;
                bool dup = false;
                for (const auto& x : extras) if (x == rel) { dup = true; break; }
                if (!dup) extras.push_back(rel);
            }
        }

        int got = 1; // place gia' ok
        int cached = 0;
        for (size_t i = 0; i < extras.size(); ++i) {
            const float t = 0.35f + 0.60f * ((float)(i + 1) / (float)std::max<size_t>(1, extras.size()));
            setDownloadProgress(t, "Asset " + std::to_string(i + 1) + "/"
                                   + std::to_string(extras.size()) + "...");
            // Non riscaricare il place gia' preso.
            bool isPlace = false;
            for (const auto& p : placeCandidates) if (p == extras[i]) { isPlace = true; break; }
            if (isPlace) { ++cached; continue; }
            const int r = fetchGameFile(hostPort, gameId, dir, extras[i]);
            if (r > 0) ++got;
            else if (r == 0) { ++got; ++cached; }
        }

        if (isDownloaded(gameId)) {
            if (known.version > 0) {
                std::ofstream vf(versionFile, std::ios::binary);
                if (vf) vf << known.version;
            }
            {
                std::lock_guard<std::mutex> lock(dlMutex());
                dlDoneSlot() = gameId;
                dlCurrentIdSlot().clear();
            }
            dlLastFinishedLaunch().store(dlLaunchWhenDone().load());
            setDownloadToast(std::string("Pronto \"") + gameId + "\" ("
                + std::to_string(got) + " file"
                + (cached > 0 ? (", " + std::to_string(cached) + " cache") : "")
                + ")");
        } else {
            setDownloadToast("Download di \"" + gameId + "\" fallito");
            std::lock_guard<std::mutex> lock(dlMutex());
            dlCurrentIdSlot().clear();
        }
        dlProgressAtom().store(1.f);
        LOG_INFO("games", "download %s: %d file (%d cache)", gameId.c_str(), got, cached);
        dlBusy().store(false);
        pumpDownloadQueue();
    }).detach();
}

inline bool takeLastDownloadLaunch() { return dlLastFinishedLaunch().exchange(false); }


// ---- Contatto col server ----------------------------------------------

inline std::atomic<long long>& lastContactRef() {
    static std::atomic<long long> t{0};
    return t;
}

inline void noteServerContact() { lastContactRef().store((long long)std::time(nullptr)); }

/**
 * Il server dei giochi ci ha risposto di recente?
 *
 * Serve al controllo "Blocky needs Wi-Fi": quel controllo esiste per impedire
 * di giocare completamente scollegati, ma finora l'unica prova che accettava
 * era essere client di una partita multigiocatore. Cosi' un mondo scaricato
 * dal server e giocato da soli veniva rifiutato, pur essendo arrivato dalla
 * rete un attimo prima. Aver parlato col server dei giochi e' esattamente la
 * prova che quel controllo cerca.
 */
inline bool serverSeenRecently(int withinSecs = 600) {
    const long long t = lastContactRef().load();
    if (t <= 0) return false;
    return (long long)std::time(nullptr) - t <= withinSecs;
}


/**
 * Manda al server TUTTI i giochi gia' pubblicati su questo computer.
 *
 * uploadGame() parte da sola solo quando si pubblica qualcosa di nuovo, quindi
 * i giochi pubblicati prima che il server esistesse restavano chiusi qui
 * dentro: sul PC c'erano, sul telefono no, e non c'era modo di rimediare se
 * non ripubblicandoli uno a uno.
 *
 * Vanno in fila su UN SOLO thread invece che tutti insieme: ognuno spedisce
 * qualche megabyte, e lanciarne cinque in parallelo verso lo stesso server
 * domestico li farebbe solo litigare per la banda.
 */
inline void syncAllGames(const std::string& hostPort,
                         const std::vector<std::pair<std::string, std::string>>& idAndDir) {
    if (hostPort.empty() || idAndDir.empty()) return;
    if (isCloud(hostPort)) {
        setUploadMessage("Il catalogo online si aggiorna pubblicando dallo Studio");
        return;
    }
    bool expected = false;
    if (!uploadBusy().compare_exchange_strong(expected, true)) {
        setUploadMessage("Invio gia' in corso, riprova tra poco");
        return;
    }

    std::thread([hostPort, idAndDir]() {
        std::string host;
        int port = kDefaultPort;
        splitHostPort(hostPort, host, port);

        int sent = 0, failed = 0;
        for (const auto& [id, dir] : idAndDir) {
            std::vector<std::uint8_t> zipBytes;
            std::string error;
            if (!ZipArchive::createFromFolder(dir, zipBytes, error)) { ++failed; continue; }

            std::string reply;
            if (blk::http::put(host, port, "/publish/" + id, zipBytes, reply)) ++sent;
            else ++failed;
        }

        char msg[160];
        std::snprintf(msg, sizeof(msg), "Sincronizzati %d giochi su %s%s",
                      sent, host.c_str(),
                      failed ? (" (" + std::to_string(failed) + " falliti)").c_str() : "");
        setUploadMessage(msg);
        LOG_INFO("games", "sync: %d inviati, %d falliti", sent, failed);
        uploadBusy().store(false);

        if (sent > 0) startFetch(hostPort);
    }).detach();
}

} // namespace remotegames
} // namespace blk
