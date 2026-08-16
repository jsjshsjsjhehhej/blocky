#pragma once

/**
 * Multiplayer Blocky — UDP server/client (Winsock).
 * - Host in-game oppure server dedicato (BlockyServer.exe)
 * - Cap: maxPlayers (scelto in Blocky Game Engine alla publish)
 * - Sync: pos / yaw / walking + join / leave / reject se pieno
 */

#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include "../platform/Log.h"
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using NetSocket = SOCKET;
static constexpr NetSocket kInvalidSock = INVALID_SOCKET;
#else
#error "LanNet richiede Windows per ora"
#endif

enum class NetRole : uint8_t { Offline = 0, Host, Client };

enum class NetMsg : uint8_t {
    Hello = 1,
    Welcome = 2,
    State = 3,
    Leave = 4,
    Ping = 5,
    Reject = 6,
    Discover = 7,  // broadcast: chi cerca una sessione
    Announce = 8,  // risposta host: qui c'è posto
    RemoteEvent = 9, // payload stringa (nome evento + dati)
    ScriptReport = 10 // client: hash degli script che sta eseguendo
};

enum class NetRejectReason : uint8_t {
    Full = 1,
    BadGame = 2,
    BadClient = 3,  // impronta di integrita' assente o non in elenco
    BadScript = 4   // esegue uno script che non appartiene al gioco
};

#pragma pack(push, 1)
struct NetHeader {
    char     magic[4]; // "BLK1"
    uint8_t  type;
    uint8_t  peerId;
    uint16_t gameIndex;
};

struct NetHello {
    NetHeader hdr;
    char      name[24];
    /**
     * Impronta di integrita' del client (16 esadecimali + NUL), da Integrity.h.
     * Il client la calcola, il SERVER la giudica: e' l'unico punto in cui la
     * misura diventa una decisione. Vuota = client che non l'ha calcolata.
     */
    char      fingerprint[17];
};

/**
 * Elenco degli hash degli script che il client sta eseguendo.
 * 48 per pacchetto: sta comodo sotto l'MTU (48*4 + header = ~200 byte).
 */
struct NetScriptReport {
    NetHeader hdr;
    uint8_t   count;
    uint8_t   total;      // quanti in totale (se > 48 il resto e' troncato)
    uint32_t  hashes[48];
};

struct NetWelcome {
    NetHeader hdr;
    uint8_t   assignedId;
    uint8_t   maxPlayers;
    uint8_t   curPlayers;
    float     spawnX, spawnY, spawnZ;
};

struct NetState {
    NetHeader hdr;
    float     x, y, z;
    float     yaw;
    uint8_t   walking;
    char      name[24];
};

struct NetLeave {
    NetHeader hdr;
};

struct NetReject {
    NetHeader hdr;
    uint8_t   reason;
};

struct NetDiscover {
    NetHeader hdr;
};

struct NetAnnounce {
    NetHeader hdr;
    uint8_t   maxPlayers;
    uint8_t   curPlayers;
    uint16_t  port;
};

/** RemoteEvent: nome + payload testo (Lua string). */
struct NetRemoteEvent {
    NetHeader hdr;
    char      name[32];
    char      payload[160];
};
#pragma pack(pop)

struct IncomingRemoteEvent {
    char name[32] = {};
    char payload[160] = {};
    char fromName[24] = {};
    uint8_t fromId = 0;
    bool toServer = true; // true = OnServerEvent, false = OnClientEvent
};

struct RemotePlayer {
    uint8_t   id = 0;
    char      name[24] = {};
    glm::vec3 pos{0};
    float     yaw = 180.f;
    bool      walking = false;
    float     lastSeen = 0.f;
    glm::vec3 displayPos{0};
    float     displayYaw = 180.f;
};

struct LanNet {
    static constexpr int kMaxPeers = 32;

    NetRole role = NetRole::Offline;
    NetSocket sock = kInvalidSock;
    sockaddr_in hostAddr{};
    uint8_t localId = 0;
    uint8_t nextId = 1;
    int gameIndex = 0;
    int maxPlayers = 8;
    bool dedicated = false; // true = BlockyServer (nessun avatar locale)
    char localName[24] = "Player";
    char status[128] = {};
    char joinIp[64] = "127.0.0.1";
    int  joinPort = 27015;
    bool connected = false;
    bool rejected = false;

    /** Impronta del client locale: la riempie il chiamante (vedi Integrity.h). */
    char localFingerprint[17] = {};
    /**
     * Lato server: impronte ammesse.
     * Elenco vuoto = modalita' apprendimento — accetta e registra chi entra,
     * cosi' non ti chiudi fuori dal tuo stesso server la prima volta.
     * Elenco pieno = solo chi e' in elenco.
     */
    std::vector<std::string> allowedFingerprints;
    std::vector<std::string> seenFingerprints;   // raccolte in apprendimento

    /**
     * Lato server: hash degli script legittimi del gioco.
     * Vuoto = apprendimento (registra quello che arriva, non caccia nessuno).
     */
    std::vector<uint32_t> allowedScripts;
    std::vector<uint32_t> seenScripts;
    /** Lato client: hash da riportare, riempiti dal chiamante ogni volta che cambiano. */
    std::vector<uint32_t> localScriptHashes;
    float scriptReportAcc = 0.f;
    float sendAcc = 0.f;
    float now = 0.f;

    std::vector<RemotePlayer> remotes;

    struct PeerSlot {
        bool used = false;
        uint8_t id = 0;
        sockaddr_in addr{};
        float lastSeen = 0.f;
        char name[24] = {};
    };
    PeerSlot peers[kMaxPeers]{};
    std::vector<IncomingRemoteEvent> incomingRemotes;

    static bool ensureWsa() {
        static bool ok = false;
        static bool tried = false;
        if (tried) return ok;
        tried = true;
        WSADATA wsa{};
        ok = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
        return ok;
    }

    static void fillMagic(NetHeader& h, NetMsg t, uint8_t id, uint16_t gi) {
        h.magic[0] = 'B'; h.magic[1] = 'L'; h.magic[2] = 'K'; h.magic[3] = '1';
        h.type = (uint8_t)t;
        h.peerId = id;
        h.gameIndex = gi;
    }

    static bool checkMagic(const NetHeader& h) {
        return h.magic[0] == 'B' && h.magic[1] == 'L' && h.magic[2] == 'K' && h.magic[3] == '1';
    }

    void setStatus(const char* s) {
        std::snprintf(status, sizeof(status), "%s", s);
        std::printf("[Net] %s\n", status);
    }

    void clearRemotes() { remotes.clear(); }

    int connectedClients() const {
        int n = 0;
        for (const auto& p : peers)
            if (p.used) ++n;
        return n;
    }

    /** Slot totali occupati (host giocatore + client, oppure solo client se dedicated). */
    int occupiedSlots() const {
        int n = connectedClients();
        if (role == NetRole::Host && !dedicated) ++n; // host conta come giocatore
        return n;
    }

    int freeSlots() const {
        return std::max(0, maxPlayers - occupiedSlots());
    }

    bool isFull() const { return freeSlots() <= 0; }

    void shutdown() {
        if (sock != kInvalidSock) {
            if (role == NetRole::Client && connected) {
                NetLeave leave{};
                fillMagic(leave.hdr, NetMsg::Leave, localId, (uint16_t)gameIndex);
                sendto(sock, (const char*)&leave, sizeof(leave), 0,
                       (sockaddr*)&hostAddr, sizeof(hostAddr));
            }
            closesocket(sock);
            sock = kInvalidSock;
        }
        role = NetRole::Offline;
        connected = false;
        rejected = false;
        dedicated = false;
        localId = 0;
        nextId = 1;
        clearRemotes();
        incomingRemotes.clear();
        for (auto& p : peers) p = {};
        setStatus("Offline");
    }

    bool bindUdp(int port) {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == kInvalidSock) { setStatus("socket fallita"); return false; }
        u_long nonblock = 1;
        ioctlsocket(sock, FIONBIO, &nonblock);
        BOOL reuse = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
        BOOL broadcast = TRUE;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));

        sockaddr_in bindAddr{};
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        bindAddr.sin_port = htons((u_short)port);
        if (::bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) != 0) {
            setStatus("bind porta fallita (in uso?)");
            closesocket(sock);
            sock = kInvalidSock;
            return false;
        }
        return true;
    }

    /** Host giocatore (conta 1 slot). maxP = limite dal Game Engine. */
    bool startHost(int port, int gameIdx, const char* name, int maxP = 8) {
        shutdown();
        if (!ensureWsa()) { setStatus("WSAStartup fallita"); return false; }
        maxPlayers = std::clamp(maxP, 1, kMaxPeers);
        if (!bindUdp(port)) return false;

        role = NetRole::Host;
        dedicated = false;
        connected = true;
        localId = 1;
        nextId = 2;
        gameIndex = gameIdx;
        joinPort = port;
        std::snprintf(localName, sizeof(localName), "%.23s", name && name[0] ? name : "Host");
        std::snprintf(status, sizeof(status), "Server :%d · max %d · 1/%d", port, maxPlayers, maxPlayers);
        std::printf("[Net] %s\n", status);
        return true;
    }

    /** Server dedicato (nessun avatar): fino a maxP client. */
    bool startDedicated(int port, int gameIdx, int maxP, const char* title) {
        shutdown();
        if (!ensureWsa()) { setStatus("WSAStartup fallita"); return false; }
        maxPlayers = std::clamp(maxP, 1, kMaxPeers);
        if (!bindUdp(port)) return false;

        role = NetRole::Host;
        dedicated = true;
        connected = true;
        localId = 0;
        nextId = 1;
        gameIndex = gameIdx;
        joinPort = port;
        std::snprintf(localName, sizeof(localName), "%.23s", title && title[0] ? title : "Server");
        std::snprintf(status, sizeof(status), "DEDICATO :%d · 0/%d · %s", port, maxPlayers, localName);
        std::printf("[Net] %s\n", status);
        return true;
    }

    bool startClient(const char* ip, int port, int gameIdx, const char* name) {
        shutdown();
        if (!ensureWsa()) { setStatus("WSAStartup fallita"); return false; }

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == kInvalidSock) { setStatus("socket fallita"); return false; }
        u_long nonblock = 1;
        ioctlsocket(sock, FIONBIO, &nonblock);

        hostAddr = {};
        hostAddr.sin_family = AF_INET;
        hostAddr.sin_port = htons((u_short)port);
        if (inet_pton(AF_INET, ip && ip[0] ? ip : "127.0.0.1", &hostAddr.sin_addr) != 1) {
            setStatus("IP non valido");
            closesocket(sock);
            sock = kInvalidSock;
            return false;
        }

        role = NetRole::Client;
        connected = false;
        rejected = false;
        localId = 0;
        gameIndex = gameIdx;
        std::snprintf(joinIp, sizeof(joinIp), "%s", ip && ip[0] ? ip : "127.0.0.1");
        joinPort = port;
        std::snprintf(localName, sizeof(localName), "%.23s", name && name[0] ? name : "Player");

        NetHello hello{};
        fillMagic(hello.hdr, NetMsg::Hello, 0, (uint16_t)gameIdx);
        std::snprintf(hello.name, sizeof(hello.name), "%s", localName);
        std::snprintf(hello.fingerprint, sizeof(hello.fingerprint), "%s", localFingerprint);
        sendto(sock, (const char*)&hello, sizeof(hello), 0,
               (sockaddr*)&hostAddr, sizeof(hostAddr));
        setStatus("Connessione al server…");
        return true;
    }

    RemotePlayer* findRemote(uint8_t id) {
        for (auto& r : remotes)
            if (r.id == id) return &r;
        return nullptr;
    }

    void upsertRemote(uint8_t id, const char* name, glm::vec3 pos, float yaw, bool walking) {
        if (id == 0 || id == localId) return;
        RemotePlayer* r = findRemote(id);
        if (!r) {
            remotes.push_back({});
            r = &remotes.back();
            r->id = id;
            r->displayPos = pos;
            r->displayYaw = yaw;
        }
        if (name && name[0])
            std::snprintf(r->name, sizeof(r->name), "%.23s", name);
        r->pos = pos;
        r->yaw = yaw;
        r->walking = walking;
        r->lastSeen = now;
    }

    void removeRemote(uint8_t id) {
        remotes.erase(std::remove_if(remotes.begin(), remotes.end(),
            [id](const RemotePlayer& r) { return r.id == id; }), remotes.end());
    }

    int findPeerSlotByAddr(const sockaddr_in& a) {
        for (int i = 0; i < kMaxPeers; ++i) {
            if (!peers[i].used) continue;
            if (peers[i].addr.sin_addr.s_addr == a.sin_addr.s_addr
                && peers[i].addr.sin_port == a.sin_port)
                return i;
        }
        return -1;
    }

    int allocPeer(const sockaddr_in& a, const char* name) {
        int existing = findPeerSlotByAddr(a);
        if (existing >= 0) return existing;
        if (isFull()) return -1;
        for (int i = 0; i < kMaxPeers; ++i) {
            if (peers[i].used) continue;
            peers[i].used = true;
            peers[i].id = nextId++;
            if (nextId == 0) nextId = 1;
            peers[i].addr = a;
            peers[i].lastSeen = now;
            std::snprintf(peers[i].name, sizeof(peers[i].name), "%.23s",
                          name && name[0] ? name : "Player");
            return i;
        }
        return -1;
    }

    void sendTo(const sockaddr_in& addr, const void* data, int len) {
        sendto(sock, (const char*)data, len, 0, (const sockaddr*)&addr, sizeof(addr));
    }

    void relayExcept(uint8_t exceptId, const void* data, int len) {
        for (auto& p : peers) {
            if (!p.used || p.id == exceptId) continue;
            sendTo(p.addr, data, len);
        }
    }

    void broadcastAll(const void* data, int len) {
        for (auto& p : peers) {
            if (!p.used) continue;
            sendTo(p.addr, data, len);
        }
    }

    void updateHostStatus() {
        if (role != NetRole::Host) return;
        if (dedicated)
            std::snprintf(status, sizeof(status), "DEDICATO :%d · %d/%d",
                          joinPort, occupiedSlots(), maxPlayers);
        else
            std::snprintf(status, sizeof(status), "Server :%d · %d/%d",
                          joinPort, occupiedSlots(), maxPlayers);
    }

    void poll(float timeSec) {
        now = timeSec;
        if (sock == kInvalidSock || role == NetRole::Offline) return;

        for (;;) {
            char buf[512];
            sockaddr_in from{};
            int fromLen = sizeof(from);
            int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
            if (n <= 0) break;
            if (n < (int)sizeof(NetHeader)) continue;
            NetHeader hdr{};
            std::memcpy(&hdr, buf, sizeof(hdr));
            if (!checkMagic(hdr)) continue;

            if (role == NetRole::Host)
                handleHostPacket(buf, n, from, hdr);
            else
                handleClientPacket(buf, n, hdr);
        }

        for (int i = (int)remotes.size() - 1; i >= 0; --i) {
            if (now - remotes[i].lastSeen > 4.f)
                remotes.erase(remotes.begin() + i);
        }
        if (role == NetRole::Host) {
            for (auto& p : peers) {
                if (p.used && now - p.lastSeen > 6.f) {
                    removeRemote(p.id);
                    NetLeave leave{};
                    fillMagic(leave.hdr, NetMsg::Leave, p.id, (uint16_t)gameIndex);
                    relayExcept(p.id, &leave, sizeof(leave));
                    p = {};
                    updateHostStatus();
                }
            }
        }
    }

    /** Caccia un peer: lo avvisa, lo toglie dalla lista e lo annuncia agli altri. */
    void kickPeer(int slot, NetRejectReason why, const char* detail) {
        if (slot < 0 || slot >= kMaxPeers || !peers[slot].used) return;
        NetReject rej{};
        fillMagic(rej.hdr, NetMsg::Reject, 0, (uint16_t)gameIndex);
        rej.reason = (uint8_t)why;
        sendTo(peers[slot].addr, &rej, sizeof(rej));

        uint8_t id = peers[slot].id;
        LOG_SEC("net", "KICK #%d %s — %s", (int)id, peers[slot].name, detail);
        removeRemote(id);
        NetLeave leave{};
        fillMagic(leave.hdr, NetMsg::Leave, id, (uint16_t)gameIndex);
        relayExcept(id, &leave, sizeof(leave));
        peers[slot] = {};
        updateHostStatus();
    }

    /**
     * Verifica gli script dichiarati da un client.
     * Uno che non appartiene al gioco = script iniettato = fuori.
     */
    void handleScriptReport(const char* buf, int n, const sockaddr_in& from) {
        if (n < (int)sizeof(NetScriptReport)) return;
        int slot = findPeerSlotByAddr(from);
        if (slot < 0) return;

        NetScriptReport rep{};
        std::memcpy(&rep, buf, sizeof(rep));
        int cnt = rep.count > 48 ? 48 : rep.count;

        if (allowedScripts.empty()) {
            // Apprendimento: raccogli, non cacciare.
            for (int i = 0; i < cnt; ++i) {
                uint32_t h = rep.hashes[i];
                bool known = false;
                for (uint32_t s : seenScripts) if (s == h) { known = true; break; }
                if (!known && seenScripts.size() < 512) {
                    seenScripts.push_back(h);
                    LOG_SEC("net", "Script visto: %08X (da %s)",
                            (unsigned)h, peers[slot].name);
                }
            }
            return;
        }

        for (int i = 0; i < cnt; ++i) {
            uint32_t h = rep.hashes[i];
            bool ok = false;
            for (uint32_t a : allowedScripts) if (a == h) { ok = true; break; }
            if (!ok) {
                // Il dettaglio serve solo al log di sviluppo; in distribuzione
                // non costruirlo, cosi' la stringa non resta nel binario.
                const char* detail = "";
#ifdef BLOCKY_DEV
                char buf2[96];
                std::snprintf(buf2, sizeof(buf2), "script estraneo %08X", (unsigned)h);
                detail = buf2;
#endif
                kickPeer(slot, NetRejectReason::BadScript, detail);
                return;
            }
        }
    }

    void handleHostPacket(const char* buf, int n, const sockaddr_in& from, const NetHeader& hdr) {
        if (hdr.type == (uint8_t)NetMsg::ScriptReport) {
            handleScriptReport(buf, n, from);
            return;
        }
        // Qualcuno cerca una partita → rispondi se c'è posto (stesso gioco)
        if (hdr.type == (uint8_t)NetMsg::Discover) {
            if ((int)hdr.gameIndex != gameIndex) return;
            if (isFull()) return;
            NetAnnounce ann{};
            fillMagic(ann.hdr, NetMsg::Announce, dedicated ? 0 : 1, (uint16_t)gameIndex);
            ann.maxPlayers = (uint8_t)maxPlayers;
            ann.curPlayers = (uint8_t)occupiedSlots();
            ann.port = (uint16_t)joinPort;
            sendTo(from, &ann, sizeof(ann));
            return;
        }

        if (hdr.type == (uint8_t)NetMsg::Hello && n >= (int)sizeof(NetHello)) {
            NetHello hello{};
            std::memcpy(&hello, buf, sizeof(hello));

            // --- verdetto sull'integrita' del client ---
            // Il client si e' misurato da solo; qui decidiamo noi se vale.
            hello.fingerprint[16] = '\0';
            std::string fp = hello.fingerprint;
            if (!allowedFingerprints.empty()) {
                bool listed = false;
                for (const std::string& a : allowedFingerprints)
                    if (a == fp) { listed = true; break; }
                if (fp.empty() || !listed) {
                    NetReject rej{};
                    fillMagic(rej.hdr, NetMsg::Reject, 0, (uint16_t)gameIndex);
                    rej.reason = (uint8_t)NetRejectReason::BadClient;
                    sendTo(from, &rej, sizeof(rej));
                    LOG_SEC("net", "Reject CLIENT '%s' impronta=%s",
                            hello.name, fp.empty() ? "(assente)" : fp.c_str());
                    return;
                }
            } else if (!fp.empty()) {
                bool known = false;
                for (const std::string& s : seenFingerprints)
                    if (s == fp) { known = true; break; }
                if (!known && seenFingerprints.size() < 64) {
                    seenFingerprints.push_back(fp);
                    LOG_SEC("net", "Apprendimento: impronta %s (%s)",
                            fp.c_str(), hello.name);
                }
            }

            // già connesso → re-welcome
            int existing = findPeerSlotByAddr(from);
            if (existing < 0 && isFull()) {
                NetReject rej{};
                fillMagic(rej.hdr, NetMsg::Reject, 0, (uint16_t)gameIndex);
                rej.reason = (uint8_t)NetRejectReason::Full;
                sendTo(from, &rej, sizeof(rej));
                std::printf("[Net] Reject FULL da %s\n", hello.name);
                return;
            }

            int slot = allocPeer(from, hello.name);
            if (slot < 0) {
                NetReject rej{};
                fillMagic(rej.hdr, NetMsg::Reject, 0, (uint16_t)gameIndex);
                rej.reason = (uint8_t)NetRejectReason::Full;
                sendTo(from, &rej, sizeof(rej));
                return;
            }
            peers[slot].lastSeen = now;

            NetWelcome wel{};
            fillMagic(wel.hdr, NetMsg::Welcome, dedicated ? 0 : 1, (uint16_t)gameIndex);
            wel.assignedId = peers[slot].id;
            wel.maxPlayers = (uint8_t)maxPlayers;
            wel.curPlayers = (uint8_t)occupiedSlots();
            wel.spawnX = (float)((peers[slot].id % 5) * 3);
            wel.spawnY = 0.f;
            wel.spawnZ = (float)((peers[slot].id / 5) * 3 + 2);
            sendTo(from, &wel, sizeof(wel));

            upsertRemote(peers[slot].id, peers[slot].name,
                         {wel.spawnX, wel.spawnY, wel.spawnZ}, 180.f, false);
            updateHostStatus();
            std::printf("[Net] Join #%d %s (%d/%d)\n",
                        (int)peers[slot].id, peers[slot].name, occupiedSlots(), maxPlayers);
            return;
        }

        if (hdr.type == (uint8_t)NetMsg::State && n >= (int)sizeof(NetState)) {
            NetState st{};
            std::memcpy(&st, buf, sizeof(st));
            int slot = findPeerSlotByAddr(from);
            if (slot < 0) {
                slot = allocPeer(from, st.name);
                if (slot < 0) return;
            }
            peers[slot].lastSeen = now;
            if (st.name[0])
                std::snprintf(peers[slot].name, sizeof(peers[slot].name), "%.23s", st.name);

            upsertRemote(peers[slot].id, peers[slot].name,
                         {st.x, st.y, st.z}, st.yaw, st.walking != 0);

            st.hdr.peerId = peers[slot].id;
            relayExcept(peers[slot].id, &st, sizeof(st));
            return;
        }

        if (hdr.type == (uint8_t)NetMsg::Leave) {
            int slot = findPeerSlotByAddr(from);
            if (slot >= 0) {
                uint8_t id = peers[slot].id;
                removeRemote(id);
                NetLeave leave{};
                fillMagic(leave.hdr, NetMsg::Leave, id, (uint16_t)gameIndex);
                relayExcept(id, &leave, sizeof(leave));
                peers[slot] = {};
                updateHostStatus();
            }
            return;
        }

        if (hdr.type == (uint8_t)NetMsg::RemoteEvent && n >= (int)sizeof(NetRemoteEvent)) {
            NetRemoteEvent ev{};
            std::memcpy(&ev, buf, sizeof(ev));
            int slot = findPeerSlotByAddr(from);
            const char* fromNm = (slot >= 0) ? peers[slot].name : "Player";
            uint8_t fromId = (slot >= 0) ? peers[slot].id : hdr.peerId;

            IncomingRemoteEvent in{};
            std::snprintf(in.name, sizeof(in.name), "%.31s", ev.name);
            std::snprintf(in.payload, sizeof(in.payload), "%.159s", ev.payload);
            std::snprintf(in.fromName, sizeof(in.fromName), "%.23s", fromNm);
            in.fromId = fromId;
            in.toServer = true; // client → host → OnServerEvent
            incomingRemotes.push_back(in);
            return;
        }
    }

    void handleClientPacket(const char* buf, int n, const NetHeader& hdr) {
        if (hdr.type == (uint8_t)NetMsg::Reject && n >= (int)sizeof(NetReject)) {
            NetReject rej{};
            std::memcpy(&rej, buf, sizeof(rej));
            rejected = true;
            connected = false;
            if (rej.reason == (uint8_t)NetRejectReason::Full)
                setStatus("Server pieno — riprova più tardi");
            else
                setStatus("Connessione rifiutata");
            return;
        }
        if (hdr.type == (uint8_t)NetMsg::Welcome && n >= (int)sizeof(NetWelcome)) {
            NetWelcome wel{};
            std::memcpy(&wel, buf, sizeof(wel));
            localId = wel.assignedId;
            if (wel.maxPlayers > 0) maxPlayers = wel.maxPlayers;
            connected = true;
            rejected = false;
            std::snprintf(status, sizeof(status), "In gioco #%d · %d/%d @ %s:%d",
                          (int)localId, (int)wel.curPlayers, maxPlayers, joinIp, joinPort);
            std::printf("[Net] %s\n", status);
            return;
        }
        if (hdr.type == (uint8_t)NetMsg::State && n >= (int)sizeof(NetState)) {
            NetState st{};
            std::memcpy(&st, buf, sizeof(st));
            if (st.hdr.peerId == localId) return;
            upsertRemote(st.hdr.peerId, st.name, {st.x, st.y, st.z}, st.yaw, st.walking != 0);
            return;
        }
        if (hdr.type == (uint8_t)NetMsg::Leave) {
            removeRemote(hdr.peerId);
            return;
        }
        if (hdr.type == (uint8_t)NetMsg::RemoteEvent && n >= (int)sizeof(NetRemoteEvent)) {
            NetRemoteEvent ev{};
            std::memcpy(&ev, buf, sizeof(ev));
            IncomingRemoteEvent in{};
            std::snprintf(in.name, sizeof(in.name), "%.31s", ev.name);
            std::snprintf(in.payload, sizeof(in.payload), "%.159s", ev.payload);
            std::snprintf(in.fromName, sizeof(in.fromName), "%.23s",
                          ev.hdr.peerId ? "Peer" : "Host");
            // Cerca nome peer
            for (const auto& r : remotes) {
                if (r.id == ev.hdr.peerId) {
                    std::snprintf(in.fromName, sizeof(in.fromName), "%.23s", r.name);
                    break;
                }
            }
            if (ev.hdr.peerId == 0 || ev.hdr.peerId == 1)
                std::snprintf(in.fromName, sizeof(in.fromName), "Host");
            in.fromId = ev.hdr.peerId;
            in.toServer = false; // host → client (o relay)
            incomingRemotes.push_back(in);
        }
    }

    /** Client → Host (OnServerEvent). Host esegue e relay. */
    bool sendRemoteEventToServer(const char* name, const char* payload) {
        if (sock == kInvalidSock || role == NetRole::Offline) return false;
        NetRemoteEvent ev{};
        fillMagic(ev.hdr, NetMsg::RemoteEvent, localId, (uint16_t)gameIndex);
        std::snprintf(ev.name, sizeof(ev.name), "%.31s", name ? name : "Event");
        std::snprintf(ev.payload, sizeof(ev.payload), "%.159s", payload ? payload : "");
        if (role == NetRole::Host) {
            // Locale: tratta come arrivo server
            IncomingRemoteEvent in{};
            std::snprintf(in.name, sizeof(in.name), "%s", ev.name);
            std::snprintf(in.payload, sizeof(in.payload), "%s", ev.payload);
            std::snprintf(in.fromName, sizeof(in.fromName), "%s", localName);
            in.fromId = localId;
            in.toServer = true;
            incomingRemotes.push_back(in);
            return true; // host non invia a se stesso in rete
        }
        sendto(sock, (const char*)&ev, sizeof(ev), 0,
               (sockaddr*)&hostAddr, sizeof(hostAddr));
        return true;
    }

    /** Host → tutti i client (OnClientEvent). */
    bool fireRemoteEventAllClients(const char* name, const char* payload) {
        if (sock == kInvalidSock || role != NetRole::Host) return false;
        NetRemoteEvent ev{};
        fillMagic(ev.hdr, NetMsg::RemoteEvent, localId, (uint16_t)gameIndex);
        std::snprintf(ev.name, sizeof(ev.name), "%.31s", name ? name : "Event");
        std::snprintf(ev.payload, sizeof(ev.payload), "%.159s", payload ? payload : "");
        // Locale anche OnClientEvent
        IncomingRemoteEvent in{};
        std::snprintf(in.name, sizeof(in.name), "%s", ev.name);
        std::snprintf(in.payload, sizeof(in.payload), "%s", ev.payload);
        std::snprintf(in.fromName, sizeof(in.fromName), "%s", localName);
        in.fromId = localId;
        in.toServer = false;
        incomingRemotes.push_back(in);
        broadcastAll(&ev, sizeof(ev));
        return true;
    }

    /**
     * Manda al server gli hash degli script in esecuzione.
     * Ripetuto ogni 3s e non solo all'ingresso: un executor inietta DOPO
     * essere entrato, quindi un controllo solo iniziale non vedrebbe nulla.
     */
    void sendScriptReport(float dt) {
        if (role != NetRole::Client || !connected) return;
        scriptReportAcc += dt;
        if (scriptReportAcc < 3.f) return;
        scriptReportAcc = 0.f;

        NetScriptReport rep{};
        fillMagic(rep.hdr, NetMsg::ScriptReport, localId, (uint16_t)gameIndex);
        size_t n = localScriptHashes.size();
        rep.total = (uint8_t)(n > 255 ? 255 : n);
        size_t cnt = n > 48 ? 48 : n;
        rep.count = (uint8_t)cnt;
        for (size_t i = 0; i < cnt; ++i) rep.hashes[i] = localScriptHashes[i];
        sendto(sock, (const char*)&rep, sizeof(rep), 0,
               (sockaddr*)&hostAddr, sizeof(hostAddr));
    }

    void sendLocalState(float dt, glm::vec3 pos, float yaw, bool walking) {
        if (sock == kInvalidSock || role == NetRole::Offline) return;
        if (dedicated) return; // server dedicato non ha avatar
        sendScriptReport(dt);

        if (role == NetRole::Client && !connected) {
            if (rejected) return;
            sendAcc += dt;
            if (sendAcc >= 1.f) {
                sendAcc = 0.f;
                NetHello hello{};
                fillMagic(hello.hdr, NetMsg::Hello, 0, (uint16_t)gameIndex);
                std::snprintf(hello.name, sizeof(hello.name), "%s", localName);
                std::snprintf(hello.fingerprint, sizeof(hello.fingerprint), "%s", localFingerprint);
                sendto(sock, (const char*)&hello, sizeof(hello), 0,
                       (sockaddr*)&hostAddr, sizeof(hostAddr));
            }
            return;
        }

        sendAcc += dt;
        if (sendAcc < 0.05f) return;
        sendAcc = 0.f;

        NetState st{};
        fillMagic(st.hdr, NetMsg::State, localId, (uint16_t)gameIndex);
        st.x = pos.x; st.y = pos.y; st.z = pos.z;
        st.yaw = yaw;
        st.walking = walking ? 1 : 0;
        std::snprintf(st.name, sizeof(st.name), "%s", localName);

        if (role == NetRole::Host)
            broadcastAll(&st, sizeof(st));
        else
            sendto(sock, (const char*)&st, sizeof(st), 0,
                   (sockaddr*)&hostAddr, sizeof(hostAddr));
    }

    void updateDisplay(float dt) {
        for (auto& r : remotes) {
            r.displayPos = glm::mix(r.displayPos, r.pos, std::min(1.f, 12.f * dt));
            float dy = r.yaw - r.displayYaw;
            while (dy > 180.f) dy -= 360.f;
            while (dy < -180.f) dy += 360.f;
            r.displayYaw += dy * std::min(1.f, 12.f * dt);
        }
    }

    int playerCount() const { return occupiedSlots(); }

    /**
     * Auto-match su Play: cerca un host LAN per questo gioco;
     * se nessuno risponde entro ~0.7s, diventi tu il server.
     * Così tutti quelli che premono PLAY finiscono nella stessa sessione.
     */
    bool startAutoMatch(int port, int gameIdx, const char* name, int maxP = 8) {
        if (!ensureWsa()) { setStatus("WSAStartup fallita"); return false; }
        maxPlayers = std::clamp(maxP, 1, kMaxPeers);
        gameIndex = gameIdx;

        NetSocket probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (probe == kInvalidSock) {
            return startHost(port, gameIdx, name, maxP);
        }
        u_long nonblock = 1;
        ioctlsocket(probe, FIONBIO, &nonblock);
        BOOL broadcast = TRUE;
        setsockopt(probe, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));

        sockaddr_in any{};
        any.sin_family = AF_INET;
        any.sin_addr.s_addr = htonl(INADDR_ANY);
        any.sin_port = htons(0);
        ::bind(probe, (sockaddr*)&any, sizeof(any));

        NetDiscover disc{};
        fillMagic(disc.hdr, NetMsg::Discover, 0, (uint16_t)gameIdx);

        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_port = htons((u_short)port);

        // localhost + broadcast LAN
        const char* targets[] = { "127.0.0.1", "255.255.255.255" };
        for (int round = 0; round < 3; ++round) {
            for (const char* tip : targets) {
                inet_pton(AF_INET, tip, &dest.sin_addr);
                sendto(probe, (const char*)&disc, sizeof(disc), 0,
                       (sockaddr*)&dest, sizeof(dest));
            }
#ifdef _WIN32
            Sleep(80);
#endif
        }

        char foundIp[64] = {};
        int foundPort = port;
        for (int i = 0; i < 70; ++i) {
            char buf[256];
            sockaddr_in from{};
            int fromLen = sizeof(from);
            int n = recvfrom(probe, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
            if (n >= (int)sizeof(NetAnnounce)) {
                NetHeader hdr{};
                std::memcpy(&hdr, buf, sizeof(hdr));
                if (checkMagic(hdr) && hdr.type == (uint8_t)NetMsg::Announce
                    && (int)hdr.gameIndex == gameIdx) {
                    NetAnnounce ann{};
                    std::memcpy(&ann, buf, sizeof(ann));
                    if (ann.curPlayers < ann.maxPlayers) {
                        inet_ntop(AF_INET, &from.sin_addr, foundIp, sizeof(foundIp));
                        foundPort = ann.port > 0 ? (int)ann.port : port;
                        break;
                    }
                }
            }
#ifdef _WIN32
            Sleep(10);
#endif
        }
        closesocket(probe);

        if (foundIp[0]) {
            std::printf("[Net] Trovato server %s:%d — join automatico\n", foundIp, foundPort);
            return startClient(foundIp, foundPort, gameIdx, name);
        }
        std::printf("[Net] Nessun server — diventi host (max %d)\n", maxP);
        return startHost(port, gameIdx, name, maxP);
    }
};
