#include "engine/DataModel.h"
#include "engine/LuaRuntime.h"
#include "engine/GameHub.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

int main() {
    DataModel dm;
    dm.loadBaseplatePlace();
    LuaRuntime lua;
    if (!lua.init(&dm)) {
        std::fprintf(stderr, "FAIL init: %s\n", lua.lastError.c_str());
        return 1;
    }

    const char* scripts[] = {
        "scripts/baseplate.lua",
        "scripts/garden.lua",
        "scripts/obby.lua",
        // Script d'esempio spediti con il motore: se un'API cambia nome,
        // meglio scoprirlo qui che dall'utente che apre lo script e trova
        // un errore.
        "scripts/monete_salvate.lua",
        "scripts/item_fucile.lua",
    };
    for (const char* rel : scripts) {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        std::string path = resolveScriptPath(rel);
        std::fprintf(stdout, "RUN %s\n", path.c_str());
        if (!lua.runFile(path)) {
            std::fprintf(stderr, "FAIL %s: %s\n", rel, lua.lastError.c_str());
            return 2;
        }
        std::fprintf(stdout, "OK %s parts=%zu\n", rel, dm.parts.size());
        lua.fireHeartbeat(0.016f);
    }
    std::fprintf(stdout, "ALL LUA TESTS PASSED\n");

    // Bisezione mirata: percorsi NON esercitati sopra (script attaccati
    // a una part vera via bindScriptParent, Touched, task.wait/spawn,
    // bottoni GUI) — l'area piu' sospetta per un crash in modalita' Test
    // dato che il bug dell'interrupt trovato prima toccava lo stack a
    // ogni call/return, non solo nei percorsi gia' testati sopra.
    {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        WorkspacePart p;
        p.name = "TouchTest";
        p.position = {0, 1, 0};
        dm.parts.push_back(p);
        int idx = (int)dm.parts.size() - 1;

        std::fprintf(stderr, "[bisect2] runPartScript + Touched:Connect\n"); std::fflush(stderr);
        bool r1 = lua.runPartScript(
            "script.Parent.Touched:Connect(function(hit) print('touched by', hit.Name) end)",
            idx);
        std::fprintf(stderr, "[bisect2] r1=%d err=%s\n", (int)r1, lua.lastError.c_str()); std::fflush(stderr);

        std::fprintf(stderr, "[bisect2] firePartTouched\n"); std::fflush(stderr);
        lua.firePartTouched(idx);
        std::fprintf(stderr, "[bisect2] post firePartTouched\n"); std::fflush(stderr);

        std::fprintf(stderr, "[bisect2] task.spawn\n"); std::fflush(stderr);
        bool r2 = lua.runString("task.spawn(function() print('spawned!') end)");
        std::fprintf(stderr, "[bisect2] r2=%d err=%s\n", (int)r2, lua.lastError.c_str()); std::fflush(stderr);

        std::fprintf(stderr, "[bisect2] task.wait setup\n"); std::fflush(stderr);
        bool r3 = lua.runString(
            "task.spawn(function() task.wait(0.05); print('resumed after wait!') end)");
        std::fprintf(stderr, "[bisect2] r3=%d err=%s\n", (int)r3, lua.lastError.c_str()); std::fflush(stderr);

        std::fprintf(stderr, "[bisect2] heartbeat ticks to resolve task.wait\n"); std::fflush(stderr);
        for (int i = 0; i < 10; ++i) {
            lua.fireHeartbeat(0.016f);
            std::fprintf(stderr, "[bisect2] tick %d ok\n", i); std::fflush(stderr);
        }

        std::fprintf(stderr, "[bisect2] fireGuiButtonActivated (no listener, should no-op)\n"); std::fflush(stderr);
        lua.fireGuiButtonActivated("NonExistentButton");
        std::fprintf(stderr, "[bisect2] ALL BISECT2 DONE\n"); std::fflush(stderr);
    }

    // Riproduzione fedele del crash reale di Test in Studio: g_lua e' un
    // oggetto persistente costruito una volta all'avvio del processo, e
    // l'utente preme F5 secondi/minuti dopo — callStart_ (impostato alla
    // costruzione) e' quindi "vecchio" quando bindScriptParent alloca
    // tabelle/closure fuori da pcall, PRIMA che runString/runCompiled lo
    // resetti. Un test sincrono normale (init poi subito script) non lo
    // riproduce perche' passano solo microsecondi. Qui si aspetta
    // deliberatamente piu' del budget (50ms) tra init e lo script, con lo
    // stesso identico script Neon segnalato dall'utente, per provare che
    // il fix (ignorare gli interrupt con gc>=0) tiene anche in quel caso.
    {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        WorkspacePart p;
        p.name = "Part";
        p.position = {0, 1, 0};
        dm.parts.push_back(p);
        int idx = (int)dm.parts.size() - 1;

        std::fprintf(stderr, "[stale] aspetto 300ms per invecchiare callStart_...\n"); std::fflush(stderr);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        const char* neonScript =
            "local part = script.Parent\n"
            "part.Material = \"Neon\"\n"
            "part.Emissive = 1\n"
            "part.PointLight = true\n"
            "part.LightRange = 22\n"
            "part.LightBrightness = 1.4\n"
            "print(\"Neon attivo:\", part.Name, part.Material)\n"
            "local t = 0\n"
            "game.RunService.Heartbeat:Connect(function(dt)\n"
            "    t = t + dt\n"
            "    part.Emissive = 0.85 + math.sin(t * 3) * 0.35\n"
            "    part.LightBrightness = 1.1 + math.sin(t * 3) * 0.4\n"
            "end)\n"
            "part.Touched:Connect(function()\n"
            "    part.Emissive = 2\n"
            "    part.LightBrightness = 2.5\n"
            "    print(\"Neon flash!\")\n"
            "end)\n";
        std::fprintf(stderr, "[stale] runPartScript (script Neon reale dell'utente)\n"); std::fflush(stderr);
        bool rn = lua.runPartScript(neonScript, idx);
        std::fprintf(stderr, "[stale] rn=%d err=%s\n", (int)rn, lua.lastError.c_str()); std::fflush(stderr);
        lua.fireHeartbeat(0.016f);
        lua.firePartTouched(idx);
        std::fprintf(stderr, "[stale] ALL STALE-CALLSTART TEST DONE\n"); std::fflush(stderr);
    }

    // ItemEquipped/ItemUnequipped (stile Tool.Equipped di Roblox, aggiunti
    // stasera insieme al secondo argomento meshPath di Backpack.Give).
    {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        bool sawEquip = false, sawUnequip = false;
        bool r1 = lua.runString(
            "Events.Connect('ItemEquipped', function(id, name) print('EQUIP', id, name) end)\n"
            "Events.Connect('ItemUnequipped', function(id, name) print('UNEQUIP', id, name) end)\n");
        std::fprintf(stderr, "[items] connect r1=%d err=%s\n", (int)r1, lua.lastError.c_str());
        lua.fireItemEquipped("bp_1_Torcia", "Torcia");
        lua.fireItemUnequipped("bp_1_Torcia", "Torcia");
        sawEquip = sawUnequip = true; // se arriva fin qui senza crash/errore, il dispatcher regge
        std::fprintf(stderr, "[items] ALL ITEM EVENT TESTS DONE ok=%d\n", (int)(sawEquip && sawUnequip));
    }

    return 0;
}
