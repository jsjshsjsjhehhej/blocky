// Test isolato del relay "blk://" (PoolProtocol.h + le aggiunte a LanNet.h):
// un host e un client si parlano SOLO passando da un BlockyPool.exe reale in
// esecuzione in locale (niente rete finta/mock — e' lo stesso identico
// percorso che userebbero due giocatori veri dietro CGNAT), verificando che
// il join, lo scambio di stato e gli eventi remoti in entrambe le direzioni
// funzionino esattamente come nel percorso LAN diretto.
//
// Uso: avviare prima `BlockyPool.exe --port 28020`, poi lanciare questo
// eseguibile (nessun argomento: usa la stessa porta di default).

#include "engine/LanNet.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

static bool waitUntil(const char* label, int maxTicks, bool (*cond)(LanNet&, LanNet&),
                       LanNet& a, LanNet& b, float& clock) {
    for (int i = 0; i < maxTicks; ++i) {
        clock += 0.05f;
        a.poll(clock);
        b.poll(clock);
        if (cond(a, b)) return true;
#ifdef _WIN32
        Sleep(20);
#endif
    }
    std::printf("[test_pool] TIMEOUT aspettando: %s\n", label);
    return false;
}

int main(int argc, char** argv) {
    int poolPort = 28020;
    if (argc > 1) poolPort = std::atoi(argv[1]);

    LanNet host, client;
    float clock = 0.f;

    if (!host.startDedicated(28001, 0, 4, "TestHost")) {
        std::printf("[test_pool] FALLITO: startDedicated\n");
        return 1;
    }

    if (!host.registerWithPool("127.0.0.1", poolPort, "TestHost")) {
        std::printf("[test_pool] FALLITO: registerWithPool (BlockyPool.exe e' in esecuzione su :%d?)\n", poolPort);
        return 1;
    }
    std::printf("[test_pool] Host registrato — codice blk://%s\n", host.relayCode);

    if (!client.startClientViaPool(host.relayCode, "127.0.0.1", poolPort, 0, "TestClient")) {
        std::printf("[test_pool] FALLITO: startClientViaPool\n");
        return 1;
    }

    bool ok = true;

    ok &= waitUntil("client connesso (Welcome ricevuto)", 100,
        [](LanNet&, LanNet& c) { return c.connected; }, host, client, clock);

    ok &= waitUntil("host vede 1 giocatore", 40,
        [](LanNet& h, LanNet&) { return h.playerCount() == 1; }, host, client, clock);

    client.sendRemoteEventToServer("Ping", "hello");
    ok &= waitUntil("host riceve l'evento del client (client->host via relay)", 40,
        [](LanNet& h, LanNet&) {
            for (auto& e : h.incomingRemotes)
                if (std::strcmp(e.name, "Ping") == 0 && std::strcmp(e.payload, "hello") == 0) return true;
            return false;
        }, host, client, clock);

    host.fireRemoteEventAllClients("Pong", "world");
    ok &= waitUntil("client riceve l'evento dell'host (host->client via relay)", 40,
        [](LanNet&, LanNet& c) {
            for (auto& e : c.incomingRemotes)
                if (std::strcmp(e.name, "Pong") == 0 && std::strcmp(e.payload, "world") == 0) return true;
            return false;
        }, host, client, clock);

    std::printf(ok ? "[test_pool] TUTTO OK\n" : "[test_pool] QUALCOSA NON VA\n");
    return ok ? 0 : 1;
}
