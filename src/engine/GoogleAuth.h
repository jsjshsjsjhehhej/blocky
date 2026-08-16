#pragma once

/**
 * Accesso Google (desktop): apre il browser su Google Account.
 * Dopo il login, l'utente conferma l'email Gmail nell'app Blocky.
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

inline bool openUrlInBrowser(const char* url) {
    if (!url || !url[0]) return false;
#ifdef _WIN32
    HINSTANCE h = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)h > 32;
#else
    std::string cmd = std::string("xdg-open \"") + url + "\"";
    return std::system(cmd.c_str()) == 0;
#endif
}

/** Apre la pagina di accesso / scelta account Google nel browser. */
inline bool openGoogleAccountSignIn() {
    // AccountChooser → se non sei loggato chiede email/password Google
    return openUrlInBrowser(
        "https://accounts.google.com/AccountChooser"
        "?hl=it"
        "&continue=https%3A%2F%2Fmyaccount.google.com%2F");
}

/** Da email → username Blocky (parte prima di @, sanitizzata). */
inline bool googleEmailToUsername(const char* email, char* out, size_t outN) {
    if (!email || !out || outN < 3) return false;
    const char* at = std::strchr(email, '@');
    if (!at || at == email) return false;
    // Deve sembrare un'email
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
