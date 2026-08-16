#pragma once

/**
 * Account salvati + passkey (PIN locale) per rientrare senza Google ogni volta.
 * File: %APPDATA%/Blocky/accounts.txt
 */

#include "engine/GameHub.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
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

inline std::string blockyAccountsPath() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata && appdata[0]) {
        std::string dir = std::string(appdata) + "\\Blocky";
        _mkdir(dir.c_str());
        return dir + "\\accounts.txt";
    }
#endif
    return "blocky_accounts.txt";
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
    // secondo passaggio
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
        // user|email|google|hasPk|saltHex|hashHex
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

inline SavedAccount* findSavedAccount(const char* username) {
    if (!username || !username[0]) return nullptr;
    for (auto& a : savedAccounts()) {
        if (a.username == username) return &a;
    }
    return nullptr;
}

/** Registra / aggiorna account dopo login. */
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
    // max 12 account
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
    a->passHash = hashPasskey(pin, a->salt, username);
    a->hasPasskey = true;
    saveSavedAccounts();
    return true;
}

inline bool verifyAccountPasskey(const char* username, const char* pin) {
    const SavedAccount* a = findSavedAccount(username);
    if (!a || !a->hasPasskey || !pin) return false;
    return hashPasskey(pin, a->salt, username) == a->passHash;
}

inline void removeSavedAccount(const char* username) {
    auto& list = savedAccounts();
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (it->username == username) {
            list.erase(it);
            saveSavedAccounts();
            return;
        }
    }
}
