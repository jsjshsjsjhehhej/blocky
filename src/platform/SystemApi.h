#pragma once

/**
 * SystemApi — l'UNICO posto del motore da cui si chiama il sistema operativo.
 *
 * ── Perche' esiste ────────────────────────────────────────────────────────
 * Il motore e' nato su Windows e chiamava Windows da dove capitava:
 * ShellExecuteA in sei file diversi, PlaySound dentro SoundService,
 * GetModuleFileNameA dentro Platform.h, WSAStartup sparso nella rete. Finche'
 * si compilava solo per Windows non si vedeva il problema.
 *
 * Portando su Android e' venuto fuori tutto insieme, e sempre nello stesso
 * modo: non un errore di compilazione, ma un FALLIMENTO SILENZIOSO. Il gioco
 * girava muto, l'account non si salvava, i rapporti di crash non venivano
 * scritti, il giocatore veniva scollegato un istante dopo essersi connesso.
 * Nessuno di quei bug era difficile — erano invisibili, e li abbiamo trovati
 * uno per uno, a mano, leggendo i log.
 *
 * ── La regola ─────────────────────────────────────────────────────────────
 * Il resto del motore non chiama MAI il sistema operativo direttamente.
 * Chiama plat::sys::*, e basta. Chi porta Blocky su una piattaforma nuova
 * apre QUESTO file, vede la lista completa di cio' che serve, e la riempie.
 * Non deve piu' scoprirla scontrandosi con i sintomi.
 *
 * Non e' un principio astratto: e' esattamente la differenza fra le due meta'
 * del porting Android. plat::selfDir()/dataDir(), che erano gia' centralizzati
 * qui accanto, hanno richiesto una riga a testa. Tutto il resto, sparso, ci e'
 * costato una serata.
 *
 * ── Il punto piu' importante ──────────────────────────────────────────────
 * Una funzione che la piattaforma non sa fare deve restituire "non l'ho
 * fatto" (false), MAI fingere di averlo fatto. Un ripiego silenzioso e'
 * peggio di un errore: l'errore lo vedi.
 *
 * Da qui il divieto assoluto di ripieghi con percorsi relativi, del tipo
 * `return "."` quando non si sa dov'e' l'eseguibile. Quel singolo ripiego, in
 * Integrity.h, ha scollegato il telefono da ogni partita per tutta una serata
 * facendolo sembrare un problema di rete.
 *
 * ── Come si verifica che la regola sia rispettata ─────────────────────────
 * tools/check_platform_boundary.sh: cerca chiamate diverte al sistema fuori
 * da src/platform/ e fallisce se ne trova. Serve perche' un layer che nessuno
 * e' OBBLIGATO a usare marcisce — e' gia' successo a gfx/GfxCore.h, che
 * esiste, e' scritto bene, e viene usato da un file solo su venti.
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "platform/Platform.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <digitalv.h>
#elif defined(__ANDROID__)
#include <unistd.h>
#include "platform/AndroidAudio.h"
#else
#include <unistd.h>
#endif

namespace plat {
namespace sys {

// ─────────────────────────────────────────────────────────────────────────
//  Aprire un indirizzo o un file col programma di sistema
// ─────────────────────────────────────────────────────────────────────────

/**
 * Apre un URL (o un file) con l'applicazione predefinita del sistema.
 *
 * Android non ha niente di simile a ShellExecute: serve un Intent, quindi
 * Java. Il lato Android registra il gancio all'avvio (setAndroidOpenUrlHook)
 * e qui lo si chiama. Se non e' installato, la risposta e' "non fatto".
 */
inline bool openUrl(const std::string& url) {
    if (url.empty()) return false;
#if defined(__ANDROID__)
    if (androidOpenUrlHook()) return androidOpenUrlHook()(url.c_str());
    return false;   // nessun gancio: non fingiamo di averlo aperto
#elif defined(_WIN32)
    HINSTANCE h = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)h > 32;
#elif defined(__APPLE__)
    return std::system(("open \"" + url + "\" >/dev/null 2>&1 &").c_str()) == 0;
#else
    return std::system(("xdg-open \"" + url + "\" >/dev/null 2>&1 &").c_str()) == 0;
#endif
}

/** Mostra una cartella nel gestore file di sistema. */
inline bool revealFolder(const std::string& dir) {
    if (dir.empty()) return false;
#if defined(__ANDROID__)
    return false;   // non c'e' un gestore file da pilotare
#elif defined(_WIN32)
    HINSTANCE h = ShellExecuteA(nullptr, "open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)h > 32;
#elif defined(__APPLE__)
    return std::system(("open \"" + dir + "\" >/dev/null 2>&1 &").c_str()) == 0;
#else
    return std::system(("xdg-open \"" + dir + "\" >/dev/null 2>&1 &").c_str()) == 0;
#endif
}

// ─────────────────────────────────────────────────────────────────────────
//  Avviare un altro eseguibile di Blocky
// ─────────────────────────────────────────────────────────────────────────

/**
 * Lancia un eseguibile e torna subito, senza aspettarlo.
 *
 * Serve ai lanciatori sottili (il motore, il server dedicato, l'host HTTP)
 * che sono processi separati attorno alla stessa libblocky.
 *
 * Su Android NON esiste: un'app non avvia altri eseguibili. La risposta e'
 * "non fatto", e chi chiama deve avere una strada alternativa — non un
 * messaggio di errore generico a schermo.
 */
inline bool launchProcess(const std::string& exePath,
                          const std::string& args = {},
                          const std::string& workDir = {}) {
    if (exePath.empty()) return false;
#if defined(__ANDROID__)
    (void)args; (void)workDir;
    return false;
#elif defined(_WIN32)
    HINSTANCE h = ShellExecuteA(nullptr, "open", exePath.c_str(),
                                args.empty()    ? nullptr : args.c_str(),
                                workDir.empty() ? nullptr : workDir.c_str(),
                                SW_SHOWNORMAL);
    return (INT_PTR)h > 32;
#else
    std::string cmd;
    if (!workDir.empty()) cmd += "cd \"" + workDir + "\" && ";
    cmd += "\"" + exePath + "\"";
    if (!args.empty()) cmd += " " + args;
    cmd += " >/dev/null 2>&1 &";
    return std::system(cmd.c_str()) == 0;
#endif
}

// ─────────────────────────────────────────────────────────────────────────
//  Processi da tenere d'occhio (Test multiplayer di Studio)
// ─────────────────────────────────────────────────────────────────────────
//
// Come launchProcess, ma con variabili d'ambiente in piu' SOLO per il figlio
// (il motore non legge la riga di comando) e un id per sapere se e' ancora
// aperto e poterlo chiudere. 0 = non avviato. Solo Windows per ora: altrove
// risponde 0, non finge.

namespace detail {
#if defined(_WIN32) && !defined(__ANDROID__)
inline std::vector<std::pair<uint32_t, HANDLE>>& trackedProcesses() {
    static std::vector<std::pair<uint32_t, HANDLE>> v;
    return v;
}
#endif
}

inline uint32_t launchTracked(const std::string& exePath, const std::string& args,
                              const std::string& workDir,
                              const std::vector<std::pair<std::string, std::string>>& extraEnv) {
#if defined(_WIN32) && !defined(__ANDROID__)
    if (exePath.empty()) return 0;
    // Blocco ambiente = quello attuale + le variabili in piu', "K=V\0...\0".
    std::string env;
    if (char* cur = GetEnvironmentStringsA()) {
        for (const char* p = cur; *p; p += std::strlen(p) + 1) env.append(p, std::strlen(p) + 1);
        FreeEnvironmentStringsA(cur);
    }
    for (const auto& kv : extraEnv) env += kv.first + "=" + kv.second + '\0';
    env += '\0';

    std::string cmd = "\"" + exePath + "\"";
    if (!args.empty()) cmd += " " + args;
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(exePath.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, env.data(),
                        workDir.empty() ? nullptr : workDir.c_str(), &si, &pi))
        return 0;
    CloseHandle(pi.hThread);
    detail::trackedProcesses().push_back({(uint32_t)pi.dwProcessId, pi.hProcess});
    return (uint32_t)pi.dwProcessId;
#else
    (void)exePath; (void)args; (void)workDir; (void)extraEnv;
    return 0;
#endif
}

inline bool isTrackedAlive(uint32_t id) {
#if defined(_WIN32) && !defined(__ANDROID__)
    for (const auto& t : detail::trackedProcesses())
        if (t.first == id) return WaitForSingleObject(t.second, 0) == WAIT_TIMEOUT;
#else
    (void)id;
#endif
    return false;
}

// Chiude solo processi avviati con launchTracked da questo motore.
inline void terminateTracked(uint32_t id) {
#if defined(_WIN32) && !defined(__ANDROID__)
    auto& v = detail::trackedProcesses();
    for (size_t k = 0; k < v.size(); ++k) {
        if (v[k].first != id) continue;
        if (WaitForSingleObject(v[k].second, 0) == WAIT_TIMEOUT) TerminateProcess(v[k].second, 0);
        CloseHandle(v[k].second);
        v.erase(v.begin() + (long)k);
        return;
    }
#else
    (void)id;
#endif
}

// ─────────────────────────────────────────────────────────────────────────
//  Rete
// ─────────────────────────────────────────────────────────────────────────

/**
 * Prepara lo stack di rete. Va chiamata prima di QUALSIASI socket().
 *
 * Su Windows senza WSAStartup socket() fallisce — e fallisce restituendo un
 * errore che nessuno guardava, quindi la ricerca dei server in LAN sembrava
 * semplicemente "non trova niente". E' costato mezz'ora di caccia alla causa
 * sbagliata. Altrove non serve nulla, e la funzione dice subito di si'.
 *
 * Idempotente: chiamarla piu' volte non costa niente, quindi la si puo'
 * mettere all'inizio di ogni funzione che apre un socket senza pensarci.
 */
inline bool netInit() {
#ifdef _WIN32
    static bool done = false;
    static bool ok = false;
    if (!done) {
        WSADATA w{};
        ok = (WSAStartup(MAKEWORD(2, 2), &w) == 0);
        done = true;
    }
    return ok;
#else
    return true;
#endif
}

// ─────────────────────────────────────────────────────────────────────────
//  Tempo
// ─────────────────────────────────────────────────────────────────────────

/** Mette in pausa il thread corrente. Millisecondi ovunque. */
inline void sleepMs(int ms) {
    if (ms <= 0) return;
#ifdef _WIN32
    ::Sleep((DWORD)ms);
#else
    ::usleep((useconds_t)ms * 1000);
#endif
}


// ─────────────────────────────────────────────────────────────────────────
//  Audio
// ─────────────────────────────────────────────────────────────────────────

/**
 * Riproduce un file audio dal percorso. Un suono per volta.
 *
 * E' il caso che spiega meglio perche' questo file esiste. L'audio stava
 * dentro SoundService, scritto con PlaySound e MCI, cioe' due API di
 * Windows in mezzo alla logica di gioco. Fuori da Windows il ramo #else
 * diceva "Audio non supportato su questa piattaforma" e nessuno lo leggeva:
 * il gioco girava semplicemente muto sul telefono, senza un errore, per
 * settimane.
 *
 * Ora i backend stanno qui e SoundService non sa piu' su cosa gira.
 */
inline bool playSound(const std::string& path, bool looped) {
    if (path.empty()) return false;
#if defined(__ANDROID__)
    return blk::androidaudio::playFile(path, looped);
#elif defined(_WIN32)
    std::string p = path;
    for (char& c : p) if (c == '/') c = '\\';

    std::string lower = p;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    const bool isWav = lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".wav") == 0;

    if (isWav) {
        DWORD flags = SND_FILENAME | SND_ASYNC | SND_NODEFAULT;
        if (looped) flags |= SND_LOOP;
        return PlaySoundA(p.c_str(), nullptr, flags) != FALSE;
    }
    // Non-wav: passa da MCI, che sa decodificare mp3/ogg.
    mciSendStringA("close blocky_snd", nullptr, 0, nullptr);
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd), "open \"%s\" type mpegvideo alias blocky_snd", p.c_str());
    if (mciSendStringA(cmd, nullptr, 0, nullptr) != 0) {
        std::snprintf(cmd, sizeof(cmd), "open \"%s\" alias blocky_snd", p.c_str());
        if (mciSendStringA(cmd, nullptr, 0, nullptr) != 0) return false;
    }
    mciSendStringA(looped ? "play blocky_snd repeat" : "play blocky_snd", nullptr, 0, nullptr);
    return true;
#else
    (void)looped;
    return false;   // macOS/Linux: CoreAudio / ALSA ancora da fare
#endif
}

/** Ferma tutto quello che sta suonando. Sicura da chiamare sempre. */
inline void stopSound() {
#if defined(__ANDROID__)
    blk::androidaudio::stop();
#elif defined(_WIN32)
    PlaySoundA(nullptr, nullptr, SND_ASYNC | SND_PURGE);
    mciSendStringA("stop blocky_snd", nullptr, 0, nullptr);
    mciSendStringA("close blocky_snd", nullptr, 0, nullptr);
#endif
}
} // namespace sys
} // namespace plat
