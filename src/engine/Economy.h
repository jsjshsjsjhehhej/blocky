#pragma once


#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <direct.h>
#endif

struct EconomyState {
    int coins = 500;
    int shadowXor = 500 ^ 0x5A5A5A5A;
    uint32_t seal = 0;
    bool tampered = false;
    bool banned = false;
    int cheatStrikes = 0;
    std::string lastError;
    float lastStrikeTime = -1000.f;

    static constexpr int kMaxCoins = 250000;
    static constexpr int kStartCoins = 500;
    static constexpr int kMaxGrant = 5000;
    static constexpr int kXorKey = (int)0x5A5A5A5A;
    static constexpr int kStrikesToBan = 2;
    static constexpr float kStrikeCooldownSec = 8.f;
};

enum class EcoCheatKind {
    DiskForgery,
    MemoryPoke,
    IllegalGrant,
    HostileInject
};

inline EconomyState& economy() {
    static EconomyState e;
    return e;
}

inline uint32_t ecoFnv(const void* data, size_t n, uint32_t seed) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint32_t h = seed;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

inline uint32_t ecoHashUserCoins(const char* user, int coins) {
    uint32_t h = 0x811C9DC5u;
    const char* key = "BlockyBucksSealV1";
    h = ecoFnv(key, std::strlen(key), h);
    if (user && user[0])
        h = ecoFnv(user, std::strlen(user), h);
    h = ecoFnv(&coins, sizeof(coins), h);
    h ^= 0xC01A5EEDu;
    return h;
}

inline uint32_t ecoDiskSignature(const char* user, int coins) {
    uint32_t h = ecoHashUserCoins(user, coins);
    h ^= 0xD15C5160u;
    const char* diskKey = "profile.disk.v1";
    h = ecoFnv(diskKey, std::strlen(diskKey), h);
    return h;
}

inline bool ecoCheckDiskSignature(const char* user, int coins, uint32_t sig) {
    return ecoDiskSignature(user, coins) == sig;
}

inline std::string ecoIntegrityDir() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0]) {
        std::string dir = std::string(appdata) + "\\Blocky";
        _mkdir(dir.c_str());
        return dir;
    }
#endif
    return ".";
}

inline std::string ecoIntegrityPath(const char* user) {
    uint32_t h = ecoFnv(user ? user : "", user ? std::strlen(user) : 0, 0xBA11u);
    char name[64];
    std::snprintf(name, sizeof(name), "integrity_%08X.dat", (unsigned)h);
    return ecoIntegrityDir() + "/" + name;
}

inline void ecoLoadIntegrity(const char* user) {
    auto& e = economy();
    e.cheatStrikes = 0;
    e.banned = false;
    if (!user || !user[0]) return;
    FILE* f = std::fopen(ecoIntegrityPath(user).c_str(), "rb");
    if (!f) return;
    int strikes = 0, banned = 0;
    if (std::fscanf(f, "%d %d", &strikes, &banned) >= 1) {
        e.cheatStrikes = std::max(0, std::min(strikes, 99));
        e.banned = banned != 0 || e.cheatStrikes >= EconomyState::kStrikesToBan;
    }
    std::fclose(f);
}

inline void ecoSaveIntegrity(const char* user) {
    if (!user || !user[0]) return;
    auto& e = economy();
    FILE* f = std::fopen(ecoIntegrityPath(user).c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "%d %d\n", e.cheatStrikes, e.banned ? 1 : 0);
    std::fclose(f);
}

inline void ecoReseal(const char* user) {
    auto& e = economy();
    if (e.coins < 0) e.coins = 0;
    if (e.coins > EconomyState::kMaxCoins) e.coins = EconomyState::kMaxCoins;
    e.shadowXor = e.coins ^ EconomyState::kXorKey;
    e.seal = ecoHashUserCoins(user ? user : "", e.coins);
}

inline bool ecoReportConfirmedCheat(const char* user, EcoCheatKind kind, float nowTime,
                                    const char* detail) {
    auto& e = economy();
    e.tampered = true;

    const char* kindStr = "cheat";
    if (kind == EcoCheatKind::DiskForgery) kindStr = "disk forgery";
    else if (kind == EcoCheatKind::MemoryPoke) kindStr = "memory poke";
    else if (kind == EcoCheatKind::IllegalGrant) kindStr = "illegal grant";
    else if (kind == EcoCheatKind::HostileInject) kindStr = "inject/cheat tool";

    if (e.banned) {
        e.lastError = "Account bannato per cheat economia";
        return true;
    }

    if (nowTime - e.lastStrikeTime < EconomyState::kStrikeCooldownSec) {
        e.lastError = std::string("B$ protetti (") + kindStr + ") — saldo ripristinato";
        std::fprintf(stderr, "[Economy] cheat debounce (%s): %s\n", kindStr,
                     detail ? detail : "");
        return false;
    }

    e.lastStrikeTime = nowTime;
    ++e.cheatStrikes;
    std::fprintf(stderr, "[Economy] CHEAT strike %d/%d (%s): %s\n",
                 e.cheatStrikes, EconomyState::kStrikesToBan, kindStr,
                 detail ? detail : "");

    if (e.cheatStrikes >= EconomyState::kStrikesToBan) {
        e.banned = true;
        e.lastError = "BANNATO PERMANENTE — cheat economia (2 tentativi)";
        e.coins = 0;
        ecoReseal(user);
        ecoSaveIntegrity(user);
        return true;
    }

    e.lastError = std::string("Avviso sicurezza (") + std::to_string(e.cheatStrikes)
        + "/" + std::to_string(EconomyState::kStrikesToBan)
        + "): " + kindStr + " — ancora un tentativo = ban";
    e.coins = EconomyState::kStartCoins;
    ecoReseal(user);
    ecoSaveIntegrity(user);
    return false;
}

inline void ecoBoot(const char* user, int coins) {
    auto& e = economy();
    e.tampered = false;
    e.lastError.clear();
    ecoLoadIntegrity(user);
    if (e.banned) {
        e.coins = 0;
        ecoReseal(user);
        e.lastError = "Account bannato per cheat economia";
        return;
    }
    if (coins < 0) coins = 0;
    if (coins > EconomyState::kMaxCoins)
        coins = EconomyState::kMaxCoins;
    e.coins = coins;
    ecoReseal(user);
}

inline bool ecoVerify(const char* user, float nowTime = 0.f) {
    auto& e = economy();
    if (e.banned) {
        e.coins = 0;
        ecoReseal(user);
        return false;
    }
    int fromShadow = e.shadowXor ^ EconomyState::kXorKey;
    uint32_t expect = ecoHashUserCoins(user ? user : "", e.coins);
    if (fromShadow != e.coins || expect != e.seal) {
        if (nowTime > 0.f)
            ecoReportConfirmedCheat(user, EcoCheatKind::MemoryPoke, nowTime, "seal/shadow mismatch");
        else {
            e.tampered = true;
            e.lastError = "B$ inconsistenti — ripristino";
            e.coins = EconomyState::kStartCoins;
            ecoReseal(user);
        }
        return false;
    }
    return true;
}

inline int ecoBalance(const char* user, float nowTime = 0.f) {
    if (economy().banned) return 0;
    ecoVerify(user, nowTime);
    return economy().coins;
}

inline bool ecoCanSpend(const char* user, int amount) {
    if (economy().banned || amount <= 0) return false;
    return ecoBalance(user) >= amount;
}

inline bool ecoSpend(const char* user, int amount, const char* reason = nullptr) {
    auto& e = economy();
    if (e.banned) {
        e.lastError = "Account bannato";
        return false;
    }
    if (!ecoVerify(user, 0.f)) {
        return false;
    }
    if (amount <= 0 || e.coins < amount) {
        e.lastError = "B$ insufficienti";
        return false;
    }
    e.coins -= amount;
    ecoReseal(user);
    if (reason)
        std::fprintf(stdout, "[Economy] -%d B$ (%s) -> %d\n", amount, reason, e.coins);
    return true;
}

inline bool ecoGrant(const char* user, int amount, const char* reason = nullptr,
                     float nowTime = 0.f) {
    auto& e = economy();
    if (e.banned) {
        e.lastError = "Account bannato";
        return false;
    }
    if (!ecoVerify(user, 0.f)) return false;
    if (amount <= 0) return false;
    if (amount > EconomyState::kMaxGrant) {
        if (nowTime > 0.f)
            ecoReportConfirmedCheat(user, EcoCheatKind::IllegalGrant, nowTime, "grant eccessivo");
        else {
            e.tampered = true;
            e.lastError = "grant B$ troppo alto bloccato";
        }
        return false;
    }
    long long next = (long long)e.coins + amount;
    if (next > EconomyState::kMaxCoins) next = EconomyState::kMaxCoins;
    e.coins = (int)next;
    ecoReseal(user);
    if (reason)
        std::fprintf(stdout, "[Economy] +%d B$ (%s) -> %d\n", amount, reason, e.coins);
    return true;
}

inline int ecoSanitizeLoadedCoins(const char* user, int coins, bool hasSig, bool sigOk,
                                  bool* outConfirmedDiskCheat) {
    if (outConfirmedDiskCheat) *outConfirmedDiskCheat = false;
    if (coins < 0) coins = 0;

    if (hasSig && !sigOk) {
        if (outConfirmedDiskCheat) *outConfirmedDiskCheat = true;
        return EconomyState::kStartCoins;
    }

    if (!hasSig) {
        if (coins > EconomyState::kMaxCoins)
            coins = EconomyState::kMaxCoins;
        return coins;
    }

    if (coins > EconomyState::kMaxCoins)
        coins = EconomyState::kMaxCoins;
    return coins;
}
