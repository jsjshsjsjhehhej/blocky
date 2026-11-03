#pragma once

/**
 * Trova da solo il PC che ha pubblicato i giochi, sulla stessa Wi-Fi.
 *
 * Senza questo, sul telefono bisognava scrivere a mano l'IP del computer.
 * Il PC annuncia "i giochi sono qui, porta HTTP X"; il telefono ascolta
 * e chiede l'elenco. Stesso Wi-Fi, Blocky aperto sul PC: i giochi comparono.
 */

#include "engine/LanNet.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace blk {
namespace gameslan {

inline constexpr int kBeaconPort = 18081;

#pragma pack(push, 1)
struct Beacon {
    char     magic[4];
    uint8_t  kind;      // 1 = chi cerca, 2 = "sono io"
    uint8_t  pad = 0;
    uint16_t httpPort;  // htons
};
#pragma pack(pop)

inline void fillMagic(Beacon& b) {
    b.magic[0] = 'B'; b.magic[1] = 'L'; b.magic[2] = 'K'; b.magic[3] = 'G';
}

inline bool checkMagic(const Beacon& b) {
    return b.magic[0] == 'B' && b.magic[1] == 'L' && b.magic[2] == 'K' && b.magic[3] == 'G';
}

inline std::mutex& foundMutex() { static std::mutex m; return m; }
inline std::string& foundSlot() { static std::string s; return s; }
inline std::atomic<bool>& foundReady() { static std::atomic<bool> b{false}; return b; }
inline std::atomic<bool>& running() { static std::atomic<bool> b{false}; return b; }
inline std::atomic<bool>& discovering() { static std::atomic<bool> b{false}; return b; }
inline std::atomic<int>& httpPort() { static std::atomic<int> p{8080}; return p; }

inline void noteFound(const std::string& hostPort) {
    if (hostPort.empty()) return;
    {
        std::lock_guard<std::mutex> lock(foundMutex());
        foundSlot() = hostPort;
    }
    foundReady().store(true);
}

inline bool takeFoundHost(std::string& out) {
    if (!foundReady().exchange(false)) return false;
    std::lock_guard<std::mutex> lock(foundMutex());
    out = std::move(foundSlot());
    foundSlot().clear();
    return !out.empty();
}

inline std::vector<std::string> broadcastTargets() {
    std::vector<std::string> t;
    const std::string subnet = localSubnetBroadcast();
    if (!subnet.empty()) t.push_back(subnet);
    t.push_back("255.255.255.255");
    t.push_back("127.0.0.1");
    return t;
}

inline NetSocket openUdp(bool bindBeaconPort) {
    if (!netEnsureSockets()) return kInvalidSock;
    NetSocket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kInvalidSock) return kInvalidSock;
    unsigned long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);
    BOOL broadcast = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast));
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(bindBeaconPort ? (u_short)kBeaconPort : 0);
    if (::bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(s);
        return kInvalidSock;
    }
    return s;
}

inline void sendBeacon(NetSocket s, const Beacon& pkt, const char* ip, int port) {
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons((u_short)port);
    inet_pton(AF_INET, ip, &dest.sin_addr);
    sendto(s, (const char*)&pkt, (int)sizeof(pkt), 0, (sockaddr*)&dest, sizeof(dest));
}

inline void announceLoop() {
    NetSocket s = openUdp(true);
    if (s == kInvalidSock) {
        LOG_INFO("games", "beacon: bind UDP %d fallito", kBeaconPort);
        running().store(false);
        return;
    }
    LOG_INFO("games", "beacon attivo UDP %d (HTTP %d)", kBeaconPort, httpPort().load());

    int tick = 0;
    while (running().load()) {
        Beacon ann{};
        fillMagic(ann);
        ann.kind = 2;
        ann.httpPort = htons((uint16_t)httpPort().load());
        for (const std::string& ip : broadcastTargets())
            sendBeacon(s, ann, ip.c_str(), kBeaconPort);

        for (int i = 0; i < 15 && running().load(); ++i) {
            char buf[64];
            sockaddr_in from{};
            NetAddrLen fromLen = (NetAddrLen)sizeof(from);
            int n = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
            if (n >= (int)sizeof(Beacon)) {
                Beacon got{};
                std::memcpy(&got, buf, sizeof(got));
                if (checkMagic(got) && got.kind == 1) {
                    Beacon reply = ann;
                    sendto(s, (const char*)&reply, (int)sizeof(reply), 0,
                           (sockaddr*)&from, fromLen);
                }
            }
            netSleepMs(100);
        }
        ++tick;
        (void)tick;
    }
    closesocket(s);
}

inline void discoverOnce() {
    NetSocket s = openUdp(false);
    if (s == kInvalidSock) {
        discovering().store(false);
        return;
    }

    Beacon disc{};
    fillMagic(disc);
    disc.kind = 1;
    disc.httpPort = 0;
    for (int round = 0; round < 3; ++round) {
        for (const std::string& ip : broadcastTargets())
            sendBeacon(s, disc, ip.c_str(), kBeaconPort);
        netSleepMs(60);
    }

    for (int i = 0; i < 40; ++i) {
        char buf[64];
        sockaddr_in from{};
        NetAddrLen fromLen = (NetAddrLen)sizeof(from);
        int n = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &fromLen);
        if (n >= (int)sizeof(Beacon)) {
            Beacon got{};
            std::memcpy(&got, buf, sizeof(got));
            if (checkMagic(got) && got.kind == 2) {
                char ip[64] = {};
                inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
                int port = (int)ntohs(got.httpPort);
                if (port < 1) port = 8080;
                char hostPort[96];
                std::snprintf(hostPort, sizeof(hostPort), "%s:%d", ip, port);
                noteFound(hostPort);
                LOG_INFO("games", "trovato PC %s", hostPort);
                break;
            }
        }
        netSleepMs(50);
    }
    closesocket(s);
    discovering().store(false);
}

inline void startAnnounce(int port) {
    httpPort().store(port > 0 ? port : 8080);
    bool expected = false;
    if (!running().compare_exchange_strong(expected, true)) {
        httpPort().store(port > 0 ? port : 8080);
        return;
    }
    std::thread(announceLoop).detach();
}

inline void startDiscover() {
    bool expected = false;
    if (!discovering().compare_exchange_strong(expected, true)) return;
    std::thread(discoverOnce).detach();
}

inline void stop() {
    running().store(false);
}

} // namespace gameslan
} // namespace blk
