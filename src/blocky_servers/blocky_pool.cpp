
/**
 * BlockyPool — relay/matchmaking pubblico per il multiplayer "blk://" via
 * internet (vedi PoolProtocol.h e i commenti in LanNet.h). Va fatto girare
 * su un server con un IP vero (una VPS), MAI sulla rete di un giocatore:
 * esiste solo perche' host e client non possono raggiungersi direttamente
 * dietro CGNAT — parlano entrambi in uscita SOLO verso questo indirizzo.
 *
 * Non capisce (e non deve capire) il gioco: inoltra i pacchetti BLK1 di
 * LanNet byte-per-byte, senza guardarci dentro. Tutta la logica di
 * gioco/anticheat resta lato host, identica a un game normale in LAN.
 */

#include "engine/PoolProtocol.h"
using namespace blk::PoolProtocol;

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif

struct ClientSlot {
    bool used = false;
    sockaddr_in addr{};
    double lastSeen = 0.0;
};

struct Room {
    char code[8] = {};
    sockaddr_in hostAddr{};
    double lastSeen = 0.0;
    ClientSlot slots[kPoolMaxClientsPerRoom];
};

static std::vector<Room> g_rooms;

static bool sameAddr(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

static void makeCode(char out[8]) {
    const int n = (int)std::strlen(kPoolCodeAlphabet);
    for (int i = 0; i < kPoolCodeLen; ++i)
        out[i] = kPoolCodeAlphabet[std::rand() % n];
    out[kPoolCodeLen] = '\0';
}

static Room* findRoom(const char* code) {
    for (auto& r : g_rooms)
        if (std::strncmp(r.code, code, sizeof(r.code)) == 0) return &r;
    return nullptr;
}

static int findOrAssignClientSlot(Room& r, const sockaddr_in& from, double now) {
    int free = -1;
    for (int i = 0; i < kPoolMaxClientsPerRoom; ++i) {
        if (r.slots[i].used && sameAddr(r.slots[i].addr, from)) {
            r.slots[i].lastSeen = now;
            return i;
        }
        if (!r.slots[i].used && free < 0) free = i;
    }
    if (free < 0) return -1;
    r.slots[free].used = true;
    r.slots[free].addr = from;
    r.slots[free].lastSeen = now;
    return free;
}

static void expireStale(double now) {
    for (size_t i = g_rooms.size(); i-- > 0;) {
        Room& r = g_rooms[i];
        if (now - r.lastSeen > 30.0) {
            g_rooms.erase(g_rooms.begin() + i);
            continue;
        }
        for (auto& s : r.slots)
            if (s.used && now - s.lastSeen > 30.0) s = ClientSlot{};
    }
}

int main(int argc, char** argv) {
    int port = 27020; // = PoolConfig::kPoolPort di default in PoolConfig.h
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            port = std::atoi(argv[++i]);
    }
    if (port < 1024 || port > 65535) port = 27020;

#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::fprintf(stderr, "WSAStartup fallita\n");
        return 1;
    }
#endif

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        std::fprintf(stderr, "socket() fallita\n");
        return 1;
    }
    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindAddr.sin_port = htons((u_short)port);
    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) != 0) {
        std::fprintf(stderr, "bind() fallita sulla porta %d — occupata?\n", port);
        return 1;
    }

    std::srand((unsigned)std::time(nullptr));
    std::printf("========================================\n");
    std::printf("  BlockyPool ATTIVO — porta UDP %d\n", port);
    std::printf("  Relay per multiplayer via internet (blk://)\n");
    std::printf("  Ctrl+C per chiudere\n");
    std::printf("========================================\n");

    LARGE_INTEGER freq{}, counter{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    double start = (double)counter.QuadPart / (double)freq.QuadPart;
    double lastExpireCheck = 0.0;

    char buf[600];
    for (;;) {
        sockaddr_in from{};
        int fromLen = sizeof(from);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
        QueryPerformanceCounter(&counter);
        double now = (double)counter.QuadPart / (double)freq.QuadPart - start;

        if (n >= (int)sizeof(PoolHeader) && poolCheckMagic(buf, n)) {
            const PoolHeader* hdr = (const PoolHeader*)buf;

            if (hdr->type == (uint8_t)PoolMsg::Register && n >= (int)sizeof(PoolRegister)) {
                Room r{};
                do { makeCode(r.code); } while (findRoom(r.code));
                r.hostAddr = from;
                r.lastSeen = now;
                g_rooms.push_back(r);

                PoolRegistered rep{};
                poolFillMagic(rep.hdr, PoolMsg::Registered);
                rep.ok = 1;
                std::snprintf(rep.code, sizeof(rep.code), "%s", r.code);
                sendto(sock, (const char*)&rep, sizeof(rep), 0, (sockaddr*)&from, fromLen);
                std::printf("[Pool] Nuova stanza %s\n", r.code);

            } else if (hdr->type == (uint8_t)PoolMsg::Heartbeat && n >= (int)sizeof(PoolHeartbeat)) {
                const PoolHeartbeat* hb = (const PoolHeartbeat*)buf;
                if (Room* r = findRoom(hb->code)) {
                    r->hostAddr = from; // il NAT dell'host puo' aver cambiato porta
                    r->lastSeen = now;
                }

            } else if (hdr->type == (uint8_t)PoolMsg::Resolve && n >= (int)sizeof(PoolResolve)) {
                const PoolResolve* res = (const PoolResolve*)buf;
                Room* r = findRoom(res->code);
                PoolResolved rep{};
                poolFillMagic(rep.hdr, PoolMsg::Resolved);
                if (!r) {
                    rep.result = (uint8_t)PoolResolveResult::NotFound;
                } else {
                    int slot = findOrAssignClientSlot(*r, from, now);
                    if (slot < 0) {
                        rep.result = (uint8_t)PoolResolveResult::Full;
                    } else {
                        rep.result = (uint8_t)PoolResolveResult::Ok;
                        std::printf("[Pool] %s: nuovo client (slot %d)\n", r->code, slot);
                    }
                }
                sendto(sock, (const char*)&rep, sizeof(rep), 0, (sockaddr*)&from, fromLen);

            } else if (hdr->type == (uint8_t)PoolMsg::Relay && n >= (int)sizeof(PoolRelayHeader)) {
                const PoolRelayHeader* rh = (const PoolRelayHeader*)buf;
                Room* r = findRoom(rh->code);
                if (r) {
                    const char* inner = buf + sizeof(PoolRelayHeader);
                    int innerLen = n - (int)sizeof(PoolRelayHeader);
                    if (sameAddr(from, r->hostAddr)) {
                        // Host -> un client specifico (rh->slot)
                        r->lastSeen = now;
                        int slot = rh->slot;
                        if (slot >= 0 && slot < kPoolMaxClientsPerRoom && r->slots[slot].used) {
                            char out[600];
                            PoolRelayHeader oh = *rh;
                            std::memcpy(out, &oh, sizeof(oh));
                            std::memcpy(out + sizeof(oh), inner, (size_t)innerLen);
                            sendto(sock, out, (int)(sizeof(oh) + (size_t)innerLen), 0,
                                   (sockaddr*)&r->slots[slot].addr, sizeof(sockaddr_in));
                        }
                    } else {
                        // Un client -> host
                        int slot = findOrAssignClientSlot(*r, from, now);
                        if (slot >= 0) {
                            char out[600];
                            PoolRelayHeader oh{};
                            poolFillMagic(oh.hdr, PoolMsg::Relay);
                            std::snprintf(oh.code, sizeof(oh.code), "%s", r->code);
                            oh.slot = (uint8_t)slot;
                            std::memcpy(out, &oh, sizeof(oh));
                            std::memcpy(out + sizeof(oh), inner, (size_t)innerLen);
                            sendto(sock, out, (int)(sizeof(oh) + (size_t)innerLen), 0,
                                   (sockaddr*)&r->hostAddr, sizeof(sockaddr_in));
                        }
                    }
                }
            }
        }

        if (now - lastExpireCheck > 5.0) {
            lastExpireCheck = now;
            expireStale(now);
        }
    }
    return 0;
}
