#pragma once

/**
 * Strato di piattaforma per Blocky.
 *
 * Serve a un motivo solo: oggi `windows.h` e' incluso in 20 file e `_mkdir`
 * chiamato in 12. Finche' e' cosi', portare Blocky altrove significa toccare
 * venti file. Con questo strato ne tocchi uno.
 *
 * Regola: da qui in poi nessun file di gioco include `windows.h` per fare
 * cose ordinarie (cartelle, path, apertura file). Chi ha bisogno di API di
 * sistema vere — anti-cheat, integrity, crash handler — resta specifico per
 * piattaforma di proposito: quelle NON si astraggono, si riscrivono per ogni
 * sistema, ed e' giusto cosi'.
 *
 * Buona parte del lavoro la fa std::filesystem (C++17), che il progetto gia'
 * usa come standard: niente wrapper scritti a mano dove la libreria basta.
 */

#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#else
#include <unistd.h>
#include <climits>
#endif

#include <cstdlib>

namespace plat {

namespace fs = std::filesystem;

/** Percorso dell'eseguibile in esecuzione. */
inline std::string selfPath() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return n ? std::string(buf, n) : std::string();
#elif defined(__APPLE__)
    char buf[PATH_MAX] = {};
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return {};
    return buf;
#else
    char buf[PATH_MAX] = {};
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    return n > 0 ? std::string(buf, n) : std::string();
#endif
}

inline std::string selfDir() {
    std::string p = selfPath();
    if (p.empty()) return ".";
    return fs::path(p).parent_path().string();
}

/**
 * Cartella dati utente, secondo la convenzione di ogni sistema:
 *   Windows  %APPDATA%\Blocky
 *   macOS    ~/Library/Application Support/Blocky
 *   Linux    $XDG_DATA_HOME/blocky  oppure  ~/.local/share/blocky
 *
 * Non e' pedanteria: mettere i dati nel posto sbagliato su Linux o macOS
 * significa file che non vengono salvati nei backup e permessi negati.
 */
inline std::string dataDir() {
    fs::path base;
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA")) base = appdata;
    else base = fs::temp_directory_path();
    base /= "Blocky";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) base = fs::path(home) / "Library" / "Application Support";
    else base = fs::temp_directory_path();
    base /= "Blocky";
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME")) base = xdg;
    else if (const char* home = std::getenv("HOME")) base = fs::path(home) / ".local" / "share";
    else base = fs::temp_directory_path();
    base /= "blocky";
#endif
    std::error_code ec;
    fs::create_directories(base, ec);   // niente eccezioni: se fallisce si vede dopo
    return base.string();
}

/** Crea la cartella e tutte le intermedie. Sostituisce i vari _mkdir. */
inline bool makeDirs(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec || fs::exists(path);
}

inline bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(path, ec);
}

inline uintmax_t fileSize(const std::string& path) {
    std::error_code ec;
    uintmax_t n = fs::file_size(path, ec);
    return ec ? 0 : n;
}

/** Separatore nativo, per i messaggi mostrati all'utente. */
inline const char* sep() {
#ifdef _WIN32
    return "\\";
#else
    return "/";
#endif
}

/**
 * Vero solo dove esiste un dialogo file di sistema utilizzabile.
 * Su Android e iOS non esiste: chi chiama deve avere una via alternativa
 * (elenco interno degli asset) invece di restare senza interfaccia.
 */
inline constexpr bool hasNativeFileDialog() {
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    return true;
#else
    return false;
#endif
}

/** Nome della piattaforma, per log e diagnostica. */
inline const char* name() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "sconosciuta";
#endif
}

} // namespace plat
