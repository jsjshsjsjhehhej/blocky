#pragma once

/**
 * L'elenco completo di cio' che il motore chiede al sistema operativo.
 *
 * Perche' esiste, in una frase: **tutti** i difetti emersi portando Blocky su
 * Android erano servizi che su quella piattaforma non esistevano e fallivano
 * IN SILENZIO. L'account non si salvava, i rapporti di crash non venivano
 * scritti, il gioco girava muto, e il controllo d'integrita' scollegava il
 * giocatore un istante dopo la connessione. Nessuno di quei bug era difficile:
 * erano invisibili.
 *
 * Qui ogni servizio e' dichiarato una volta sola, e ogni piattaforma dice a
 * chiare lettere se ce l'ha. All'avvio il motore stampa la tabella. Un porting
 * nuovo parte quindi da una lista di cose da fare, non da una caccia a guasti
 * che non si manifestano.
 *
 * REGOLA: un servizio che manca deve rispondere "non ce l'ho", mai fingere di
 * aver funzionato. Un ripiego silenzioso e' peggio di un errore.
 */

#include <cstdio>

#include "platform/Platform.h"
#include "platform/Log.h"

namespace plat {

enum class Service {
    Paths,          // cartelle dati/asset scrivibili e stabili
    Audio,          // riprodurre un file sonoro
    OpenUrl,        // aprire un indirizzo nel browser di sistema
    FileDialog,     // far scegliere un file all'utente
    Gamepad,        // controller fisico
    SoftKeyboard,   // tastiera a schermo
    LaunchProcess,  // avviare un altro eseguibile di Blocky
    CrashReport,    // salvare un rapporto quando il gioco muore
    CodeIntegrity,  // verificare che il client non sia manomesso
    Count
};

inline const char* serviceName(Service s) {
    switch (s) {
    case Service::Paths:         return "Percorsi dati";
    case Service::Audio:         return "Audio";
    case Service::OpenUrl:       return "Apertura link";
    case Service::FileDialog:    return "Scelta file";
    case Service::Gamepad:       return "Controller";
    case Service::SoftKeyboard:  return "Tastiera a schermo";
    case Service::LaunchProcess: return "Avvio altri processi";
    case Service::CrashReport:   return "Rapporti di crash";
    case Service::CodeIntegrity: return "Integrita' del codice";
    default:                     return "?";
    }
}

/**
 * Questo servizio esiste sulla piattaforma corrente?
 *
 * Volutamente in un punto solo: aggiungere una piattaforma significa
 * aggiungere una colonna qui, non cercare gli #ifdef sparsi per il motore.
 */
inline bool has(Service s) {
    switch (s) {
    case Service::Paths:
        return true;   // plat::dataDir()/selfDir() coprono Windows, macOS, Linux, Android

    case Service::Audio:
#if defined(_WIN32) || defined(__ANDROID__)
        return true;   // PlaySound/MCI su Windows, OpenSL ES su Android
#else
        return false;  // macOS/Linux: CoreAudio / ALSA ancora da fare
#endif

    case Service::OpenUrl:
#if defined(__ANDROID__)
        return androidOpenUrlHook() != nullptr;   // Intent, installato all'avvio
#elif defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
        return true;   // ShellExecute / open / xdg-open
#else
        return false;
#endif

    case Service::FileDialog:
        return hasNativeFileDialog();

    case Service::Gamepad:
#if defined(_WIN32) || defined(__ANDROID__)
        // Windows: XInput. Android: gli eventi di GameActivity, riversati
        // nello shim GLFW da android_entry.cpp — cosi' GamepadInput.h chiede
        // i joystick a GLFW esattamente come su PC, senza sapere dove gira.
        return true;
#else
        return false;  // macOS/Linux: ancora da collegare
#endif

    case Service::SoftKeyboard:
#ifdef __ANDROID__
        return androidSoftKeyboardHook() != nullptr;
#else
        return false;  // su desktop c'e' una tastiera vera
#endif

    case Service::LaunchProcess:
#ifdef __ANDROID__
        // Un app non avvia altri eseguibili: i lanciatori sottili che su PC
        // aprono motore e server non hanno equivalente.
        return false;
#else
        return true;   // ShellExecute su Windows, fork+exec altrove
#endif

    case Service::CrashReport:
        return true;   // segnali POSIX ovunque, piu' il filtro eccezioni su Windows

    case Service::CodeIntegrity:
#if defined(_WIN32) || defined(__ANDROID__)
        // PC: hash dell'eseguibile, sezione .text, scansione moduli.
        // Android: firma dell'APK (vedi Integrity.h) — un utente ha
        // confermato con un tentativo vero che senza questo un APK
        // modificato passava il gate senza essere scoperto.
        return true;
#else
        return false;  // macOS/Linux: ancora da collegare
#endif

    default:
        return false;
    }
}

/**
 * Stampa la tabella dei servizi.
 *
 * Va chiamata all'avvio, accanto al banner del motore. Costa nulla e rende
 * immediatamente leggibile cosa questa piattaforma sa fare e cosa no — che e'
 * esattamente l'informazione che e' mancata per tutta la porta Android.
 */
inline void reportServices() {
    // Passa dal log, non da printf: su Android stdout non va da nessuna parte,
    // e questa tabella era invisibile proprio sulla piattaforma per cui e'
    // stata scritta. Verificato: l'app partiva senza stampare una riga.
    LOG_INFO("Servizi", "Servizi di sistema:");
    for (int i = 0; i < (int)Service::Count; ++i) {
        const Service s = (Service)i;
        LOG_INFO("Servizi", "  %-24s %s", serviceName(s), has(s) ? "si" : "NO");
    }
}

} // namespace plat
