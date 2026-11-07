#pragma once

/**
 * Richieste web verso il server Blocky online (catalogo giochi, ...):
 * qualsiasi metodo, corpo, token, http:// e https://.
 *
 *   http://   socket normali (come HttpClient.h), per provare in rete locale
 *   https://  Windows: WinHTTP (TLS del sistema, certificati di Windows)
 *             Android: un aggancio registrato dalla piattaforma
 *             (android_bridge.cpp, HttpsURLConnection via JNI)
 *
 * Sincrono: chi chiama sta gia' su un thread a parte (RemoteGames).
 */

#include "platform/SystemApi.h"

#include <algorithm>
#include <cctype>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace blk {
namespace web {

struct Request {
    std::string method = "GET";
    std::string url;
    std::string body;
    std::string contentType;   // es. application/json
    std::string bearer;        // token: "Authorization: Bearer <...>"
    int timeoutMs = 15000;
    size_t maxBytes = 64u * 1024 * 1024;
};

struct Response {
    bool transport = false;    // la richiesta e' arrivata e c'e' una risposta
    int status = 0;
    std::string body;
    std::string error;         // perche' non e' arrivata (rete, TLS, ...)
    bool ok() const { return transport && status >= 200 && status < 300; }
};

struct Url {
    bool https = false;
    std::string host;
    int port = 0;
    std::string path = "/";
};

inline bool hasScheme(const std::string& s) {
    return s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0;
}

inline bool parseUrl(const std::string& url, Url& out) {
    size_t p;
    if (url.rfind("https://", 0) == 0) { out.https = true; p = 8; }
    else if (url.rfind("http://", 0) == 0) { out.https = false; p = 7; }
    else return false;
    const size_t slash = url.find('/', p);
    std::string hostPort = url.substr(p, slash == std::string::npos ? std::string::npos : slash - p);
    out.path = slash == std::string::npos ? "/" : url.substr(slash);
    out.port = out.https ? 443 : 80;
    const size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos) {
        out.port = std::atoi(hostPort.c_str() + colon + 1);
        hostPort = hostPort.substr(0, colon);
    }
    out.host = hostPort;
    return !out.host.empty() && out.port > 0 && out.port <= 65535;
}

/** Unisce una base ("https://api.x.it" o ".../") e un percorso ("/games"). */
inline std::string join(std::string base, const std::string& path) {
    while (!base.empty() && base.back() == '/') base.pop_back();
    return base + (path.empty() || path[0] == '/' ? path : "/" + path);
}

/** HTTPS fornito dalla piattaforma (Android). Vero = c'e' stata una risposta. */
using HttpsHook = bool (*)(const Request&, Response&);
inline HttpsHook& httpsHook() { static HttpsHook h = nullptr; return h; }

namespace detail {

#ifdef _WIN32
using Sock = SOCKET;
inline void closeSock(Sock s) { ::closesocket(s); }
inline bool validSock(Sock s) { return s != INVALID_SOCKET; }
#else
using Sock = int;
inline void closeSock(Sock s) { ::close(s); }
inline bool validSock(Sock s) { return s >= 0; }
#endif

inline int statusOf(const std::string& raw) {
    // "HTTP/1.1 200 OK"
    const size_t sp = raw.find(' ');
    if (sp == std::string::npos) return 0;
    return std::atoi(raw.c_str() + sp + 1);
}

/** http:// con i socket. Risposte con Content-Length o fino alla chiusura. */
inline Response sendPlain(const Request& rq, const Url& u) {
    Response out;
    if (!plat::sys::netInit()) { out.error = "rete non disponibile"; return out; }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(u.port);
    if (getaddrinfo(u.host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        out.error = "indirizzo non trovato: " + u.host;
        return out;
    }
    Sock s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (!validSock(s)) { freeaddrinfo(res); out.error = "socket fallita"; return out; }
#ifdef _WIN32
    DWORD tv = (DWORD)rq.timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
    timeval tv{rq.timeoutMs / 1000, (rq.timeoutMs % 1000) * 1000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
    if (connect(s, res->ai_addr, (int)res->ai_addrlen) != 0) {
        closeSock(s);
        freeaddrinfo(res);
        out.error = "connessione rifiutata da " + u.host + ":" + portStr;
        return out;
    }
    freeaddrinfo(res);

    std::string head = rq.method + " " + u.path + " HTTP/1.1\r\n"
                       "Host: " + u.host + ":" + portStr + "\r\n"
                       "User-Agent: Blocky\r\n"
                       "Accept: */*\r\n"
                       "Connection: close\r\n";
    if (!rq.contentType.empty()) head += "Content-Type: " + rq.contentType + "\r\n";
    if (!rq.bearer.empty()) head += "Authorization: Bearer " + rq.bearer + "\r\n";
    if (!rq.body.empty() || rq.method == "PUT" || rq.method == "POST")
        head += "Content-Length: " + std::to_string(rq.body.size()) + "\r\n";
    head += "\r\n";
    auto sendAll = [&](const char* d, size_t n) {
        while (n > 0) {
            const int k = (int)::send(s, d, (int)std::min<size_t>(n, 1u << 20), 0);
            if (k <= 0) return false;
            d += k;
            n -= (size_t)k;
        }
        return true;
    };
    if (!sendAll(head.data(), head.size()) || !sendAll(rq.body.data(), rq.body.size())) {
        closeSock(s);
        out.error = "invio interrotto";
        return out;
    }

    std::string raw;
    char buf[16384];
    long long want = -1;
    size_t headEnd = std::string::npos;
    for (;;) {
        const int n = (int)recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        raw.append(buf, (size_t)n);
        if (headEnd == std::string::npos) {
            headEnd = raw.find("\r\n\r\n");
            if (headEnd != std::string::npos) {
                std::string h = raw.substr(0, headEnd);
                for (char& c : h) c = (char)std::tolower((unsigned char)c);
                const size_t cl = h.find("\ncontent-length:");
                if (cl != std::string::npos) want = std::atoll(h.c_str() + cl + 16);
            }
        }
        if (headEnd != std::string::npos && want >= 0 && raw.size() - (headEnd + 4) >= (size_t)want) break;
        if (raw.size() > rq.maxBytes + 65536) { out.error = "risposta troppo grande"; break; }
    }
    closeSock(s);
    if (headEnd == std::string::npos) {
        if (out.error.empty()) out.error = "nessuna risposta";
        return out;
    }
    out.transport = true;
    out.status = statusOf(raw);
    out.body = raw.substr(headEnd + 4);
    if (want >= 0 && out.body.size() > (size_t)want) out.body.resize((size_t)want);
    return out;
}

#ifdef _WIN32
inline std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

/** https:// su Windows con WinHTTP: TLS e certificati li controlla il sistema. */
inline Response sendWinHttp(const Request& rq, const Url& u) {
    Response out;
    HINTERNET session = WinHttpOpen(L"Blocky", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { out.error = "WinHTTP non disponibile"; return out; }
    WinHttpSetTimeouts(session, rq.timeoutMs, rq.timeoutMs, rq.timeoutMs, rq.timeoutMs);
    HINTERNET conn = WinHttpConnect(session, widen(u.host).c_str(), (INTERNET_PORT)u.port, 0);
    HINTERNET req = conn ? WinHttpOpenRequest(conn, widen(rq.method).c_str(), widen(u.path).c_str(),
                                             nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             u.https ? WINHTTP_FLAG_SECURE : 0)
                         : nullptr;
    if (!req) {
        out.error = "connessione a " + u.host + " fallita";
    } else {
        std::wstring headers;
        if (!rq.contentType.empty()) headers += L"Content-Type: " + widen(rq.contentType) + L"\r\n";
        if (!rq.bearer.empty()) headers += L"Authorization: Bearer " + widen(rq.bearer) + L"\r\n";
        const BOOL sent = WinHttpSendRequest(
            req, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
            headers.empty() ? 0 : (DWORD)-1L,
            rq.body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)rq.body.data(),
            (DWORD)rq.body.size(), (DWORD)rq.body.size(), 0);
        if (!sent || !WinHttpReceiveResponse(req, nullptr)) {
            out.error = "richiesta a " + u.host + " fallita (codice " + std::to_string(GetLastError()) + ")";
        } else {
            DWORD status = 0, len = sizeof(status);
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
            out.status = (int)status;
            out.transport = true;
            for (;;) {
                DWORD avail = 0;
                if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
                const size_t old = out.body.size();
                out.body.resize(old + avail);
                DWORD got = 0;
                if (!WinHttpReadData(req, &out.body[old], avail, &got)) { out.body.resize(old); break; }
                out.body.resize(old + got);
                if (out.body.size() > rq.maxBytes) { out.error = "risposta troppo grande"; break; }
            }
        }
    }
    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return out;
}
#endif

} // namespace detail

inline Response send(const Request& rq) {
    Url u;
    if (!parseUrl(rq.url, u)) {
        Response r;
        r.error = "indirizzo non valido: " + rq.url;
        return r;
    }
    if (!u.https) return detail::sendPlain(rq, u);
#ifdef _WIN32
    return detail::sendWinHttp(rq, u);
#else
    Response r;
    if (httpsHook() && httpsHook()(rq, r)) return r;
    if (r.error.empty()) r.error = "HTTPS non disponibile su questa piattaforma";
    return r;
#endif
}

inline Response get(const std::string& url, int timeoutMs = 15000) {
    Request rq;
    rq.url = url;
    rq.timeoutMs = timeoutMs;
    return send(rq);
}

// ---- base64 (per spedire i file del gioco dentro il JSON) ----
inline std::string base64(const std::string& in) {
    static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const uint32_t v = ((uint8_t)in[i] << 16) | ((uint8_t)in[i + 1] << 8) | (uint8_t)in[i + 2];
        out += k[(v >> 18) & 63]; out += k[(v >> 12) & 63]; out += k[(v >> 6) & 63]; out += k[v & 63];
    }
    if (i + 1 == in.size()) {
        const uint32_t v = (uint8_t)in[i] << 16;
        out += k[(v >> 18) & 63]; out += k[(v >> 12) & 63]; out += "==";
    } else if (i + 2 == in.size()) {
        const uint32_t v = ((uint8_t)in[i] << 16) | ((uint8_t)in[i + 1] << 8);
        out += k[(v >> 18) & 63]; out += k[(v >> 12) & 63]; out += k[(v >> 6) & 63]; out += '=';
    }
    return out;
}

} // namespace web
} // namespace blk
