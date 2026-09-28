#pragma once


#include "Integrity.h"
#include "LanNet.h"

#include <string>

enum class GateState : int {
    Unknown = 0,
    Checking,
    Allowed,
    NoConnection,
    Rejected
};

struct GateResult {
    GateState state = GateState::Unknown;
    std::string title;
    std::string detail;
    bool canRetry = true;
};

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

inline const std::string& gateClientFingerprint() {
    return integrity().fingerprint;
}
