// Quanta API di Roblox c'e' in Blocky: prova DAVVERO ogni classe, proprieta',
// metodo, evento ed enum (dall'API dump, tools/roblox_api/api_probe_data.lua)
// nel Luau del motore, e scrive il rapporto in docs/api_coverage.md.
//
//   test_api_coverage [--min PERCENTUALE]
//
// Una proprieta' c'e' se leggerla da' un valore (le proprieta' che puntano a un
// oggetto, "&Nome" nella lista, possono valere nil: non si contano). Un metodo
// c'e' se e' una funzione; un evento se ha Connect. Ogni classe gira in una
// chiamata sua (il motore ferma le chiamate lunghe piu' di 50 ms).

#include "engine/DataModel.h"
#include "engine/GameHub.h"
#include "engine/LuaRuntime.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef BLOCKY_SOURCE_DIR
#define BLOCKY_SOURCE_DIR "."
#endif

static const char* kProbe = R"LUA(
local function obtain(probe)
    if probe == nil then return nil, "nessun oggetto concreto su cui provarla" end
    local kind, name = probe:match("^(%a+):(.+)$")
    local ok, obj = pcall(function()
        if kind == "new" then return Instance.new(name) end
        if kind == "service" then return game:GetService(name) end
        if name == "game" then return game end
        if name == "workspace" then return workspace end
        if name == "localplayer" then return game:GetService("Players").LocalPlayer end
        if name == "camera" then return workspace.CurrentCamera end
        if name == "terrain" then return workspace.Terrain end
        return nil
    end)
    if not ok then return nil, tostring(obj) end
    if obj == nil then return nil, "non esiste in Blocky" end
    return obj
end

local function has(obj, key)
    local ok, v = pcall(function() return obj[key] end)
    if not ok then return false, nil end
    return v ~= nil, v
end

local guiRoot = nil
local function place(obj, where)
    if obj == nil or where == nil then return end
    pcall(function()
        if where == "workspace" then
            obj.Parent = workspace
        elseif where == "gui" then
            if guiRoot == nil then
                guiRoot = Instance.new("ScreenGui")
                guiRoot.Parent = game:GetService("StarterGui")
            end
            obj.Parent = guiRoot
        end
    end)
end

function PROBE_CLASS(i)
    local c = PROBE_DATA.classes[i]
    local obj, why = obtain(c.probe)
    place(obj, c.place)
    local miss = {}
    local np, op, nf, of, ne, oe, nref = 0, 0, 0, 0, 0, 0, 0
    local gen = 0   -- di quelli che ci sono, quanti solo dallo strato "API presente"
    local function generated(k)
        return obj ~= nil and type(__blkapi_generated) == "function" and __blkapi_generated(obj, k) == true
    end
    for _, p in ipairs(c.props) do
        if p:sub(1, 1) == "&" then
            nref += 1
        else
            np += 1
            if obj and has(obj, p) then op += 1 if generated(p) then gen += 1 end else table.insert(miss, p) end
        end
    end
    for _, f in ipairs(c.funcs) do
        nf += 1
        local ok, v = false, nil
        if obj then ok, v = has(obj, f) end
        if ok and type(v) == "function" then of += 1 if generated(f) then gen += 1 end else table.insert(miss, f .. "()") end
    end
    for _, e in ipairs(c.events) do
        ne += 1
        local ok, v = false, nil
        if obj then ok, v = has(obj, e) end
        local conn = false
        if ok then conn = pcall(function() assert(v.Connect ~= nil) end) end
        if conn then oe += 1 if generated(e) then gen += 1 end else table.insert(miss, e .. " (evento)") end
    end
    LAST = table.concat({ c.name, obj and "1" or "0", tostring(why or ""), np, op, nf, of, ne, oe, nref,
                          table.concat(miss, ", "), gen }, "\t")
end

function PROBE_ENUMS()
    local ok, miss = 0, {}
    for _, e in ipairs(PROBE_DATA.enums) do
        local has = pcall(function() assert(Enum[e] ~= nil) end)
        if has then ok += 1 else table.insert(miss, e) end
    end
    LAST = table.concat({ #PROBE_DATA.enums, ok, table.concat(miss, ", ") }, "\t")
end
)LUA";

struct ClassResult {
    std::string name, why, missing;
    bool obtained = false;
    int np = 0, op = 0, nf = 0, of = 0, ne = 0, oe = 0, nref = 0;
    int gen = 0;   // presenti solo grazie allo strato generato (non fanno ancora niente)
    int total() const { return np + nf + ne; }
    int ok() const { return op + of + oe; }
    int working() const { return ok() - gen; }
};

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

static std::string readGlobal(LuaRuntime& lua, const char* name) {
    lua_State* L = lua.luaState();
    lua_getglobal(L, name);
    std::string s = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
    lua_pop(L, 1);
    return s;
}

static std::string pct(int a, int b) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f%%", b ? 100.0 * a / b : 0.0);
    return buf;
}

int main(int argc, char** argv) {
    double minPct = -1.0;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--min") == 0 && i + 1 < argc) minPct = std::atof(argv[++i]);

    const std::string root = BLOCKY_SOURCE_DIR;
    std::ifstream in(root + "/tools/roblox_api/api_probe_data.lua", std::ios::binary);
    if (!in) { std::printf("[api] manca tools/roblox_api/api_probe_data.lua (node tools/roblox_api/gen_probe.mjs)\n"); return 1; }
    std::stringstream ss;
    ss << in.rdbuf();

    DataModel dm;
    dm.loadBaseplatePlace();
    LuaRuntime lua;
    // Gli stessi servizi del gioco che non chiedono la grafica (come test_compat).
    SoundService sounds;
    LightingService lighting;
    Animator animator;
    LanNet net;
    Humanoid hum = Humanoid::createR6Blocky();
    glm::vec3 playerPos{0.f, 3.f, 0.f};
    float playerYaw = 0.f;
    NavGrid nav;
    Tag tags;
    Classifica classifica;
    Prossimita proximity;
    lua.soundService = &sounds;
    lua.lighting = &lighting;
    lua.animator = &animator;
    lua.net = &net;
    lua.playerHumanoid = &hum;
    lua.playerPos = &playerPos;
    lua.playerYaw = &playerYaw;
    lua.nav = &nav;
    lua.tags = &tags;
    lua.classifica = &classifica;
    lua.proximity = &proximity;
    lua.runIsClient = true;
    lua.runIsServer = true;
    if (!lua.init(&dm)) { std::printf("[api] runtime: %s\n", lua.lastError.c_str()); return 1; }
    if (!lua.runString("PROBE_DATA = (function()\n" + ss.str() + "\nend)()")) {
        std::printf("[api] dati: %s\n", lua.lastError.c_str());
        return 1;
    }
    if (!lua.runString(kProbe)) { std::printf("[api] sonda: %s\n", lua.lastError.c_str()); return 1; }
    lua.runString("PROBE_COUNT = #PROBE_DATA.classes");
    const int count = std::atoi(lua.evalWatch("PROBE_COUNT").c_str());

    std::vector<ClassResult> res;
    for (int i = 1; i <= count; ++i) {
        lua.runString("LAST = nil");
        if (!lua.runString("PROBE_CLASS(" + std::to_string(i) + ")")) {
            ClassResult r;
            r.name = "#" + std::to_string(i);
            r.why = "errore della sonda: " + lua.lastError;
            res.push_back(r);
            continue;
        }
        const auto f = split(readGlobal(lua, "LAST"), '\t');
        if (f.size() < 11) continue;
        ClassResult r;
        r.name = f[0];
        r.obtained = f[1] == "1";
        r.why = f[2];
        r.np = std::atoi(f[3].c_str()); r.op = std::atoi(f[4].c_str());
        r.nf = std::atoi(f[5].c_str()); r.of = std::atoi(f[6].c_str());
        r.ne = std::atoi(f[7].c_str()); r.oe = std::atoi(f[8].c_str());
        r.nref = std::atoi(f[9].c_str());
        r.missing = f[10];
        r.gen = f.size() > 11 ? std::atoi(f[11].c_str()) : 0;
        res.push_back(r);
    }
    lua.runString("PROBE_ENUMS()");
    const auto ef = split(readGlobal(lua, "LAST"), '\t');
    const int enumTot = ef.size() >= 2 ? std::atoi(ef[0].c_str()) : 0;
    const int enumOk = ef.size() >= 2 ? std::atoi(ef[1].c_str()) : 0;

    int np = 0, op = 0, nf = 0, of = 0, ne = 0, oe = 0, nref = 0, classesOk = 0, classesAny = 0, gen = 0;
    for (const auto& r : res) {
        np += r.np; op += r.op; nf += r.nf; of += r.of; ne += r.ne; oe += r.oe; nref += r.nref; gen += r.gen;
        classesOk += r.obtained;
        classesAny += r.ok() > 0;
    }
    const int tot = np + nf + ne, ok = op + of + oe;

    // Le classi che usano quasi tutti i giochi: prima nel rapporto.
    static const char* kPopular[] = {
        "Instance", "BasePart", "Part", "Model", "PVInstance", "Humanoid", "Players", "Player", "Workspace", "DataModel",
        "RunService", "UserInputService", "TweenService", "ReplicatedStorage", "Camera", "Sound", "SoundService",
        "Lighting", "Debris", "CollectionService", "HttpService", "DataStoreService", "GlobalDataStore", "RemoteEvent",
        "RemoteFunction", "BindableEvent", "BindableFunction", "Tool", "Animator", "Animation", "AnimationTrack",
        "ScreenGui", "GuiObject", "Frame", "TextLabel", "TextButton", "ImageLabel", "ImageButton", "TextBox",
        "BillboardGui", "SurfaceGui", "UIListLayout", "UICorner", "ProximityPrompt", "ClickDetector", "Attachment",
        "WeldConstraint", "Weld", "Motor6D", "ParticleEmitter", "Beam", "Trail", "PointLight", "SpotLight", "Highlight",
        "Explosion", "Team", "Teams", "MarketplaceService", "TeleportService", "PathfindingService", "Path",
        "ContextActionService", "StarterGui", "TextChatService", "MeshPart", "SpawnLocation", "Seat", "VehicleSeat",
        "IntValue", "NumberValue", "StringValue", "BoolValue", "ObjectValue", "Vector3Value", "CFrameValue", "Folder",
    };
    std::set<std::string> popular(std::begin(kPopular), std::end(kPopular));

    std::ostringstream md;
    md << "# Copertura dell'API di Roblox in Blocky\n\n"
       << "Generato da `test_api_coverage` (tools/roblox_api): ogni membro provato davvero nel Luau del motore.\n"
       << "Rifallo con `node tools/roblox_api/gen_probe.mjs` e `build/bin/test_api_coverage.exe`.\n\n"
       << "| | Ci sono | Totale | Copertura |\n|---|---|---|---|\n"
       << "| **Funzionante** (fa davvero la cosa) | " << (ok - gen) << " | " << tot << " | **" << pct(ok - gen, tot) << "** |\n"
       << "| **Presente** (non fa crashare lo script; comprende lo strato generato) | " << ok << " | " << tot << " | **" << pct(ok, tot) << "** |\n"
       << "| Proprieta' | " << op << " | " << np << " | " << pct(op, np) << " |\n"
       << "| Metodi | " << of << " | " << nf << " | " << pct(of, nf) << " |\n"
       << "| Eventi | " << oe << " | " << ne << " | " << pct(oe, ne) << " |\n"
       << "| Enum | " << enumOk << " | " << enumTot << " | " << pct(enumOk, enumTot) << " |\n"
       << "| Classi che si possono ottenere | " << classesOk << " | " << res.size() << " | " << pct(classesOk, (int)res.size()) << " |\n\n"
       << "Proprieta' che puntano a un oggetto (non verificabili leggendole): " << nref << ".\n\n";
    auto table = [&](bool pop) {
        md << "| Classe | Funzionante | Presente | Proprieta' | Metodi | Eventi | Cosa manca |\n|---|---|---|---|---|---|---|\n";
        std::vector<const ClassResult*> list;
        for (const auto& r : res)
            if (popular.count(r.name) == (size_t)pop) list.push_back(&r);
        std::sort(list.begin(), list.end(), [](const ClassResult* a, const ClassResult* b) {
            const double pa = a->total() ? (double)a->ok() / a->total() : 1.0, pb = b->total() ? (double)b->ok() / b->total() : 1.0;
            return pa != pb ? pa < pb : a->name < b->name;
        });
        for (const ClassResult* r : list) {
            std::string miss = r->obtained ? r->missing : ("_" + r->why + "_");
            if (miss.size() > 400) miss = miss.substr(0, 400) + "...";
            md << "| " << r->name << " | " << pct(r->working(), r->total()) << " | " << pct(r->ok(), r->total()) << " | " << r->op << "/" << r->np << " | "
               << r->of << "/" << r->nf << " | " << r->oe << "/" << r->ne << " | " << miss << " |\n";
        }
        md << "\n";
    };
    md << "## Classi piu' usate dai giochi\n\n";
    table(true);
    md << "## Tutte le altre\n\n";
    table(false);
    if (ef.size() >= 3 && !ef[2].empty()) md << "## Enum mancanti\n\n" << ef[2] << "\n";

    std::filesystem::create_directories(root + "/docs");
    std::ofstream(root + "/docs/api_coverage.md", std::ios::binary) << md.str();

    int popTot = 0, popOk = 0, popGen = 0;
    for (const auto& r : res)
        if (popular.count(r.name)) { popTot += r.total(); popOk += r.ok(); popGen += r.gen; }
    std::printf("[api] API di Roblox in Blocky: funzionante %s, presente %s (%d + %d dallo strato generato, su %d)\n",
                pct(ok - gen, tot).c_str(), pct(ok, tot).c_str(), ok - gen, gen, tot);
    std::printf("[api]   proprieta' %s, metodi %s, eventi %s, enum %s\n", pct(op, np).c_str(), pct(of, nf).c_str(),
                pct(oe, ne).c_str(), pct(enumOk, enumTot).c_str());
    std::printf("[api]   classi piu' usate dai giochi: funzionante %s, presente %s (su %d)\n", pct(popOk - popGen, popTot).c_str(),
                pct(popOk, popTot).c_str(), popTot);
    std::printf("[api]   classi che si possono ottenere: %d su %zu, con almeno qualcosa: %d\n", classesOk, res.size(), classesAny);
    std::printf("[api] rapporto: docs/api_coverage.md\n");
    if (minPct >= 0.0 && (tot == 0 || 100.0 * ok / tot < minPct)) {
        std::printf("[api] sotto il minimo (%.1f%%)\n", minPct);
        return 1;
    }
    return 0;
}
