#include "engine/DataModel.h"
#include "engine/LuaRuntime.h"
#include "engine/GameHub.h"
#include "systems/ClassificaLua.h"
#include "engine/PartGroupsLua.h"
#include "engine/PartConstraintsLua.h"

#include <cmath>

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

    // Proprieta' generate da PartProps.def: prima Anchored non esisteva da
    // Lua e CanCollide si poteva scrivere ma leggendolo tornava nil.
    {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        WorkspacePart p;
        p.name = "Props";
        dm.parts.push_back(p);
        const int idx = (int)dm.parts.size() - 1;
        const bool ok = lua.runPartScript(
            "local p = script.Parent\n"
            "assert(p.Anchored == true, 'Anchored non leggibile')\n"
            "assert(p.CanCollide == true, 'CanCollide legge nil')\n"
            "assert(math.abs(p.Density - 0.7) < 1e-4, 'Density default')\n"
            "p.Anchored = false\n"
            "p.CanCollide = false\n"
            "p.Friction = 5\n"            // oltre il massimo (2): va limitato
            "p.Elasticity = 'ciao'\n"     // tipo sbagliato: resta com'era
            "assert(p.Anchored == false and p.CanCollide == false, 'scrittura bool')\n"
            "assert(p.Friction == 2, 'Friction limitata a 2')\n"
            "assert(math.abs(p.Elasticity - 0.1) < 1e-4, 'tipo sbagliato ignorato')\n"
            "local n = Workspace:Add({Name = 'Nuova', Anchored = false, Density = 3})\n",
            idx);
        const WorkspacePart& q = dm.parts[(size_t)idx];
        const WorkspacePart& added = dm.parts.back();
        const bool pass = ok && !q.anchored && !q.canCollide && q.friction == 2.f
                       && added.name == "Nuova" && !added.anchored && added.density == 3.f;
        std::fprintf(stderr, "[props] %s %s\n", pass ? "OK" : "FAIL", lua.lastError.c_str());
        if (!pass) return 3;
    }

    // Sistemi generati: la tabella Lua arriva dalle estensioni di LuaRuntime.
    {
        Classifica cls;
        cls.reset();
        cls.setLocalPlayer("Walter");
        lua.extensions.push_back({"Classifica", &classificalua::registerLua, &cls});
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        const bool ok = lua.runString(
            "Classifica.Set('Monete', 5)\n"
            "assert(Classifica.Add('Monete', 2) == 7, 'Add')\n"
            "assert(Classifica.Get('Monete') == 7, 'Get')\n"
            "assert(Classifica.Get('Monete', 'Nessuno') == 0, 'Get di un altro')\n"
            "assert(not pcall(Classifica.Set, 'a=b', 1), 'nome non valido deve dare errore')\n"
            "assert(not pcall(function() Classifica.Set = nil end), 'tabella protetta')\n");
        const bool pass = ok && cls.get("Monete") == 7;
        std::fprintf(stderr, "[classifica] %s %s\n", pass ? "OK" : "FAIL", lua.lastError.c_str());
        lua.extensions.clear();
        lua.init(&dm);
        if (!pass) return 4;
    }

    // Model (Ctrl+G) dagli script.
    {
        dm.clear();
        dm.loadBaseplatePlace();
        dm.addPart("Ruota", {0, 1, 0}, {2, 2, 2}, {0.5f, 0.5f, 0.5f});
        dm.addPart("Telaio", {4, 1, 0}, {2, 2, 2}, {0.5f, 0.5f, 0.5f});
        const int n = (int)dm.parts.size();
        partgroups::find(dm, partgroups::group(dm, {n - 2, n - 1}))->name = "Auto";
        lua.extensions.push_back({"Models", &partgroupslua::registerLua, &dm});
        lua.init(&dm);
        const bool ok = lua.runString(
            "assert(Models.Exists('Auto') and not Models.Exists('Nessuno'), 'Exists')\n"
            "assert(Models.Count('Auto') == 2, 'Count')\n"
            "Models.MoveTo('Auto', Vector3.new(0, 10, 0))\n"
            "local p = Models.GetPivot('Auto')\n"
            "assert(math.abs(p.Y - 10) < 1e-3 and math.abs(p.X) < 1e-3, 'MoveTo/GetPivot')\n"
            "Models.SetAnchored('Auto', false)\n"
            "assert(not pcall(Models.MoveTo, 'Nessuno', Vector3.new(0,0,0)), 'Model inesistente: errore')\n"
            "assert(not pcall(function() Models.MoveTo = nil end), 'tabella protetta')\n");
        const bool pass = ok && !dm.parts[(size_t)n - 1].anchored
                       && std::fabs(dm.parts[(size_t)n - 1].position.y - 10.f) < 1e-3f;
        std::fprintf(stderr, "[models] %s %s\n", pass ? "OK" : "FAIL", lua.lastError.c_str());
        lua.extensions.clear();
        lua.init(&dm);
        if (!pass) return 5;
    }

    // Vincoli dagli script (part per oggetto o per nome).
    {
        dm.clear();
        dm.loadBaseplatePlace();
        dm.addPart("Soffitto", {0, 20, 0}, {2, 1, 2}, {0.5f, 0.5f, 0.5f});
        dm.addPart("Lampada", {0, 15, 0}, {1, 1, 1}, {0.5f, 0.5f, 0.5f});
        const size_t before = dm.welds.size();
        lua.extensions.push_back({"Constraints", &partconstraintslua::registerLua, &dm});
        lua.init(&dm);
        const bool ok = lua.runString(
            "local r = Constraints.Rope('Soffitto', 'Lampada', 8)\n"
            "assert(type(r) == 'string', 'ritorna il nome')\n"
            "local h = Constraints.Hinge('Soffitto', 'Lampada', Vector3.new(1, 0, 0))\n"
            "Constraints.SetEnabled(h, false)\n"
            "assert(Constraints.Count() == 2, 'Count')\n"
            "assert(Constraints.Remove(h) and not Constraints.Remove(h), 'Remove')\n"
            "assert(not pcall(Constraints.Rope, 'Soffitto', 'Nessuna'), 'part inesistente: errore')\n"
            "assert(not pcall(Constraints.Weld, 'Soffitto', 'Soffitto'), 'stessa part: errore')\n");
        const bool pass = ok && dm.welds.size() == before + 1 && dm.welds.back().kind == ConstraintKind::Rope
                       && std::fabs(dm.welds.back().length - 8.f) < 1e-4f;
        std::fprintf(stderr, "[constraints] %s %s\n", pass ? "OK" : "FAIL", lua.lastError.c_str());
        lua.extensions.clear();
        lua.init(&dm);
        if (!pass) return 6;
    }

    // Strumenti da sviluppatore: Script Performance e Watch.
    {
        dm.clear();
        dm.loadBaseplatePlace();
        dm.addPart("Lenta", {0, 1, 0}, {1, 1, 1}, {0.5f, 0.5f, 0.5f});
        dm.addPart("Veloce", {3, 1, 0}, {1, 1, 1}, {0.5f, 0.5f, 0.5f});
        lua.init(&dm);
        const int slow = (int)dm.parts.size() - 2, fast = (int)dm.parts.size() - 1;
        lua.runPartScript(
            "punti = 42\n"
            "inventario = {spada = 1, scudo = 2}\n"
            "game.RunService.Heartbeat:Connect(function()\n"
            "    local s = 0\n"
            "    for i = 1, 3000 do s = s + i end\n"
            "end)\n", slow);
        lua.runPartScript(
            "game.RunService.Heartbeat:Connect(function() local x = 1 end)\n", fast);
        for (int i = 0; i < 10; ++i) lua.fireHeartbeat(0.016f);
        lua.rollScriptCosts();
        const auto s = lua.scriptCosts.find("Lenta.Script");
        const auto f = lua.scriptCosts.find("Veloce.Script");
        const bool perf = s != lua.scriptCosts.end() && f != lua.scriptCosts.end()
                       && s->second.calls >= 11 && s->second.lastSecMs > f->second.lastSecMs * 5.0;
        std::fprintf(stderr, "[perf] %s chiamate=%ld lenta=%.3f ms veloce=%.3f ms\n", perf ? "OK" : "FAIL",
                     s != lua.scriptCosts.end() ? s->second.calls : -1L,
                     s != lua.scriptCosts.end() ? s->second.lastSecMs : -1.0,
                     f != lua.scriptCosts.end() ? f->second.lastSecMs : -1.0);
        if (!perf) return 7;

        bool hasPunti = false, hasGame = false;
        for (const auto& g : lua.userGlobals()) {
            if (g.first == "punti" && g.second == "42") hasPunti = true;
            if (g.first == "game" || g.first == "workspace") hasGame = true;
        }
        const std::string twice = lua.evalWatch("punti * 2");
        const std::string inv = lua.evalWatch("inventario");
        const std::string bad = lua.evalWatch("punti +");
        const bool watch = hasPunti && !hasGame && twice == "84"
                        && inv.find("spada") != std::string::npos && bad.rfind("errore", 0) == 0;
        std::fprintf(stderr, "[watch] %s punti*2=%s inventario=%s\n", watch ? "OK" : "FAIL",
                     twice.c_str(), inv.c_str());
        if (!watch) return 8;
    }

    // game.PlaceId / game.JobId e TeleportService (Place Launcher).
    {
        dm.clear();
        dm.loadBaseplatePlace();
        const std::string job1 = blk::jobid::generate(), job2 = blk::jobid::generate();
        lua.placeId = "obby_party";
        lua.jobId = job1;
        lua.init(&dm);
        bool ok = lua.evalWatch("game.PlaceId") == "\"obby_party\"" && lua.evalWatch("game.JobId") == "\"" + job1 + "\"";
        // Il client riceve il JobId dopo l'avvio degli script.
        lua.setGameIdentity("obby_party", job2);
        ok &= lua.evalWatch("game.JobId") == "\"" + job2 + "\"";

        blk::teleport::queue().clear();
        ok &= lua.runString(
            "fallito = nil\n"
            "local ts = game:GetService('TeleportService')\n"
            "ts.TeleportInitFailed:Connect(function(plr, motivo, msg, place) fallito = motivo .. '|' .. place end)\n"
            "ok1 = ts:TeleportToPlaceInstance('tower_run', '" + job1 + "')\n"
            "ok2 = ts:Teleport('tower run')\n");
        ok &= lua.evalWatch("ok1") == "true" && lua.evalWatch("ok2") == "false";
        ok &= blk::teleport::queue().size() == 1 && blk::teleport::queue()[0].placeId == "tower_run"
           && blk::teleport::queue()[0].jobId == job1;
        lua.fireTeleportFailed("GameFull", "pieno", "tower_run");
        ok &= lua.evalWatch("fallito") == "\"GameFull|tower_run\"";
        std::fprintf(stderr, "[jobid] %s JobId=%s fallito=%s\n", ok ? "OK" : "FAIL",
                     lua.evalWatch("game.JobId").c_str(), lua.evalWatch("fallito").c_str());
        blk::teleport::queue().clear();
        lua.placeId.clear();
        lua.jobId.clear();
        if (!ok) return 10;
    }

    // API Luau stile Roblox: connessioni, task, CFrame, Vector3, Random, OOP.
    {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        bool ok = lua.runString(R"(
            local RS = game:GetService("RunService")
            hb = 0; once = 0
            conn = RS.Heartbeat:Connect(function() hb += 1 end)
            RS.Heartbeat:Once(function() once += 1 end)
            connType = typeof(conn)
            for i = 1, 200 do local c = RS.Heartbeat:Connect(function() end) c:Disconnect() end
            churnOk = true

            order = {}
            task.defer(function() table.insert(order, "defer") end)
            task.spawn(function() table.insert(order, "spawn") end)
            spawnFirst = order[1]
            task.spawn(function(a, b) argsOk = (a == 1 and b == 2) end, 1, 2)
            local th = task.delay(0.05, function() delayed = true end)
            threadType = type(th)
            task.cancel(th)
            task.spawn(function() waited = task.wait(0.03) end)
            task.spawn(function() gotHb = RS.Heartbeat:Wait() end)

            local cf = CFrame.new(1, 2, 3) * CFrame.Angles(0, math.rad(90), 0)
            cfPoint = (cf * Vector3.new(0, 0, -1)):FuzzyEq(Vector3.new(0, 2, 3), 1e-4)
            cfInv = (cf * cf:Inverse()):FuzzyEq(CFrame.new(), 1e-4)
            local rx, ry, rz = CFrame.Angles(0.3, 0.5, 0.7):ToEulerAnglesXYZ()
            cfEuler = math.abs(rx - 0.3) < 1e-4 and math.abs(ry - 0.5) < 1e-4 and math.abs(rz - 0.7) < 1e-4
            cfLook = CFrame.lookAt(Vector3.new(0, 0, 0), Vector3.new(0, 0, -10)).LookVector:FuzzyEq(Vector3.new(0, 0, -1), 1e-4)
            cfLerp = CFrame.new(0, 0, 0):Lerp(CFrame.new(10, 0, 0), 0.5).Position:FuzzyEq(Vector3.new(5, 0, 0))
            cfObj = cf:ToObjectSpace(cf * CFrame.new(0, 5, 0)).Position:FuzzyEq(Vector3.new(0, 5, 0), 1e-4)
            cfType = typeof(cf)
            local part = Instance.new("Part")
            part.CFrame = CFrame.new(5, 6, 7) * CFrame.fromEulerAnglesYXZ(0, math.rad(45), 0)
            partCF = part.CFrame.Position:FuzzyEq(Vector3.new(5, 6, 7), 1e-3) and math.abs(part.Orientation.Y - 45) < 0.1
                     and part.CFrame.LookVector:FuzzyEq((CFrame.fromEulerAnglesYXZ(0, math.rad(45), 0)).LookVector, 1e-3)

            vec = Vector3.new(1, 2, 3):Dot(Vector3.new(1, 0, 0)) == 1 and (Vector3.new(2, 4, 6) / 2) == Vector3.new(1, 2, 3)
                  and tostring(Vector3.new(1, 2, 3)) == "1, 2, 3" and Vector3.zero.X == 0 and typeof(Vector3.one) == "Vector3"
                  and Vector3.new(0, 0, 1):Cross(Vector3.new(1, 0, 0)):FuzzyEq(Vector3.new(0, 1, 0))

            local r1, r2 = Random.new(42), Random.new(42)
            rnd = r1:NextInteger(1, 100) == r2:NextInteger(1, 100)
            local counts = {0, 0, 0, 0, 0, 0}
            for i = 1, 6000 do local v = r1:NextInteger(1, 6) counts[v] += 1 end
            rndDist = true
            for i = 1, 6 do if counts[i] < 800 or counts[i] > 1200 then rndDist = false end end
            local n = r1:NextNumber(5, 6)
            rndRange = n >= 5 and n < 6

            local Class = {}
            Class.__index = Class
            function Class.new(v) return setmetatable({ v = v }, Class) end
            function Class:get() return self.v end
            oop = Class.new(7):get() == 7 and getmetatable(Class.new(1)) == Class
            mtLocked = getmetatable(Vector3.new(1, 2, 3)) == "Vector3"
            setLocked = not pcall(setmetatable, Vector3.new(1, 2, 3), {})
            libs = bit32.band(6, 3) == 2 and type(os.clock()) == "number" and type(os.time()) == "number"

            local np = Instance.new("Part")
            np.Name = "NuovaDaScript"
            np.CFrame = CFrame.new(10, 3, -4)
            np.Parent = workspace
            np.Position = Vector3.new(11, 3, -4) -- la stessa variabile muove la part vera
            local found = workspace:FindFirstChild("NuovaDaScript")
            parented = found ~= nil and found.Position:FuzzyEq(Vector3.new(11, 3, -4), 1e-3) and np.CFrame.X == 11
        )");
        if (!ok) std::fprintf(stderr, "[api] script: %s\n", lua.lastError.c_str());
        for (int i = 0; i < 3; ++i) lua.fireHeartbeat(0.016f);
        ok &= lua.runString("conn:Disconnect() connAfter = conn.Connected");
        for (int i = 0; i < 5; ++i) lua.fireHeartbeat(0.016f);

        struct Expect { const char* expr; const char* want; };
        const Expect ex[] = {
            {"hb", "3"}, {"once", "1"}, {"connType", "\"RBXScriptConnection\""}, {"connAfter", "false"}, {"churnOk", "true"},
            {"spawnFirst", "\"spawn\""}, {"order[2]", "\"defer\""}, {"argsOk", "true"}, {"threadType", "\"thread\""},
            {"delayed", "nil"}, {"waited ~= nil and waited >= 0.03", "true"}, {"gotHb ~= nil", "true"},
            {"cfPoint", "true"}, {"cfInv", "true"}, {"cfEuler", "true"}, {"cfLook", "true"}, {"cfLerp", "true"},
            {"cfObj", "true"}, {"cfType", "\"CFrame\""}, {"partCF", "true"}, {"vec", "true"}, {"rnd", "true"},
            {"rndDist", "true"}, {"rndRange", "true"}, {"oop", "true"}, {"mtLocked", "true"}, {"setLocked", "true"},
            {"libs", "true"}, {"parented", "true"},
        };
        int bad = 0;
        for (const auto& e : ex) {
            const std::string got = lua.evalWatch(e.expr);
            if (got != e.want) {
                std::fprintf(stderr, "[api] FAIL %s = %s (atteso %s)\n", e.expr, got.c_str(), e.want);
                ++bad;
            }
        }
        std::fprintf(stderr, "[api] %s (%d controlli, %d falliti)\n", bad == 0 && ok ? "OK" : "FAIL",
                     (int)(sizeof(ex) / sizeof(ex[0])), bad);
        if (bad || !ok) return 11;
    }

    // Altre API: Instance generici (albero, eventi, attributi), tipi, servizi, Enum.
    {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        bool ok = lua.runString(R"(
            local cfg = Instance.new("Configuration")
            cfg.Name = "Impostazioni"
            local att = Instance.new("UICorner", cfg)
            att.Name = "Punto"
            local deep = Instance.new("UIStroke")
            deep.Parent = att
            added = 0; removed = 0; desc = 0; changed = {}; attrHits = 0; destroying = 0
            cfg.ChildAdded:Connect(function() added += 1 end)
            cfg.ChildRemoved:Connect(function() removed += 1 end)
            cfg.DescendantAdded:Connect(function() desc += 1 end)
            att.Changed:Connect(function(p) table.insert(changed, p) end)
            att:GetPropertyChangedSignal("Name"):Connect(function() nameSig = true end)
            att:GetAttributeChangedSignal("Vita"):Connect(function() attrHits += 1 end)
            att.Destroying:Connect(function() destroying += 1 end)

            local extra = Instance.new("Trail")
            extra.Parent = cfg               -- ChildAdded + DescendantAdded
            att.Name = "PuntoA"               -- Changed("Name") + segnale proprieta'
            att:SetAttribute("Vita", 50)
            tree = cfg:FindFirstChild("PuntoA") == att and cfg:FindFirstChild("UIStroke", true) == deep
                   and #cfg:GetChildren() == 2 and #cfg:GetDescendants() == 3 and cfg.PuntoA == att
            isa = att:IsA("UICorner") and att:IsA("Instance") and not att:IsA("Part")
                  and deep:IsDescendantOf(cfg) and cfg:IsAncestorOf(deep) and deep:FindFirstAncestor("Impostazioni") == cfg
            fullName = deep:GetFullName()
            attr = att:GetAttribute("Vita") == 50 and att:GetAttributes().Vita == 50
            local copy = cfg:Clone()
            cloneOk = copy ~= cfg and copy.Name == "Impostazioni" and #copy:GetDescendants() == 3 and copy.Parent == nil
            cyc = not pcall(function() cfg.Parent = deep end)
            att:Destroy()
            destroyedOk = att.Parent == nil and #cfg:GetChildren() == 1 and destroying == 1
            reparentDead = not pcall(function() att.Parent = cfg end)
            instType = typeof(cfg)
            task.spawn(function() waitedChild = cfg:WaitForChild("Tardivo", 2) end)
            task.delay(0.05, function() local t = Instance.new("Atmosphere") t.Name = "Tardivo" t.Parent = cfg end)

            local r = Ray.new(Vector3.new(0, 0, 0), Vector3.new(10, 0, 0))
            ray = r:ClosestPoint(Vector3.new(5, 3, 0)):FuzzyEq(Vector3.new(5, 0, 0)) and math.abs(r:Distance(Vector3.new(5, 3, 0)) - 3) < 1e-6
                  and r.Unit.Direction:FuzzyEq(Vector3.new(1, 0, 0)) and typeof(r) == "Ray"
            local cs = ColorSequence.new(Color3.new(1, 0, 0), Color3.new(0, 0, 1))
            seq = #cs.Keypoints == 2 and cs.Keypoints[2].Time == 1 and typeof(cs) == "ColorSequence"
            local pp = PhysicalProperties.new(2, 0.5, 0.1)
            phys = pp.Density == 2 and pp.Friction == 0.5 and typeof(pp) == "PhysicalProperties"
            local d = DateTime.fromUnixTimestamp(0)
            dt = d:ToIsoDate() == "1970-01-01T00:00:00Z" and d:ToUniversalTime().Year == 1970 and DateTime.now().UnixTimestamp > 1700000000
            svcs = game:GetService("TextService"):GetTextSize("ciao", 10) ~= nil
                   and game:GetService("PolicyService"):GetPolicyInfoForPlayerAsync().AreAdsAllowed == false
                   and game:GetService("BadgeService"):UserHasBadgeAsync(1, 2) == false
                   and game:GetService("ReplicatedFirst").ClassName == "ReplicatedFirst"
                   and game:FindService("NonEsiste") == nil
                   and not pcall(function() game:GetService("NonEsiste") end)
            enums = Enum.HumanoidStateType.Jumping ~= nil and Enum.SortOrder.LayoutOrder ~= nil and Enum.CameraType.Scriptable ~= nil
                    and Enum.ZIndexBehavior.Sibling ~= nil and Enum.MouseBehavior.LockCenter ~= nil and Enum.AutomaticSize.XY ~= nil
            globals = type(elapsedTime()) == "number" and type(version()) == "string"
            unknownRendered = Instance.new("Part").ClassName == "Part" -- le classi del motore restano quelle del motore
        )");
        if (!ok) std::fprintf(stderr, "[api2] script: %s\n", lua.lastError.c_str());
        for (int i = 0; i < 10; ++i) lua.fireHeartbeat(0.016f);
        struct Expect { const char* expr; const char* want; };
        const Expect ex[] = {
            {"added", "2"}, {"desc", "2"}, {"removed", "1"}, {"table.find(changed, \"Name\") ~= nil", "true"},
            {"nameSig", "true"}, {"attrHits", "1"}, {"tree", "true"}, {"isa", "true"},
            {"fullName", "\"Impostazioni.PuntoA.UIStroke\""}, {"attr", "true"}, {"cloneOk", "true"}, {"cyc", "true"},
            {"destroyedOk", "true"}, {"reparentDead", "true"}, {"instType", "\"Instance\""},
            {"waitedChild ~= nil and waitedChild.Name", "\"Tardivo\""}, {"ray", "true"}, {"seq", "true"},
            {"phys", "true"}, {"dt", "true"}, {"svcs", "true"}, {"enums", "true"}, {"globals", "true"},
            {"unknownRendered", "true"},
        };
        int bad = 0;
        for (const auto& e : ex) {
            const std::string got = lua.evalWatch(e.expr);
            if (got != e.want) { std::fprintf(stderr, "[api2] FAIL %s = %s (atteso %s)\n", e.expr, got.c_str(), e.want); ++bad; }
        }
        std::fprintf(stderr, "[api2] %s (%d controlli, %d falliti)\n", bad == 0 && ok ? "OK" : "FAIL",
                     (int)(sizeof(ex) / sizeof(ex[0])), bad);
        if (bad || !ok) return 13;
    }

    // Animator: animazione .blkanim (come la salva l'Animation Studio) caricata da uno script.
    {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        blk::anim::Action wave;
        wave.name = "Saluto";
        blk::anim::Transform up;
        up.rot = glm::angleAxis(glm::radians(150.f), glm::vec3(0, 0, 1));
        wave.insertKeys("Right Shoulder", blk::anim::Transform{}, 0);
        wave.insertKeys("Right Shoulder", up, 15);
        wave.insertKeys("Right Shoulder", blk::anim::Transform{}, 30);
        wave.frameEnd = 30;
        wave.loop = false;
        wave.markers.push_back({15.f, "Su"});
        plat::makeDirs(blockyDataDir() + "/animations");
        const std::string file = blockyDataDir() + "/animations/test_blocky_saluto.blkanim";
        bool ok = blk::anim::saveFile(file, lua.charAnim.arm, {wave});
        ok &= lua.runString(R"(
            local anim = Instance.new("Animation")
            anim.AnimationId = "blkanim://test_blocky_saluto"
            track = Humanoid.Animator:LoadAnimation(anim)
            trackType = typeof(track)
            len = track.Length
            marker = 0; stopped = 0; ended = 0
            track:GetMarkerReachedSignal("Su"):Connect(function() marker += 1 end)
            track.Stopped:Connect(function() stopped += 1 end)
            track.Ended:Connect(function() ended += 1 end)
            track.Priority = Enum.AnimationPriority.Action
            prio = track.Priority == Enum.AnimationPriority.Action
            track:Play(0)
            playing = track.IsPlaying
            playingList = #Humanoid:GetPlayingAnimationTracks()
            refused = not pcall(function() Humanoid:LoadAnimation("rbxassetid://123") end)
            badName = not pcall(function() Humanoid:LoadAnimation("blkanim://../segreti") end)
        )");
        if (!ok) std::fprintf(stderr, "[animator] script: %s\n", lua.lastError.c_str());
        for (int i = 0; i < 30; ++i) lua.fireHeartbeat(1.f / 60.f); // 0.5 s: braccio in alto
        Humanoid h = Humanoid::createR6Blocky();
        lua.applyCharacterAnimation(h);
        float armW = 0.f, armZ = 0.f, legW = 0.f;
        for (const auto& p : h.parts) {
            if (p.name == "Right Arm") { armW = p.animWeight; armZ = p.anim.rot.z; }
            if (p.name == "Left Leg") legW = p.animWeight;
        }
        for (int i = 0; i < 60; ++i) lua.fireHeartbeat(1.f / 60.f); // fine (1 s) + dissolvenza
        lua.applyCharacterAnimation(h);
        float armAfter = 1.f;
        for (const auto& p : h.parts) if (p.name == "Right Arm") armAfter = p.animWeight;

        struct Expect { const char* expr; const char* want; };
        const Expect ex[] = {
            {"trackType", "\"AnimationTrack\""}, {"math.abs(len - 1) < 1e-3", "true"}, {"prio", "true"},
            {"playing", "true"}, {"playingList", "1"}, {"marker", "1"}, {"stopped", "1"}, {"ended", "1"},
            {"track.IsPlaying", "false"}, {"refused", "true"}, {"badName", "true"},
        };
        int bad = 0;
        for (const auto& e : ex) {
            const std::string got = lua.evalWatch(e.expr);
            if (got != e.want) { std::fprintf(stderr, "[animator] FAIL %s = %s (atteso %s)\n", e.expr, got.c_str(), e.want); ++bad; }
        }
        const bool body = armW > 0.99f && armZ > 100.f && legW == 0.f && armAfter == 0.f;
        std::fprintf(stderr, "[animator] %s corpo: braccio peso %.2f rotZ %.1f, gamba %.2f, dopo %.2f (%d falliti)\n",
                     bad == 0 && ok && body ? "OK" : "FAIL", armW, armZ, legW, armAfter, bad);
        std::remove(file.c_str());
        if (bad || !ok || !body) return 12;
    }

    // Log in memoria per la Developer Console: le ultime 400 righe, in ordine.
    {
        for (int i = 0; i < 450; ++i) LOG_INFO("testlog", "riga %d", i);
        std::vector<blog::Entry> rows;
        for (const auto& e : blog::recentOrdered())
            if (e.tag == "testlog") rows.push_back(e);
        const bool ok = rows.size() <= blog::kRecent && !rows.empty()
                     && rows.back().text == "riga 449" && rows.front().text != "riga 0";
        std::fprintf(stderr, "[log] %s (%d righe, ultima '%s')\n", ok ? "OK" : "FAIL", (int)rows.size(),
                     rows.empty() ? "" : rows.back().text.c_str());
        if (!ok) return 9;
    }

    // UserInputService.InputBegan / InputEnded: prima registrati ma mai chiamati.
    {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        const char* src =
            "local UIS = game:GetService('UserInputService')\n"
            "PRESI = ''\n"
            "UIS.InputBegan:Connect(function(input, gp)\n"
            "    if input.KeyCode == Enum.KeyCode.E then PRESI = PRESI .. 'E' end\n"
            "    if input.UserInputType == Enum.UserInputType.MouseButton1 then PRESI = PRESI .. 'M' end\n"
            "    if gp then PRESI = PRESI .. 'g' end\n"
            "end)\n"
            "UIS.InputEnded:Connect(function(input) if input.KeyCode == Enum.KeyCode.E then PRESI = PRESI .. 'e' end end)\n";
        const bool ran = lua.runString(src);
        lua.fireInput(true, "E", "Keyboard", 10, 20, false);
        lua.fireInput(false, "E", "Keyboard", 10, 20, false);
        lua.fireInput(true, "Unknown", "MouseButton1", 10, 20, true);
        const std::string got = lua.evalWatch("PRESI");
        const bool ok = ran && got == "\"EeMg\"";
        std::fprintf(stderr, "[input] %s (%s)\n", ok ? "OK" : "FAIL", got.c_str());
        if (!ok) return 11;
    }

    // ClickDetector.MouseClick: scatta con il clic, non con il tocco (prima era Touched).
    {
        dm.clear();
        dm.loadBaseplatePlace();
        lua.init(&dm);
        const char* src =
            "local p = Instance.new('Part')\n"
            "p.Parent = workspace\n"
            "p.Name = 'Bottone'\n"
            "local cd = Instance.new('ClickDetector', p)\n"
            "CLIC = 0\n"
            "CHI = ''\n"
            "cd.MouseClick:Connect(function(player) CLIC += 1 CHI = typeof(player) end)\n";
        const bool ran = lua.runString(src);
        const int idx = (int)dm.parts.size() - 1;
        lua.firePartTouched(idx);
        const std::string afterTouch = lua.evalWatch("CLIC");
        const bool hasClick = lua.partHasClick(idx);
        lua.firePartClicked(idx);
        const std::string afterClick = lua.evalWatch("CLIC");
        const bool ok = ran && hasClick && afterTouch == "0" && afterClick == "1";
        std::fprintf(stderr, "[click] %s (dopo il tocco %s, dopo il clic %s, giocatore %s)\n", ok ? "OK" : "FAIL",
                     afterTouch.c_str(), afterClick.c_str(), lua.evalWatch("CHI").c_str());
        if (!ok) return 12;
    }

    // Script che non finisce mai: va fermato dal budget, non deve mandare in
    // crash il processo (prima: SIGILL quando l'interrupt trovava lo stack pieno).
    {
        int stopped = 0;
        for (int round = 0; round < 6; ++round) {
            dm.clear();
            dm.loadBaseplatePlace();
            lua.init(&dm);
            const char* src = round % 2 == 0 ? "local x = 0\nwhile true do x += 1 end\n"
                                             : "for i = 1, 1e9 do local t = Instance.new('Part') t.Parent = workspace end\n";
            lua.lastError.clear();
            if (!lua.runString(src) && lua.lastError.find("budget") != std::string::npos) ++stopped;
        }
        std::fprintf(stderr, "[budget] script infiniti fermati %d / 6\n", stopped);
        if (stopped != 6) return 10;
    }

    std::fprintf(stdout, "ALL LUA TESTS PASSED\n");
    return 0;
}
