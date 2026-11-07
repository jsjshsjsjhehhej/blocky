// Server dedicato multiplayer, in una finestra di console. La logica vera
// vive in dedicated_server_core.h, condivisa con blocky_rcc_service.cpp (il
// servizio Windows): questo file resta solo il "come lo si ospita" da
// console. Con --place carica anche il mondo ed esegue gli Script server
// (ServerWorld.h); senza, resta il ripetitore di sempre.

#include "dedicated_server_core.h"
#include "ServerWorld.h"

#include <memory>

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

    // Il mondo e' grande (LuaRuntime + DataModel): sullo heap, non sullo stack.
    std::unique_ptr<blkserver::ServerWorld> world;
    ServerTickFn tick;
    if (!cfg.placePath.empty()) {
        world = std::make_unique<blkserver::ServerWorld>();
        if (world->load(cfg.placePath, net, consoleLog)) {
            consoleLog("  Script server: ATTIVI (il server e' l'autorita' sulle part)");
            tick = [&](float dt) { world->tick(net, dt); };
        } else {
            consoleLog("  Script server: NON attivi, il server fa solo da ripetitore");
            world.reset();
        }
    } else {
        consoleLog("  Nessun --place: solo ripetitore (gli Script server non girano)");
    }

    std::atomic<bool> keepRunning{true};
    runDedicatedServerLoop(net, cfg.maxPlayers, keepRunning, consoleLog, tick);
    return 0;
}
