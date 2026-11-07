#pragma once

/**
 * Indirizzo del backend (backend/, Node + PostgreSQL): impostazioni del
 * client e versioni. Stesso schema di PoolConfig.h: finche' kHost e' vuoto
 * non parte nessuna chiamata, e il client usa le impostazioni salvate o i
 * default. Per provare in sviluppo: variabile BLOCKY_BACKEND=host:porta.
 */

#include <cstdlib>
#include <string>

namespace blk::BackendConfig {

inline const char* kHost = "";
inline int         kPort = 8090;

struct Address {
    std::string host;
    int port = 0;
    bool configured() const { return !host.empty() && port > 0 && port <= 65535; }
};

inline Address address() {
    Address a{kHost, kPort};
    if (const char* env = std::getenv("BLOCKY_BACKEND")) {
        const std::string s = env;
        const size_t colon = s.rfind(':');
        if (colon != std::string::npos && colon > 0) {
            a.host = s.substr(0, colon);
            a.port = std::atoi(s.c_str() + colon + 1);
        } else if (!s.empty()) {
            a.host = s;
        }
    }
    return a;
}

} // namespace blk::BackendConfig
