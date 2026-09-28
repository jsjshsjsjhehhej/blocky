#pragma once


#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#include "platform/Platform.h"
#include "platform/SystemApi.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

/**
 * Apre un indirizzo nel browser di sistema.
 *
 * Ogni piattaforma ha il suo modo, e nessuno dei tre funziona sulle altre:
 * Windows ha ShellExecute, macOS il comando "open", i desktop Linux
 * "xdg-open", e Android non ha un comando affatto — li' serve un Intent, che
 * richiede Java, quindi passa dal gancio installato da android_entry.cpp.
 */
inline bool openUrlInBrowser(const char* url) {
    // L'implementazione vera sta in plat::sys::openUrl. Stava qui, dentro un
    // file di autenticazione, e intanto sei altri punti del motore chiamavano
    // ShellExecute per conto loro: una primitiva di sistema con sette copie e
    // nessun proprietario. Questo nome resta solo per i chiamanti esistenti.
    return plat::sys::openUrl(url ? url : "");
}


inline bool openGoogleAccountSignIn() {
    return openUrlInBrowser(
        "https://accounts.google.com/AccountChooser"
        "?hl=it"
        "&continue=https%3A%2F%2Fmyaccount.google.com%2F");
}

inline bool googleEmailToUsername(const char* email, char* out, size_t outN) {
    if (!email || !out || outN < 3) return false;
    const char* at = std::strchr(email, '@');
    if (!at || at == email) return false;
    if (std::strchr(at + 1, '.') == nullptr) return false;

    std::string local(email, at);
    std::string user;
    for (unsigned char c : local) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            user.push_back((char)c);
        else if (c == '.' || c == '_' || c == '-')
            user.push_back('_');
    }
    if (user.empty()) return false;
    if (user.size() > 24) user.resize(24);
    std::snprintf(out, outN, "%s", user.c_str());
    return out[0] != '\0';
}

inline bool looksLikeEmail(const char* s) {
    if (!s || !s[0]) return false;
    const char* at = std::strchr(s, '@');
    return at && at != s && std::strchr(at + 1, '.') != nullptr;
}
