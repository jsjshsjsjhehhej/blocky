#pragma once

/**
 * Place Launcher: decide IN QUALE server entrare, come PlaceLauncher di Roblox.
 *
 * Prima il gioco entrava nel primo server che rispondeva (startAutoMatch) e un
 * server preciso si raggiungeva solo scrivendo IP e porta. Qui:
 *   - con un JobId si entra in QUEL server, oppure si dice perche' no
 *     (pieno, o non esiste piu');
 *   - senza JobId si sceglie il server piu' pieno tra quelli con posto, come
 *     fa Roblox: i giocatori si radunano invece di sparpagliarsi in tanti
 *     server mezzi vuoti;
 *   - con un codice blk:// si passa dal pool via internet.
 *
 * La ricerca e' quella di ServerBrowser (thread a parte): tick() va chiamato
 * a ogni frame finche' busy() e' vero, e non blocca mai.
 */

#include <cstdio>
#include <string>
#include <vector>

#include "engine/PlaceInstance.h"
#include "engine/ServerBrowser.h"

namespace blk {
namespace placelauncher {

enum class Status {
    Idle,        // nessuna richiesta
    Searching,   // ricerca in corso
    Join,        // entra in servers[serverIndex]
    JoinPool,    // entra con il codice blk://
    HostNew,     // nessun server: avvia la partita normalmente (diventi host)
    GameFull,    // il JobId chiesto c'e' ma e' pieno
    JobNotFound, // il JobId chiesto non ha risposto
    NoServer,    // solo server dedicati e nessuno con posto: non si gioca
};

inline bool isFailure(Status s) {
    return s == Status::GameFull || s == Status::JobNotFound || s == Status::NoServer;
}

struct Decision {
    Status status = Status::Idle;
    int serverIndex = -1;
};

/**
 * La scelta, separata dalla rete per poterla provare nei test.
 * wantJob vuoto = qualsiasi server del gioco. serverOnly = come Roblox: si
 * gioca solo sui server dedicati, i giocatori che ospitano non contano e
 * senza server non si apre una partita sul proprio dispositivo.
 */
inline Decision choose(const std::vector<serverbrowser::Entry>& servers, const std::string& wantJob,
                       bool serverOnly = false) {
    Decision d;
    auto usable = [&](const serverbrowser::Entry& e) { return !serverOnly || e.dedicated; };
    if (!wantJob.empty()) {
        for (size_t i = 0; i < servers.size(); ++i) {
            if (servers[i].jobId != wantJob || !usable(servers[i])) continue;
            d.serverIndex = (int)i;
            d.status = servers[i].full() ? Status::GameFull : Status::Join;
            return d;
        }
        d.status = Status::JobNotFound;
        return d;
    }
    // Il piu' pieno con almeno un posto; a parita', quello che ha risposto
    // prima (di solito il piu' vicino).
    int best = -1;
    for (size_t i = 0; i < servers.size(); ++i) {
        if (servers[i].full() || !usable(servers[i])) continue;
        if (best < 0 || servers[i].players > servers[(size_t)best].players) best = (int)i;
    }
    d.serverIndex = best;
    d.status = best >= 0 ? Status::Join : (serverOnly ? Status::NoServer : Status::HostNew);
    return d;
}

struct Request {
    int gameIndex = 0;
    std::string placeId;
    std::string jobId;    // vuoto = qualsiasi server
    std::string poolCode; // se c'e', niente ricerca: si entra dal pool
    int port = 27015;
    bool serverOnly = false; // solo server dedicati (vedi choose)
};

class Launcher {
public:
    /** Falso se c'e' gia' una richiesta in corso. */
    bool start(const Request& r) {
        if (busy()) return false;
        req_ = r;
        servers_.clear();
        decision_ = {};
        if (!req_.poolCode.empty()) {
            decision_.status = Status::JoinPool;
            setMessage("Entro nella stanza blk://" + req_.poolCode);
            return true;
        }
        decision_.status = Status::Searching;
        scanStarted_ = false;
        setMessage(req_.jobId.empty() ? "Cerco un server..."
                                      : "Cerco il server " + jobid::shortForm(req_.jobId) + "...");
        tick();
        return true;
    }

    void tick() {
        if (decision_.status != Status::Searching) return;
        if (!scanStarted_) {
            // Se l'elenco server del menu sta gia' cercando, si aspetta che
            // finisca: i suoi risultati potrebbero essere di un'altra porta.
            if (serverbrowser::isScanning()) return;
            serverbrowser::startScan(req_.port, req_.gameIndex);
            scanStarted_ = true;
            return;
        }
        if (serverbrowser::isScanning() || !serverbrowser::hasResult()) return;
        servers_ = serverbrowser::result();
        decision_ = choose(servers_, req_.jobId, req_.serverOnly);
        describe();
    }

    bool busy() const { return decision_.status == Status::Searching; }
    Status status() const { return decision_.status; }
    const Decision& decision() const { return decision_; }
    const Request& request() const { return req_; }
    const std::vector<serverbrowser::Entry>& servers() const { return servers_; }
    const std::string& message() const { return message_; }
    /** Testo mostrato durante il caricamento (es. "Avvio un server..."). */
    void showMessage(const std::string& m) { message_ = m; }

    /** Il server scelto (solo con Status::Join). */
    const serverbrowser::Entry* chosen() const {
        if (decision_.status != Status::Join) return nullptr;
        if (decision_.serverIndex < 0 || decision_.serverIndex >= (int)servers_.size()) return nullptr;
        return &servers_[(size_t)decision_.serverIndex];
    }

    void reset() {
        decision_ = {};
        servers_.clear();
        message_.clear();
        scanStarted_ = false;
    }

private:
    void setMessage(const std::string& m) { message_ = m; }

    void describe() {
        char buf[200];
        const serverbrowser::Entry* s = decision_.serverIndex >= 0 && decision_.serverIndex < (int)servers_.size()
                                      ? &servers_[(size_t)decision_.serverIndex] : nullptr;
        switch (decision_.status) {
        case Status::Join:
            std::snprintf(buf, sizeof(buf), "Entro in %s (%d/%d)", s->name.empty() ? "Server" : s->name.c_str(),
                          s->players, s->maxPlayers);
            break;
        case Status::HostNew:
            std::snprintf(buf, sizeof(buf), "Nessun server con posto: ne apri uno tu");
            break;
        case Status::GameFull:
            std::snprintf(buf, sizeof(buf), "Il server %s e' pieno (%d/%d): riprova piu' tardi",
                          jobid::shortForm(req_.jobId).c_str(), s->players, s->maxPlayers);
            break;
        case Status::NoServer:
            std::snprintf(buf, sizeof(buf),
                          servers_.empty() ? "Nessun server raggiungibile: controlla la connessione"
                                           : "Nessun server attivo con posto per questo gioco: riprova tra poco");
            break;
        case Status::JobNotFound:
            std::snprintf(buf, sizeof(buf),
                          "Server %s non trovato: la partita e' finita o non e' sulla tua rete",
                          jobid::shortForm(req_.jobId).c_str());
            break;
        default:
            buf[0] = '\0';
            break;
        }
        message_ = buf;
        std::printf("[PlaceLauncher] %s (%d server in rete)\n", message_.c_str(), (int)servers_.size());
    }

    Request req_;
    Decision decision_;
    std::vector<serverbrowser::Entry> servers_;
    std::string message_;
    bool scanStarted_ = false;
};

} // namespace placelauncher
} // namespace blk
