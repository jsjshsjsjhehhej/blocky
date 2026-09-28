#pragma once

/**
 * Richieste HTTP GET, il minimo indispensabile.
 *
 * HttpMini.h e' il lato SERVER (legge richieste, scrive risposte). Qui c'e' il
 * lato client, che finora non esisteva: serve al client per chiedere a
 * blocky_hostd l'elenco dei giochi pubblicati su un altro computer.
 *
 * Volutamente piccolo: niente HTTPS, niente redirect, niente chunked. Parla con
 * blocky_hostd, che risponde sempre con Content-Length e corpo semplice.
 */

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform/SystemApi.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using HttpSocket = SOCKET;
static constexpr HttpSocket kHttpInvalidSock = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
using HttpSocket = int;
static constexpr HttpSocket kHttpInvalidSock = -1;
#endif

namespace blk {
namespace http {

namespace detail {

inline void closeSock(HttpSocket s) {
#ifdef _WIN32
    ::closesocket(s);
#else
    ::close(s);
#endif
}

// Terza copia della stessa inizializzazione trovata nel motore (le altre due
// erano in LanNet.h). Una sola, in plat::sys, idempotente.
inline bool ensureSockets() { return plat::sys::netInit(); }

/** Timeout su lettura e scrittura: senza, un server che non risponde
 *  bloccherebbe il frame del gioco a tempo indeterminato. */
inline void setTimeouts(HttpSocket s, int timeoutMs) {
#ifdef _WIN32
    DWORD tv = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}


/** send() puo' consegnarne solo una parte: va richiamata finche' non e' finito. */
inline bool sendAllBytes(HttpSocket s, const char* data, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        const int k = (int)send(s, data + sent, (int)(n - sent), 0);
        if (k <= 0) return false;
        sent += (size_t)k;
    }
    return true;
}

} // namespace detail

/**
 * GET http://host:port/path — il corpo della risposta finisce in body.
 *
 * Ritorna false se non si e' riusciti a parlare col server o se la risposta non
 * e' 200. In entrambi i casi body resta vuoto: chi chiama non deve distinguere
 * "server spento" da "gioco non trovato", gli basta sapere che non c'e' niente.
 */
inline bool get(const std::string& host, int port, const std::string& path,
                std::string& body, int timeoutMs = 2500) {
    body.clear();
    if (host.empty() || port <= 0 || port > 65535) return false;
    if (!detail::ensureSockets()) return false;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) return false;

    HttpSocket s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == kHttpInvalidSock) { freeaddrinfo(res); return false; }
    detail::setTimeouts(s, timeoutMs);

    if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        detail::closeSock(s);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    const std::string req =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + ":" + portStr + "\r\n"
        "User-Agent: Blocky\r\n"
        "Connection: close\r\n\r\n";
    if (send(s, req.data(), (int)req.size(), 0) < 0) {
        detail::closeSock(s);
        return false;
    }

    // "Connection: close" significa che il server chiude quando ha finito:
    // si legge fino alla chiusura e non serve interpretare Content-Length.
    std::string raw;
    char buf[4096];
    for (;;) {
        const int n = (int)recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, (size_t)n);
        if (raw.size() > 8u * 1024 * 1024) break;   // tetto di sicurezza
    }
    detail::closeSock(s);

    const size_t headEnd = raw.find("\r\n\r\n");
    if (headEnd == std::string::npos) return false;
    if (raw.compare(0, 12, "HTTP/1.1 200") != 0 && raw.compare(0, 12, "HTTP/1.0 200") != 0)
        return false;

    body = raw.substr(headEnd + 4);
    return true;
}


/**
 * PUT http://host:port/path con un corpo binario.
 *
 * Usata per mandare a blocky_hostd un gioco appena pubblicato. Stessa filosofia
 * della get(): nessun HTTPS, nessun redirect, il minimo che serve per parlare
 * col nostro server.
 *
 * Il timeout e' piu' generoso della get() perche' qui si spediscono megabyte,
 * non si legge un elenco.
 */
inline bool put(const std::string& host, int port, const std::string& path,
                const std::vector<std::uint8_t>& body, std::string& reply,
                int timeoutMs = 20000) {
    reply.clear();
    if (host.empty() || port <= 0 || port > 65535) return false;
    if (!detail::ensureSockets()) return false;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) return false;

    HttpSocket s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == kHttpInvalidSock) { freeaddrinfo(res); return false; }
    detail::setTimeouts(s, timeoutMs);

    if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        detail::closeSock(s);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    const std::string head =
        "PUT " + path + " HTTP/1.1\r\n"
        "Host: " + host + ":" + portStr + "\r\n"
        "User-Agent: Blocky\r\n"
        "Content-Type: application/zip\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n";

    if (!detail::sendAllBytes(s, head.data(), head.size()) ||
        !detail::sendAllBytes(s, (const char*)body.data(), body.size())) {
        detail::closeSock(s);
        return false;
    }

    std::string raw;
    char buf[4096];
    for (;;) {
        const int n = (int)recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, (size_t)n);
        if (raw.size() > 1u * 1024 * 1024) break;
    }
    detail::closeSock(s);

    const size_t headEnd = raw.find("\r\n\r\n");
    if (headEnd != std::string::npos) reply = raw.substr(headEnd + 4);
    return raw.compare(0, 12, "HTTP/1.1 200") == 0 || raw.compare(0, 12, "HTTP/1.0 200") == 0;
}

} // namespace http
} // namespace blk
