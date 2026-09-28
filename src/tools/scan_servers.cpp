/**
 * Elenca i server Blocky raggiungibili, da riga di comando.
 *
 * Esiste per poter provare la scansione senza passare dall'interfaccia: se la
 * lista qui e' vuota il problema e' nella rete o nel server, se e' piena ma la
 * finestra non mostra niente il problema e' nell'interfaccia. Separa le due
 * cose in un colpo solo.
 */

#include "engine/ServerBrowser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    int port = 27015;
    int gameIdx = 0;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--game") == 0 && i + 1 < argc) gameIdx = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) netExtraDiscoveryHost() = argv[++i];
    }

    std::printf("Cerco server sulla porta %d (gioco %d)", port, gameIdx);
    if (!netExtraDiscoveryHost().empty())
        std::printf(", piu' richiesta diretta a %s", netExtraDiscoveryHost().c_str());
    std::printf("...\n");

    blk::serverbrowser::startScan(port, gameIdx);
    while (blk::serverbrowser::isScanning()) netSleepMs(100);

    const auto found = blk::serverbrowser::result();
    if (found.empty()) {
        std::printf("\nNessun server ha risposto.\n");
        return 1;
    }
    std::printf("\n%d server trovati:\n", (int)found.size());
    for (const auto& s : found) std::printf("  %s\n", s.label().c_str());
    return 0;
}
