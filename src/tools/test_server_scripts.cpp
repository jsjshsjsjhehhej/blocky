// Server dedicato con gli Script server (blocky_servers/ServerWorld.h), su
// rete vera 127.0.0.1:
//   1. il server carica il place ed esegue SOLO gli Script server;
//   2. il client riceve la part creata dallo script del server, senza doppioni;
//   3. Players: PlayerAdded (con leaderstats), GetPlayers, OnServerEvent con
//      il Player vero, Touched del giocatore su una part, PlayerRemoving;
//   4. fisica sul server: una cassa libera cade sul server e il client la
//      vede cadere (part mossa dal server, netDriven).
//
// Lo script del place non usa graffe: il caricatore dei place chiude una part
// alla prima graffa chiusa, anche dentro lo script.

#include "blocky_servers/ServerWorld.h"
#include "engine/PlaceExporter.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

static int g_fail = 0;
static void check(bool ok, const std::string& what) {
    std::printf("[server-scripts] %s  %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++g_fail;
}

static int countNamed(const DataModel& dm, const char* name) {
    int n = 0;
    for (const auto& p : dm.parts) n += p.name == name;
    return n;
}
static const WorkspacePart* findNamed(const DataModel& dm, const char* name) {
    for (const auto& p : dm.parts) if (p.name == name) return &p;
    return nullptr;
}

int main() {
    const char* serverScript = R"LUA(
local Players = game:GetService("Players")
local RS = game:GetService("ReplicatedStorage")
JOINED = ""
REMOVED = ""
COINS = 0
SENDER = ""

Players.PlayerAdded:Connect(function(player)
    JOINED = player.Name
    local stats = Instance.new("Folder")
    stats.Name = "leaderstats"
    stats.Parent = player
    local m = Instance.new("IntValue")
    m.Name = "Monete"
    m.Parent = stats
    PLAYER_COUNT = #Players:GetPlayers()
end)
Players.PlayerRemoving:Connect(function(player) REMOVED = player.Name end)

local p = Instance.new("Part")
p.Parent = workspace
p.Name = "DalServer"
p.Position = Vector3.new(5, 7, 5)
p.Anchored = true

local coin = Instance.new("Part")
coin.Parent = workspace
coin.Name = "Moneta"
coin.Size = Vector3.new(2, 2, 2)
coin.Position = Vector3.new(3, 1, 0)
coin.Anchored = true
coin.Touched:Connect(function(hit)
    local player = Players:GetPlayerFromCharacter(hit.Parent)
    if player then
        player.leaderstats.Monete.Value += 1
        COINS = player.leaderstats.Monete.Value
    end
end)

local box = Instance.new("Part")
box.Parent = workspace
box.Name = "Cassa"
box.Size = Vector3.new(2, 2, 2)
box.Position = Vector3.new(-6, 30, -6)
box.Anchored = false

local re = Instance.new("RemoteEvent")
re.Name = "Colora"
re.Parent = RS
re.OnServerEvent:Connect(function(player, what)
    SENDER = typeof(player) .. ":" .. tostring(player.Name) .. ":" .. tostring(what)
    p.Position = Vector3.new(5, 20, 5)
    local q = Instance.new("Part")
    q.Parent = workspace
    q.Name = "Risposta"
    q.Anchored = true
end)
)LUA";
    const char* legacyScript =
        "local x = Instance.new(\"Part\")\n"
        "x.Parent = workspace\n"
        "x.Name = \"NonSulServer\"\n";

    DataModel src;
    src.loadBaseplatePlace();
    WorkspacePart brain;
    brain.name = "Cervello";
    brain.position = {0, 1, -12};
    brain.script = serverScript;
    brain.scriptType = ScriptType::Server;
    src.parts.push_back(brain);
    WorkspacePart local;
    local.name = "Locale";
    local.position = {4, 1, -12};
    local.script = legacyScript;
    local.scriptType = ScriptType::Legacy;
    src.parts.push_back(local);
    const int placeParts = (int)src.parts.size();

    const std::string placePath = "test_server_scripts_place.json";
    {
        Humanoid hum;
        std::ofstream f(placePath, std::ios::binary);
        f << PlaceExporter::exportPlace(src, hum, {0, 0, 0}, 0.f);
    }

    // ---- Server ----
    const int port = 27971;
    LanNet server;
    if (!server.startDedicated(port, -1, 4, "ScriptTest")) {
        std::printf("[server-scripts] FAIL startDedicated sulla porta %d\n", port);
        return 1;
    }
    auto world = std::make_unique<blkserver::ServerWorld>();
    const bool loaded = world->load(placePath, server, [](const std::string& l) { std::printf("%s\n", l.c_str()); });
    check(loaded, "il server carica il place");
    check(world->scriptsRun == 1 && world->scriptErrors == 0,
          "gira 1 Script server, senza errori" + (world->lua.lastError.empty() ? "" : " (" + world->lua.lastError + ")"));
    check(countNamed(world->world, "DalServer") == 1, "lo Script server ha creato la sua part sul server");
    check(countNamed(world->world, "NonSulServer") == 0, "lo script Legacy NON gira sul server (gira sui client)");
    // evalWatch restituisce le stringhe tra virgolette: qui senza.
    auto watch = [&](const char* g) {
        std::string v = world->lua.evalWatch(g);
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
        return v;
    };

    // ---- Client: carica lo stesso place come fa il gioco (finishLoading) ----
    DataModel clientWorld;
    loadPlaceFromJson(placePath, clientWorld);
    blkserver::assignPlaceNetIds(clientWorld);
    LanNet client;
    client.startClient("127.0.0.1", port, 0, "Giocatore");

    float t = 0.f;
    glm::vec3 clientPos{0.f, 0.f, -4.f};
    auto pump = [&](int frames, const std::function<bool()>& until) {
        for (int i = 0; i < frames; ++i) {
            t += 1.f / 60.f;
            server.poll(t);
            world->tick(server, 1.f / 60.f);
            client.poll(t);
            client.sendLocalState(1.f / 60.f, clientPos, 0.f, false);
            client.applyIncomingWorld(clientWorld);
            WorldReplication::smoothNetDriven(clientWorld, 1.f / 60.f);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            if (until && until()) return true;
        }
        return false;
    };

    check(pump(600, [&] { return client.connected; }), "il client entra nel server");
    const bool joined = pump(600, [&] { return watch("JOINED") == "Giocatore"; });
    check(joined, "PlayerAdded sul server con il Player del client (JOINED=" + watch("JOINED") + ")");
    check(watch("PLAYER_COUNT") == "1", "Players:GetPlayers() vede 1 giocatore (" + watch("PLAYER_COUNT") + ")");
    check(pump(600, [&] { return countNamed(clientWorld, "DalServer") == 1; }),
          "il client riceve la part creata dallo Script server");
    pump(120, nullptr);   // lascia finire lo snapshot completo
    check((int)clientWorld.parts.size() == placeParts + 3,
          "niente part doppie: il client ha le " + std::to_string(placeParts) + " del place + 3 del server (ne ha "
              + std::to_string(clientWorld.parts.size()) + ")");
    check(countNamed(clientWorld, "Baseplate") == 1, "la Baseplate c'e' una volta sola sul client");

    // ---- Fisica sul server ----
    {
        const WorkspacePart* sbox = findNamed(world->world, "Cassa");
        const bool fell = pump(600, [&] {
            const WorkspacePart* b = findNamed(world->world, "Cassa");
            return b && b->position.y < 3.f;
        });
        sbox = findNamed(world->world, "Cassa");
        check(fell, "fisica sul server: la cassa libera cade e si posa (y="
                        + std::to_string(sbox ? sbox->position.y : -1.f) + ")");
        const bool seen = pump(600, [&] {
            const WorkspacePart* b = findNamed(clientWorld, "Cassa");
            return b && b->position.y < 3.5f;
        });
        const WorkspacePart* cbox = findNamed(clientWorld, "Cassa");
        check(seen && cbox && cbox->netDriven,
              "il client vede la cassa cadere, mossa dal server (y=" + std::to_string(cbox ? cbox->position.y : -1.f)
                  + ", netDriven=" + std::to_string(cbox ? (int)cbox->netDriven : -1) + ")");
    }

    // ---- Touched del giocatore (calcolato dal server) ----
    clientPos = {3.f, 0.f, 0.f};   // dentro la moneta
    const bool coin = pump(600, [&] { return watch("COINS") == "1"; });
    check(coin, "Touched: il giocatore tocca la moneta e le leaderstats salgono (COINS=" + watch("COINS") + ")");
    pump(60, nullptr);
    check(watch("COINS") == "1", "Touched scatta una volta sola finche' resta sulla moneta");

    // ---- FireServer: OnServerEvent riceve il Player, il client vede l'effetto ----
    client.sendRemoteEventToServer("Colora", "rosso");
    check(pump(600, [&] {
              const WorkspacePart* p = findNamed(clientWorld, "DalServer");
              return p && p->position.y > 19.f && countNamed(clientWorld, "Risposta") == 1;
          }),
          "FireServer: il server sposta la sua part e ne crea una nuova, il client vede tutto");
    check(watch("SENDER") == "Instance:Giocatore:rosso", "OnServerEvent riceve il Player vero (" + watch("SENDER") + ")");

    // ---- Uscita ----
    client.shutdown();
    const bool left = pump(900, [&] { return watch("REMOVED") == "Giocatore"; });
    check(left, "PlayerRemoving quando il client esce (REMOVED=" + watch("REMOVED") + ")");
    check(world->playerCount() == 0, "nessun giocatore rimasto sul server");

    server.shutdown();
    std::remove(placePath.c_str());
    if (g_fail) {
        std::printf("[server-scripts] %d controlli falliti\n", g_fail);
        return 1;
    }
    std::printf("ALL SERVER SCRIPT TESTS PASSED\n");
    return 0;
}
