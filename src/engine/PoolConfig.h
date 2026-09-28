#pragma once

/**
 * Indirizzo del pool "blk://" (relay per multiplayer via internet — vedi
 * PoolProtocol.h e LanNet.h). Stesso schema di AutoUpdater.h: finche'
 * kPoolHost resta il placeholder, "Ospita Online"/"Entra con codice" restano
 * disattivati in UI e non parte nessuna chiamata di rete a vuoto. Riempire
 * con l'indirizzo reale di una VPS che fa girare BlockyPool.exe.
 */
#include <string>

namespace blk::PoolConfig {
inline const char* kPoolHost = "REPLACE_ME";
inline int         kPoolPort = 27020;

inline bool isConfigured() {
    return std::string(kPoolHost) != "REPLACE_ME" && kPoolHost[0] != '\0';
}
}
