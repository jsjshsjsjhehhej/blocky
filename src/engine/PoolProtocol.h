#pragma once

/**
 * Protocollo del "pool" — un relay pubblico che permette a host e client di
 * giocare via internet senza aprire porte (risolve il CGNAT: entrambi i lati
 * parlano SOLO in uscita verso l'indirizzo noto del pool, mai in entrata).
 *
 * Magic diverso da quello di LanNet ("BLK1") apposta: "BLKP" (Blocky Pool) e'
 * un protocollo separato che il pool capisce, mentre il payload che incapsula
 * (dentro PoolRelay) e' un pacchetto BLK1 di LanNet lasciato byte-per-byte
 * intatto — il pool non lo interpreta mai, lo gira e basta.
 *
 * Condiviso tra LanNet.h (host/client) e blocky_pool.cpp (il relay stesso).
 */

#include <cstdint>
#include <cstring>

namespace blk::PoolProtocol {

enum class PoolMsg : uint8_t {
    Register   = 1, // host->pool: registra una stanza
    Registered = 2, // pool->host: risposta con il codice assegnato
    Heartbeat  = 3, // host->pool: tiene viva la stanza e il binding NAT
    Resolve    = 4, // client->pool: vuole entrare nella stanza "code"
    Resolved   = 5, // pool->client: ok/errore
    Relay      = 6, // bidirezionale: incapsula un pacchetto BLK1 di LanNet
};

enum class PoolResolveResult : uint8_t {
    Ok = 0,
    NotFound = 1,
    Full = 2,
};

// Codice stanza a 6 caratteri, alfabeto senza 0/O/1/I per evitare ambiguita'
// quando l'utente lo legge/detta/copia a mano.
inline const char kPoolCodeAlphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
inline constexpr int kPoolCodeLen = 6;
inline constexpr int kPoolMaxClientsPerRoom = 32; // = LanNet::kMaxPeers

#pragma pack(push, 1)
struct PoolHeader {
    char    magic[4]; // "BLKP"
    uint8_t type;
};

struct PoolRegister {
    PoolHeader hdr;
    char       title[32];
};

struct PoolRegistered {
    PoolHeader hdr;
    uint8_t    ok;
    char       code[8];
};

struct PoolHeartbeat {
    PoolHeader hdr;
    char       code[8];
};

struct PoolResolve {
    PoolHeader hdr;
    char       code[8];
};

struct PoolResolved {
    PoolHeader hdr;
    uint8_t    result; // PoolResolveResult
};

// Header di un pacchetto Relay — il payload BLK1 di LanNet segue subito dopo
// in memoria, fino alla fine del datagramma ricevuto.
struct PoolRelayHeader {
    PoolHeader hdr;
    char       code[8];
    uint8_t    slot; // quale client (lato pool<->host); ignorato lato client
};
#pragma pack(pop)

inline void poolFillMagic(PoolHeader& h, PoolMsg t) {
    h.magic[0] = 'B'; h.magic[1] = 'L'; h.magic[2] = 'K'; h.magic[3] = 'P';
    h.type = (uint8_t)t;
}

inline bool poolCheckMagic(const void* buf, int n) {
    if (n < (int)sizeof(PoolHeader)) return false;
    const PoolHeader* h = (const PoolHeader*)buf;
    return h->magic[0] == 'B' && h->magic[1] == 'L' && h->magic[2] == 'K' && h->magic[3] == 'P';
}

} // namespace blk::PoolProtocol
