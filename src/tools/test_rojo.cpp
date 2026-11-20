// Progetti Rojo (engine/RojoProject.h): un progetto vero su disco diventa le
// Instance del place con le classi giuste; i file cambiati si aggiornano da
// soli; require(script.Parent.X) e moduli annidati funzionano nel Luau vero.

#include "engine/DataModel.h"
#include "engine/LuaRuntime.h"
#include "engine/RojoProject.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;

static int g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

static void writeFile(const fs::path& p, const std::string& text) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
}

static const SharedInstance* find(const DataModel& w, const std::string& name, int* idx = nullptr) {
    for (size_t i = 0; i < w.instances.size(); ++i)
        if (w.instances[i].name == name) { if (idx) *idx = (int)i; return &w.instances[i]; }
    return nullptr;
}

int main() {
    std::printf("=== Progetti Rojo ===\n");
    const fs::path dir = fs::temp_directory_path() / "blocky_test_rojo";
    std::error_code ec;
    fs::remove_all(dir, ec);
    writeFile(dir / "default.project.json", R"({
  "name": "prova",
  "tree": {
    "$className": "DataModel",
    "ReplicatedStorage": {
      "Shared": { "$path": "src/shared" },
      "Colpo": { "$className": "RemoteEvent" }
    },
    "ServerScriptService": { "Server": { "$path": "src/server" } },
    "StarterPlayer": { "StarterPlayerScripts": { "$path": "src/client" } },
    "Lighting": {},
    "ServizioInventato": {}
  }
})");
    writeFile(dir / "src/shared/Config.luau", "return { danno = 25, nome = \"fucile\" }\n");
    writeFile(dir / "src/shared/Armi/init.luau", "local Dati = require(script.Dati)\nreturn { danno = function() return Dati.base * 2 end }\n");
    writeFile(dir / "src/shared/Armi/Dati.luau", "return { base = 21 }\n");
    writeFile(dir / "src/shared/Mappa.json", R"({"lato": 64, "colori": ["rosso", "blu"]})");
    writeFile(dir / "src/shared/modello.rbxm", "binario");
    writeFile(dir / "src/server/Arena.server.luau",
              "local Config = require(game.ReplicatedStorage.Shared.Config)\n"
              "local Armi = require(game.ReplicatedStorage.Shared.Armi)\n"
              "risultato = Config.danno + Armi.danno()\n"
              "padre = script.Parent.Name\n");
    writeFile(dir / "src/client/Fps.client.luau", "print('client')\n");

    blk::rojo::Project p;
    std::string err;
    check(blk::rojo::loadProject((dir / "default.project.json").string(), p, err), "default.project.json si legge");
    if (!err.empty()) std::printf("      %s\n", err.c_str());

    DataModel w;
    w.addShared(SharedService::ReplicatedStorage, "ModuleScript", "MioModulo");   // gia' nel place: resta
    blk::rojo::applyToWorld(p, w);
    std::printf("      Instance nel place: %zu, avvisi: %zu\n", w.instances.size(), p.warnings.size());
    for (const auto& wv : p.warnings) std::printf("      avviso: %s\n", wv.c_str());

    const SharedInstance* cfg = find(w, "Config");
    const SharedInstance* armi = find(w, "Armi");
    const SharedInstance* arena = find(w, "Arena");
    const SharedInstance* fps = find(w, "Fps");
    const SharedInstance* mappa = find(w, "Mappa");
    const SharedInstance* colpo = find(w, "Colpo");
    check(cfg && cfg->className == "ModuleScript" && cfg->service == SharedService::ReplicatedStorage, "nome.luau -> ModuleScript in ReplicatedStorage");
    check(armi && armi->className == "ModuleScript" && armi->source.find("Dati") != std::string::npos, "cartella con init.luau -> ModuleScript (con i figli)");
    check(arena && arena->className == "Script" && arena->service == SharedService::ServerScriptService, "nome.server.luau -> Script in ServerScriptService");
    check(fps && fps->className == "LocalScript", "nome.client.luau -> LocalScript (StarterPlayerScripts)");
    check(mappa && mappa->source.find("return") == 0 && mappa->source.find("\"lato\"") != std::string::npos, "nome.json -> ModuleScript che restituisce i dati");
    check(colpo && colpo->className == "RemoteEvent", "$className RemoteEvent");
    check(find(w, "MioModulo") != nullptr, "le Instance del place che non sono del progetto restano");
    bool warnRbxm = false, warnLighting = false, warnInventato = false;
    for (const auto& wv : p.warnings) {
        warnRbxm = warnRbxm || wv.find("modello.rbxm") != std::string::npos;
        warnLighting = warnLighting || wv.find("Lighting") != std::string::npos;
        warnInventato = warnInventato || wv.find("ServizioInventato") != std::string::npos;
    }
    check(warnRbxm && warnInventato && !warnLighting, "i file sconosciuti si segnalano; Lighting vuoto e' il servizio del motore");

    // Ricollegare non deve fare doppioni.
    blk::rojo::applyToWorld(p, w);
    int configs = 0;
    for (const auto& s : w.instances) configs += s.name == "Config";
    check(configs == 1, "ricollegando il progetto niente doppioni");

    std::printf("\nLuau:\n");
    {
        LuaRuntime lua;
        check(lua.init(&w), "runtime pronto");
        int ai = -1;
        find(w, "Arena", &ai);
        const bool ran = lua.runSharedScript(w.instances[(size_t)ai].source, ai);
        if (!ran) std::printf("      errore: %s\n", lua.lastError.c_str());
        check(ran && lua.evalWatch("risultato") == "67", "require(game.ReplicatedStorage.Shared.Config) e modulo annidato con require(script.Dati)");
        check(lua.evalWatch("padre") == "\"Server\"", "script.Parent e' la cartella dello script");
        const std::string m = lua.evalWatch("require(game.ReplicatedStorage.Shared.Mappa).colori[2]");
        check(m == "\"blu\"", "i dati .json si leggono con require");
    }

    std::printf("\nSincronizzazione dal vivo:\n");
    {
        DataModel w2;
        blk::rojo::Watcher watch;
        check(watch.connect((dir / "default.project.json").string(), w2, err), "collegato");
        std::vector<std::string> changed;
        watch.poll(w2, changed);
        check(changed.empty(), "nessun cambiamento: niente da aggiornare");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        writeFile(dir / "src/shared/Config.luau", "return { danno = 99, nome = \"fucile\" }\n");
        const bool structure = watch.poll(w2, changed);
        const SharedInstance* c2 = find(w2, "Config");
        std::printf("      struttura=%d cambiati=%zu sorgente=%s", (int)structure, changed.size(), c2 ? c2->source.c_str() : "(manca)\n");
        for (const auto& e : watch.errors()) std::printf("      errore: %s\n", e.c_str());
        check(!structure && changed.size() == 1 && c2 && c2->source.find("99") != std::string::npos, "file cambiato: si aggiorna solo il suo sorgente");
        writeFile(dir / "src/shared/Nuovo.luau", "return 1\n");
        const bool structure2 = watch.poll(w2, changed);
        check(structure2 && find(w2, "Nuovo") != nullptr, "file nuovo: compare nel place");
        fs::remove(dir / "src/shared/Nuovo.luau", ec);
        watch.poll(w2, changed);
        check(find(w2, "Nuovo") == nullptr, "file tolto: sparisce dal place");
        watch.disconnect(&w2);
        check(w2.instances.empty(), "scollegando si toglie quello che era arrivato dal progetto");
    }

    std::printf("\nEsportazione di un place:\n");
    {
        DataModel w3;
        auto& folder = w3.addShared(SharedService::ReplicatedStorage, "Folder", "Moduli");
        (void)folder;
        auto& mod = w3.addShared(SharedService::ReplicatedStorage, "ModuleScript", "Utili", 0);
        mod.source = "return 42\n";
        auto& srv = w3.addShared(SharedService::ServerScriptService, "Script", "Avvio");
        srv.source = "print(require(game.ReplicatedStorage.Moduli.Utili))\n";
        w3.addShared(SharedService::ReplicatedStorage, "RemoteEvent", "Spara");
        const fs::path out = fs::temp_directory_path() / "blocky_test_rojo_export";
        fs::remove_all(out, ec);
        const int n = blk::rojo::exportPlace(w3, out.string(), "esportato", err);
        check(n == 2 && fs::exists(out / "src/ReplicatedStorage/Moduli/Utili.luau") && fs::exists(out / "src/ServerScriptService/Avvio.server.luau"),
              "gli script finiscono nei file giusti");
        check(fs::exists(out / "blocky.d.luau") && fs::exists(out / ".vscode/settings.json"),
              "con i tipi di Blocky per VS Code (luau-lsp)");
        blk::rojo::Project back;
        DataModel w4;
        blk::rojo::loadProject((out / "default.project.json").string(), back, err);
        blk::rojo::applyToWorld(back, w4);
        const SharedInstance* u = find(w4, "Utili");
        const SharedInstance* sp = find(w4, "Spara");
        check(u && u->source == "return 42\n" && sp && sp->className == "RemoteEvent", "e il progetto esportato si ricarica uguale");
        fs::remove_all(out, ec);
    }
    fs::remove_all(dir, ec);
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
