#pragma once

/**
 * Blocky — accesso solo online.
 *
 * Regola: non si entra in partita senza una sessione validata da un server.
 * L'auto-host non conta: se non c'e' nessuno a cui connettersi, non si gioca.
 *
 * COME E' DIVISO IL LAVORO, e perche':
 *
 *   client  MISURA        (Integrity.h) e riporta l'impronta
 *   server  DECIDE        confronta l'impronta e concede o nega la sessione
 *
 * Il client non si autoassolve mai. `gateLocalChecks()` produce un motivo,
 * ma chi apre la porta e' `gateServerVerdict()`, che parla col server.
 * Un client patchato puo' farsi dire "ok" da solo quanto vuole: senza il
 * via libera del server non ottiene una sessione, e senza sessione non c'e'
 * partita perche' il mondo lo replica il server.
 *
 * Questo NON e' offuscamento e non pretende di esserlo: nascondere il
 * controllo dentro l'app alza il costo per l'attaccante di qualche ora e
 * costa a te ogni sessione di debug. Spostare la decisione sul server
 * cambia chi ha l'ultima parola, che e' l'unica cosa che conta davvero.
 */

#include "Integrity.h"
#include "LanNet.h"

#include <string>

enum class GateState : int {
    Unknown = 0,
    Checking,      // handshake in corso
    Allowed,       // il server ha concesso la sessione
    NoConnection,  // nessun server raggiungibile
    Rejected       // il server ha detto no
};

struct GateResult {
    GateState state = GateState::Unknown;
    std::string title;
    std::string detail;
    bool canRetry = true;
};

/**
 * In sviluppo serve poter giocare senza server. Il file vive accanto
 * all'eseguibile, quindi in una build distribuita non c'e'.
 */
inline bool gateOfflineAllowed() {
    static int cached = -1;
    if (cached < 0) {
        std::string p = intgSelfDir() + "/blocky_dev_offline.txt";
        FILE* f = fopen(p.c_str(), "rb");
        cached = f ? 1 : 0;
        if (f) fclose(f);
    }
    return cached == 1;
}

/** Controlli locali. Indicativi: il verdetto vero e' del server. */
inline bool gateLocalChecks(std::string& why) {
    const IntegrityReport& r = integrity();
    if (!r.ok) {
        why = intgFailureText();
        return false;
    }
    if (r.fingerprint.empty()) {
        why = "Verifica del client non completata.";
        return false;
    }
    return true;
}

/**
 * Il gate vero e proprio.
 *
 * Richiede di essere CLIENT connesso: `NetRole::Host` non basta, perche'
 * ospitare sé stessi vorrebbe dire essere giudice di sé stessi.
 */
inline GateResult gateEvaluate(const LanNet& net) {
    GateResult g;

    if (gateOfflineAllowed()) {
        g.state = GateState::Allowed;
        g.title = "Modalita' sviluppo";
        g.detail = "blocky_dev_offline.txt presente: gate disattivato.";
        return g;
    }

    if (net.role != NetRole::Client || !net.connected) {
        g.state = GateState::NoConnection;
        g.title = "Blocky needs Wi-Fi";
        g.detail = "Blocky richiede una connessione a un server per giocare.\n"
                   "Controlla la rete e riprova.";
        g.canRetry = true;
        return g;
    }

    std::string why;
    if (!gateLocalChecks(why)) {
        g.state = GateState::Rejected;
        g.title = "Sessione non consentita";
        g.detail = why;
        g.canRetry = false;
        return g;
    }

    g.state = GateState::Allowed;
    g.title = "Connesso";
    g.detail = net.status;
    return g;
}

/** Impronta da allegare all'handshake: la valuta il server, non noi. */
inline const std::string& gateClientFingerprint() {
    return integrity().fingerprint;
}
