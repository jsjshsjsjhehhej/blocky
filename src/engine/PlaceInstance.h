#pragma once

/**
 * Identita' di una partita in corso: PlaceId + JobId, come in Roblox.
 *
 *   PlaceId — QUALE gioco (l'id di GameEntry, es. "obby_party").
 *   JobId   — QUALE server di quel gioco: ogni host o server dedicato ne
 *             genera uno nuovo all'avvio (un GUID). Due server dello stesso
 *             gioco hanno lo stesso PlaceId e JobId diversi.
 *
 * Con il JobId si entra in un server PRECISO (un amico, un server privato)
 * invece che nel primo che risponde. Qui c'e' solo la parte senza rete:
 * generare/validare i JobId, leggere gli inviti, la coda dei teleport chiesti
 * dagli script. La ricerca vera e' in PlaceLauncher.h.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace blk {
namespace jobid {

inline constexpr int kLen = 36; // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx

inline bool isHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/** GUID versione 4 in minuscolo, lo stesso formato dei JobId di Roblox. */
inline std::string generate() {
    // random_device da solo su alcune toolchain e' deterministico: si mescola
    // anche l'orologio, cosi' due server avviati insieme non si scontrano.
    static std::mt19937_64 rng = [] {
        std::random_device rd;
        const uint64_t t = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
        std::seed_seq seq{rd(), rd(), rd(), (unsigned)t, (unsigned)(t >> 32)};
        return std::mt19937_64(seq);
    }();
    const uint64_t a = rng(), b = rng();
    unsigned char bytes[16];
    for (int i = 0; i < 8; ++i) {
        bytes[i] = (unsigned char)(a >> (i * 8));
        bytes[8 + i] = (unsigned char)(b >> (i * 8));
    }
    bytes[6] = (unsigned char)((bytes[6] & 0x0F) | 0x40); // versione 4
    bytes[8] = (unsigned char)((bytes[8] & 0x3F) | 0x80); // variante RFC 4122
    char out[kLen + 1];
    std::snprintf(out, sizeof(out),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                  bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return out;
}

inline bool isValid(const std::string& s) {
    if ((int)s.size() != kLen) return false;
    for (int i = 0; i < kLen; ++i) {
        const bool dash = (i == 8 || i == 13 || i == 18 || i == 23);
        if (dash ? s[(size_t)i] != '-' : !isHex(s[(size_t)i])) return false;
    }
    return true;
}

/**
 * Quello che l'utente incolla diventa un JobId canonico, o "" se non lo e'.
 * Accetta maiuscole, spazi ai lati, graffe {..} e i 32 caratteri senza
 * trattini (come lo stampano certi strumenti).
 */
inline std::string normalize(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '{')) s.erase(s.begin());
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'
                          || s.back() == '}')) s.pop_back();
    for (char& c : s)
        if (c >= 'A' && c <= 'F') c = (char)(c - 'A' + 'a');
    if (s.size() == 32) {
        for (char c : s)
            if (!isHex(c)) return {};
        s = s.substr(0, 8) + "-" + s.substr(8, 4) + "-" + s.substr(12, 4) + "-" + s.substr(16, 4)
          + "-" + s.substr(20);
    }
    return isValid(s) ? s : std::string();
}

/** Le prime 8 cifre: bastano a distinguere i server in un elenco. */
inline std::string shortForm(const std::string& s) {
    return s.size() > 8 ? s.substr(0, 8) : s;
}

} // namespace jobid

namespace placeinvite {

/** Id di un gioco: lettere, cifre, "_", "-", "." (niente spazi ne' "/" o "?"). */
inline bool isPlaceIdChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '_' || c == '-' || c == '.';
}

inline bool isValidPlaceId(const std::string& id) {
    if (id.empty() || id.size() > 64) return false;
    for (char c : id)
        if (!isPlaceIdChar(c)) return false;
    return true;
}

struct Target {
    std::string placeId;  // vuoto = il gioco gia' selezionato
    std::string jobId;    // vuoto = qualsiasi server
    std::string poolCode; // codice blk:// (via internet), alternativo al JobId
};

inline const char* kScheme = "blocky://place/";

/** Il link da mandare a un amico: blocky://place/<placeId>?job=<jobId>. */
inline std::string make(const std::string& placeId, const std::string& jobId) {
    std::string s = std::string(kScheme) + placeId;
    if (!jobId.empty()) s += "?job=" + jobId;
    return s;
}

inline bool isPoolCode(const std::string& s) {
    if (s.size() != 6) return false;
    for (char c : s) {
        const char u = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        bool ok = false;
        for (const char* a = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ"; *a; ++a)
            if (*a == u) { ok = true; break; }
        if (!ok) return false;
    }
    return true;
}

/**
 * Legge quello che l'utente incolla nel campo "Unisciti":
 *   blocky://place/<placeId>?job=<jobId>   invito completo
 *   blocky://place/<placeId>               quel gioco, qualsiasi server
 *   <jobId>                                server preciso del gioco scelto
 *   blk://ABC234  oppure  ABC234           stanza via internet
 * Falso se non e' nessuna di queste cose.
 */
inline bool parse(std::string text, Target& out) {
    out = {};
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.erase(text.begin());
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'
                             || text.back() == '\n')) text.pop_back();
    if (text.empty()) return false;

    const std::string scheme = kScheme;
    if (text.compare(0, scheme.size(), scheme) == 0) {
        std::string rest = text.substr(scheme.size());
        std::string job;
        const size_t q = rest.find('?');
        if (q != std::string::npos) {
            const std::string query = rest.substr(q + 1);
            rest = rest.substr(0, q);
            if (query.compare(0, 4, "job=") != 0) return false;
            job = jobid::normalize(query.substr(4));
            if (job.empty()) return false;
        }
        while (!rest.empty() && rest.back() == '/') rest.pop_back();
        if (!isValidPlaceId(rest)) return false;
        out.placeId = rest;
        out.jobId = job;
        return true;
    }
    if (text.compare(0, 6, "blk://") == 0) {
        const std::string code = text.substr(6);
        if (!isPoolCode(code)) return false;
        for (char c : code) out.poolCode += (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        return true;
    }
    const std::string job = jobid::normalize(text);
    if (!job.empty()) {
        out.jobId = job;
        return true;
    }
    if (isPoolCode(text)) {
        for (char c : text) out.poolCode += (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        return true;
    }
    return false;
}

} // namespace placeinvite

namespace teleport {

/**
 * Teleport chiesti dagli script (TeleportService). Lo script non puo' cambiare
 * partita da solo — il motore deve chiudere la rete, scaricare il gioco e
 * ricaricare — quindi la richiesta si mette in coda e la legge il ciclo
 * principale tra un frame e l'altro.
 */
struct Request {
    std::string placeId;
    std::string jobId; // vuoto = qualsiasi server di quel gioco
};

inline std::vector<Request>& queue() { static std::vector<Request> q; return q; }

/** Falso (e niente in coda) se gli id non sono validi: lo script lo sa subito. */
inline bool request(const std::string& placeId, const std::string& jobIdText) {
    if (!placeinvite::isValidPlaceId(placeId)) return false;
    std::string job;
    if (!jobIdText.empty()) {
        job = jobid::normalize(jobIdText);
        if (job.empty()) return false;
    }
    // Uno solo alla volta: un loop che chiama Teleport ogni frame non deve
    // accodare mille cambi di partita.
    queue().clear();
    queue().push_back({placeId, job});
    return true;
}

} // namespace teleport
} // namespace blk
