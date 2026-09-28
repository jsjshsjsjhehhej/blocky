#pragma once

/**
 * Elenco dei server disponibili per un gioco.
 *
 * Perche' esiste: finora l'unico modo di entrare in una partita era
 * startAutoMatch(), che manda una richiesta in giro, prende la PRIMA risposta e
 * se non ne arriva nessuna si mette a ospitare per conto proprio. Se qualcosa
 * non funzionava — una risposta persa, un router che non inoltra il broadcast —
 * non restava niente da guardare: solo "nessun server", senza sapere se il
 * server non ci fosse o non si riuscisse a parlargli.
 *
 * Qui invece si raccolgono TUTTE le risposte e si mostrano. Se la lista e'
 * vuota lo si vede, se c'e' un server lo si sceglie: e' anche una diagnosi,
 * non solo una comodita'.
 *
 * La scansione gira su un thread a parte perche' aspetta le risposte per
 * qualche decimo di secondo, e farlo dentro il frame bloccherebbe il gioco.
 */

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "engine/LanNet.h"

namespace blk {
namespace serverbrowser {

struct Entry {
    std::string ip;
    int   port = 27015;
    std::string name;
    int   players = 0;
    int   maxPlayers = 8;

    std::string label() const {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "%s  -  %d/%d  -  %s:%d",
                      name.empty() ? "Server" : name.c_str(),
                      players, maxPlayers, ip.c_str(), port);
        return buf;
    }
};

inline std::mutex& mtx() { static std::mutex m; return m; }
inline std::vector<Entry>& resultRef() { static std::vector<Entry> v; return v; }
inline std::atomic<bool>& scanning() { static std::atomic<bool> b{false}; return b; }
inline std::atomic<bool>& done() { static std::atomic<bool> b{false}; return b; }

inline bool isScanning() { return scanning().load(); }
inline bool hasResult() { return done().load(); }

inline std::vector<Entry> result() {
    std::lock_guard<std::mutex> lock(mtx());
    return resultRef();
}

/**
 * Cerca i server e riempie l'elenco.
 *
 * Si usa un socket nuovo, separato da quello della partita: la scansione deve
 * poter girare anche mentre si e' gia' collegati da qualche parte, senza
 * rubare i pacchetti a quella connessione.
 */
inline void startScan(int port, int gameIdx) {
    bool expected = false;
    if (!scanning().compare_exchange_strong(expected, true)) return;
    done().store(false);

    std::thread([port, gameIdx]() {
        std::vector<Entry> found;

        netEnsureSockets();
        NetSocket probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (probe != kInvalidSock) {
            u_long nb = 1;
            ioctlsocket(probe, FIONBIO, &nb);
            BOOL yes = TRUE;
            setsockopt(probe, SOL_SOCKET, SO_BROADCAST, (const char*)&yes, sizeof(yes));

            // Gli stessi bersagli della ricerca automatica: il broadcast della
            // sottorete, quello globale, se stessi, e l'indirizzo del computer
            // che ospita i giochi — che di solito e' anche quello che ospita
            // le partite, e a cui si puo' chiedere direttamente anche quando il
            // router non inoltra il broadcast.
            std::vector<std::string> targets;
            const std::string subnet = localSubnetBroadcast();
            if (!subnet.empty()) targets.push_back(subnet);
            targets.push_back("255.255.255.255");
            targets.push_back("127.0.0.1");
            if (!netExtraDiscoveryHost().empty()) targets.push_back(netExtraDiscoveryHost());

            NetDiscover disc{};
            // Firma dei pacchetti Blocky: DEVE essere la stessa di fillMagic in
            // LanNet.h, altrimenti il server riceve la richiesta e la scarta.
            std::memcpy(disc.hdr.magic, "BLK1", 4);
            disc.hdr.type = (uint8_t)NetMsg::Discover;
            disc.hdr.peerId = 0;
            disc.hdr.gameIndex = (uint16_t)gameIdx;

            sockaddr_in dest{};
            dest.sin_family = AF_INET;
            dest.sin_port = htons((u_short)port);

            for (int round = 0; round < 3; ++round) {
                for (const std::string& tip : targets) {
                    inet_pton(AF_INET, tip.c_str(), &dest.sin_addr);
                    sendto(probe, (const char*)&disc, sizeof(disc), 0,
                           (sockaddr*)&dest, sizeof(dest));
                }
                netSleepMs(60);
            }

            // Si ascolta per circa un secondo e si tiene TUTTO quello che
            // risponde, non solo il primo.
            for (int i = 0; i < 100; ++i) {
                char buf[512];
                sockaddr_in from{};
                NetAddrLen fromLen = (NetAddrLen)sizeof(from);
                const int n = recvfrom(probe, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
                if (n >= (int)sizeof(NetAnnounce)) {
                    NetAnnounce ann{};
                    std::memcpy(&ann, buf, sizeof(ann));
                    if (std::memcmp(ann.hdr.magic, "BLK1", 4) == 0 &&
                        ann.hdr.type == (uint8_t)NetMsg::Announce) {
                        Entry e;
                        char ip[64] = {};
                        inet_ntop(AF_INET, (void*)&from.sin_addr, ip, sizeof(ip));
                        e.ip = ip;
                        e.port = ann.port ? ann.port : port;
                        e.players = ann.curPlayers;
                        e.maxPlayers = ann.maxPlayers ? ann.maxPlayers : 8;
                        ann.name[sizeof(ann.name) - 1] = 0;
                        e.name = ann.name;

                        bool dup = false;
                        for (const auto& f : found)
                            if (f.ip == e.ip && f.port == e.port) { dup = true; break; }
                        if (!dup) found.push_back(std::move(e));
                    }
                }
                netSleepMs(10);
            }
            closesocket(probe);
        }

        {
            std::lock_guard<std::mutex> lock(mtx());
            resultRef() = std::move(found);
        }
        std::printf("[Net] scansione finita: %d server\n", (int)resultRef().size());
        for (const auto& e : resultRef()) std::printf("[Net]   %s\n", e.label().c_str());
        done().store(true);
        scanning().store(false);
    }).detach();
}

} // namespace serverbrowser
} // namespace blk
