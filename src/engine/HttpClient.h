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

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
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

struct GetResult {
    bool ok = false;
    bool skipped = false;         // cache hit: locale gia' allineato a Content-Length
    int status = 0;
    long long contentLength = -1; // -1 = sconosciuto
    std::string body;
};

inline long long parseContentLength(const std::string& headers) {
    // Case-insensitive "Content-Length:"
    for (size_t i = 0; i + 15 < headers.size(); ++i) {
        char c0 = headers[i];
        if (c0 != 'C' && c0 != 'c') continue;
        std::string slice = headers.substr(i, 15);
        for (char& c : slice) c = (char)std::tolower((unsigned char)c);
        if (slice != "content-length:") continue;
        size_t j = i + 15;
        while (j < headers.size() && (headers[j] == ' ' || headers[j] == '\t')) ++j;
        return std::atoll(headers.c_str() + j);
    }
    return -1;
}

inline int parseStatusCode(const std::string& raw) {
    // "HTTP/1.x NNN ..."
    if (raw.size() < 12) return 0;
    size_t sp = raw.find(' ');
    if (sp == std::string::npos || sp + 3 >= raw.size()) return 0;
    return std::atoi(raw.c_str() + sp + 1);
}

/**
 * GET http://host:port/path — corpo + Content-Length.
 * Se knownLocalBytes >= 0 e coincide con Content-Length, chiude subito
 * (skipped=true) senza tenere il corpo: cache locale valida.
 * maxBodyBytes: tetto sicurezza (place grandi fino a ~32 MB).
 */
inline GetResult getEx(const std::string& host, int port, const std::string& path,
                       int timeoutMs = 2500, size_t maxBodyBytes = 32u * 1024 * 1024,
                       long long knownLocalBytes = -1) {
    GetResult out;
    if (host.empty() || port <= 0 || port > 65535) return out;
    if (!detail::ensureSockets()) return out;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) return out;

    HttpSocket s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == kHttpInvalidSock) { freeaddrinfo(res); return out; }
    detail::setTimeouts(s, timeoutMs);

    if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        detail::closeSock(s);
        freeaddrinfo(res);
        return out;
    }
    freeaddrinfo(res);

    const std::string req =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + ":" + portStr + "\r\n"
        "User-Agent: Blocky\r\n"
        "Connection: close\r\n\r\n";
    if (!detail::sendAllBytes(s, req.data(), req.size())) {
        detail::closeSock(s);
        return out;
    }

    std::string raw;
    char buf[8192];
    size_t headEnd = std::string::npos;
    for (;;) {
        const int n = (int)recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, (size_t)n);
        if (headEnd == std::string::npos) {
            headEnd = raw.find("\r\n\r\n");
            if (headEnd != std::string::npos) {
                const std::string headers = raw.substr(0, headEnd);
                out.status = parseStatusCode(raw);
                out.contentLength = parseContentLength(headers);
                if (out.status != 200) {
                    detail::closeSock(s);
                    return out;
                }
                // Cache: stesso byte-count del server → non serve il corpo.
                if (knownLocalBytes >= 0 && out.contentLength >= 0
                    && knownLocalBytes == out.contentLength) {
                    detail::closeSock(s);
                    out.ok = true;
                    out.skipped = true;
                    return out;
                }
            }
        }
        if (raw.size() > maxBodyBytes + 64u * 1024) break;
    }
    detail::closeSock(s);

    if (headEnd == std::string::npos) {
        headEnd = raw.find("\r\n\r\n");
        if (headEnd == std::string::npos) return out;
        out.status = parseStatusCode(raw);
        out.contentLength = parseContentLength(raw.substr(0, headEnd));
    }
    if (out.status != 200) return out;
    out.body = raw.substr(headEnd + 4);
    if (out.contentLength < 0)
        out.contentLength = (long long)out.body.size();
    // Se Content-Length c'era e il corpo e' troncato, fallisce.
    if (out.contentLength >= 0
        && (long long)out.body.size() < out.contentLength
        && (size_t)out.contentLength <= maxBodyBytes)
        return out;
    out.ok = true;
    return out;
}

/**
 * GET — API storica: true solo su 200 con corpo.
 */
inline bool get(const std::string& host, int port, const std::string& path,
                std::string& body, int timeoutMs = 2500) {
    body.clear();
    GetResult r = getEx(host, port, path, timeoutMs);
    if (!r.ok) return false;
    body = std::move(r.body);
    return true;
}

inline void sleepMs(int ms) {
    if (ms > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/** GET con fino a maxAttempts tentativi (rete domestica instabile). */
inline bool getRetry(const std::string& host, int port, const std::string& path,
                     std::string& body, int timeoutMs = 8000, int maxAttempts = 3,
                     long long knownLocalBytes = -1) {
    body.clear();
    for (int a = 0; a < maxAttempts; ++a) {
        GetResult r = getEx(host, port, path, timeoutMs, 32u * 1024 * 1024, knownLocalBytes);
        if (r.ok && r.skipped) return true; // cache hit, body vuoto ok
        if (r.ok && !r.body.empty()) {
            body = std::move(r.body);
            return true;
        }
        if (a + 1 < maxAttempts) sleepMs(120 * (a + 1));
    }
    return false;
}

/** Come getRetry, ma espone skipped/contentLength per la cache file. */
inline GetResult getRetryEx(const std::string& host, int port, const std::string& path,
                            int timeoutMs = 8000, int maxAttempts = 3,
                            long long knownLocalBytes = -1,
                            size_t maxBodyBytes = 32u * 1024 * 1024) {
    GetResult last;
    for (int a = 0; a < maxAttempts; ++a) {
        last = getEx(host, port, path, timeoutMs, maxBodyBytes, knownLocalBytes);
        if (last.ok) return last;
        if (a + 1 < maxAttempts) sleepMs(120 * (a + 1));
    }
    return last;
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
