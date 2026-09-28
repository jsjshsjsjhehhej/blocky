#pragma once

/**
 * Chiedere un file all'utente — su ogni piattaforma, con una forma sola.
 *
 * ── Perche' non e' una semplice funzione che ritorna un percorso ──────────
 * Su Windows GetOpenFileNameW apre il dialogo, aspetta, e restituisce il
 * percorso scelto: tutto dentro la stessa chiamata.
 *
 * Su Android non e' possibile, e non per una mancanza nostra: dalla versione
 * 11 in poi l'app NON PUO' leggere i file dell'utente. Verificato su questo
 * tablet (Android 15):
 *
 *     ls /sdcard/Download   ->   Permission denied
 *
 * L'unica via e' chiedere al sistema di mostrare il SUO selettore. L'utente
 * esce dal gioco, sceglie, e il sistema restituisce un permesso temporaneo su
 * quel singolo file — quando vuole lui, anche dopo mezzo minuto.
 *
 * Non sono due modi di scrivere la stessa cosa: sono due cose diverse.
 * Sincrono contro asincrono, percorso contro URI, "leggo dove voglio" contro
 * "leggo solo cio' che mi hanno consegnato". Nessuno strumento automatico
 * potrebbe convertire l'una nell'altra, ed e' il motivo per cui questa e'
 * l'ultima funzione del motore a non essere portabile.
 *
 * ── La forma comune ───────────────────────────────────────────────────────
 * Si chiede con una richiamata invece che con un valore di ritorno. Cosi' il
 * codice del motore e' identico ovunque, e il caso "la risposta arriva dopo"
 * e' quello normale invece che l'eccezione. Su Windows la richiamata scatta
 * al frame successivo, su Android quando l'utente ha scelto: chi chiama non
 * deve sapere quale dei due.
 *
 * La richiamata arriva SEMPRE, anche se l'utente annulla: in quel caso con
 * un percorso vuoto. Un'attesa che non finisce mai sarebbe peggio di un no.
 */

#include <functional>
#include <string>
#include <vector>

#include "platform/Platform.h"
#include "platform/PlatformServices.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace plat {

using FilePickCallback = std::function<void(const std::string& path)>;

namespace detail {

struct PickState {
    FilePickCallback pending;      // chi aspetta la risposta
    std::string      result;       // percorso arrivato
    bool             hasResult = false;
    bool             waiting = false;
};

inline PickState& pickState() { static PickState s; return s; }

} // namespace detail

/** Il lato Android consegna qui il percorso scelto (vuoto = annullato). */
inline void deliverPickedFile(const std::string& path) {
    auto& s = detail::pickState();
    s.result = path;
    s.hasResult = true;
}

/** C'e' gia' una richiesta in corso? */
inline bool filePickPending() { return detail::pickState().waiting; }

/**
 * Chiede un file. La richiamata arriva da pumpFilePicks(), mai qui dentro:
 * cosi' chi chiama non si ritrova il proprio codice eseguito a meta' di se
 * stesso, che su Windows sarebbe successo e su Android no.
 */
inline void requestFile(const char* title, const char* mimeType, FilePickCallback cb) {
    auto& s = detail::pickState();
    if (s.waiting) return;   // una per volta: due selettori insieme non hanno senso
    s.pending = std::move(cb);
    s.waiting = true;
    s.hasResult = false;
    s.result.clear();

#if defined(__ANDROID__)
    (void)title;
    if (androidFilePickHook()) androidFilePickHook()(mimeType ? mimeType : "*/*");
    else deliverPickedFile("");
#elif defined(_WIN32)
    (void)mimeType;
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter =
        L"Modelli 3D (*.glb;*.gltf;*.fbx;*.obj)\0*.glb;*.gltf;*.fbx;*.obj\0"
        L"Tutti i file\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    ofn.lpstrDefExt = L"glb";

    std::wstring wtitle;
    if (title) {
        const int n = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
        if (n > 0) { wtitle.resize((size_t)n - 1); MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle.data(), n); }
        ofn.lpstrTitle = wtitle.c_str();
    }

    std::string out;
    if (GetOpenFileNameW(&ofn)) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, file, -1, nullptr, 0, nullptr, nullptr);
        if (n > 1) {
            out.resize((size_t)n - 1);
            WideCharToMultiByte(CP_UTF8, 0, file, -1, out.data(), n, nullptr, nullptr);
        }
    }
    deliverPickedFile(out);
#else
    (void)title; (void)mimeType;
    deliverPickedFile("");   // macOS/Linux: dialogo nativo ancora da fare
#endif
}

/**
 * Consegna le risposte arrivate. Una volta per frame.
 *
 * La richiamata viene staccata dallo stato PRIMA di eseguirla: se durante
 * l'esecuzione chiede un altro file (scegli modello -> scegli texture), la
 * nuova richiesta trova il posto libero invece di essere rifiutata.
 */
inline void pumpFilePicks() {
    auto& s = detail::pickState();
    if (!s.hasResult) return;

    FilePickCallback cb = std::move(s.pending);
    const std::string path = std::move(s.result);
    s.pending = nullptr;
    s.result.clear();
    s.hasResult = false;
    s.waiting = false;

    if (cb) cb(path);
}

} // namespace plat
