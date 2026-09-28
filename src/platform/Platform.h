#pragma once


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

#ifdef __ANDROID__
/**
 * Su Android non esiste un eseguibile con accanto la cartella "assets": il
 * motore e' una .so dentro l APK, e /proc/self/exe punta a
 * /system/bin/app_process64 — da li' nessun percorso relativo del motore ha
 * senso. Gli asset dell APK non sono nemmeno file veri (vivono compressi
 * dentro l archivio), quindi ifstream/stb_image/Assimp non possono aprirli.
 *
 * Soluzione: all avvio android_entry.cpp li estrae una volta nella memoria
 * privata dell app e registra qui la radice risultante. Da quel momento
 * selfDir() risponde come su desktop e TUTTO il resto del motore —
 * resolveScriptPath, le texture, gli script Lua, theme.json — funziona
 * senza una riga di codice Android.
 */
inline std::string& androidBaseDirRef() {
    static std::string dir;
    return dir;
}
inline void setAndroidBaseDir(const std::string& dir) { androidBaseDirRef() = dir; }

/**
 * Impronta SHA-256 della firma dell'APK, letta una volta all'avvio da
 * android_entry.cpp e consultata da Integrity.h. Vuota = non letta ancora,
 * o non leggibile — che Integrity.h deve trattare come "manomesso", non
 * come "va bene cosi'".
 */
inline std::string& androidSigningCertSha256Ref() {
    static std::string s;
    return s;
}
inline void setAndroidSigningCertSha256(const std::string& s) { androidSigningCertSha256Ref() = s; }

/**
 * Ganci installati da android_entry.cpp all avvio.
 *
 * Aprire un link o far comparire la tastiera su Android richiede Java
 * (Intent, InputMethodManager): cose che il motore non puo' e non deve
 * conoscere. Invece di riempire di #ifdef i punti che ne hanno bisogno, il
 * lato Android registra qui due funzioni e il motore chiama quelle. Se non
 * sono installate il comportamento e la semplice risposta "non fatto",
 * esattamente come su una piattaforma che non sa aprire browser.
 */
using AndroidOpenUrlFn = bool (*)(const char*);
using AndroidSoftKeyboardFn = void (*)(bool);
using AndroidFilePickFn = void (*)(const char*);
using AndroidSignInFn   = void (*)(const char*);   // argomento: web client id

inline AndroidOpenUrlFn& androidOpenUrlHook() { static AndroidOpenUrlFn f = nullptr; return f; }
inline AndroidSoftKeyboardFn& androidSoftKeyboardHook() { static AndroidSoftKeyboardFn f = nullptr; return f; }
inline void setAndroidOpenUrlHook(AndroidOpenUrlFn f) { androidOpenUrlHook() = f; }
inline void setAndroidSoftKeyboardHook(AndroidSoftKeyboardFn f) { androidSoftKeyboardHook() = f; }

inline AndroidFilePickFn& androidFilePickHook() { static AndroidFilePickFn f = nullptr; return f; }
inline void setAndroidFilePickHook(AndroidFilePickFn f) { androidFilePickHook() = f; }

/** Il gancio vero e proprio: dichiarato qui con gli altri, installato solo
 *  da android_bridge.cpp. Il punto CALLABILE da codice condiviso (PC
 *  compreso) e' notifyHostingStateChanged() qui sotto, fuori da questo
 *  #ifdef — stesso schema di hasNativeFileDialog(). */
using AndroidHostingHookFn = void (*)(bool);
inline AndroidHostingHookFn& androidHostingHook() { static AndroidHostingHookFn f = nullptr; return f; }
inline void setAndroidHostingHook(AndroidHostingHookFn f) { androidHostingHook() = f; }

inline AndroidSignInFn& androidSignInHook() { static AndroidSignInFn f = nullptr; return f; }
inline void setAndroidSignInHook(AndroidSignInFn f) { androidSignInHook() = f; }
#endif

/**
 * Il telefono e' appena diventato (o smesso di essere) host di una partita.
 *
 * LanNet.h (codice condiviso con PC) chiama questa funzione a ogni cambio
 * SENZA sapere che Android esiste: fuori da Android, o prima che
 * android_bridge.cpp installi il gancio, non fa nulla — esattamente come
 * hasNativeFileDialog() sopra fa da ponte cross-platform verso un gancio
 * dichiarato solo dentro l'#ifdef __ANDROID__.
 *
 * Avvia/ferma il Foreground Service (BlockyHostService, lato Kotlin) che
 * tiene il processo vivo mentre si ospita in background — ed e' un limite
 * dichiarato, non un difetto nascosto: il Foreground Service tiene vivo il
 * PROCESSO, ma non rimette da solo in moto net.poll() una volta che
 * GameActivity perde la superficie di disegno (schermo spento, app in
 * secondo piano — vedi APP_CMD_TERM_WINDOW in android_entry.cpp). Quella e'
 * la parte che resta da fare: sganciare il tick di rete dal ciclo di
 * rendering, con un mutex attorno a g_net, in un giro dedicato — toccare la
 * rete multiplayer di corsa in questo stesso giro sarebbe stato il modo
 * giusto per introdurre una race condition invisibile.
 */
inline void notifyHostingStateChanged(bool hosting) {
#ifdef __ANDROID__
    if (androidHostingHook()) androidHostingHook()(hosting);
#else
    (void)hosting;
#endif
}

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
#ifdef __ANDROID__
    if (!androidBaseDirRef().empty()) return androidBaseDirRef();
    return ".";
#endif
    std::string p = selfPath();
    if (p.empty()) return ".";
    return fs::path(p).parent_path().string();
}

inline std::string dataDir() {
#ifdef __ANDROID__
    // Separata dagli asset estratti: quelli vengono risovrascritti a ogni
    // aggiornamento dell APK, i salvataggi e i DataStore no.
    fs::path androidBase = fs::path(selfDir()) / "userdata";
    std::error_code androidEc;
    fs::create_directories(androidBase, androidEc);
    return androidBase.string();
#else
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
    fs::create_directories(base, ec);
    return base.string();
#endif
}

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

inline const char* sep() {
#ifdef _WIN32
    return "\\";
#else
    return "/";
#endif
}

/**
 * La piattaforma ha un selettore di file di sistema?
 *
 * ATTENZIONE alla condizione: Android definisce __linux__, perche' e' un
 * kernel Linux. Senza escluderlo esplicitamente questa funzione rispondeva
 * "si" sul telefono, dove un selettore di file non esiste affatto — ed e'
 * esattamente il tipo di bugia silenziosa che questo strato dovrebbe impedire.
 */
/**
 * L'utente puo' scegliere un file?
 *
 * Su Android ora si', ma in modo diverso: non un dialogo che aspetta, bensi'
 * il selettore di sistema che risponde dopo (vedi platform/FilePick.h). Il
 * motore non deve distinguere i due casi — chiede con plat::requestFile e
 * riceve la risposta da pumpFilePicks — quindi qui basta sapere SE si puo'.
 *
 * Android va scritto per primo di proposito: Android definisce anche
 * __linux__, e nell'ordine sbagliato finirebbe nel ramo dei desktop. E' gia'
 * successo, e la tabella dei servizi dichiarava "Scelta file: si" su un
 * telefono dove non esisteva.
 */
inline bool hasNativeFileDialog() {
#if defined(__ANDROID__)
    return androidFilePickHook() != nullptr;
#elif defined(_WIN32)
    return true;
#else
    return false;   // macOS/Linux: dialogo nativo ancora da collegare
#endif
}

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

}
