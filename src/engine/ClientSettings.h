#pragma once

/**
 * Impostazioni del client decise dal server, come i FastFlag di Roblox.
 *
 * Il motore resta C++ dentro l'app, ma molte decisioni le prende il backend
 * (backend/, Node + PostgreSQL): quali funzioni sono accese, numeri e testi,
 * qual e' l'ultima versione e se quella installata e' ancora ammessa. Per
 * cambiarle basta il server, senza pubblicare una nuova app.
 *
 * All'avvio si usano subito le ultime impostazioni ricevute (salvate su
 * disco), poi in background si chiedono quelle nuove: l'avvio non aspetta
 * mai la rete, e senza rete restano le ultime buone. Se non c'e' nemmeno la
 * cache vale il default scritto nel codice dove la flag si legge.
 *
 * Nomi come sul server: FFlag... = vero/falso, FInt... = intero,
 * FString... = testo. Un valore del tipo sbagliato viene scartato.
 */

#include <atomic>
#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "engine/HttpClient.h"
#include "engine/MiniJson.h"

namespace blk {
namespace clientsettings {

// Versione del client per il backend (min_version, aggiornamenti).
inline constexpr const char* kClientVersion = "0.1.0";

inline const char* platformName() {
#if defined(__ANDROID__)
    return "android";
#elif defined(_WIN32)
    return "windows";
#else
    return "linux";
#endif
}

struct Flag {
    enum class Kind { Bool, Int, String };
    Kind kind = Kind::Bool;
    bool b = false;
    long long i = 0;
    std::string s;
};
using FlagMap = std::map<std::string, Flag>;

inline bool startsWith(const std::string& s, const char* p) { return s.rfind(p, 0) == 0; }

/** Il nome promette un tipo: il valore deve rispettarlo. */
inline bool flagFromJson(const std::string& name, const json::Value& v, Flag& out) {
    if (name.size() > 96) return false;
    if (startsWith(name, "FFlag") && v.type == json::Value::Type::Bool) {
        out.kind = Flag::Kind::Bool;
        out.b = v.b;
        return true;
    }
    if (startsWith(name, "FInt") && v.type == json::Value::Type::Number && v.isInteger) {
        out.kind = Flag::Kind::Int;
        out.i = v.integer;
        return true;
    }
    if (startsWith(name, "FString") && v.type == json::Value::Type::String && v.str.size() <= 2000) {
        out.kind = Flag::Kind::String;
        out.s = v.str;
        return true;
    }
    return false;
}

/** {"flags": {...}}: risposta del server e formato della cache. */
inline bool parseSettings(const std::string& body, FlagMap& out) {
    json::Value root;
    if (!json::parse(body, root)) return false;
    const json::Value* flags = root.get("flags");
    if (!flags || !flags->isObject()) return false;
    out.clear();
    for (const auto& kv : flags->obj) {
        Flag f;
        if (flagFromJson(kv.first, kv.second, f)) out[kv.first] = f;
        else std::printf("[ClientSettings] flag scartata (tipo sbagliato): %s\n", kv.first.c_str());
    }
    return true;
}

inline std::string settingsToJson(const FlagMap& flags) {
    std::string s = "{\"flags\":{";
    bool first = true;
    for (const auto& kv : flags) {
        if (!first) s += ',';
        first = false;
        s += json::quote(kv.first) + ':';
        switch (kv.second.kind) {
        case Flag::Kind::Bool: s += kv.second.b ? "true" : "false"; break;
        case Flag::Kind::Int: s += std::to_string(kv.second.i); break;
        case Flag::Kind::String: s += json::quote(kv.second.s); break;
        }
    }
    return s + "}}";
}

struct VersionInfo {
    bool known = false; // risposta ricevuta
    std::string latest, minSupported, url, sha256, signature, notes;
    bool updateAvailable = false;
    bool mustUpdate = false;
};

inline bool parseVersion(const std::string& body, VersionInfo& out) {
    json::Value root;
    if (!json::parse(body, root) || !root.isObject()) return false;
    out = VersionInfo{};
    auto str = [&](const char* k) {
        const json::Value* v = root.get(k);
        return v && v->type == json::Value::Type::String ? v->str : std::string();
    };
    auto flag = [&](const char* k) {
        const json::Value* v = root.get(k);
        return v && v->type == json::Value::Type::Bool && v->b;
    };
    out.latest = str("latest");
    out.minSupported = str("minSupported");
    out.url = str("url");
    out.sha256 = str("sha256");
    out.signature = str("signature");
    out.notes = str("notes");
    out.updateAvailable = flag("updateAvailable");
    out.mustUpdate = flag("mustUpdate");
    // Solo link web: un "url" con altri schemi non si apre.
    if (!out.url.empty() && !startsWith(out.url, "https://") && !startsWith(out.url, "http://")) out.url.clear();
    out.known = true;
    return true;
}

enum class Source { Defaults, Cache, Server };

inline const char* sourceName(Source s) {
    return s == Source::Server ? "server" : s == Source::Cache ? "ultime salvate" : "default nel codice";
}

struct Snapshot {
    FlagMap flags;
    Source source = Source::Defaults;
    VersionInfo version;
    std::string lastError;
};

namespace detail {
inline std::mutex& mtx() { static std::mutex m; return m; }
inline Snapshot& state() { static Snapshot s; return s; }
inline std::atomic<bool>& fetching() { static std::atomic<bool> b{false}; return b; }
} // namespace detail

inline Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(detail::mtx());
    return detail::state();
}

inline bool isFetching() { return detail::fetching().load(); }

inline bool getBool(const std::string& name, bool def) {
    std::lock_guard<std::mutex> lock(detail::mtx());
    const auto& f = detail::state().flags;
    const auto it = f.find(name);
    return it != f.end() && it->second.kind == Flag::Kind::Bool ? it->second.b : def;
}

inline long long getInt(const std::string& name, long long def) {
    std::lock_guard<std::mutex> lock(detail::mtx());
    const auto& f = detail::state().flags;
    const auto it = f.find(name);
    return it != f.end() && it->second.kind == Flag::Kind::Int ? it->second.i : def;
}

inline std::string getString(const std::string& name, const std::string& def) {
    std::lock_guard<std::mutex> lock(detail::mtx());
    const auto& f = detail::state().flags;
    const auto it = f.find(name);
    return it != f.end() && it->second.kind == Flag::Kind::String ? it->second.s : def;
}

/** Le ultime impostazioni ricevute, da disco. Falso se mancano o sono rotte. */
inline bool loadCache(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    FlagMap flags;
    if (!parseSettings(ss.str(), flags)) return false;
    std::lock_guard<std::mutex> lock(detail::mtx());
    detail::state().flags = std::move(flags);
    detail::state().source = Source::Cache;
    return true;
}

inline bool saveCache(const std::string& path, const FlagMap& flags) {
    // Scrittura su un file a parte e poi rinomina: un crash a meta' non
    // lascia una cache troncata.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << settingsToJson(flags);
        if (!f) return false;
    }
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

inline bool isSafeQueryValue(const std::string& s) {
    if (s.empty() || s.size() > 64) return false;
    for (const char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.'))
            return false;
    return true;
}

/**
 * Chiede al server impostazioni e versione, su un thread: l'avvio e i frame
 * non aspettano. device = id casuale del dispositivo (per i rollout a
 * percentuale), mai dati dell'utente.
 */
inline void startFetch(const std::string& host, int port, const std::string& device, const std::string& cachePath) {
    if (host.empty() || port <= 0) return;
    bool expected = false;
    if (!detail::fetching().compare_exchange_strong(expected, true)) return;
    std::thread([host, port, device, cachePath]() {
        std::string query = std::string("platform=") + platformName() + "&version=" + kClientVersion;
        if (isSafeQueryValue(device)) query += "&device=" + device;

        std::string error;
        const auto s = http::getEx(host, port, "/v1/client/settings?" + query, 4000, 256 * 1024);
        FlagMap flags;
        const bool settingsOk = s.ok && s.status == 200 && parseSettings(s.body, flags);
        if (!settingsOk) error = s.ok ? "risposta del server non valida (" + std::to_string(s.status) + ")"
                                      : "server non raggiungibile";
        else saveCache(cachePath, flags);

        const auto v = http::getEx(host, port, std::string("/v1/client/version?platform=") + platformName()
                                   + "&version=" + kClientVersion, 4000, 64 * 1024);
        VersionInfo ver;
        const bool versionOk = v.ok && v.status == 200 && parseVersion(v.body, ver);

        {
            std::lock_guard<std::mutex> lock(detail::mtx());
            Snapshot& st = detail::state();
            if (settingsOk) {
                st.flags = std::move(flags);
                st.source = Source::Server;
            }
            if (versionOk) st.version = ver;
            st.lastError = error;
        }
        std::printf("[ClientSettings] %s\n", settingsOk ? "impostazioni ricevute dal server" : error.c_str());
        detail::fetching().store(false);
    }).detach();
}

/** Per i test: torna allo stato iniziale. */
inline void resetForTests() {
    std::lock_guard<std::mutex> lock(detail::mtx());
    detail::state() = Snapshot{};
}

} // namespace clientsettings
} // namespace blk
