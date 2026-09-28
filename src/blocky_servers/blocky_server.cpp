// Server dedicato multiplayer, in una finestra di console. La logica vera
// vive in dedicated_server_core.h, condivisa con blocky_rcc_service.cpp (il
// servizio Windows): questo file resta solo il "come lo si ospita" da
// console, byte per byte come si comportava prima dell'estrazione.

#include "dedicated_server_core.h"

int main(int argc, char** argv) {
    // Log senza buffer: l output di un server serve MENTRE succede. Verso una
    // pipe o un file printf accumula in un buffer da qualche kilobyte che in un
    // processo che non finisce mai non si riempie mai — e non si vedeva niente,
    // nemmeno il banner d avvio.
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    DedicatedServerConfig cfg;
    bool wantsHelp = false;
    parseServerArgs(argc, argv, cfg, wantsHelp);
    if (wantsHelp) {
        printServerHelp();
        return 0;
    }

    const ServerLogFn consoleLog = [](const std::string& line) {
        std::printf("%s\n", line.c_str());
    };

    LanNet net;
    if (!startDedicatedServer(net, cfg, consoleLog)) return 1;

    std::atomic<bool> keepRunning{true};
    runDedicatedServerLoop(net, cfg.maxPlayers, keepRunning, consoleLog);
    return 0;
}
