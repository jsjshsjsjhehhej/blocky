#pragma once

/**
 * Server HTTP/1.1 minimale, bloccante, senza keep-alive.
 *
 * Copre esattamente quello che serve a blocky_hostd: GET per servire file
 * statici, PUT per ricevere un pacchetto da pubblicare. Niente chunked
 * transfer in ingresso (il client deve dichiarare Content-Length), niente
 * pipelining, una connessione = una richiesta = una risposta, poi si chiude.
 *
 * Va bene per LAN e per uso da studio: non è un web server per produzione
 * esposta su internet — niente TLS, niente limiti di concorrenza raffinati.
 * Se un giorno serve di più, qui è il punto di incapsulamento: il resto del
 * motore parla solo con HttpRequest/le funzioni sendXxx, non con i socket.
 */

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace HttpMini {

struct Request {
    std::string method;
    std::string path;                          // solo il path, senza query string
    std::map<std::string, std::string> headers; // chiavi già minuscole
    std::vector<std::uint8_t> body;

    std::string header(const char* key, const std::string& def = {}) const {
        std::string k = key;
        for (char& c : k) c = (char)tolower((unsigned char)c);
        auto it = headers.find(k);
        return it != headers.end() ? it->second : def;
    }
};

inline std::string toLower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

// ---------------------------------------------------------------- lettura

/** Legge esattamente `n` byte, con timeout già impostato sul socket. False se la
 *  connessione cade o il timeout scatta prima di riempire il buffer. */
inline bool recvExact(SOCKET s, std::vector<std::uint8_t>& out, size_t n) {
    out.resize(n);
    size_t got = 0;
    while (got < n) {
        const int r = recv(s, (char*)out.data() + got, (int)(n - got), 0);
        if (r <= 0) return false;
        got += (size_t)r;
    }
    return true;
}

/**
 * Legge finché non trova "\r\n\r\n". Ritorna false se la connessione cade
 * prima. `leftover` riceve i byte letti oltre il terminatore — capita quasi
 * sempre, perché recv() non si ferma esattamente al bordo degli header.
 */
inline bool recvUntilHeadersEnd(SOCKET s, std::string& headerBlock,
                                std::vector<std::uint8_t>& leftover) {
    std::string buf;
    buf.reserve(4096);
    char chunk[2048];

    for (;;) {
        const size_t marker = buf.find("\r\n\r\n");
        if (marker != std::string::npos) {
            headerBlock = buf.substr(0, marker);
            leftover.assign(buf.begin() + (long)(marker + 4), buf.end());
            return true;
        }
        if (buf.size() > 64 * 1024) return false;   // header abnormemente grande: rifiuta

        const int r = recv(s, chunk, sizeof(chunk), 0);
        if (r <= 0) return false;
        buf.append(chunk, (size_t)r);
    }
}

/** Uscita false = richiesta malformata o connessione caduta durante la lettura. */
inline bool parseRequest(SOCKET s, Request& req, std::string& error,
                         size_t maxBodyBytes = 256ull * 1024 * 1024) {
    std::string headerBlock;
    std::vector<std::uint8_t> leftover;
    if (!recvUntilHeadersEnd(s, headerBlock, leftover)) {
        error = "connessione chiusa leggendo gli header";
        return false;
    }

    std::istringstream lines(headerBlock);
    std::string requestLine;
    if (!std::getline(lines, requestLine) || requestLine.empty()) {
        error = "request line mancante";
        return false;
    }
    if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

    std::istringstream rl(requestLine);
    std::string fullPath;
    if (!(rl >> req.method >> fullPath)) {
        error = "request line malformata";
        return false;
    }
    // Via la query string: nessuna rotta qui dentro ne ha bisogno.
    req.path = fullPath.substr(0, fullPath.find('?'));

    std::string headerLine;
    while (std::getline(lines, headerLine)) {
        if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
        if (headerLine.empty()) continue;
        const size_t colon = headerLine.find(':');
        if (colon == std::string::npos) continue;
        std::string key = toLower(headerLine.substr(0, colon));
        size_t vs = colon + 1;
        while (vs < headerLine.size() && headerLine[vs] == ' ') ++vs;
        req.headers[key] = headerLine.substr(vs);
    }

    const std::string clHeader = req.header("content-length");
    if (!clHeader.empty()) {
        long long want = 0;
        try { want = std::stoll(clHeader); } catch (...) { want = -1; }
        if (want < 0 || (size_t)want > maxBodyBytes) {
            error = "Content-Length assente, negativo o oltre il limite";
            return false;
        }
        req.body = leftover;
        if (req.body.size() < (size_t)want) {
            std::vector<std::uint8_t> rest;
            if (!recvExact(s, rest, (size_t)want - req.body.size())) {
                error = "connessione chiusa leggendo il body";
                return false;
            }
            req.body.insert(req.body.end(), rest.begin(), rest.end());
        } else if (req.body.size() > (size_t)want) {
            req.body.resize((size_t)want);   // pipelining non supportato: si scarta l'eccesso
        }
    }

    return true;
}

// ---------------------------------------------------------------- scrittura

inline bool sendAll(SOCKET s, const char* data, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        const int r = send(s, data + sent, (int)(n - sent), 0);
        if (r <= 0) return false;
        sent += (size_t)r;
    }
    return true;
}

inline void sendResponse(SOCKET s, int status, const char* statusText,
                         const std::string& contentType, const std::uint8_t* body,
                         size_t bodyLen, const std::string& extraHeaders = {}) {
    std::ostringstream h;
    h << "HTTP/1.1 " << status << " " << statusText << "\r\n"
      << "Content-Type: " << contentType << "\r\n"
      << "Content-Length: " << bodyLen << "\r\n"
      << "Connection: close\r\n";
    if (!extraHeaders.empty()) h << extraHeaders;
    h << "\r\n";

    const std::string head = h.str();
    if (!sendAll(s, head.data(), head.size())) return;
    if (bodyLen > 0) sendAll(s, (const char*)body, bodyLen);
}

inline void sendText(SOCKET s, int status, const char* statusText, const std::string& text,
                     const std::string& contentType = "text/plain; charset=utf-8") {
    sendResponse(s, status, statusText, contentType,
                (const std::uint8_t*)text.data(), text.size());
}

inline void sendJson(SOCKET s, int status, const char* statusText, const std::string& json) {
    sendResponse(s, status, statusText, "application/json",
                (const std::uint8_t*)json.data(), json.size());
}

// ------------------------------------------------------------------- MIME

inline std::string mimeFor(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    const std::string ext = dot == std::string::npos ? "" : toLower(path.substr(dot));

    if (ext == ".json") return "application/json";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".glb")  return "model/gltf-binary";
    if (ext == ".gltf") return "model/gltf+json";
    if (ext == ".fbx")  return "application/octet-stream";
    if (ext == ".wav")  return "audio/wav";
    if (ext == ".ogg")  return "audio/ogg";
    if (ext == ".lua")  return "text/x-lua; charset=utf-8";
    if (ext == ".bbc")  return "application/octet-stream";
    if (ext == ".txt")  return "text/plain; charset=utf-8";
    if (ext == ".html") return "text/html; charset=utf-8";
    return "application/octet-stream";
}

/** Serve un file dal disco, a blocchi: gli asset possono pesare decine di MB
 *  e non conviene caricarli interi in RAM per ogni richiesta. */
inline bool sendFile(SOCKET s, const std::string& fsPath) {
    std::ifstream f(fsPath, std::ios::binary | std::ios::ate);
    if (!f) return false;

    const std::streamoff size = f.tellg();
    if (size < 0) return false;
    f.seekg(0);

    std::ostringstream h;
    h << "HTTP/1.1 200 OK\r\n"
      << "Content-Type: " << mimeFor(fsPath) << "\r\n"
      << "Content-Length: " << size << "\r\n"
      << "Connection: close\r\n\r\n";
    const std::string head = h.str();
    if (!sendAll(s, head.data(), head.size())) return true;   // header inviato: risposta "gestita"

    char buf[65536];
    while (f) {
        f.read(buf, sizeof(buf));
        const std::streamsize got = f.gcount();
        if (got <= 0) break;
        if (!sendAll(s, buf, (size_t)got)) break;
    }
    return true;
}

} // namespace HttpMini
