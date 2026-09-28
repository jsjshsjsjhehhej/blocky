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

#include <atomic>
#include <map>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/GameHub.h"
#include "engine/HttpClient.h"
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
/** Nome dei file che il server ha per un gioco, imparato leggendo il catalogo. */
struct RemoteFiles {
    std::string thumbnail;   // es. "thumbnail.png"
    std::string place;       // es. "place.blockyplace"
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
inline std::string ensureThumbnail(const std::string& host, int port,
                                   const std::string& id, const std::string& remoteName) {
    if (remoteName.empty()) return {};
    const std::string local = thumbnailPath(id);
    if (plat::fileExists(local) && plat::fileSize(local) > 0) return local;

    std::string body;
    if (!blk::http::get(host, port, "/games/" + id + "/" + remoteName, body, 8000) || body.empty())
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

    std::string host;
    int port = kDefaultPort;
    splitHostPort(hostPort, host, port);

    std::string body;
    if (!blk::http::get(host, port, "/games", body) || body.empty()) {
        LOG_INFO("games", "nessuna risposta da %s:%d", host.c_str(), port);
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
        pos += 4;
        if (e.id.empty()) continue;

        if (e.title.empty()) e.title = e.id;
        e.subtitle = desc.empty() ? ("Dal server " + host) : desc;
        e.isPublished = true;
        e.remote = true;
        e.accent = glm::vec3(0.18f, 0.45f, 0.78f);   // blu: viene dalla rete

        {
            std::lock_guard<std::mutex> lock(filesMutex());
            filesMap()[e.id] = RemoteFiles{thumb, place};
        }
        e.iconPath = ensureThumbnail(host, port, e.id, thumb);

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

inline std::mutex& dlMutex() { static std::mutex m; return m; }
inline std::string& dlDoneSlot() { static std::string s; return s; }
inline std::string& dlStatusSlot() { static std::string s; return s; }
inline std::atomic<bool>& dlBusy() { static std::atomic<bool> b{false}; return b; }

inline bool isDownloading() { return dlBusy().load(); }

/** Id del gioco appena scaricato, una volta sola. Vuoto se non e' pronto. */
inline std::string takeDownloadedId() {
    std::lock_guard<std::mutex> lock(dlMutex());
    std::string s = std::move(dlDoneSlot());
    dlDoneSlot().clear();
    return s;
}

inline std::string takeDownloadStatus() {
    std::lock_guard<std::mutex> lock(dlMutex());
    std::string s = std::move(dlStatusSlot());
    dlStatusSlot().clear();
    return s;
}

/**
 * Scarica dal server i file di un gioco e li mette in una cartella locale.
 *
 * Fino a ieri le card dei giochi del server si vedevano ma non si aprivano:
 * c'era l'elenco, non il contenuto. blocky_hostd serve gia' ogni file del
 * gioco su "GET /games/<id>/<file>", quindi bastava chiederglieli.
 *
 * I file sono pochi e noti (il posto, il pacchetto, lo script): si chiedono per
 * nome invece di leggere l'indice, perche' e' una richiesta in meno e l'elenco
 * di cosa contiene un pacchetto lo decide comunque publishGamePackage.
 */
inline void downloadGame(const std::string& hostPort, const std::string& gameId) {
    if (hostPort.empty() || gameId.empty()) return;
    bool expected = false;
    if (!dlBusy().compare_exchange_strong(expected, true)) return;

    std::thread([hostPort, gameId]() {
        std::string host;
        int port = kDefaultPort;
        splitHostPort(hostPort, host, port);

        const std::string dir = localGameDir(gameId);
        plat::makeDirs(dir);
        plat::makeDirs(dir + "/scripts");

        // place.json e' obbligatorio; gli altri sono utili ma un gioco senza
        // script resta comunque visitabile, quindi non fanno fallire tutto.
        // Il nome del file del mondo lo dice il catalogo: puo' essere
        // place.blockyplace, place.blocky o place.json a seconda di come il
        // gioco e' stato pubblicato. Se il server non l'ha detto si provano
        // comunque tutti, cosi' un gioco vecchio resta scaricabile.
        std::vector<std::string> files;
        const RemoteFiles known = filesFor(gameId);
        if (!known.place.empty()) files.push_back(known.place);
        for (const char* n : {"place.blockyplace", "place.blocky", "place.json"})
            if (known.place != n) files.push_back(n);
        for (const char* n : {"info.json", "game.blocky.json", "game_index.json",
                              "scripts/main.bbc", "scripts/main.lua"})
            files.push_back(n);
        if (!known.thumbnail.empty()) files.push_back(known.thumbnail);

        int got = 0;
        for (const std::string& rel : files) {
            std::string body;
            if (!blk::http::get(host, port, "/games/" + gameId + "/" + rel, body, 8000))
                continue;
            if (body.empty()) continue;
            std::ofstream f(dir + "/" + rel, std::ios::binary);
            if (!f) continue;
            f.write(body.data(), (std::streamsize)body.size());
            ++got;
        }

        {
            std::lock_guard<std::mutex> lock(dlMutex());
            if (isDownloaded(gameId)) {
                dlDoneSlot() = gameId;
                dlStatusSlot() = "Scaricato \"" + gameId + "\" (" + std::to_string(got) + " file)";
            } else {
                dlStatusSlot() = "Download di \"" + gameId + "\" fallito";
            }
        }
        LOG_INFO("games", "download %s: %d file", gameId.c_str(), got);
        dlBusy().store(false);
    }).detach();
}


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
