// BlockyServer — server dedicato multiplayer (senza grafica)
// Uso: BlockyServer.exe --port 27015 --max 8 --name "Il mio gioco"
//
// I client Blocky si uniscono con Join IP (stessa rete / localhost).
// Il limite --max è quello impostato in Blocky Game Engine alla publish.

#include "engine/LanNet.h"
#include "engine/Integrity.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * Elenco impronte ammesse: un'impronta per riga, '#' per i commenti.
 * File assente o vuoto = modalita' apprendimento (accetta tutti e registra),
 * cosi' la prima volta non ti chiudi fuori dal tuo stesso server.
 */
static std::vector<std::string> loadAllowedFingerprints(const std::string& path,
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

/**
 * Hash degli script leciti del gioco: uno per riga, esadecimale, '#' commenti.
 * File assente = apprendimento (registra e non caccia nessuno).
 */
static std::vector<uint32_t> loadAllowedScripts(const std::string& path, bool& fileFound) {
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

static void printHelp() {
    std::printf(
        "BlockyServer — multiplayer dedicato\n"
        "  --port N     porta UDP (default 27015)\n"
        "  --max N      max giocatori 1-32 (default 8)\n"
        "  --name STR   nome sessione\n"
        "  --game N     indice gioco (default 0)\n"
        "\nEsempio:\n"
        "  BlockyServer.exe --port 27015 --max 12 --name \"Obby Party\"\n");
}

int main(int argc, char** argv) {
    int port = 27015;
    int maxP = 8;
    int gameIdx = 0;
    std::string name = "Blocky Server";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            printHelp();
            return 0;
        }
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            port = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--max") == 0 && i + 1 < argc)
            maxP = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--game") == 0 && i + 1 < argc)
            gameIdx = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--name") == 0 && i + 1 < argc)
            name = argv[++i];
    }

    maxP = std::clamp(maxP, 1, 32);
    if (port < 1024 || port > 65535) port = 27015;

    LanNet net;
    if (!net.startDedicated(port, gameIdx, maxP, name.c_str())) {
        std::fprintf(stderr, "Impossibile avviare il server.\n");
        return 1;
    }

    // Qui il server prende la sua decisione: chi entra e chi no.
    bool listFound = false;
    std::string listPath = intgSelfDir() + "/allowed_clients.txt";
    net.allowedFingerprints = loadAllowedFingerprints(listPath, listFound);

    std::printf("========================================\n");
    std::printf("  BlockyServer ATTIVO\n");
    std::printf("  Porta: %d\n", port);
    std::printf("  Max giocatori: %d\n", maxP);
    std::printf("  Nome: %s\n", name.c_str());
    std::printf("  Join da Blocky con l'IP di questo PC\n");
    if (!net.allowedFingerprints.empty()) {
        std::printf("  Integrita': SEVERO — %d impronte ammesse\n",
                    (int)net.allowedFingerprints.size());
    } else {
        std::printf("  Integrita': APPRENDIMENTO — accetta tutti e registra\n");
        std::printf("    %s\n", listFound ? "(elenco presente ma vuoto)"
                                          : "(nessun allowed_clients.txt)");
        std::printf("    Per chiudere: copia le impronte viste in\n");
        std::printf("    %s\n", listPath.c_str());
    }
    bool scriptsFound = false;
    std::string scriptsPath = intgSelfDir() + "/allowed_scripts.txt";
    net.allowedScripts = loadAllowedScripts(scriptsPath, scriptsFound);
    if (!net.allowedScripts.empty()) {
        std::printf("  Script:     SEVERO — %d hash ammessi (chi ne esegue altri e' cacciato)\n",
                    (int)net.allowedScripts.size());
    } else {
        std::printf("  Script:     APPRENDIMENTO — registra gli hash che arrivano\n");
        std::printf("    %s\n", scriptsPath.c_str());
    }
    std::printf("  Ctrl+C per chiudere\n");
    std::printf("========================================\n");

    float t0 = 0.f;
#ifdef _WIN32
    LARGE_INTEGER freq{}, counter{};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    double start = (double)counter.QuadPart / (double)freq.QuadPart;
#endif

    int lastCount = -1;
    for (;;) {
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
            std::printf("[Server] Giocatori: %d / %d\n", c, maxP);
        }
    }
    return 0;
}
