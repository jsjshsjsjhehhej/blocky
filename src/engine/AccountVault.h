#pragma once


#include "engine/GameHub.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#include "platform/Platform.h"

#include <ctime>
#include <fstream>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#include <direct.h>
#endif

struct SavedAccount {
    std::string username;
    std::string email;
    bool google = false;
    bool hasPasskey = false;
    uint32_t salt = 0;
    uint32_t passHash = 0;
};

/**
 * Dove vivono gli account ricordati.
 *
 * Prima c'era un ramo Windows (%APPDATA%\Blocky) e, fuori da Windows, il nome
 * di file relativo "blocky_accounts.txt" — che funziona solo se la cartella di
 * lavoro e' scrivibile. Su Android la cartella di lavoro e' "/", di sola
 * lettura: l'account non veniva mai salvato davvero e al riavvio si tornava
 * sempre al login. plat::dataDir() risponde su tutte le piattaforme e su
 * Windows restituisce esattamente %APPDATA%\Blocky, quindi gli account gia'
 * salvati sul PC restano dove sono.
 */
inline std::string blockyAccountsPath() {
    return plat::dataDir() + "/accounts.txt";
}

inline uint32_t accountHashMix(uint32_t h, unsigned char c) {
    h ^= c;
    h *= 16777619u;
    return h;
}

inline uint32_t hashPasskey(const char* pin, uint32_t salt, const char* username) {
    uint32_t h = 2166136261u ^ salt;
    if (username) {
        for (const char* p = username; *p; ++p)
            h = accountHashMix(h, (unsigned char)*p);
    }
    h = accountHashMix(h, '|');
    if (pin) {
        for (const char* p = pin; *p; ++p)
            h = accountHashMix(h, (unsigned char)*p);
    }
    uint32_t h2 = h;
    for (int i = 0; i < 8; ++i)
        h2 = accountHashMix(h2, (unsigned char)((h >> (i * 4)) & 0xF));
    return h2;
}

inline uint32_t makeSalt() {
    return (uint32_t)(std::rand() ^ (std::rand() << 16) ^ 0xA5A5u);
}

inline std::vector<SavedAccount>& savedAccounts() {
    static std::vector<SavedAccount> list;
    return list;
}

inline void loadSavedAccounts() {
    auto& list = savedAccounts();
    list.clear();
    FILE* f = std::fopen(blockyAccountsPath().c_str(), "rb");
    if (!f) return;
    char line[512];
    while (std::fgets(line, sizeof(line), f)) {
        size_t n = std::strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n == 0 || line[0] == '#') continue;
        char user[64] = {}, email[128] = {};
        int google = 0, hasPk = 0;
        unsigned salt = 0, hash = 0;
        if (std::sscanf(line, "%63[^|]|%127[^|]|%d|%d|%x|%x",
                        user, email, &google, &hasPk, &salt, &hash) < 3)
            continue;
        if (user[0] == '\0') continue;
        SavedAccount a;
        a.username = user;
        a.email = (std::strcmp(email, "-") == 0) ? "" : email;
        a.google = google != 0;
        a.hasPasskey = hasPk != 0;
        a.salt = (uint32_t)salt;
        a.passHash = (uint32_t)hash;
        list.push_back(std::move(a));
    }
    std::fclose(f);
}

inline void saveSavedAccounts() {
    FILE* f = std::fopen(blockyAccountsPath().c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "# Blocky saved accounts\n");
    for (const auto& a : savedAccounts()) {
        std::fprintf(f, "%s|%s|%d|%d|%08X|%08X\n",
                     a.username.c_str(),
                     a.email.empty() ? "-" : a.email.c_str(),
                     a.google ? 1 : 0,
                     a.hasPasskey ? 1 : 0,
                     (unsigned)a.salt,
                     (unsigned)a.passHash);
    }
    std::fclose(f);
}

/**
 * Chiave di confronto di un username.
 *
 * Senza normalizzare, "Walter", "walter" e "Walter " finivano in TRE account
 * distinti: il confronto era carattere per carattere. Un nome utente e uno
 * solo, a prescindere da come lo si e digitato.
 */
inline std::string accountNameKey(const char* name) {
    if (!name) return {};
    std::string s(name);
    size_t b = s.find_first_not_of(" 	");
    size_t e = s.find_last_not_of(" 	");
    if (b == std::string::npos) return {};
    s = s.substr(b, e - b + 1);
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}


inline SavedAccount* findSavedAccount(const char* username) {
    const std::string key = accountNameKey(username);
    if (key.empty()) return nullptr;
    for (auto& a : savedAccounts()) {
        if (accountNameKey(a.username.c_str()) == key) return &a;
    }
    return nullptr;
}


/**
 * Quel nome e' gia' di qualcuno su questo dispositivo?
 *
 * Serve a impedire il caso che dava fastidio: due account diversi con lo
 * stesso nome. Un nome appartiene a un account solo, e per entrarci bisogna
 * essere quell'account — non basta riscriverne il nome.
 */
inline bool accountNameTaken(const char* username) {
    return findSavedAccount(username) != nullptr;
}

/**
 * Fonde i doppioni gia' finiti nel file.
 *
 * Il confronto era sensibile alle maiuscole, quindi chi ha scritto una volta
 * "Walter" e una volta "walter" ha gia' due voci salvate sul disco. Qui si
 * tiene la prima — la piu' recente, visto che le nuove vengono inserite in
 * testa — e si scartano le altre, cosi' il file si ripulisce da solo al primo
 * avvio invece di restare sporco per sempre.
 */
inline void dedupeSavedAccounts() {
    auto& list = savedAccounts();
    std::vector<SavedAccount> unique;
    for (const auto& a : list) {
        const std::string key = accountNameKey(a.username.c_str());
        bool seen = false;
        for (const auto& u : unique)
            if (accountNameKey(u.username.c_str()) == key) { seen = true; break; }
        if (!seen) unique.push_back(a);
    }
    if (unique.size() != list.size()) {
        list = std::move(unique);
        saveSavedAccounts();
    }
}

inline void rememberLoginAccount(const char* username, const char* email, bool google) {
    if (!username || !username[0]) return;
    SavedAccount* existing = findSavedAccount(username);
    if (existing) {
        if (email && email[0]) existing->email = email;
        existing->google = google || existing->google;
    } else {
        SavedAccount a;
        a.username = username;
        if (email && email[0]) a.email = email;
        a.google = google;
        savedAccounts().insert(savedAccounts().begin(), std::move(a));
    }
    if (savedAccounts().size() > 12)
        savedAccounts().resize(12);
    saveSavedAccounts();
}

inline bool setAccountPasskey(const char* username, const char* pin) {
    if (!username || !pin || std::strlen(pin) < 4) return false;
    SavedAccount* a = findSavedAccount(username);
    if (!a) {
        rememberLoginAccount(username, nullptr, false);
        a = findSavedAccount(username);
    }
    if (!a) return false;
    a->salt = makeSalt();
    a->passHash = hashPasskey(pin, a->salt, a->username.c_str());
    a->hasPasskey = true;
    saveSavedAccounts();
    return true;
}

inline bool verifyAccountPasskey(const char* username, const char* pin) {
    const SavedAccount* a = findSavedAccount(username);
    if (!a || !a->hasPasskey || !pin) return false;
    // Si firma col nome MEMORIZZATO, non con quello appena digitato: chi
    // scrive "walter" invece di "Walter" deve comunque poter entrare.
    return hashPasskey(pin, a->salt, a->username.c_str()) == a->passHash;
}


// ---- Dispositivo fidato -----------------------------------------------
//
// La passkey serve a proteggere l'account su un computer condiviso, ma
// richiederla a OGNI avvio sullo stesso dispositivo e' solo fastidio: chi ha
// gia' dimostrato una volta di conoscerla, su questo dispositivo, non deve
// rifarlo ogni volta.
//
// Il ricordo e' legato a tre cose insieme: l'utente, il dispositivo (il file
// vive nei dati privati dell'app) e il valore attuale della passkey. Se la
// passkey cambia, o l'account viene rimosso, il ricordo smette da solo di
// valere — non e' un lasciapassare permanente.

inline constexpr long long kTrustDurationSecs = 30LL * 24 * 3600;   // un mese

inline std::string trustedDevicePath() { return plat::dataDir() + "/trusted_device.txt"; }

/** Impronta dell'account COSI' COM'E' ADESSO: cambia se cambia la passkey. */
inline uint32_t accountFingerprint(const SavedAccount& a) {
    uint32_t h = 2166136261u ^ a.salt;
    for (char c : a.username) h = accountHashMix(h, (unsigned char)c);
    h = accountHashMix(h, '|');
    for (int i = 0; i < 4; ++i)
        h = accountHashMix(h, (unsigned char)((a.passHash >> (i * 8)) & 0xFF));
    return h;
}

/** Da chiamare quando l'utente ha appena dimostrato di conoscere la passkey. */
inline void trustDevice(const char* username) {
    if (!username || !username[0]) return;
    const SavedAccount* acc = findSavedAccount(username);
    if (!acc || !acc->hasPasskey) return;

    std::ofstream f(trustedDevicePath(), std::ios::binary);
    if (!f) return;
    f << username << "\n"
      << (long long)std::time(nullptr) + kTrustDurationSecs << "\n"
      << accountFingerprint(*acc) << "\n";
}

/** L'utente esce davvero: il ricordo va cancellato, non lasciato scadere. */
inline void forgetTrustedDevice() {
    std::remove(trustedDevicePath().c_str());
}

/** true se su questo dispositivo la passkey e' gia' stata verificata di recente. */
inline bool isDeviceTrustedFor(const char* username) {
    if (!username || !username[0]) return false;
    const SavedAccount* acc = findSavedAccount(username);
    if (!acc || !acc->hasPasskey) return false;

    std::ifstream f(trustedDevicePath(), std::ios::binary);
    if (!f) return false;

    std::string savedUser;
    long long expiry = 0;
    unsigned long fingerprint = 0;
    if (!std::getline(f, savedUser)) return false;
    if (!(f >> expiry >> fingerprint)) return false;

    while (!savedUser.empty() && (savedUser.back() == '\r' || savedUser.back() == '\n'))
        savedUser.pop_back();

    if (savedUser != username) return false;
    if (expiry <= (long long)std::time(nullptr)) return false;
    return (uint32_t)fingerprint == accountFingerprint(*acc);
}

inline void removeSavedAccount(const char* username) {
    auto& list = savedAccounts();
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (accountNameKey(it->username.c_str()) == accountNameKey(username)) {
            list.erase(it);
            saveSavedAccounts();
            return;
        }
    }
}
