#pragma once

/**
 * Il server dedicato vero e proprio, senza il "come lo si ospita".
 *
 * Prima questa logica viveva tutta dentro main() di blocky_server.cpp: un
 * for(;;) che non si poteva fermare se non uccidendo il processo, output
 * fatto di printf diretti, nessun modo di dire "fermati" dall'esterno. Andava
 * benissimo per una console dove l'utente preme Ctrl+C, ma un servizio
 * Windows deve poter fermare il ciclo quando la Service Control Manager
 * glielo chiede — e un servizio non ha nemmeno una console su cui fare
 * printf.
 *
 * Estratta qui, PAROLA PER PAROLA dal comportamento originale (stesso
 * ordine dei messaggi, stessa logica di clamp), cosi' che:
 *   - blocky_server.cpp (la console) continua a comportarsi esattamente come
 *     prima — e' lo stesso identico output, solo prodotto da qui;
 *   - blocky_rcc_service.cpp (il servizio Windows) puo' rieseguire lo stesso
 *     ciclo con un log su file al posto della console e un modo vero di
 *     fermarsi.
 */

#include "engine/LanNet.h"
#include "engine/Integrity.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

struct DedicatedServerConfig {
    int port = 27015;
    int maxPlayers = 8;
    int gameIndex = -1;
    std::string name = "Blocky Server";
};

/** Dove va il testo di stato: una console lo stampa, un servizio lo scrive su file. */
using ServerLogFn = std::function<void(const std::string&)>;

inline std::vector<std::string> loadAllowedFingerprints(const std::string& path,
                                                        bool& fileFound) {
    std::vector<std::string> out;
    std::ifstream f(path);
    fileFound = (bool)f;
    if (!f) return out;
    std::string line;
    while (std::getline(f, line)) {
        size_t a = line.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) continue;
        size_t b = line.find_last_not_of(" \t\r\n");
        std::string s = line.substr(a, b - a + 1);
        if (s.empty() || s[0] == '#') continue;
        for (char& c : s)
            if (c >= 'a' && c <= 'f') c = (char)(c - 'a' + 'A');
        out.push_back(s);
    }
    return out;
}

inline std::vector<uint32_t> loadAllowedScripts(const std::string& path, bool& fileFound) {
    std::vector<uint32_t> out;
    std::ifstream f(path);
    fileFound = (bool)f;
    if (!f) return out;
    std::string line;
    while (std::getline(f, line)) {
        size_t a = line.find_first_not_of(" \t\r\n");
        if (a == std::string::npos) continue;
        if (line[a] == '#') continue;
        unsigned v = 0;
        if (std::sscanf(line.c_str() + a, "%x", &v) == 1 && v != 0)
            out.push_back((uint32_t)v);
    }
    return out;
}

inline void printServerHelp() {
    std::printf(
        "BlockyServer — multiplayer dedicato\n"
        "  --port N     porta UDP (default 27015)\n"
        "  --max N      max giocatori 1-32 (default 8)\n"
        "  --name STR   nome sessione\n"
        "  --game N     restringi a un indice gioco (default: qualsiasi)\n"
        "\nEsempio:\n"
        "  BlockyServer.exe --port 27015 --max 12 --name \"Obby Party\"\n");
}

/**
 * Stessa lettura degli argomenti di sempre, solo spostata qui. Su --help/-h
 * esce SUBITO, senza applicare il clamp sotto — esattamente come faceva
 * main() tornando 0 a meta' del ciclo originale.
 */
inline void parseServerArgs(int argc, char** argv, DedicatedServerConfig& cfg, bool& wantsHelp) {
    wantsHelp = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            wantsHelp = true;
            return;
        }
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            cfg.port = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--max") == 0 && i + 1 < argc)
            cfg.maxPlayers = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--game") == 0 && i + 1 < argc)
            cfg.gameIndex = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--name") == 0 && i + 1 < argc)
            cfg.name = argv[++i];
    }
    cfg.maxPlayers = std::clamp(cfg.maxPlayers, 1, 32);
    if (cfg.port < 1024 || cfg.port > 65535) cfg.port = 27015;
}

/**
 * Avvia il net e stampa (via log()) lo stesso banner di sempre, riga per
 * riga nello stesso ordine. showCtrlCHint si spegne per il servizio: non
 * c'e' nessuna console su cui premere Ctrl+C, la riga confonderebbe solo chi
 * legge il file di log.
 */
inline bool startDedicatedServer(LanNet& net, const DedicatedServerConfig& cfg,
                                 const ServerLogFn& log, bool showCtrlCHint = true) {
    if (!net.startDedicated(cfg.port, cfg.gameIndex, cfg.maxPlayers, cfg.name.c_str())) {
        log("Impossibile avviare il server.");
        return false;
    }

    bool listFound = false;
    std::string listPath = intgSelfDir() + "/allowed_clients.txt";
    net.allowedFingerprints = loadAllowedFingerprints(listPath, listFound);

    log("========================================");
    log("  BlockyServer ATTIVO");
    log("  Porta: " + std::to_string(cfg.port));
    log("  Max giocatori: " + std::to_string(cfg.maxPlayers));
    log("  Nome: " + cfg.name);
    log(std::string("  Gioco: ") + (cfg.gameIndex < 0
        ? "qualsiasi (auto-match risponde a tutti)"
        : std::to_string(cfg.gameIndex)));
    log("  Join da Blocky con l'IP di questo PC");
    if (!net.allowedFingerprints.empty()) {
        log("  Integrita': SEVERO — " + std::to_string(net.allowedFingerprints.size())
            + " impronte ammesse");
    } else {
        log("  Integrita': APPRENDIMENTO — accetta tutti e registra");
        log(std::string("    ") + (listFound ? "(elenco presente ma vuoto)"
                                             : "(nessun allowed_clients.txt)"));
        log("    Per chiudere: copia le impronte viste in");
        log("    " + listPath);
    }

    bool scriptsFound = false;
    std::string scriptsPath = intgSelfDir() + "/allowed_scripts.txt";
    net.allowedScripts = loadAllowedScripts(scriptsPath, scriptsFound);
    if (!net.allowedScripts.empty()) {
        log("  Script:     SEVERO — " + std::to_string(net.allowedScripts.size())
            + " hash ammessi (chi ne esegue altri e' cacciato)");
    } else {
        log("  Script:     APPRENDIMENTO — registra gli hash che arrivano");
        log("    " + scriptsPath);
    }
    if (showCtrlCHint) log("  Ctrl+C per chiudere");
    log("========================================");
    return true;
}

/**
 * Il ciclo vero. Gira finche' keepRunning e' true: un servizio lo ferma
 * mettendolo a false dal proprio control handler quando la SCM chiede lo
 * stop, invece di dover uccidere il processo — che era l'unico modo che
 * questo ciclo offriva finora, essendo un for(;;) senza via d'uscita.
 */
inline void runDedicatedServerLoop(LanNet& net, int maxPlayers,
                                   std::atomic<bool>& keepRunning,
                                   const ServerLogFn& log) {
    float t0 = 0.f;
#ifdef _WIN32
    LARGE_INTEGER freq{}, counter{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    double start = (double)counter.QuadPart / (double)freq.QuadPart;
#endif

    int lastCount = -1;
    while (keepRunning.load(std::memory_order_relaxed)) {
#ifdef _WIN32
        QueryPerformanceCounter(&counter);
        double now = (double)counter.QuadPart / (double)freq.QuadPart;
        float t = (float)(now - start);
        Sleep(5);
#else
        float t = t0 += 0.016f;
#endif
        net.poll(t);
        int c = net.playerCount();
        if (c != lastCount) {
            lastCount = c;
            log("[Server] Giocatori: " + std::to_string(c) + " / " + std::to_string(maxPlayers));
        }
    }
}
