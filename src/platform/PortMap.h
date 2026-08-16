#pragma once

/**
 * Apertura automatica della porta sul router (UPnP IGD).
 *
 * Ospitare una partita fra regioni diverse funziona già: LanNet accetta
 * qualsiasi indirizzo IP. Quello che manca è che il router di chi ospita
 * consegni i pacchetti al computer giusto, e configurarlo a mano è il
 * passaggio su cui la gente si ferma.
 *
 * Qui si fa da soli, in tre passi:
 *   1. SSDP — un messaggio in multicast chiede "chi è il router?"
 *   2. HTTP — si scarica la sua descrizione e si trova il servizio giusto
 *   3. SOAP — si chiede di inoltrare la porta, e si legge l'IP pubblico
 *
 * Limiti dichiarati: non funziona se il router ha UPnP disattivato (comune
 * per scelta di sicurezza) né dietro CGNAT, tipico delle connessioni mobili.
 * In quei casi resta il port forwarding manuale, e va detto all'utente invece
 * di lasciarlo davanti a un errore muto.
 */

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace PortMap {

struct Result {
    bool mapped = false;
    std::string externalIp;    // IP pubblico visto dal router
    std::string localIp;       // IP di questa macchina sulla rete di casa
    std::string error;         // vuoto se è andata
};

// --------------------------------------------------------------- utilità

/** Cerca il testo fra due tag, senza tirare dentro un parser XML. */
inline std::string between(const std::string& src, const std::string& openTag,
                           const std::string& closeTag, size_t from = 0) {
    const size_t a = src.find(openTag, from);
    if (a == std::string::npos) return {};
    const size_t s = a + openTag.size();
    const size_t b = src.find(closeTag, s);
    if (b == std::string::npos) return {};
    return src.substr(s, b - s);
}

inline bool containsNoCase(const std::string& hay, const char* needle) {
    std::string h = hay, n = needle;
    for (char& c : h) c = (char)tolower((unsigned char)c);
    for (char& c : n) c = (char)tolower((unsigned char)c);
    return h.find(n) != std::string::npos;
}

/** Spezza "http://192.168.1.1:5000/rootDesc.xml" in host, porta e percorso. */
inline bool splitUrl(const std::string& url, std::string& host, int& port, std::string& path) {
    const std::string prefix = "http://";
    if (url.compare(0, prefix.size(), prefix) != 0) return false;

    const size_t start = prefix.size();
    const size_t slash = url.find('/', start);
    std::string authority = (slash == std::string::npos) ? url.substr(start)
                                                         : url.substr(start, slash - start);
    path = (slash == std::string::npos) ? "/" : url.substr(slash);

    const size_t colon = authority.find(':');
    if (colon == std::string::npos) {
        host = authority;
        port = 80;
    } else {
        host = authority.substr(0, colon);
        port = std::atoi(authority.c_str() + colon + 1);
        if (port <= 0) port = 80;
    }
    return !host.empty();
}

/** Richiesta HTTP grezza; ritorna il corpo della risposta. */
inline std::string httpRequest(const std::string& host, int port, const std::string& raw,
                               int timeoutMs = 4000) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%d", port);

    addrinfo* info = nullptr;
    if (getaddrinfo(host.c_str(), portStr, &hints, &info) != 0 || !info) return {};

    SOCKET s = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (s == INVALID_SOCKET) { freeaddrinfo(info); return {}; }

    DWORD tv = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

    if (connect(s, info->ai_addr, (int)info->ai_addrlen) != 0) {
        closesocket(s);
        freeaddrinfo(info);
        return {};
    }
    freeaddrinfo(info);

    send(s, raw.c_str(), (int)raw.size(), 0);

    std::string response;
    char buf[2048];
    for (;;) {
        const int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, (size_t)n);
        if (response.size() > 256 * 1024) break;   // descrizioni patologiche
    }
    closesocket(s);

    const size_t bodyStart = response.find("\r\n\r\n");
    return bodyStart == std::string::npos ? response : response.substr(bodyStart + 4);
}

/** IP di questa macchina sulla rete che porta al router. */
inline std::string localAddressToward(const std::string& routerHost) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return {};

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(9);   // discard: non viene inviato nulla
    inet_pton(AF_INET, routerHost.c_str(), &dest.sin_addr);

    std::string out;
    // Un socket UDP "connesso" non manda pacchetti: sceglie solo l'interfaccia,
    // che è il modo pulito per sapere con quale IP usciremmo verso il router.
    if (connect(s, (sockaddr*)&dest, sizeof(dest)) == 0) {
        sockaddr_in local{};
        int len = sizeof(local);
        if (getsockname(s, (sockaddr*)&local, &len) == 0) {
            char ip[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
            out = ip;
        }
    }
    closesocket(s);
    return out;
}

// --------------------------------------------------------------- SSDP

/** Chiede in multicast chi è il gateway. Ritorna l'URL della sua descrizione. */
inline std::string discoverGateway(int timeoutMs = 2000) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return {};

    DWORD tv = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &dest.sin_addr);

    static const char* kSearch =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";

    sendto(s, kSearch, (int)std::strlen(kSearch), 0, (sockaddr*)&dest, sizeof(dest));

    // Più router possono rispondere: si prende il primo con un LOCATION.
    char buf[2048];
    for (int attempt = 0; attempt < 4; ++attempt) {
        const int n = recv(s, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        buf[n] = '\0';

        const std::string reply(buf);
        for (const char* key : {"LOCATION: ", "Location: ", "location: "}) {
            const size_t at = reply.find(key);
            if (at == std::string::npos) continue;
            const size_t start = at + std::strlen(key);
            const size_t end = reply.find_first_of("\r\n", start);
            closesocket(s);
            return reply.substr(start, end - start);
        }
    }
    closesocket(s);
    return {};
}

/** Nella descrizione del router trova l'URL di controllo del servizio WAN. */
inline bool findControlUrl(const std::string& xml, std::string& outControl,
                           std::string& outServiceType) {
    // WANIPConnection copre le connessioni via cavo/fibra, WANPPPConnection l'ADSL.
    for (const char* want : {"WANIPConnection", "WANPPPConnection"}) {
        size_t pos = 0;
        while ((pos = xml.find("<service>", pos)) != std::string::npos) {
            const size_t end = xml.find("</service>", pos);
            if (end == std::string::npos) break;

            const std::string block = xml.substr(pos, end - pos);
            const std::string type = between(block, "<serviceType>", "</serviceType>");
            if (containsNoCase(type, want)) {
                std::string ctrl = between(block, "<controlURL>", "</controlURL>");
                if (!ctrl.empty()) {
                    outControl = ctrl;
                    outServiceType = type;
                    return true;
                }
            }
            pos = end + 1;
        }
    }
    return false;
}

inline std::string soapCall(const std::string& host, int port, const std::string& path,
                            const std::string& serviceType, const std::string& action,
                            const std::string& argsXml) {
    const std::string body =
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body><u:" + action + " xmlns:u=\"" + serviceType + "\">" +
        argsXml +
        "</u:" + action + "></s:Body></s:Envelope>";

    char header[512];
    std::snprintf(header, sizeof(header),
        "POST %s HTTP/1.1\r\n"
        "HOST: %s:%d\r\n"
        "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
        "SOAPACTION: \"%s#%s\"\r\n"
        "CONTENT-LENGTH: %d\r\n"
        "CONNECTION: close\r\n\r\n",
        path.c_str(), host.c_str(), port, serviceType.c_str(), action.c_str(), (int)body.size());

    return httpRequest(host, port, std::string(header) + body);
}

// --------------------------------------------------------------- API

/**
 * Apre `port` in UDP verso questa macchina.
 * `leaseSeconds` a 0 = permanente finché non si chiude (alcuni router
 * rifiutano le durate limitate, quindi 0 è il default più compatibile).
 */
inline Result open(int port, const char* description = "Blocky", int leaseSeconds = 0) {
    Result r;

    const std::string location = discoverGateway();
    if (location.empty()) {
        r.error = "Nessun router UPnP trovato (potrebbe essere disattivato)";
        return r;
    }

    std::string host, path;
    int httpPort = 80;
    if (!splitUrl(location, host, httpPort, path)) {
        r.error = "Indirizzo del router non interpretabile";
        return r;
    }

    char get[512];
    std::snprintf(get, sizeof(get),
        "GET %s HTTP/1.1\r\nHOST: %s:%d\r\nCONNECTION: close\r\n\r\n",
        path.c_str(), host.c_str(), httpPort);

    const std::string xml = httpRequest(host, httpPort, get);
    if (xml.empty()) {
        r.error = "Il router non ha risposto alla descrizione";
        return r;
    }

    std::string control, serviceType;
    if (!findControlUrl(xml, control, serviceType)) {
        r.error = "Il router non espone il servizio di inoltro porte";
        return r;
    }
    if (!control.empty() && control[0] != '/') control = "/" + control;

    r.localIp = localAddressToward(host);
    if (r.localIp.empty()) {
        r.error = "Impossibile determinare l'IP locale";
        return r;
    }

    // --- IP pubblico: è quello da dare agli amici ---
    const std::string extResp = soapCall(host, httpPort, control, serviceType,
                                         "GetExternalIPAddress", "");
    r.externalIp = between(extResp, "<NewExternalIPAddress>", "</NewExternalIPAddress>");

    // --- inoltro della porta ---
    char args[1024];
    std::snprintf(args, sizeof(args),
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>%d</NewExternalPort>"
        "<NewProtocol>UDP</NewProtocol>"
        "<NewInternalPort>%d</NewInternalPort>"
        "<NewInternalClient>%s</NewInternalClient>"
        "<NewEnabled>1</NewEnabled>"
        "<NewPortMappingDescription>%s</NewPortMappingDescription>"
        "<NewLeaseDuration>%d</NewLeaseDuration>",
        port, port, r.localIp.c_str(), description, leaseSeconds);

    const std::string addResp = soapCall(host, httpPort, control, serviceType,
                                         "AddPortMapping", args);

    if (addResp.find("AddPortMappingResponse") != std::string::npos) {
        r.mapped = true;
    } else {
        const std::string desc = between(addResp, "<errorDescription>", "</errorDescription>");
        r.error = desc.empty() ? "Il router ha rifiutato l'inoltro" : desc;
    }
    return r;
}

/** Chiude l'inoltro. Da chiamare quando il server si spegne. */
inline bool close(int port) {
    const std::string location = discoverGateway(1200);
    if (location.empty()) return false;

    std::string host, path;
    int httpPort = 80;
    if (!splitUrl(location, host, httpPort, path)) return false;

    char get[512];
    std::snprintf(get, sizeof(get),
        "GET %s HTTP/1.1\r\nHOST: %s:%d\r\nCONNECTION: close\r\n\r\n",
        path.c_str(), host.c_str(), httpPort);

    const std::string xml = httpRequest(host, httpPort, get);
    std::string control, serviceType;
    if (!findControlUrl(xml, control, serviceType)) return false;
    if (!control.empty() && control[0] != '/') control = "/" + control;

    char args[256];
    std::snprintf(args, sizeof(args),
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>%d</NewExternalPort>"
        "<NewProtocol>UDP</NewProtocol>", port);

    const std::string resp = soapCall(host, httpPort, control, serviceType,
                                      "DeletePortMapping", args);
    return resp.find("DeletePortMappingResponse") != std::string::npos;
}

} // namespace PortMap

#else   // --- piattaforme non Windows: stub, si compila e non fa nulla ---

#include <string>

namespace PortMap {
struct Result {
    bool mapped = false;
    std::string externalIp, localIp;
    std::string error = "UPnP non implementato su questa piattaforma";
};
inline Result open(int, const char* = "Blocky", int = 0) { return {}; }
inline bool close(int) { return false; }
}

#endif
