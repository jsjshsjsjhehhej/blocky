#pragma once


#include <glm/glm.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include "../platform/Log.h"
#include "../platform/SystemApi.h"
#include "../platform/TargetPlatform.h"
#include "PoolProtocol.h"
using namespace blk::PoolProtocol; // pacchetti Register/Resolve/Relay... vedi PoolProtocol.h
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using NetSocket = SOCKET;
static constexpr NetSocket kInvalidSock = INVALID_SOCKET;
using NetAddrLen = int;          // recvfrom() su Winsock vuole un int*
#else
// Socket BSD (Android/Linux). Winsock e' nato come copia di questa API,
// quindi il corpo di LanNet resta identico: cambiano solo il tipo del
// socket, il valore "non valido", come si chiude e come si mette in
// non-bloccante. Le funzioni vere (socket/bind/sendto/recvfrom/inet_pton)
// hanno lo stesso nome e la stessa firma.
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using NetSocket = int;
static constexpr NetSocket kInvalidSock = -1;
using NetAddrLen = socklen_t;    // ...su POSIX invece un socklen_t*
using BOOL = int;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
inline int closesocket(NetSocket s) { return ::close(s); }
/** Equivalente di ioctlsocket(FIONBIO): l'unica differenza vera di API. */
inline int ioctlsocket(NetSocket s, long cmd, unsigned long* argp) {
    (void)cmd;
    int flags = ::fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    if (argp && *argp) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    return ::fcntl(s, F_SETFL, flags);
}
#ifndef FIONBIO
#define FIONBIO 0x5421
#endif
#endif

/**
 * Pausa portabile.
 *
 * Prima queste attese erano scritte come "#ifdef _WIN32 Sleep(n) #endif", il
 * che fuori da Windows non le eseguiva affatto: i cicli che aspettano una
 * risposta dalla rete giravano a vuoto in pochi microsecondi e rinunciavano
 * prima che il server potesse anche solo ricevere la domanda. Su Android era
 * il motivo per cui la ricerca automatica del server non trovava mai nulla e
 * il codice blk:// non si collegava.
 */
inline void netSleepMs(int ms) {
    if (ms <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}



/**
 * Indirizzo di broadcast della rete a cui siamo attaccati (es. 192.168.1.255).
 *
 * Il broadcast "globale" 255.255.255.255 sembra la scelta ovvia, ma Android
 * spesso non lo instrada affatto: la ricerca automatica del server partiva e
 * non raggiungeva nessuno, quindi il telefono concludeva "nessun server" e si
 * metteva a ospitare per conto suo. Il broadcast della propria sottorete passa
 * su tutte le piattaforme.
 *
 * L'indirizzo locale si ricava "collegando" un socket UDP a un indirizzo
 * qualsiasi: UDP non apre davvero una connessione e non manda nulla, ma il
 * sistema sceglie comunque da quale scheda di rete uscirebbe — ed e' proprio
 * quello che vogliamo sapere. Si assume una rete /24, che e' quella di
 * praticamente ogni router domestico.
 */
inline std::string localSubnetBroadcast() {
    NetSocket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kInvalidSock) return {};

    sockaddr_in probe{};
    probe.sin_family = AF_INET;
    probe.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &probe.sin_addr);

    std::string out;
    if (connect(s, (sockaddr*)&probe, sizeof(probe)) == 0) {
        sockaddr_in local{};
        NetAddrLen len = (NetAddrLen)sizeof(local);
        if (getsockname(s, (sockaddr*)&local, &len) == 0) {
            char ip[64] = {};
            if (inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip))) {
                std::string text(ip);
                const size_t dot = text.rfind('.');
                if (dot != std::string::npos) out = text.substr(0, dot) + ".255";
            }
        }
    }
    closesocket(s);
    return out;
}


/**
 * Indirizzo aggiuntivo a cui chiedere "c'e' un server?", oltre al broadcast.
 *
 * Il broadcast non arriva sempre a destinazione: parecchi router domestici non
 * lo inoltrano tra la parte Wi-Fi e quella via cavo, ed e' esattamente il caso
 * "telefono sul Wi-Fi, PC attaccato al router". Una domanda diretta all'IP del
 * computer invece passa sempre — e quell'IP lo conosciamo gia', e' lo stesso
 * del server dei giochi che il client interroga all'avvio.
 */
inline std::string& netExtraDiscoveryHost() {
    static std::string host;
    return host;
}


/**
 * Inizializza lo stack di rete, se la piattaforma lo richiede.
 *
 * Su Windows ogni chiamata a socket() prima di WSAStartup fallisce, e fallisce
 * in SILENZIO: si ottiene un socket non valido e nessun errore visibile. Dentro
 * LanNet questo era gia' gestito, ma chi crea un socket per conto proprio —
 * come la scansione dei server — non aveva modo di farlo, e la ricerca tornava
 * sempre vuota senza spiegazione.
 */
inline bool netEnsureSockets() { return plat::sys::netInit(); }

enum class NetRole : uint8_t { Offline = 0, Host, Client };

enum class NetMsg : uint8_t {
    Hello = 1,
    Welcome = 2,
    State = 3,
    Leave = 4,
    Ping = 5,
    Reject = 6,
    Discover = 7,
    Announce = 8,
    RemoteEvent = 9,
    ScriptReport = 10
};

enum class NetRejectReason : uint8_t {
    Full = 1,
    BadGame = 2,
    BadClient = 3,
    BadScript = 4,
    Cheat = 5
};

#pragma pack(push, 1)
struct NetHeader {
    char     magic[4];
    uint8_t  type;
    uint8_t  peerId;
    uint16_t gameIndex;
};

struct NetHello {
    NetHeader hdr;
    char      name[24];
    char      fingerprint[17];
};

struct NetScriptReport {
    NetHeader hdr;
    uint8_t   count;
    uint8_t   total;
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
    // Bit di stato del personaggio, non un semplice "sta camminando".
    //
    // Impacchettati in un byte gia esistente invece di aggiungerne uno: la
    // dimensione del pacchetto non cambia, quindi non si rompe nulla di cio
    // che lo legge. Prima qui viaggiava solo la camminata, ed era il motivo
    // per cui il salto degli altri giocatori non si vedeva: l informazione
    // non partiva proprio.
    //   bit 0  = cammina
    //   bit 1  = e in aria (salto o caduta)
    //   bit 2-4 = piattaforma + 1 (0 = non dichiarata)
    //
    // La piattaforma sta QUI e non in un campo suo per un motivo preciso: a
    // riga 173 c e #pragma pack(push, 1), quindi queste strutture non hanno
    // nessun byte di riempimento da riusare. Aggiungere un campo avrebbe
    // portato il pacchetto da 49 a 50 byte, e ogni client gia in giro avrebbe
    // smesso di collegarsi. Nei bit liberi invece non cambia niente.
    //
    // Si salva piattaforma+1 perche uno zero deve voler dire "client vecchio
    // che non lo dice", non "PC" (che vale 0 in TargetPlatform).
    uint8_t   walking;
    char      name[24];
};

static_assert(sizeof(NetState) == 49,
              "NetState ha cambiato dimensione: romperesti i client gia in giro");

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
    // Nome del server, per poterlo distinguere in un elenco: prima
    // l annuncio diceva solo quanti posti c erano, non di chi fossero.
    char      name[24];
};

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
    bool toServer = true;
};

struct RemotePlayer {
    uint8_t   id = 0;
    char      name[24] = {};
    // Su cosa gioca: arriva nel saluto. 0 = client vecchio che non lo dice.
    uint8_t   platform = 0;
    glm::vec3 pos{0};
    float     yaw = 180.f;
    bool      walking = false;
    bool      airborne = false;
    float     lastSeen = 0.f;
    glm::vec3 displayPos{0};
    float     displayYaw = 180.f;

    // ---- Antilag: dove sara', non dov'era --------------------------------
    //
    // Gli stati arrivano 20 volte al secondo, e viaggiando ci mettono altro
    // tempo. Disegnare l'ultima posizione ricevuta significa disegnare il
    // giocatore dove si trovava decine di millisecondi fa: in corsa sono
    // parecchi metri di errore, e in uno scontro a fuoco e' la differenza fra
    // colpire e mancare.
    //
    // Con la velocita' stimata si puo' proiettare in avanti e disegnarlo dove
    // e' ADESSO. Non e' indovinare: e' la stessa cosa che fa ogni sparatutto
    // online da vent'anni.
    glm::vec3 velocity{0};        // unita' al secondo, stimata dai pacchetti
    glm::vec3 lastNetPos{0};      // posizione dell'ultimo pacchetto
    float     lastNetTime = -1.f; // quando e' arrivato
    bool      hasVelocity = false;
};

struct LanNet {
    static constexpr int kMaxPeers = 32;

    static constexpr float kMaxTeleportStuds = 80.f;
    static constexpr float kTeleportTolerance = 8.f;
    static constexpr int   kSpeedViolationsBeforeKick = 6;

    NetRole role = NetRole::Offline;
    NetSocket sock = kInvalidSock;
    sockaddr_in hostAddr{};
    uint8_t localId = 0;
    uint8_t nextId = 1;
    int gameIndex = 0;
    int maxPlayers = 8;
    bool dedicated = false;
    char localName[24] = "Player";
    char status[128] = {};
    char joinIp[64] = "127.0.0.1";
    int  joinPort = 27015;
    bool connected = false;
    bool rejected = false;

    // "Pool" (blk://) — relay via internet, vedi PoolProtocol.h. Quando
    // relayActive e' true, ogni indirizzo di peer "sintetico" (vedi
    // makeSyntheticAddr) viene incapsulato e mandato a relayServerAddr
    // invece che spedito via UDP diretto — cosi' tutta la logica esistente
    // di peer/anticheat (PeerSlot, findPeerSlotByAddr, kickPeer...) continua
    // a funzionare indistinta, senza sapere se un peer e' via LAN o via pool.
    static constexpr uint8_t kRelayHostSlot = 0; // lato client: "l'host" e' sempre questo slot sintetico
    bool relayActive = false;
    sockaddr_in relayServerAddr{};
    char relayCode[8] = {};
    float lastHeartbeatSent = -1000.f;

    char localFingerprint[17] = {};
    std::vector<std::string> allowedFingerprints;
    std::vector<std::string> seenFingerprints;

    std::vector<uint32_t> allowedScripts;
    std::vector<uint32_t> seenScripts;
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
        glm::vec3 lastValidPos{0};
        bool hasValidPos = false;
        float lastStateTime = 0.f;
        int speedViolations = 0;
    };
    PeerSlot peers[kMaxPeers]{};
    std::vector<IncomingRemoteEvent> incomingRemotes;

    // Seconda copia della stessa inizializzazione, nello stesso file. Ora
    // entrambe passano da plat::sys::netInit(), che e idempotente.
    static bool ensureWsa() { return plat::sys::netInit(); }


    /** I tre bit della piattaforma dentro il byte di stato. */
    static uint8_t packPlatformBits(uint8_t base) {
        const uint8_t p = (uint8_t)detectTargetPlatform();
        // +1: zero resta riservato a "non dichiarata".
        return (uint8_t)(base | (uint8_t)(((p + 1) & 0x07) << 2));
    }

    /** Legge la piattaforma da un byte di stato. 0 = non dichiarata. */
    static uint8_t unpackPlatform(uint8_t walkingByte) {
        return (uint8_t)((walkingByte >> 2) & 0x07);
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

    int occupiedSlots() const {
        int n = connectedClients();
        if (role == NetRole::Host && !dedicated) ++n;
        return n;
    }

    int freeSlots() const {
        return std::max(0, maxPlayers - occupiedSlots());
    }

    bool isFull() const { return freeSlots() <= 0; }

    void shutdown() {
        // Solo se stavamo ospitando dal telefono (non il server dedicato):
        // startDedicated() gira nell'eseguibile PC, non nell'app Android, e
        // il gancio esiste solo per il Foreground Service di quest'ultima.
        const bool wasPhoneHost = (role == NetRole::Host && !dedicated);

        if (sock != kInvalidSock) {
            if (role == NetRole::Client && connected) {
                NetLeave leave{};
                fillMagic(leave.hdr, NetMsg::Leave, localId, (uint16_t)gameIndex);
                sendTo(hostAddr, &leave, sizeof(leave));
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
        relayActive = false;
        relayServerAddr = {};
        relayCode[0] = '\0';
        clearRemotes();
        incomingRemotes.clear();
        for (auto& p : peers) p = {};
        setStatus("Offline");

        if (wasPhoneHost) plat::notifyHostingStateChanged(false);
    }

    bool bindUdp(int port) {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == kInvalidSock) { setStatus("socket fallita"); return false; }
        u_long nonblock = 1;
        ioctlsocket(sock, FIONBIO, &nonblock);
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

    bool startHost(int port, int gameIdx, const char* name, int maxP = 8) {
        shutdown();
        if (!ensureWsa()) { setStatus("Rete di sistema non inizializzata"); return false; }
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
        plat::notifyHostingStateChanged(true);
        return true;
    }

    bool startDedicated(int port, int gameIdx, int maxP, const char* title) {
        shutdown();
        if (!ensureWsa()) { setStatus("Rete di sistema non inizializzata"); return false; }
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
        if (!ensureWsa()) { setStatus("Rete di sistema non inizializzata"); return false; }

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
        sendTo(hostAddr, &hello, sizeof(hello));
        setStatus("Connessione al server…");
        return true;
    }

    /**
     * Da chiamare DOPO essere gia' diventati host (startHost/startAutoMatch)
     * per registrarsi sul pool "blk://" e ottenere un codice condivisibile —
     * risolve il CGNAT: da questo momento i pacchetti per i client via pool
     * passano dal relay invece che da un indirizzo IP:porta raggiungibile
     * dall'esterno. Handshake bloccante (~2s max), va bene qui perche' viene
     * chiamato una volta sola su richiesta esplicita dell'utente ("Ospita
     * Online"), non nel loop di gioco.
     */
    bool registerWithPool(const char* poolHost, int poolPort, const char* title) {
        if (sock == kInvalidSock || role != NetRole::Host) return false;

        relayServerAddr = {};
        relayServerAddr.sin_family = AF_INET;
        relayServerAddr.sin_port = htons((u_short)poolPort);
        if (inet_pton(AF_INET, poolHost, &relayServerAddr.sin_addr) != 1) {
            setStatus("Indirizzo pool non valido");
            return false;
        }

        PoolRegister reg{};
        poolFillMagic(reg.hdr, PoolMsg::Register);
        std::snprintf(reg.title, sizeof(reg.title), "%.31s", title && title[0] ? title : "Blocky");
        sendto(sock, (const char*)&reg, sizeof(reg), 0,
               (const sockaddr*)&relayServerAddr, sizeof(relayServerAddr));

        for (int i = 0; i < 40; ++i) {
            char buf[128];
            sockaddr_in from{};
            NetAddrLen fromLen = (NetAddrLen)sizeof(from);
            int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
            if (n >= (int)sizeof(PoolRegistered) && poolCheckMagic(buf, n) && sameAddr(from, relayServerAddr)) {
                const PoolRegistered* rep = (const PoolRegistered*)buf;
                if (rep->hdr.type == (uint8_t)PoolMsg::Registered && rep->ok) {
                    std::snprintf(relayCode, sizeof(relayCode), "%.7s", rep->code);
                    relayActive = true;
                    lastHeartbeatSent = now;
                    setStatus((std::string("Online — codice blk://") + relayCode).c_str());
                    return true;
                }
                setStatus("Il pool ha rifiutato la registrazione");
                return false;
            }
            netSleepMs(50);
        }
        setStatus("Pool non raggiungibile");
        return false;
    }

    /** Analogo di startClient(), ma entra in una stanza tramite un codice
     *  "blk://XXXXXX" invece che con IP:porta diretti. */
    bool startClientViaPool(const char* code, const char* poolHost, int poolPort,
                             int gameIdx, const char* name) {
        shutdown();
        if (!ensureWsa()) { setStatus("Rete di sistema non inizializzata"); return false; }

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == kInvalidSock) { setStatus("socket fallita"); return false; }
        u_long nonblock = 1;
        ioctlsocket(sock, FIONBIO, &nonblock);

        relayServerAddr = {};
        relayServerAddr.sin_family = AF_INET;
        relayServerAddr.sin_port = htons((u_short)poolPort);
        if (inet_pton(AF_INET, poolHost, &relayServerAddr.sin_addr) != 1) {
            setStatus("Indirizzo pool non valido");
            closesocket(sock);
            sock = kInvalidSock;
            return false;
        }

        std::snprintf(relayCode, sizeof(relayCode), "%.7s", code ? code : "");
        PoolResolve res{};
        poolFillMagic(res.hdr, PoolMsg::Resolve);
        std::snprintf(res.code, sizeof(res.code), "%.7s", relayCode);
        sendto(sock, (const char*)&res, sizeof(res), 0,
               (const sockaddr*)&relayServerAddr, sizeof(relayServerAddr));

        bool resolved = false;
        for (int i = 0; i < 40; ++i) {
            char buf[128];
            sockaddr_in from{};
            NetAddrLen fromLen = (NetAddrLen)sizeof(from);
            int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
            if (n >= (int)sizeof(PoolResolved) && poolCheckMagic(buf, n) && sameAddr(from, relayServerAddr)) {
                const PoolResolved* rep = (const PoolResolved*)buf;
                if (rep->hdr.type == (uint8_t)PoolMsg::Resolved
                    && rep->result == (uint8_t)PoolResolveResult::Ok) {
                    resolved = true;
                } else if (rep->result == (uint8_t)PoolResolveResult::NotFound) {
                    setStatus("Codice non trovato");
                } else {
                    setStatus("Stanza piena");
                }
                break;
            }
            netSleepMs(50);
        }
        if (!resolved) {
            closesocket(sock);
            sock = kInvalidSock;
            return false;
        }

        relayActive = true;
        role = NetRole::Client;
        connected = false;
        rejected = false;
        localId = 0;
        gameIndex = gameIdx;
        std::snprintf(joinIp, sizeof(joinIp), "blk://%s", relayCode);
        joinPort = poolPort;
        std::snprintf(localName, sizeof(localName), "%.23s", name && name[0] ? name : "Player");
        hostAddr = makeSyntheticAddr(kRelayHostSlot);

        NetHello hello{};
        fillMagic(hello.hdr, NetMsg::Hello, 0, (uint16_t)gameIdx);
        std::snprintf(hello.name, sizeof(hello.name), "%s", localName);
        std::snprintf(hello.fingerprint, sizeof(hello.fingerprint), "%s", localFingerprint);
        sendTo(hostAddr, &hello, sizeof(hello));
        setStatus("Connessione al server (online)…");
        return true;
    }

    RemotePlayer* findRemote(uint8_t id) {
        for (auto& r : remotes)
            if (r.id == id) return &r;
        return nullptr;
    }

    void upsertRemote(uint8_t id, const char* name, glm::vec3 pos, float yaw,
                      bool walking, bool airborne = false, uint8_t platform = 0) {
        // id 0 = il server dedicato (non ha un personaggio); localId = noi.
        if (id == 0 || id == localId) return;
        RemotePlayer* r = findRemote(id);
        if (!r) {
            remotes.push_back({});
            r = &remotes.back();
            r->id = id;
            r->displayPos = pos;
            r->displayYaw = yaw;
            std::printf("[Net] nuovo giocatore #%d \"%s\" (ora %d)\n",
                        (int)id, name ? name : "?", (int)remotes.size());
        }
        if (name && name[0])
            std::snprintf(r->name, sizeof(r->name), "%.23s", name);

        // ---- Stima della velocita, dai pacchetti veri ---------------------
        //
        // Si usa l intervallo REALE fra due pacchetti, non quello nominale di
        // 20 Hz: su una rete vera i pacchetti arrivano irregolari, e dividere
        // per un tempo sbagliato produce velocita fantasma che fanno vibrare
        // il personaggio.
        if (r->lastNetTime >= 0.f) {
            const float gap = now - r->lastNetTime;
            // Sotto i 5 ms il rumore domina, sopra mezzo secondo non e piu una
            // velocita ma un salto (rientro, teletrasporto): in entrambi i
            // casi meglio non dedurre niente.
            if (gap > 0.005f && gap < 0.5f) {
                const glm::vec3 v = (pos - r->lastNetPos) / gap;
                // Media mobile: un singolo pacchetto in ritardo non deve
                // sterzare la previsione.
                r->velocity = r->hasVelocity ? glm::mix(r->velocity, v, 0.45f) : v;
                r->hasVelocity = true;
            } else if (gap >= 0.5f) {
                r->velocity = glm::vec3(0.f);
                r->hasVelocity = false;
            }
        }
        r->lastNetPos = pos;
        r->lastNetTime = now;

        r->pos = pos;
        r->yaw = yaw;
        r->walking = walking;
        r->airborne = airborne;
        // Solo se dichiarata: uno zero vuol dire "client vecchio che non lo dice",
        // e non deve cancellare quello che gia sapevamo di lui.
        if (platform != 0) r->platform = platform;
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

    // Indirizzo "finto" (mai realmente instradabile) che rappresenta un peer
    // raggiunto via pool invece che via UDP diretto — 127.63.0.1 non e' mai
    // usato per una vera sendto/recvfrom, serve solo come chiave stabile per
    // le strutture esistenti (PeerSlot.addr, findPeerSlotByAddr) che gia'
    // confrontano indirizzi per bytes. Lato host lo slot identifica QUALE
    // client (0..kMaxPeers-1, assegnato dal pool); lato client esiste un solo
    // peer possibile ("l'host"), sempre kRelayHostSlot.
    static sockaddr_in makeSyntheticAddr(uint8_t slot) {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        inet_pton(AF_INET, "127.63.0.1", &a.sin_addr);
        a.sin_port = htons((u_short)(51000 + slot));
        return a;
    }

    static bool sameAddr(const sockaddr_in& a, const sockaddr_in& b) {
        return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
    }

    static int synthSlotOf(const sockaddr_in& addr) {
        sockaddr_in base{};
        inet_pton(AF_INET, "127.63.0.1", &base.sin_addr);
        if (addr.sin_addr.s_addr != base.sin_addr.s_addr) return -1;
        int port = ntohs(addr.sin_port);
        if (port < 51000 || port >= 51000 + kMaxPeers) return -1;
        return port - 51000;
    }

    void relaySend(uint8_t slot, const void* data, int len) {
        char buf[600];
        if (len < 0 || (size_t)len > sizeof(buf) - sizeof(PoolRelayHeader)) return;
        PoolRelayHeader h{};
        poolFillMagic(h.hdr, PoolMsg::Relay);
        std::snprintf(h.code, sizeof(h.code), "%.7s", relayCode);
        h.slot = slot;
        std::memcpy(buf, &h, sizeof(h));
        std::memcpy(buf + sizeof(h), data, (size_t)len);
        sendto(sock, buf, (int)(sizeof(h) + (size_t)len), 0,
               (const sockaddr*)&relayServerAddr, sizeof(relayServerAddr));
    }

    void sendTo(const sockaddr_in& addr, const void* data, int len) {
        if (relayActive) {
            int slot = synthSlotOf(addr);
            if (slot >= 0) { relaySend((uint8_t)slot, data, len); return; }
        }
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
            NetAddrLen fromLen = (NetAddrLen)sizeof(from);
            int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
            if (n <= 0) break;

            const char* payload = buf;
            int payloadLen = n;
            sockaddr_in effectiveFrom = from;

            if (relayActive && sameAddr(from, relayServerAddr) && poolCheckMagic(buf, n)) {
                const PoolRelayHeader* rh = (const PoolRelayHeader*)buf;
                if (rh->hdr.type != (uint8_t)PoolMsg::Relay || n < (int)sizeof(PoolRelayHeader))
                    continue; // Registered/Resolved arrivano solo nell'handshake bloccante, non qui
                uint8_t slot = (role == NetRole::Host) ? rh->slot : kRelayHostSlot;
                effectiveFrom = makeSyntheticAddr(slot);
                payload = buf + sizeof(PoolRelayHeader);
                payloadLen = n - (int)sizeof(PoolRelayHeader);
            }

            if (payloadLen < (int)sizeof(NetHeader)) continue;
            NetHeader hdr{};
            std::memcpy(&hdr, payload, sizeof(hdr));
            if (!checkMagic(hdr)) continue;

            if (role == NetRole::Host)
                handleHostPacket(payload, payloadLen, effectiveFrom, hdr);
            else
                handleClientPacket(payload, payloadLen, hdr);
        }

        if (relayActive && role == NetRole::Host && now - lastHeartbeatSent > 8.f) {
            lastHeartbeatSent = now;
            PoolHeartbeat hb{};
            poolFillMagic(hb.hdr, PoolMsg::Heartbeat);
            std::snprintf(hb.code, sizeof(hb.code), "%.7s", relayCode);
            sendto(sock, (const char*)&hb, sizeof(hb), 0,
                   (const sockaddr*)&relayServerAddr, sizeof(relayServerAddr));
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

    void handleScriptReport(const char* buf, int n, const sockaddr_in& from) {
        if (n < (int)sizeof(NetScriptReport)) return;
        int slot = findPeerSlotByAddr(from);
        if (slot < 0) return;

        NetScriptReport rep{};
        std::memcpy(&rep, buf, sizeof(rep));
        int cnt = rep.count > 48 ? 48 : rep.count;

        if (allowedScripts.empty()) {
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
        if (hdr.type == (uint8_t)NetMsg::Discover) {
            // Un server dedicato con gameIndex negativo risponde a tutti;
            // altrimenti risponde solo a chi cerca il SUO gioco.
            if (gameIndex >= 0 && (int)hdr.gameIndex != gameIndex) return;
            if (isFull()) return;
            NetAnnounce ann{};
            fillMagic(ann.hdr, NetMsg::Announce, dedicated ? 0 : 1, (uint16_t)gameIndex);
            ann.maxPlayers = (uint8_t)maxPlayers;
            ann.curPlayers = (uint8_t)occupiedSlots();
            ann.port = (uint16_t)joinPort;
            std::snprintf(ann.name, sizeof(ann.name), "%.23s", localName);
            sendTo(from, &ann, sizeof(ann));
            return;
        }

        if (hdr.type == (uint8_t)NetMsg::Hello && n >= (int)sizeof(NetHello)) {
            NetHello hello{};
            std::memcpy(&hello, buf, sizeof(hello));

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
            PeerSlot& p = peers[slot];
            p.lastSeen = now;
            if (st.name[0])
                std::snprintf(p.name, sizeof(p.name), "%.23s", st.name);

            glm::vec3 claimed{st.x, st.y, st.z};
            if (p.hasValidPos) {
                float dt = std::max(now - p.lastStateTime, 1.f / 20.f);
                float dist = glm::length(glm::vec2(claimed.x - p.lastValidPos.x,
                                                    claimed.z - p.lastValidPos.z));
                float maxDist = kMaxTeleportStuds * dt + kTeleportTolerance;
                if (dist > maxDist) {
                    ++p.speedViolations;
                    LOG_SEC("net", "#%d movimento impossibile (%.1f > %.1f studs) — scartato",
                            (int)p.id, dist, maxDist);
                    if (p.speedViolations >= kSpeedViolationsBeforeKick) {
                        kickPeer(slot, NetRejectReason::Cheat, "speedhack/teleport ripetuto");
                        return;
                    }
                    claimed.x = p.lastValidPos.x;
                    claimed.z = p.lastValidPos.z;
                }
            }
            p.lastValidPos = claimed;
            p.hasValidPos = true;
            p.lastStateTime = now;

            upsertRemote(p.id, p.name, claimed, st.yaw,
                         (st.walking & 1) != 0, (st.walking & 2) != 0, unpackPlatform(st.walking));

            st.x = claimed.x; st.y = claimed.y; st.z = claimed.z;
            st.hdr.peerId = p.id;
            relayExcept(p.id, &st, sizeof(st));
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

            // Chat: la firma l host e la rimanda a tutti, e non la passa
            // agli script — e un messaggio fra persone, non un evento di gioco.
            if (std::strcmp(ev.name, kChatEventName) == 0) {
                relayChatFromClient(fromNm, ev.payload);
                return;
            }

            IncomingRemoteEvent in{};
            std::snprintf(in.name, sizeof(in.name), "%.31s", ev.name);
            std::snprintf(in.payload, sizeof(in.payload), "%.159s", ev.payload);
            std::snprintf(in.fromName, sizeof(in.fromName), "%.23s", fromNm);
            in.fromId = fromId;
            in.toServer = true;
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
            upsertRemote(st.hdr.peerId, st.name, {st.x, st.y, st.z}, st.yaw,
                         (st.walking & 1) != 0, (st.walking & 2) != 0, unpackPlatform(st.walking));
            return;
        }
        if (hdr.type == (uint8_t)NetMsg::Leave) {
            removeRemote(hdr.peerId);
            return;
        }
        if (hdr.type == (uint8_t)NetMsg::RemoteEvent && n >= (int)sizeof(NetRemoteEvent)) {
            NetRemoteEvent ev{};
            std::memcpy(&ev, buf, sizeof(ev));

            // Chat gia' firmata dall'host: si mostra e basta, non passa agli
            // script. Il nome dentro al testo e' quello che l'host ha scritto,
            // quindi non e' falsificabile da chi ha mandato il messaggio.
            if (std::strcmp(ev.name, kChatEventName) == 0) {
                incomingChat.push_back(ev.payload);
                return;
            }

            IncomingRemoteEvent in{};
            std::snprintf(in.name, sizeof(in.name), "%.31s", ev.name);
            std::snprintf(in.payload, sizeof(in.payload), "%.159s", ev.payload);
            std::snprintf(in.fromName, sizeof(in.fromName), "%.23s",
                          ev.hdr.peerId ? "Peer" : "Host");
            for (const auto& r : remotes) {
                if (r.id == ev.hdr.peerId) {
                    std::snprintf(in.fromName, sizeof(in.fromName), "%.23s", r.name);
                    break;
                }
            }
            if (ev.hdr.peerId == 0 || ev.hdr.peerId == 1)
                std::snprintf(in.fromName, sizeof(in.fromName), "Host");
            in.fromId = ev.hdr.peerId;
            in.toServer = false;
            incomingRemotes.push_back(in);
        }
    }

    bool sendRemoteEventToServer(const char* name, const char* payload) {
        if (sock == kInvalidSock || role == NetRole::Offline) return false;
        NetRemoteEvent ev{};
        fillMagic(ev.hdr, NetMsg::RemoteEvent, localId, (uint16_t)gameIndex);
        std::snprintf(ev.name, sizeof(ev.name), "%.31s", name ? name : "Event");
        std::snprintf(ev.payload, sizeof(ev.payload), "%.159s", payload ? payload : "");
        if (role == NetRole::Host) {
            IncomingRemoteEvent in{};
            std::snprintf(in.name, sizeof(in.name), "%s", ev.name);
            std::snprintf(in.payload, sizeof(in.payload), "%s", ev.payload);
            std::snprintf(in.fromName, sizeof(in.fromName), "%s", localName);
            in.fromId = localId;
            in.toServer = true;
            incomingRemotes.push_back(in);
            return true;
        }
        sendTo(hostAddr, &ev, sizeof(ev));
        return true;
    }


    // ---- Chat fra giocatori -------------------------------------------
    //
    // Viaggia sul canale eventi che esiste gia', con un nome riservato, invece
    // di aggiungere un tipo di pacchetto nuovo: quel canale sa gia' attraversare
    // il relay blk:// e gestire host e client, e riscriverlo daccapo per la
    // chat avrebbe voluto dire due strade separate da tenere allineate.
    //
    // Prima la chat non usciva affatto dal dispositivo: quello che si scriveva
    // restava nel proprio elenco e basta.

    static constexpr const char* kChatEventName = "__blkchat";

    /** Righe arrivate dalla rete, in attesa di essere mostrate. */
    std::vector<std::string> incomingChat;

    /**
     * Manda una riga di chat a tutti.
     *
     * Il nome di chi parla lo scrive SEMPRE chi riceve per primo — l'host — e
     * mai chi manda: cosi' un client non puo' spacciarsi per un altro
     * scrivendo un nome diverso nel messaggio.
     */
    bool sendChat(const char* text) {
        if (!text || !text[0] || sock == kInvalidSock) return false;

        if (role == NetRole::Host) {
            std::string line = std::string(localName) + ": " + text;
            if (line.size() > 159) line.resize(159);
            NetRemoteEvent ev{};
            fillMagic(ev.hdr, NetMsg::RemoteEvent, localId, (uint16_t)gameIndex);
            std::snprintf(ev.name, sizeof(ev.name), "%s", kChatEventName);
            std::snprintf(ev.payload, sizeof(ev.payload), "%s", line.c_str());
            broadcastAll(&ev, sizeof(ev));
            return true;
        }

        if (role == NetRole::Client && connected) {
            NetRemoteEvent ev{};
            fillMagic(ev.hdr, NetMsg::RemoteEvent, localId, (uint16_t)gameIndex);
            std::snprintf(ev.name, sizeof(ev.name), "%s", kChatEventName);
            std::snprintf(ev.payload, sizeof(ev.payload), "%.159s", text);
            sendTo(hostAddr, &ev, sizeof(ev));
            return true;
        }
        return false;
    }

    /** L'host ha ricevuto una chat da un client: la firma e la rilancia a tutti. */
    void relayChatFromClient(const char* fromName, const char* text) {
        std::string line = std::string(fromName ? fromName : "Player") + ": " + (text ? text : "");
        if (line.size() > 159) line.resize(159);
        incomingChat.push_back(line);

        NetRemoteEvent ev{};
        fillMagic(ev.hdr, NetMsg::RemoteEvent, localId, (uint16_t)gameIndex);
        std::snprintf(ev.name, sizeof(ev.name), "%s", kChatEventName);
        std::snprintf(ev.payload, sizeof(ev.payload), "%s", line.c_str());
        broadcastAll(&ev, sizeof(ev));
    }

    bool fireRemoteEventAllClients(const char* name, const char* payload) {
        if (sock == kInvalidSock || role != NetRole::Host) return false;
        NetRemoteEvent ev{};
        fillMagic(ev.hdr, NetMsg::RemoteEvent, localId, (uint16_t)gameIndex);
        std::snprintf(ev.name, sizeof(ev.name), "%.31s", name ? name : "Event");
        std::snprintf(ev.payload, sizeof(ev.payload), "%.159s", payload ? payload : "");
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
        sendTo(hostAddr, &rep, sizeof(rep));
    }

    void sendLocalState(float dt, glm::vec3 pos, float yaw, bool walking,
                        bool airborne = false) {
        if (sock == kInvalidSock || role == NetRole::Offline) return;
        if (dedicated) return;
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
                sendTo(hostAddr, &hello, sizeof(hello));
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
        st.walking = packPlatformBits((uint8_t)((walking ? 1 : 0) | (airborne ? 2 : 0)));
        std::snprintf(st.name, sizeof(st.name), "%s", localName);

        if (role == NetRole::Host)
            broadcastAll(&st, sizeof(st));
        else
            sendTo(hostAddr, &st, sizeof(st));
    }

    /**
     * Dove disegnare gli altri giocatori.
     *
     * ── Cosa faceva prima ─────────────────────────────────────────────────
     *     displayPos = mix(displayPos, pos, 12 * dt)
     *
     * Inseguiva l'ultima posizione ricevuta. Due difetti, entrambi visibili:
     * non raggiungeva MAI il bersaglio (un'esponenziale ci si avvicina e
     * basta), e il bersaglio stesso era gia' vecchio — il pacchetto e' partito
     * decine di millisecondi fa. Risultato: chi corre sembra scivolare, ed e'
     * sempre indietro rispetto a dov'e' davvero.
     *
     * ── Cosa fa adesso ────────────────────────────────────────────────────
     * Proietta in avanti con la velocita' stimata, di quanto tempo e'
     * realmente passato dall'ultimo pacchetto. Non e' indovinare: e' quello
     * che fa ogni sparatutto online, e il motivo per cui i giocatori remoti
     * sembrano muoversi in modo naturale invece che a scatti.
     *
     * ── Il limite, che serve davvero ──────────────────────────────────────
     * L'estrapolazione si ferma a kMaxExtrapolation. Senza un tetto, chi
     * perde la connessione continuerebbe a correre in linea retta per sempre,
     * attraversando i muri — l'errore classico di questa tecnica, e molto
     * peggio di un giocatore fermo.
     */
    void updateDisplay(float dt) {
        // Oltre un quinto di secondo non e' piu' previsione, e' fantasia.
        constexpr float kMaxExtrapolation = 0.20f;

        for (auto& r : remotes) {
            glm::vec3 target = r.pos;

            if (r.hasVelocity && r.lastNetTime >= 0.f) {
                float ahead = now - r.lastNetTime;
                if (ahead > kMaxExtrapolation) ahead = kMaxExtrapolation;
                if (ahead > 0.f) target += r.velocity * ahead;
            }

            // Lo smorzamento resta, ma ora insegue un bersaglio GIUSTO invece
            // di uno vecchio: serve solo a togliere gli scalini fra un
            // pacchetto e l'altro, non a nascondere il ritardo.
            r.displayPos = glm::mix(r.displayPos, target, std::min(1.f, 18.f * dt));

            float dy = r.yaw - r.displayYaw;
            while (dy > 180.f) dy -= 360.f;
            while (dy < -180.f) dy += 360.f;
            r.displayYaw += dy * std::min(1.f, 18.f * dt);
        }
    }

    int playerCount() const { return occupiedSlots(); }

    bool startAutoMatch(int port, int gameIdx, const char* name, int maxP = 8) {
        if (!ensureWsa()) { setStatus("Rete di sistema non inizializzata"); return false; }
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

        // Il broadcast della sottorete per primo: e l unico che Android
        // instrada davvero. Gli altri due restano come rete di sicurezza.
        std::vector<std::string> targets;
        const std::string subnet = localSubnetBroadcast();
        if (!subnet.empty()) targets.push_back(subnet);
        targets.push_back("255.255.255.255");
        targets.push_back("127.0.0.1");
        if (!netExtraDiscoveryHost().empty()) targets.push_back(netExtraDiscoveryHost());
        {
            std::string all;
            for (const auto& t : targets) { if (!all.empty()) all += ", "; all += t; }
            std::printf("[Net] ricerca su porta %d: %s\n", port,
                        all.empty() ? "(nessun bersaglio)" : all.c_str());
        }
        for (int round = 0; round < 3; ++round) {
            for (const std::string& tip : targets) {
                inet_pton(AF_INET, tip.c_str(), &dest.sin_addr);
                const int sent = sendto(probe, (const char*)&disc, sizeof(disc), 0,
                       (sockaddr*)&dest, sizeof(dest));
                if (sent < 0 && round == 0)
                    std::printf("[Net] invio a %s fallito\n", tip.c_str());
            }
            netSleepMs(80);
        }

        char foundIp[64] = {};
        int foundPort = port;
        for (int i = 0; i < 70; ++i) {
            char buf[256];
            sockaddr_in from{};
            NetAddrLen fromLen = (NetAddrLen)sizeof(from);
            int n = recvfrom(probe, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
            if (n >= (int)sizeof(NetAnnounce)) {
                NetHeader hdr{};
                std::memcpy(&hdr, buf, sizeof(hdr));
                // 0xFFFF e' il modo in cui gameIndex=-1 ("qualsiasi gioco",
                // il default di un server dedicato avviato SENZA --game)
                // viaggia in un campo uint16_t: -1 non esiste li dentro, e
                // -1 castato a uint16_t diventa 65535. Senza questo caso
                // speciale, un client che cerca un gioco preciso (indice 0,
                // 1...) non trovava MAI un server dedicato in modalita
                // "qualsiasi" — che e' il modo in cui il .bat lo avvia di
                // default. Non un guasto di rete: un confronto che non
                // considerava il caso jolly che il lato server gestisce gia'
                // (vedi "if (gameIndex >= 0 && ...) return;" nel gestore di
                // Discover, qualche schermata sopra).
                if (checkMagic(hdr) && hdr.type == (uint8_t)NetMsg::Announce
                    && (hdr.gameIndex == 0xFFFF || (int)hdr.gameIndex == gameIdx)) {
                    NetAnnounce ann{};
                    std::memcpy(&ann, buf, sizeof(ann));
                    if (ann.curPlayers < ann.maxPlayers) {
                        inet_ntop(AF_INET, &from.sin_addr, foundIp, sizeof(foundIp));
                        foundPort = ann.port > 0 ? (int)ann.port : port;
                        break;
                    }
                }
            }
            netSleepMs(10);
        }
        closesocket(probe);

        if (foundIp[0]) {
            std::printf("[Net] Trovato server %s:%d — join automatico\n", foundIp, foundPort);
            return startClient(foundIp, foundPort, gameIdx, name);
        }

        // Nessuna risposta, ma l'indirizzo del computer che ospita i giochi lo
        // conosciamo: si prova a entrare LI' prima di arrendersi.
        //
        // Serve perche' la ricerca si basa su una risposta che deve tornare
        // indietro, e basta che si perda per strada — un router che non
        // inoltra il broadcast, un pacchetto scartato — perche' il gioco
        // concluda "non c'e' nessuno" e si metta a ospitare per conto suo.
        // Risultato: due partite separate sulla stessa rete, ognuna con un
        // giocatore solo, senza che nessuno capisca perche'. Una domanda
        // diretta invece non dipende da nessuna di quelle cose.
        if (!netExtraDiscoveryHost().empty()) {
            std::printf("[Net] Nessuna risposta — provo direttamente %s:%d\n",
                        netExtraDiscoveryHost().c_str(), port);
            if (startClient(netExtraDiscoveryHost().c_str(), port, gameIdx, name))
                return true;
        }

        std::printf("[Net] Nessun server — diventi host (max %d)\n", maxP);
        if (!startHost(port, gameIdx, name, maxP)) return false;

        // Lo stato va DETTO, non solo scritto nel log.
        //
        // Finire a ospitare da soli e' indistinguibile, a schermo, dall'essere
        // entrati in una partita: in entrambi i casi compare "Server :27015".
        // Chi si aspettava di trovare un amico resta a guardare un mondo vuoto
        // senza capire se sia un guasto o se semplicemente non ci fosse nessuno.
        // Questo messaggio distingue le due cose.
        std::snprintf(status, sizeof(status),
                      "Nessun server trovato — ospiti tu :%d (1/%d)", port, maxP);
        return true;
    }
};
