#pragma once

#include "lua.h"
#include "lualib.h"
#include <Luau/Compiler.h>

#include "BrickColor.h"
#include "DataModel.h"
#include "PartPropsLua.h"
#include "PartConstraints.h"
#include "DataStore.h"
#include "GameHub.h"
#include "Humanoid.h"
#include "NavGrid.h"
#include "SoundService.h"
#include "Animator.h"
#include "LightingService.h"
#include "systems/Tag.h"
#include "render/Particles.h"
#include "SafeBytecode.h"
#include "ScriptControls.h"
#include "AnimLibrary.h"
#include "LanNet.h"
#include "LuaScriptSecurity.h"
#include "systems/Terrain.h"
#include "systems/Classifica.h"
#include "systems/Prossimita.h"
#include "engine/StudioProApis.h"
#include "engine/LuaPrelude.h"
#include "engine/anim/AnimIO.h"
#include "engine/anim/CharacterAnimator.h"
#include "platform/CrashHandler.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>

// blockyPushCFunction e blockyLuaError sono definiti in LuaScriptSecurity.h
// (incluso sopra) perche' quel file ne ha bisogno prima che questa classe
// esista.

class LuaRuntime {
public:
    // API Lua dei sistemi creati da tools/genera.py: ognuno crea la sua
    // tabella globale (es. "Classifica"), protetta come quelle del motore.
    struct Extension {
        const char* global;
        void (*registerLua)(lua_State* L, void* ctx);
        void* ctx;
    };
    std::vector<Extension> extensions;

    DataModel* workspace = nullptr;
    SoundService* soundService = nullptr;
    Animator* animator = nullptr;
    LightingService* lighting = nullptr;
    ParticleSystem* particles = nullptr;
    LanNet* net = nullptr;
    Humanoid* playerHumanoid = nullptr;
    glm::vec3* playerPos = nullptr;
    NavGrid* nav = nullptr;
    void (*navRebuild)() = nullptr;
    void (*cameraShake)(float magnitudeDeg, float durationSec) = nullptr;
    float* playerYaw = nullptr;
    // Raggio mondo dal pixel del mouse (o dal centro schermo se il cursore
    // e' locked). Origin = near plane / camera, dir normalizzata.
    bool (*getAimRay)(glm::vec3& origin, glm::vec3& dir) = nullptr;
    struct VoxelHit {
        bool ok = false;
        glm::vec3 pos{0};
        glm::vec3 normal{0, 1, 0};
        float dist = 0;
        int block = 0;
        const char* material = "Air";
    };
    bool (*voxelRaycast)(const glm::vec3& origin, const glm::vec3& dir,
                         float maxDist, VoxelHit& out) = nullptr;
    Tag* tags = nullptr;
    Classifica* classifica = nullptr;
    Prossimita* proximity = nullptr;
    float* cameraFov = nullptr;       // -> g_cam.fov
    bool* firstPersonFlag = nullptr;  // -> g_firstPerson
    int cameraType = 0; // 0 Custom · 1 Scriptable · 2 FirstPerson
    bool runIsStudio = false;
    bool runIsClient = false;
    bool runIsServer = true;

    struct ScriptParticleEmitter {
        int partIndex = -1;
        float rate = 8.f;
        float accum = 0.f;
        bool enabled = true;
        int preset = 0; // ParticlePreset
        glm::vec3 color{-1, -1, -1};
    };
    std::vector<ScriptParticleEmitter> scriptEmitters_;
    std::vector<std::string> printLog;
    std::string lastError;
    bool ok = false;

    // Script Performance: tempo speso dentro ogni script (per nome di chunk,
    // es. "Porta.Script" o il percorso del file), misurato su ogni chiamata
    // che il motore fa verso Lua (avvio, Heartbeat, eventi, Touched...).
    struct ScriptCost {
        double totalMs = 0.0;
        double windowMs = 0.0;     // secondo in corso
        double lastSecMs = 0.0;    // ultimo secondo completo = "attivita'" come in Roblox
        double peakMs = 0.0;       // chiamata singola piu' lenta
        long calls = 0;
        long lastSecCalls = 0, windowCalls = 0;
    };
    std::map<std::string, ScriptCost> scriptCosts;

    // Memoria usata da Lua (KB), per la Developer Console.
    int memoryKb() const { return L ? lua_gc(L, LUA_GCCOUNT, 0) : 0; }

    // game.PlaceId / game.JobId: quale gioco e quale server. Il JobId del
    // client arriva dopo l'avvio degli script (nel Welcome), quindi si
    // aggiorna anche a Lua gia' partito.
    std::string placeId, jobId;

    void setGameIdentity(const std::string& place, const std::string& job) {
        placeId = place;
        jobId = job;
        if (!L) return;
        lua_getglobal(L, "game");
        if (lua_istable(L, -1)) writeGameIdentity(-1);
        lua_pop(L, 1);
    }

    void fireTeleportFailed(const char* reason, const char* message, const char* place) {
        studio_pro_apis::fireTeleportInitFailed(L, reason, message, place);
    }

    // ---- Animator del personaggio (Humanoid:LoadAnimation) ----

    // Rig R6 sulle misure del player: le animazioni dell'Animation Studio (rig
    // "Inserisci Rig R6") ci girano osso per osso.
    blk::anim::CharacterAnimator charAnim;

    void setupCharacterRig() {
        if (charAnim.ready()) return;
        std::vector<blk::anim::PartBox> body;
        for (const auto& d : Humanoid::createR6Blocky().parts)
            if (d.name != "HumanoidRootPart") body.push_back({d.name, d.offset, d.size, glm::quat(1.f, 0.f, 0.f, 0.f)});
        charAnim.setupFromParts(body);
    }

    /**
     * Mette la posa delle animazioni nelle parti del corpo del player (le
     * disegna BlockPlayer). Le parti senza animazione restano al motore.
     */
    void applyCharacterAnimation(Humanoid& h) {
        if (!charAnim.ready()) return;
        std::vector<float> boneW;
        const blk::anim::Pose p = charAnim.pose(&boneW);
        const std::vector<blk::anim::Transform> parts = charAnim.partTransforms(p);
        for (size_t i = 0; i < charAnim.binds.size() && i < parts.size(); ++i) {
            const auto& b = charAnim.binds[i];
            const float w = b.bone >= 0 && b.bone < (int)boneW.size() ? boneW[(size_t)b.bone] : 0.f;
            for (auto& part : h.parts) {
                if (part.name != b.part) continue;
                if (w <= 1e-4f) {
                    if (part.animWeight > 0.f) { part.anim = RigPose{}; part.animWeight = 0.f; }
                    break;
                }
                glm::vec3 pos, eul;
                blk::anim::toBlockPartPose(parts[i], part.offset, part.pivot, pos, eul);
                part.anim.pos = pos;
                part.anim.rot = eul;
                part.anim.scale = glm::vec3(1.f);
                part.animWeight = w;
                break;
            }
        }
    }

    /** Ogni frame (da fireHeartbeat): tempo delle tracce ed eventi agli script. */
    void tickCharacterAnimation(float dt) {
        if (!L || !charAnim.ready()) return;
        std::vector<blk::anim::TrackEvent> events;
        charAnim.update(dt, events);
        for (const auto& e : events) {
            const auto it = trackRefs_.find(e.track);
            if (it == trackRefs_.end()) continue;
            lua_getref(L, it->second);
            const int t = lua_gettop(L);
            auto fire = [&](const char* sigField, const char* arg) {
                lua_getfield(L, t, sigField);
                if (lua_istable(L, -1)) fireLuaSignal(-1, arg);
                lua_pop(L, 1);
            };
            switch (e.type) {
            case blk::anim::TrackEventType::Stopped: fire("Stopped", nullptr); break;
            case blk::anim::TrackEventType::Ended: fire("Ended", nullptr); break;
            case blk::anim::TrackEventType::DidLoop: fire("DidLoop", nullptr); break;
            case blk::anim::TrackEventType::MarkerReached:
                fire("KeyframeReached", e.name.c_str());
                lua_getfield(L, t, "_markers");
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, e.name.c_str());
                    if (lua_istable(L, -1)) fireLuaSignal(-1, e.param.c_str());
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
                break;
            }
            lua_settop(L, t - 1);
        }
    }

    // ---- Watch (Studio): variabili degli script mentre il gioco gira ----

    // Un valore Lua come testo breve: tabelle con i primi campi, il resto per tipo.
    std::string describeValue(int idx, int depth = 0) const {
        idx = lua_absindex(L, idx);
        char buf[96];
        switch (lua_type(L, idx)) {
        case LUA_TNIL:     return "nil";
        case LUA_TBOOLEAN: return lua_toboolean(L, idx) ? "true" : "false";
        case LUA_TNUMBER:  std::snprintf(buf, sizeof(buf), "%.6g", lua_tonumber(L, idx)); return buf;
        case LUA_TSTRING: {
            std::string s = lua_tostring(L, idx);
            if (s.size() > 60) s = s.substr(0, 57) + "...";
            return "\"" + s + "\"";
        }
        case LUA_TFUNCTION: return "function";
        case LUA_TTABLE: {
            if (depth >= 1) return "{...}";
            std::string out = "{";
            int n = 0;
            lua_pushnil(L);
            while (lua_next(L, idx) != 0) {
                if (n < 6) {
                    lua_pushvalue(L, -2);
                    out += (n ? ", " : "") + describeValue(-1, depth + 1) + "=" + describeValue(-2, depth + 1);
                    lua_pop(L, 1);
                }
                ++n;
                lua_pop(L, 1);
            }
            if (n > 6) out += ", ... (" + std::to_string(n) + " campi)";
            return out + "}";
        }
        default: return lua_typename(L, lua_type(L, idx));
        }
    }

    // Variabili globali create dagli script (non le API del motore), ordinate.
    std::vector<std::pair<std::string, std::string>> userGlobals(size_t max = 200) const {
        std::vector<std::pair<std::string, std::string>> out;
        if (!L) return out;
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0 && out.size() < max) {
            if (lua_type(L, -2) == LUA_TSTRING) {
                const char* k = lua_tostring(L, -2);
                if (!apiGlobals_.count(k) && lua_type(L, -1) != LUA_TFUNCTION)
                    out.push_back({k, describeValue(-1)});
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        std::sort(out.begin(), out.end());
        return out;
    }

    // Valuta un'espressione ("punti", "Classifica.Get('Monete')") e la
    // restituisce come testo, o l'errore. Ha lo stesso tempo massimo degli script.
    std::string evalWatch(const std::string& expr) {
        if (!L || expr.empty()) return "";
        const std::string bc = Luau::compile("return (" + expr + ")");
        if (luau_load(L, "=Watch", bc.data(), bc.size(), 0) != 0) {
            std::string err = safeToString(-1);
            lua_pop(L, 1);
            return "errore: " + err;
        }
        callStart_ = std::chrono::steady_clock::now();
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            std::string err = safeToString(-1);
            lua_pop(L, 1);
            return "errore: " + err;
        }
        std::string v = describeValue(-1);
        lua_pop(L, 1);
        return v;
    }

    // Da chiamare una volta al secondo: chiude la finestra di misura.
    void rollScriptCosts() {
        for (auto& kv : scriptCosts) {
            kv.second.lastSecMs = kv.second.windowMs;
            kv.second.lastSecCalls = kv.second.windowCalls;
            kv.second.windowMs = 0.0;
            kv.second.windowCalls = 0;
        }
    }

    std::vector<uint32_t> scriptHashes;
    std::vector<std::string> scriptOrigins;
    bool scriptSetChanged = false;

    static uint32_t hashChunk(const void* data, size_t n) {
        const unsigned char* p = (const unsigned char*)data;
        uint32_t h = 2166136261u;
        for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
        return h ? h : 1u;
    }

    void noteScript(const void* data, size_t n, const std::string& origin) {
        if (!data || !n) return;
        uint32_t h = hashChunk(data, n);
        for (uint32_t x : scriptHashes)
            if (x == h) return;
        if (scriptHashes.size() >= 256) return;
        scriptHashes.push_back(h);
        scriptOrigins.push_back(origin);
        scriptSetChanged = true;
    }

    void clearScriptRegistry() {
        scriptHashes.clear();
        scriptOrigins.clear();
        scriptSetChanged = true;
    }
    int budgetUs = 4000;
    bool budgetExceeded = false;
    bool requireSealedScripts = false;
    // Quando e' iniziata la chiamata Lua in corso — impostato all'inizio
    // di ogni ingresso (runString/runFile/runBytecode/ogni heartbeat) e
    // controllato dal callback interrupt per il limite duro anti-loop.
    std::chrono::steady_clock::time_point callStart_ = std::chrono::steady_clock::now();
    // true SOLO durante init()/registerApi()/openSandboxLibs(): quelle
    // funzioni allocano decine di tabelle/closure fuori da qualsiasi
    // lua_pcall, e il GC incrementale di Luau chiama il callback interrupt
    // anche durante una semplice allocazione (lgc.cpp, non solo durante
    // l'esecuzione di bytecode). Con "callStart_" ancora fermo al momento
    // della costruzione dell'oggetto (secondi/minuti prima), il controllo
    // budget scattava SUBITO e chiamava luaL_error senza nessun pcall che
    // lo intercettasse — la causa reale dello STACK_OVERFLOW in Test.
    bool initializing_ = false;

    struct TaskWait {
        int coRef = LUA_NOREF;
        float remaining = 0.f;
        float total = 0.f;  // per restituire a task.wait il tempo atteso
        // -1: ripresa da task.wait (le si passa il tempo atteso). >= 0: thread
        // nuovo di task.delay/defer, con funzione e argomenti gia' sul suo stack.
        int nargs = -1;
    };
    // WaitForChild yield: riprende la coroutine quando il figlio esiste o al timeout.
    struct ChildWait {
        int coRef = LUA_NOREF;
        float remaining = 5.f;
        int kind = 0; // 0=workspace, 1=service, 2=shared, 3=part
        int parentOrSvc = -1;
        std::string name;
    };

    ~LuaRuntime() { shutdown(); }

    // Sostituisce il vecchio lua_sethook(..., LUA_MASKCOUNT, ...) di Lua
    // 5.4: Luau non conta istruzioni, chiama questo callback ai safepoint
    // (salti di loop, chiamate, GC). Stesso messaggio di errore di prima
    // cosi' il controllo "lastError.find(budget)" altrove resta valido.
    //
    // NON usare fromState(ls) qui: fa lua_getfield/lua_pop sul registry, e
    // l'interrupt scatta in mezzo all'esecuzione (call/ret/loop), non con
    // un frame di chiamata pulito come le altre funzioni C — toccare lo
    // stack in quel momento corrompe il VM (crash con SIGILL alla prima
    // chiamata qualsiasi, anche un semplice print). Per questo lua_Callbacks
    // ha un campo "userdata" pensato apposta: nessun accesso allo stack.
    //
    // Il parametro "gc" e' la vera guardia strutturale: nel sorgente di
    // Luau vale -1 SOLO quando l'interrupt scatta da un safepoint di
    // bytecode reale (lvmexecute.cpp — dentro una chiamata protetta con
    // callStart_ appena impostato), mentre vale >=0 quando scatta da un
    // passo del GC innescato da una semplice allocazione (lgc.cpp) — cosa
    // che puo' succedere OVUNQUE nel motore (registerApi, bindScriptParent,
    // qualunque futura funzione che fa lua_newtable/pushcfunction), non
    // solo mentre uno script gira dentro pcall. Con callStart_ ancora
    // "vecchio" (mai resettato dall'ultimo runCompiled/fireHeartbeat), un
    // interrupt da GC in uno di questi punti faceva scattare subito
    // l'errore Lua FUORI da qualunque pcall che potesse intercettarlo —
    // la causa reale degli STACK_OVERFLOW visti in Test. Ignorare i gc>=0
    // rende il controllo budget sicuro ovunque, anche in codice futuro,
    // senza dover ricordarsi di resettare callStart_ in ogni nuovo punto.
    static void interruptCallback(lua_State* ls, int gc) {
        if (gc >= 0) return;
        LuaRuntime* rt = static_cast<LuaRuntime*>(lua_callbacks(ls)->userdata);
        if (!rt || rt->initializing_) return;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - rt->callStart_).count();
        if (elapsed > LuaScriptSecurity::kScriptHardTimeoutMs) {
            // L'interrupt arriva anche con lo stack pieno fino al limite del
            // frame; luaL_error ci aggiunge "riga: " + messaggio, e senza
            // spazio Luau si ferma con un'asserzione (SIGILL: crash di tutto
            // il processo invece di fermare solo lo script).
            lua_checkstack(ls, 4);
            blockyLuaError(ls, "Lua budget: troppe istruzioni (script fermato)");
        }
    }

    // ---- Adattatori Luau: stesso nome/comportamento delle funzioni di
    // Lua 5.4 che questo file chiamava ovunque, cosi' le ~160 chiamate
    // gia' scritte restano quelle, solo rinominate in blocco. ----
    //
    // luaL_error di Luau e' void (macro luaL_errorL) invece che int come
    // in Lua 5.4 — "return blockyLuaError(...)" ovunque nel file smetteva di
    // compilare. Questo adattatore torna a restituire int (mai davvero,
    // dato che dentro solleva comunque l'errore Lua che salta fuori).
    template <typename... Args>
    static int blockyLuaError(lua_State* ls, const char* fmt, Args... args) {
        // -Wformat-security (errore di default con l'NDK Android): il
        // compilatore non riesce a vedere attraverso il template che `fmt`
        // e' sempre un letterale ai punti di chiamata — nessuna stringa di
        // formato arriva mai da dati esterni o da uno script.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#endif
        luaL_error(ls, fmt, args...);
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        return 0;
    }

    // luaL_ref(L, LUA_REGISTRYINDEX) di Lua 5.4 referenzia il valore in
    // cima allo stack E LO RIMUOVE. Il lua_ref(L, idx) di Luau referenzia
    // il valore all'indice idx ma NON fa il pop da solo, e non accetta
    // LUA_REGISTRYINDEX come idx (solo come destinazione implicita).
    static int blockyLuaRef(lua_State* ls) {
        int ref = lua_ref(ls, -1);
        lua_pop(ls, 1);
        return ref;
    }

    void openSandboxLibs() {
        // Luau non ha luaL_requiref. Il sorgente di Luau stesso (linit.cpp,
        // luaL_openlibs) NON chiama le luaopen_xxx come funzioni C dirette:
        // le pusha come closure + nome argomento e le invoca con
        // lua_call(L, 1, 0), cioe' PASSANDO DAL VM. lua_call imposta la
        // contabilita' interna (call-info, contatore di ricorsione C, ecc.)
        // che il GC incrementale e le chiamate successive si aspettano —
        // chiamarle dirette come feci prima salta quel setup e lascia lo
        // stato interno incoerente in un modo che non crasha subito ma
        // esplode piu' tardi come STACK_OVERFLOW nativo (visto in test reali,
        // non riprodotto in un harness piccolo perche' dipende dal passo
        // del GC). Replichiamo esattamente il pattern di linit.cpp.
        auto openLib = [&](lua_CFunction fn, const char* name) {
            lua_pushcfunction(L, fn, name);
            lua_pushstring(L, name);
            lua_call(L, 1, 0);
        };
        openLib(luaopen_base, "_G");
        openLib(luaopen_table, LUA_TABLIBNAME);
        openLib(luaopen_string, LUA_STRLIBNAME);
        openLib(luaopen_math, LUA_MATHLIBNAME);
        openLib(luaopen_utf8, LUA_UTF8LIBNAME);
        openLib(luaopen_coroutine, LUA_COLIBNAME);
        // Come Roblox: bit32 e os (in Luau os ha solo clock/date/time/difftime).
        openLib(luaopen_bit32, LUA_BITLIBNAME);
        openLib(luaopen_os, LUA_OSLIBNAME);
        lua_pushnil(L); lua_setglobal(L, "dofile");
        lua_pushnil(L); lua_setglobal(L, "loadfile");
        lua_pushnil(L); lua_setglobal(L, "load");
        blockyPushCFunction(L, luaRequire);
        lua_setglobal(L, "require");
        lua_pushnil(L); lua_setglobal(L, "collectgarbage");
        // rawset c'e' (come Roblox) ma non sugli oggetti del motore.
        blockyPushCFunction(L, luaSafeRawset);
        lua_setglobal(L, "rawset");
        // setmetatable/getmetatable servono a tutta la programmazione a oggetti
        // degli script Roblox (Class.__index = Class): ci sono, ma non toccano
        // le metatabelle del motore (vedi luaSafeSetmetatable).
        blockyPushCFunction(L, luaSafeSetmetatable);
        lua_setglobal(L, "setmetatable");
        blockyPushCFunction(L, luaSafeGetmetatable);
        lua_setglobal(L, "getmetatable");
        lua_getglobal(L, "string");
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            lua_setfield(L, -2, "dump");
        }
        lua_pop(L, 1);
    }

    bool init(DataModel* ws) {
        CrashHandler::note("LuaRuntime::init: inizio");
        shutdown();
        workspace = ws;
        scriptCosts.clear();
        scriptControls().workspaceGravity = 80.f;   // ogni gioco riparte dalla gravita' normale
        L = luaL_newstate();
        if (!L) {
            lastError = "luaL_newstate fallita";
            return false;
        }
        initializing_ = true;
        CrashHandler::note("LuaRuntime::init: pre openSandboxLibs");
        openSandboxLibs();
        CrashHandler::note("LuaRuntime::init: post openSandboxLibs");
        lua_callbacks(L)->userdata = this;
        lua_callbacks(L)->interrupt = interruptCallback;
        // Prelude Luau (Signal, CFrame, Random...) PRIMA di registerApi: le
        // tabelle degli eventi create li dentro usano gia la sua Connect.
        loadPrelude();
        CrashHandler::note("LuaRuntime::init: pre registerApi");
        if (!registerApi()) {
            shutdown();
            return false;
        }
        CrashHandler::note("LuaRuntime::init: post registerApi");
        // La parte del prelude che usa Vector3 e i servizi, prima delle
        // estensioni e del blocco anti-executor, che la proteggono.
        installPrelude();
        for (const Extension& e : extensions)
            e.registerLua(L, e.ctx);
        // Metatable anti-executor PRIMA di lua_setreadonly: su Luau non si puo'
        // setmetatable su tabelle gia' readonly (crash "attempt to modify...").
        LuaScriptSecurity::lockApiSurface(L);
        // Tutto cio' che esiste adesso e' API del motore: Watch mostra solo
        // le globali che gli script creano dopo.
        apiGlobals_.clear();
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_type(L, -2) == LUA_TSTRING) apiGlobals_.insert(lua_tostring(L, -2));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        for (const Extension& e : extensions) {
            lua_getglobal(L, e.global);
            if (lua_istable(L, -1)) lua_setreadonly(L, -1, true);
            lua_pop(L, 1);
        }
        CrashHandler::note("LuaRuntime::init: post lockApiSurface");
        initializing_ = false;
        ok = true;
        lastError.clear();
        budgetExceeded = false;
        return true;
    }

    void shutdown() {
        if (L) {
            // Prima di chiudere: via TUTTI gli eventi collegati (Touched,
            // MouseClick, PlayerAdded...) finche' i loro numeri di registro
            // sono ancora di questo runtime. Prima restavano negli elenchi e,
            // col runtime nuovo, gli stessi numeri indicavano funzioni nuove:
            // un vecchio Touched poteva chiamare il MouseClick di un altro script.
            clearHeartbeats();
            unrefRunServiceListeners(heartbeatRefs_);
            unrefRunServiceListeners(steppedRefs_);
            unrefRunServiceListeners(renderSteppedRefs_);
            for (auto& w : taskWaits_) {
                if (w.coRef != LUA_NOREF)
                    lua_unref(L,w.coRef);
            }
            taskWaits_.clear();
            for (auto& kv : moduleCache_) {
                if (kv.second != LUA_NOREF)
                    lua_unref(L, kv.second);
            }
            moduleCache_.clear();
            requireStack_.clear();
            lua_close(L);
            L = nullptr;
        }
        initializing_ = false;
        ok = false;
    }

    // Compila con Luau::compile ed esegue — sostituisce il vecchio
    // luaL_dostring/luaL_dofile di Lua 5.4, che parlavano al parser
    // interno in un solo passo. Con Luau il passo di compilazione e'
    // esplicito (produce bytecode), poi luau_load lo carica, poi
    // lua_pcall lo esegue — stesso risultato finale, un passo in piu'.
    bool runCompiled(const std::string& source, const std::string& chunkname) {
        if (!L) return false;
        const std::string bc = Luau::compile(source);
        if (luau_load(L, chunkname.c_str(), bc.data(), bc.size(), 0) != 0) {
            lastError = safeToString(-1);
            std::fprintf(stderr, "Lua error: %s\n", lastError.c_str());
            lua_pop(L, 1);
            return false;
        }
        // Il corpo dello script gira in una coroutine, come in Roblox: cosi'
        // task.wait(), WaitForChild e simili funzionano anche fuori da
        // task.spawn (prima: "task.wait() va usato dentro task.spawn").
        // Se aspetta, lo riprende lo scheduler di task (taskWaits_).
        lua_State* co = lua_newthread(L);          // L: [fn, co]
        lua_pushvalue(L, -2);
        lua_xmove(L, co, 1);                       // co: [fn]
        callStart_ = std::chrono::steady_clock::now();
        const auto t0 = callStart_;
        const int st = lua_resume(co, L, 0);
        {
            ScriptCost& c = scriptCosts[chunkname.size() > 1 && chunkname[0] == '=' ? chunkname.substr(1) : chunkname];
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            c.totalMs += ms;
            c.windowMs += ms;
            c.peakMs = std::max(c.peakMs, ms);
            ++c.calls;
        }
        if (st != LUA_OK && st != LUA_YIELD) {
            const char* err = lua_tostring(co, -1);
            lastError = err ? err : "errore";
            std::fprintf(stderr, "Lua error: %s\n", lastError.c_str());
            lua_pop(L, 2);
            return false;
        }
        if (st == LUA_OK) lua_settop(co, 0);       // valori restituiti: non servono
        lua_pop(L, 2);
        return true;
    }

    bool runFile(const std::string& path) {
        if (!L) return false;
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            lastError = "impossibile aprire " + path;
            std::fprintf(stderr, "Lua error: %s\n", lastError.c_str());
            return false;
        }
        std::string src((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        noteScript(src.data(), src.size(), path);
        return runCompiled(src, "=" + path);
    }

    bool runString(const std::string& code) {
        if (!L) return false;
        noteScript(code.data(), code.size(), "string");
        return runCompiled(code, "=string");
    }

    bool runPartScript(const std::string& code, int partIndex) {
        if (!L || !workspace) return false;
        if (partIndex < 0 || partIndex >= (int)workspace->parts.size()) {
            lastError = "Part index non valido";
            return false;
        }
        bindScriptParent(partIndex);
        // Nome vero nel chunk: errori e Script Performance dicono quale part.
        noteScript(code.data(), code.size(), "part");
        return runCompiled(code, "=" + workspace->parts[(size_t)partIndex].name + ".Script");
    }

    bool runModelScript(const std::string& code, int modelIndex) {
        if (!L || !workspace) return false;
        if (modelIndex < 0 || modelIndex >= (int)workspace->models.size()) {
            lastError = "Model index non valido";
            return false;
        }
        bindModelScriptParent(modelIndex);
        return runString(code);
    }

    bool runLabelScript(const std::string& code, int labelIndex) {
        if (!L || !workspace) return false;
        if (labelIndex < 0 || labelIndex >= (int)workspace->labels.size()) {
            lastError = "Label index non valido";
            return false;
        }
        bindLabelScriptParent(labelIndex);
        return runString(code);
    }

    bool runSharedScript(const std::string& code, int sharedIndex) {
        if (!L || !workspace) return false;
        if (sharedIndex < 0 || sharedIndex >= (int)workspace->instances.size()) {
            lastError = "SharedInstance index non valido";
            return false;
        }
        const auto& s = workspace->instances[sharedIndex];
        lua_newtable(L);
        lua_pushstring(L, s.name.c_str());
        lua_setfield(L, -2, "Name");
        lua_pushstring(L, s.className.c_str());
        lua_setfield(L, -2, "ClassName");
        lua_pushinteger(L, sharedIndex);
        lua_setfield(L, -2, "__sharedIndex");
        lua_setglobal(L, "script");
        return runString(code);
    }

    void bindLabelScriptParent(int labelIndex) {
        if (!L) return;
        lua_newtable(L);
        pushLiveLabel(L, labelIndex);
        lua_setfield(L, -2, "Parent");
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        lua_pushstring(L, "script");
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        lua_pop(L, 2);
    }

    void bindModelScriptParent(int modelIndex) {
        if (!L) return;
        lua_newtable(L);
        pushLiveModel(L, modelIndex);
        lua_setfield(L, -2, "Parent");
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        lua_pushstring(L, "script");
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        lua_pop(L, 2);
    }

    void bindScriptParent(int partIndex) {
        if (!L) return;
        lua_newtable(L);
        pushLivePart(L, partIndex);
        lua_setfield(L, -2, "Parent");
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        lua_pushstring(L, "script");
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);
        lua_pop(L, 2);
    }

    void clearHeartbeats() {
        if (!L) return;
        unrefRunServiceListeners(heartbeatRefs_);
        unrefRunServiceListeners(steppedRefs_);
        unrefRunServiceListeners(renderSteppedRefs_);
        pendingDestroy_.clear();
        for (auto& w : taskWaits_) {
            if (L && w.coRef != LUA_NOREF)
                lua_unref(L,w.coRef);
        }
        taskWaits_.clear();
        for (auto& w : childWaits_) {
            if (L && w.coRef != LUA_NOREF)
                lua_unref(L, w.coRef);
        }
        childWaits_.clear();
        auto unrefMap = [&](std::map<int, std::vector<int>>& m) {
            for (auto& kv : m)
                for (int ref : kv.second)
                    if (ref != LUA_NOREF) lua_unref(L,ref);
            m.clear();
        };
        auto unrefMapS = [&](std::map<std::string, std::vector<int>>& m) {
            for (auto& kv : m)
                for (int ref : kv.second)
                    if (ref != LUA_NOREF) lua_unref(L,ref);
            m.clear();
        };
        unrefMap(scriptControls().partTouched);
        unrefMap(scriptControls().partClicked);
        unrefMap(scriptControls().partChildAdded);
        unrefMap(scriptControls().sharedChildAdded);
        unrefMapS(scriptControls().remoteServer);
        unrefMapS(scriptControls().remoteClient);
        unrefMapS(scriptControls().guiButtonActivated);
        unrefMapS(scriptControls().toolEquipped);
        unrefMapS(scriptControls().toolUnequipped);
        unrefMapS(scriptControls().toolActivated);
        unrefMapS(scriptControls().tagAdded);
        unrefMapS(scriptControls().tagRemoved);
        unrefMapS(scriptControls().attributeChanged);
        for (int ref : scriptControls().playerRemoving)
            if (ref != LUA_NOREF) lua_unref(L, ref);
        scriptControls().playerRemoving.clear();
        for (int ref : scriptControls().inputBegan)
            if (ref != LUA_NOREF) lua_unref(L, ref);
        scriptControls().inputBegan.clear();
        for (int ref : scriptControls().inputEnded)
            if (ref != LUA_NOREF) lua_unref(L, ref);
        scriptControls().inputEnded.clear();
        for (auto& b : scriptControls().renderSteps)
            if (b.fnRef != -1) lua_unref(L, b.fnRef);
        scriptControls().renderSteps.clear();
        scriptControls().debris.clear();
        scriptControls().attachments.clear();
        scriptControls().beams.clear();
        for (auto& kv : scriptControls().remoteInvoke)
            if (kv.second != LUA_NOREF) lua_unref(L, kv.second);
        scriptControls().remoteInvoke.clear();
        for (auto& kv : scriptControls().bindableInvoke)
            if (kv.second != LUA_NOREF) lua_unref(L, kv.second);
        scriptControls().bindableInvoke.clear();
        for (int ref : scriptControls().humanoidDied)
            if (ref != LUA_NOREF) lua_unref(L, ref);
        scriptControls().humanoidDied.clear();
        for (int ref : scriptControls().humanoidHealthChanged)
            if (ref != LUA_NOREF) lua_unref(L, ref);
        scriptControls().humanoidHealthChanged.clear();
        for (int ref : scriptControls().humanoidStateChanged)
            if (ref != LUA_NOREF) lua_unref(L, ref);
        scriptControls().humanoidStateChanged.clear();
        unrefMapS(scriptControls().proximityTriggered);
        for (auto& kv : scriptControls().remoteInvokeClient)
            if (kv.second != LUA_NOREF) lua_unref(L, kv.second);
        scriptControls().remoteInvokeClient.clear();
        for (auto& p : scriptControls().pendingInvokes)
            if (p.coRef != LUA_NOREF) lua_unref(L, p.coRef);
        scriptControls().pendingInvokes.clear();
        for (int ref : scriptControls().playerAdded)
            if (ref != LUA_NOREF) lua_unref(L, ref);
        scriptControls().playerAdded.clear();
        for (int ref : scriptControls().characterAdded)
            if (ref != LUA_NOREF) lua_unref(L, ref);
        scriptControls().characterAdded.clear();
        scriptEmitters_.clear();
        scriptControls().lastGuiButton.clear();
        playerLifecycleFired_ = false;
    }

    void flushPendingDestroy() {
        if (!workspace || pendingDestroy_.empty()) return;
        std::sort(pendingDestroy_.begin(), pendingDestroy_.end(), std::greater<int>());
        pendingDestroy_.erase(std::unique(pendingDestroy_.begin(), pendingDestroy_.end()),
                              pendingDestroy_.end());
        for (int idx : pendingDestroy_)
            destroyPartImmediate(idx);
        pendingDestroy_.clear();
    }

    void applyExplosionPlayerDamage(glm::vec3 boomPos, float radius, float maxDamage) {
        if (!playerHumanoid || !playerPos) return;
        if (playerHumanoid->dead || maxDamage <= 0.f || radius <= 0.1f) return;
        glm::vec3 center = *playerPos + glm::vec3(0.f, 2.5f, 0.f);
        float d = glm::length(center - boomPos);
        if (d >= radius) return;
        float t = 1.f - d / radius;
        float dmg = maxDamage * t * t;
        if (dmg < 1.f) dmg = 1.f;
        playerHumanoid->takeDamage(dmg);
        glm::vec3 push = center - boomPos;
        if (glm::length(push) > 1e-3f) {
            push = glm::normalize(push);
            *playerPos += push * (2.5f * t);
            playerHumanoid->velocityY += 6.f * t;
            playerHumanoid->grounded = false;
        }
        std::fprintf(stdout, "[Boom] player dmg=%.0f hp=%.0f\n",
                     dmg, playerHumanoid->health);
    }

    bool runBytecode(const uint8_t* bc, size_t n, const char* name = "=(bbc)") {
        if (!L || !bc || !n) return false;
        noteScript(bc, n, name ? name : "(bbc)");
        if (luau_load(L, name, reinterpret_cast<const char*>(bc), n, 0) != 0) {
            lastError = safeToString(-1);
            std::fprintf(stderr, "BBC load error: %s\n", lastError.c_str());
            lua_pop(L, 1);
            return false;
        }
        callStart_ = std::chrono::steady_clock::now();
        if (timedPcall(0, LUA_MULTRET) != LUA_OK) {
            lastError = safeToString(-1);
            std::fprintf(stderr, "BBC run error: %s\n", lastError.c_str());
            lua_pop(L, 1);
            return false;
        }
        return true;
    }

    bool runSealedFile(const std::string& path) {
        if (!L) return false;
        auto u = SafeBytecode::unpackSealedFile(path);
        if (!u.ok) {
            lastError = u.err;
            std::fprintf(stderr, "SafeBytecode: %s\n", lastError.c_str());
            return false;
        }
        std::fprintf(stdout, "SafeBytecode OK (game=%s, %d bytes)\n",
                     u.gameId.c_str(), (int)u.luaBc.size());
        std::string chunkName = "@" + u.gameId;
        return runBytecode(u.luaBc.data(), u.luaBc.size(), chunkName.c_str());
    }

    static bool compileSealedFile(const std::string& source, const std::string& gameId,
                                  const std::string& outPath, std::string& err) {
        return SafeBytecode::compileSourceToSealedFile(source, gameId, outPath, err);
    }

    bool runScriptPathSmart(const std::string& path, bool preferSealed = true) {
        if (!L || path.empty()) return false;
        if (SafeBytecode::isBbcPath(path) || SafeBytecode::looksLikeBbcMagic(path))
            return runSealedFile(path);
        if (preferSealed) {
            std::string bbc = path;
            auto dot = bbc.find_last_of('.');
            if (dot != std::string::npos)
                bbc = bbc.substr(0, dot) + ".bbc";
            else
                bbc += ".bbc";
            if (SafeBytecode::looksLikeBbcMagic(bbc))
                return runSealedFile(bbc);
        }
        if (requireSealedScripts && !LuaScriptSecurity::isTrustedAssetScript(path)) {
            lastError = "Play richiede script .bbc sigillato (anti-tamper / anti-executor)";
            std::fprintf(stderr, "[Lua] %s: %s\n", lastError.c_str(), path.c_str());
            return false;
        }
        return runFile(path);
    }

    bool tryResolveChildWait(const ChildWait& w, lua_State* pushTo) {
        if (!workspace) return false;
        if (w.kind == 0) {
            for (int i = 0; i < (int)workspace->parts.size(); ++i) {
                if (workspace->parts[i].parentIndex < 0 && workspace->parts[i].name == w.name) {
                    pushLivePart(pushTo, i);
                    return true;
                }
            }
            for (int i = 0; i < (int)workspace->models.size(); ++i) {
                if (workspace->models[i].name == w.name) {
                    pushLiveModel(pushTo, i);
                    return true;
                }
            }
            return false;
        }
        if (w.kind == 1) {
            int child = workspace->findSharedChild((SharedService)w.parentOrSvc, -1, w.name);
            if (child < 0) return false;
            pushSharedInstance(pushTo, child);
            return true;
        }
        if (w.kind == 2) {
            if (w.parentOrSvc < 0 || w.parentOrSvc >= (int)workspace->instances.size()) return false;
            const auto& s = workspace->instances[w.parentOrSvc];
            int child = workspace->findSharedChild(s.service, w.parentOrSvc, w.name);
            if (child < 0) return false;
            pushSharedInstance(pushTo, child);
            return true;
        }
        if (w.kind == 3) {
            int child = workspace->findPartChild(w.parentOrSvc, w.name);
            if (child < 0) return false;
            pushLivePart(pushTo, child);
            return true;
        }
        return false;
    }

    void tickChildWaits(float dt) {
        if (!L || childWaits_.empty()) return;
        for (size_t i = 0; i < childWaits_.size();) {
            ChildWait& w = childWaits_[i];
            w.remaining -= dt;
            bool found = tryResolveChildWait(w, L);
            if (!found && w.remaining > 0.f) {
                ++i;
                continue;
            }
            int ref = w.coRef;
            childWaits_.erase(childWaits_.begin() + (int)i);
            if (!found) lua_pushnil(L);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            lua_State* co = lua_tothread(L, -1);
            lua_pop(L, 1);
            lua_unref(L, ref);
            if (!co) { lua_pop(L, 1); continue; }
            // Il risultato (oggetto trovato o nil) va sullo stack della
            // coroutine: lua_resume prende li' i suoi argomenti. Prima restava
            // su L e WaitForChild restituiva il thread invece dell'oggetto.
            lua_xmove(L, co, 1);
            callStart_ = std::chrono::steady_clock::now();
            int st = lua_resume(co, L, 1);
            int nres = lua_gettop(co);
            if (st != LUA_OK && st != LUA_YIELD) {
                lastError = lua_tostring(co, -1) ? lua_tostring(co, -1) : "WaitForChild resume error";
                std::fprintf(stderr, "WaitForChild error: %s\n", lastError.c_str());
                lua_pop(co, 1);
            } else if (nres > 0) {
                lua_pop(co, nres);
            }
        }
    }

    void tickHumanoidEvents() {
        if (!L || !playerHumanoid) return;
        Humanoid& h = *playerHumanoid;
        if (std::fabs(h.health - h.lastEmittedHealth) > 0.001f) {
            h.lastEmittedHealth = h.health;
            for (int ref : scriptControls().humanoidHealthChanged) {
                if (ref == LUA_NOREF) continue;
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
                if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
                lua_pushnumber(L, h.health);
                callStart_ = std::chrono::steady_clock::now();
                if (timedPcall(1, 0) != LUA_OK) {
                    lastError = safeToString(-1);
                    std::fprintf(stderr, "HealthChanged: %s\n", lastError.c_str());
                    lua_pop(L, 1);
                }
            }
        }
        if (h.dead && !h.lastEmittedDead) {
            h.lastEmittedDead = true;
            for (int ref : scriptControls().humanoidDied) {
                if (ref == LUA_NOREF) continue;
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
                if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
                callStart_ = std::chrono::steady_clock::now();
                if (timedPcall(0, 0) != LUA_OK) {
                    lastError = safeToString(-1);
                    std::fprintf(stderr, "Died: %s\n", lastError.c_str());
                    lua_pop(L, 1);
                }
            }
        }
        if (!h.dead) h.lastEmittedDead = false;
        const char* st = h.getState();
        if (h.lastEmittedState != st) {
            std::string prev = h.lastEmittedState;
            h.lastEmittedState = st;
            for (int ref : scriptControls().humanoidStateChanged) {
                if (ref == LUA_NOREF) continue;
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
                if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
                lua_pushstring(L, st);
                lua_pushstring(L, prev.c_str());
                callStart_ = std::chrono::steady_clock::now();
                if (timedPcall(2, 0) != LUA_OK) {
                    lastError = safeToString(-1);
                    std::fprintf(stderr, "StateChanged: %s\n", lastError.c_str());
                    lua_pop(L, 1);
                }
            }
        }
    }

    void tickProximity(bool activatePressed) {
        if (!proximity || !playerPos) return;
        if (workspace) {
            proximity->syncParts([&](int idx, float& x, float& y, float& z) {
                if (idx < 0 || idx >= (int)workspace->parts.size()) return false;
                const auto& p = workspace->parts[idx];
                x = p.position.x; y = p.position.y; z = p.position.z;
                return true;
            });
        }
        proximity->scan(playerPos->x, playerPos->y + 2.f, playerPos->z);
        if (!activatePressed || proximity->nearest().empty() || !L) return;
        proximity->requestActivate();
        std::string n = proximity->takeActivated();
        if (n.empty()) return;
        auto it = scriptControls().proximityTriggered.find(n);
        if (it == scriptControls().proximityTriggered.end()) return;
        for (int ref : it->second) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            lua_pushstring(L, n.c_str());
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "ProximityPrompt: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    void fireHeartbeat(float dt) {
        if (!L) return;
        tickPlayerLifecycle();
        runTime_ += dt;
        fireRunServiceEvent(steppedRefs_, dt, "Stepped");
        fireRunServiceEvent(heartbeatRefs_, dt, "Heartbeat");
        fireRunServiceEvent(renderSteppedRefs_, dt, "RenderStepped");
        tickCharacterAnimation(dt);
        tickRenderSteps(dt);
        tickDebris(dt);
        tickTaskWaits(dt);
        tickChildWaits(dt);
        tickPendingInvokes(dt);
        tickHumanoidEvents();
        tickScriptEmitters(dt);
        flushPendingDestroy();
    }

    void tickScriptEmitters(float dt) {
        if (!particles || !workspace || scriptEmitters_.empty()) return;
        for (auto& e : scriptEmitters_) {
            if (!e.enabled || e.rate <= 0.01f) continue;
            if (e.partIndex < 0 || e.partIndex >= (int)workspace->parts.size()) continue;
            e.accum += dt * e.rate;
            int n = (int)e.accum;
            if (n <= 0) continue;
            e.accum -= (float)n;
            if (n > 12) n = 12;
            const auto& p = workspace->parts[e.partIndex];
            particles->emitPreset((ParticlePreset)e.preset, p.position + glm::vec3(0, p.size.y * 0.5f, 0),
                                  n, e.color, glm::vec3(0, 1, 0));
        }
    }

    void tickPendingInvokes(float dt) {
        auto& pend = scriptControls().pendingInvokes;
        if (!L || pend.empty()) return;
        for (size_t i = 0; i < pend.size();) {
            pend[i].remaining -= dt;
            if (pend[i].remaining > 0.f) { ++i; continue; }
            int ref = pend[i].coRef;
            pend.erase(pend.begin() + (int)i);
            if (ref == LUA_NOREF) continue;
            lua_pushnil(L); // timeout -> nil
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            lua_State* co = lua_tothread(L, -1);
            lua_pop(L, 1);
            lua_unref(L, ref);
            if (!co) { lua_pop(L, 1); continue; }
            lua_xmove(L, co, 1);   // il valore va sulla coroutine (come in tickChildWaits)
            callStart_ = std::chrono::steady_clock::now();
            int st = lua_resume(co, L, 1);
            int nres = lua_gettop(co);
            if (st != LUA_OK && st != LUA_YIELD) {
                lastError = lua_tostring(co, -1) ? lua_tostring(co, -1) : "Invoke timeout";
                lua_pop(co, 1);
            } else if (nres > 0) lua_pop(co, nres);
        }
    }

    void resumePendingInvoke(const std::string& corrId, const std::string& result) {
        auto& pend = scriptControls().pendingInvokes;
        for (size_t i = 0; i < pend.size(); ++i) {
            if (pend[i].corrId != corrId) continue;
            int ref = pend[i].coRef;
            pend.erase(pend.begin() + (int)i);
            if (!L || ref == LUA_NOREF) return;
            lua_pushstring(L, result.c_str());
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            lua_State* co = lua_tothread(L, -1);
            lua_pop(L, 1);
            lua_unref(L, ref);
            if (!co) { lua_pop(L, 1); return; }
            lua_xmove(L, co, 1);   // la risposta del server va sulla coroutine
            callStart_ = std::chrono::steady_clock::now();
            int st = lua_resume(co, L, 1);
            int nres = lua_gettop(co);
            if (st != LUA_OK && st != LUA_YIELD) {
                lastError = lua_tostring(co, -1) ? lua_tostring(co, -1) : "Invoke resume";
                lua_pop(co, 1);
            } else if (nres > 0) lua_pop(co, nres);
            return;
        }
    }

    void firePartTouched(int partIndex) {
        if (!L || !workspace) return;
        if (partIndex < 0 || partIndex >= (int)workspace->parts.size()) return;
        auto it = scriptControls().partTouched.find(partIndex);
        if (it == scriptControls().partTouched.end() || it->second.empty()) return;
        for (int ref : it->second) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            lua_newtable(L);
            lua_pushstring(L, "Player");
            lua_setfield(L, -2, "Name");
            lua_pushstring(L, "Humanoid");
            lua_setfield(L, -2, "ClassName");
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "Touched error: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    // Touched tra due Part (other = live Part table). Dedup per frame via set.
    void firePartTouchedOther(int partIndex, int otherIndex) {
        if (!L || !workspace) return;
        if (partIndex < 0 || partIndex >= (int)workspace->parts.size()) return;
        if (otherIndex < 0 || otherIndex >= (int)workspace->parts.size()) return;
        auto it = scriptControls().partTouched.find(partIndex);
        if (it == scriptControls().partTouched.end() || it->second.empty()) return;
        const uint64_t key = ((uint64_t)(uint32_t)partIndex << 32) | (uint32_t)otherIndex;
        for (uint64_t k : partPartTouchFired_)
            if (k == key) return;
        if (partPartTouchFired_.size() < 1024)
            partPartTouchFired_.push_back(key);
        for (int ref : it->second) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            pushLivePart(L, otherIndex);
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "Touched(other) error: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    void clearPartPartTouchDedup() { partPartTouchFired_.clear(); }

    // Physics bridge (impostato da ScriptsAndPhysics → g_bphys). Evita di
    // includere Jolt in questo header.
    struct PhysBridge {
        void* world = nullptr;
        bool (*applyImpulse)(void* w, int part, float x, float y, float z) = nullptr;
        bool (*setLinVel)(void* w, int part, float x, float y, float z) = nullptr;
        bool (*getLinVel)(void* w, int part, float* x, float* y, float* z) = nullptr;
        bool (*setAngVel)(void* w, int part, float x, float y, float z) = nullptr;
        float (*getMass)(void* w, int part) = nullptr;
        void (*setGroupsCollidable)(void* w, int a, int b, bool on) = nullptr;
        bool (*groupsCollidable)(void* w, int a, int b) = nullptr;
        void (*radialImpulse)(void* w, float cx, float cy, float cz, float radius, float force) = nullptr;
    };
    PhysBridge phys;

    // Un solo dispatcher per i tre eventi di item (Activated/Equipped/
    // Unequipped, stile Tool di Roblox) invece di ripetere lo stesso ciclo
    // lua_rawgeti/lua_pcall tre volte.
    void fireItemEvent(const char* eventName, const std::string& itemId, const std::string& itemName) {
        if (!L) return;
        auto it = scriptControls().events.find(eventName);
        if (it == scriptControls().events.end() || it->second.empty()) return;
        for (int ref : it->second) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            lua_pushstring(L, itemId.c_str());
            lua_pushstring(L, itemName.c_str());
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(2, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "%s error: %s\n", eventName, lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }
    void fireToolNamed(const std::map<std::string, std::vector<int>>& m,
                       const std::string& itemName) {
        if (!L || itemName.empty()) return;
        auto it = m.find(itemName);
        if (it == m.end() || it->second.empty()) return;
        for (int ref : it->second) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(0, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "Tool event error: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    void fireItemActivated(const std::string& itemId, const std::string& itemName) {
        fireItemEvent("ItemActivated", itemId, itemName);
        fireToolNamed(scriptControls().toolActivated, itemName);
    }
    void fireItemEquipped(const std::string& itemId, const std::string& itemName) {
        fireItemEvent("ItemEquipped", itemId, itemName);
        fireToolNamed(scriptControls().toolEquipped, itemName);
    }
    void fireItemUnequipped(const std::string& itemId, const std::string& itemName) {
        fireItemEvent("ItemUnequipped", itemId, itemName);
        fireToolNamed(scriptControls().toolUnequipped, itemName);
    }

    void pushLocalPlayer(lua_State* ls) {
        lua_getglobal(ls, "game");
        if (!lua_istable(ls, -1)) { lua_pop(ls, 1); lua_pushnil(ls); return; }
        lua_getfield(ls, -1, "Players");
        lua_remove(ls, -2);
        if (!lua_istable(ls, -1)) { lua_pop(ls, 1); lua_pushnil(ls); return; }
        lua_getfield(ls, -1, "LocalPlayer");
        lua_remove(ls, -2);
    }

    void pushLocalCharacter(lua_State* ls) {
        pushLocalPlayer(ls);
        if (!lua_istable(ls, -1)) return;
        lua_getfield(ls, -1, "Character");
        lua_remove(ls, -2);
    }

    void firePlayerAdded() {
        if (!L) return;
        auto& list = scriptControls().playerAdded;
        if (list.empty()) return;
        for (int ref : list) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            pushLocalPlayer(L);
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "PlayerAdded: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    void fireCharacterAdded() {
        if (!L) return;
        auto& list = scriptControls().characterAdded;
        if (list.empty()) return;
        for (int ref : list) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            pushLocalCharacter(L);
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "CharacterAdded: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    // ---- Server dedicato: i giocatori collegati (blocky_servers/ServerWorld.h) ----
    // Sul server non c'e' un LocalPlayer: ogni client e' un Player Luau del
    // prelude (Internal.serverPlayer), con UserId = id del peer.

    bool serverPlayersEnabled() const { return serverPlayers_; }

    /**
     * workspace.Gravity come la vede lo script (anche dopo workspace.Gravity = x,
     * che scrive solo il campo): la fisica la legge ogni frame e la applica.
     */
    float readWorkspaceGravity() {
        float g = scriptControls().workspaceGravity;
        if (!L) return g;
        lua_getglobal(L, "workspace");
        if (lua_istable(L, -1)) {
            lua_pushstring(L, "Gravity");
            lua_rawget(L, -2);
            if (lua_isnumber(L, -1)) g = std::clamp((float)lua_tonumber(L, -1), 0.f, 1000.f);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        scriptControls().workspaceGravity = g;
        return g;
    }

    /** Part con un ClickDetector.MouseClick collegato? */
    bool partHasClick(int idx) const {
        auto it = scriptControls().partClicked.find(idx);
        return it != scriptControls().partClicked.end() && !it->second.empty();
    }

    /** Clic del giocatore sulla part: MouseClick(playerWhoClicked), come Roblox. */
    void firePartClicked(int idx) {
        if (!L) return;
        auto it = scriptControls().partClicked.find(idx);
        if (it == scriptControls().partClicked.end()) return;
        const std::vector<int> refs = it->second;
        for (int ref : refs) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            pushLocalPlayer(L);
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "MouseClick: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    /** Enum.<enumName>.<item> (come lo vede lo script); se manca, il nome come stringa. */
    void pushEnumItem(const char* enumName, const char* item) {
        lua_getglobal(L, "Enum");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, enumName);
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, item);
                if (!lua_isnil(L, -1)) {
                    lua_remove(L, -2);
                    lua_remove(L, -2);
                    return;
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        lua_pushstring(L, item);
    }

    /**
     * Un tasto o un pulsante del mouse premuto/rilasciato (da main.cpp):
     * UserInputService.InputBegan / InputEnded(input, gameProcessedEvent)
     * come Roblox. Prima le funzioni collegate non venivano mai chiamate.
     *   keyName    nome in Enum.KeyCode ("E", "Space", ... "Unknown" per il mouse)
     *   inputType  nome in Enum.UserInputType ("Keyboard", "MouseButton1", ...)
     *   gameProcessed  vero se l'input l'ha gia' preso l'interfaccia (chat, menu)
     */
    void fireInput(bool began, const char* keyName, const char* inputType,
                   float mouseX, float mouseY, bool gameProcessed) {
        auto& refs = began ? scriptControls().inputBegan : scriptControls().inputEnded;
        if (!L || refs.empty()) return;
        lua_newtable(L);
        pushEnumItem("KeyCode", keyName);
        lua_setfield(L, -2, "KeyCode");
        pushEnumItem("UserInputType", inputType);
        lua_setfield(L, -2, "UserInputType");
        pushEnumItem("UserInputState", began ? "Begin" : "End");
        lua_setfield(L, -2, "UserInputState");
        pushVec3(L, mouseX, mouseY, 0.f);
        lua_setfield(L, -2, "Position");
        pushVec3(L, 0.f, 0.f, 0.f);
        lua_setfield(L, -2, "Delta");
        lua_pushstring(L, "InputObject");
        lua_setfield(L, -2, "ClassName");
        const int obj = lua_ref(L, -1);
        lua_pop(L, 1);
        const std::vector<int> list = refs;   // copia: una callback puo' collegarne altre
        for (int ref : list) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            lua_rawgeti(L, LUA_REGISTRYINDEX, obj);
            lua_pushboolean(L, gameProcessed ? 1 : 0);
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(2, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "%s: %s\n", began ? "InputBegan" : "InputEnded", lastError.c_str());
                lua_pop(L, 1);
            }
        }
        lua_unref(L, obj);
    }

    /**
     * Toglie subito la part idx dal mondo E aggiorna tutto quello che la
     * ricorda per indice (script, vincoli, figlie, etichette...). Chi toglie
     * part durante il gioco (esplosioni, spada, rete) deve passare da qui,
     * non da parts.erase.
     */
    void destroyPartNow(int idx) {
        if (workspace && idx >= 0 && idx < (int)workspace->parts.size()) destroyPartImmediate(idx);
    }

    void enableServerPlayers() {
        if (!L) return;
        serverPlayers_ = true;
        playerLifecycleFired_ = true;   // niente PlayerAdded del giocatore locale
        if (pushPreludeField(L, "enableServerPlayers")) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) lua_pop(L, 1);
        }
    }

    /** Chiama ogni funzione in refs con l'oggetto in registry objRef come argomento. */
    void callRefsWith(const std::vector<int>& refs, int objRef, const char* what) {
        for (int ref : refs) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            lua_rawgeti(L, LUA_REGISTRYINDEX, objRef);
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "%s: %s\n", what, lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    /** Mette in cima allo stack il risultato di Internal.<fn>(id[, name]); falso se nil/errore. */
    bool pushServerPlayerCall(const char* fn, int id, const char* name = nullptr) {
        if (!pushPreludeField(L, fn)) return false;
        lua_pushinteger(L, id);
        int nargs = 1;
        if (name) { lua_pushstring(L, name); nargs = 2; }
        if (lua_pcall(L, nargs, 1, 0) != LUA_OK) { lua_pop(L, 1); return false; }
        if (lua_isnil(L, -1)) { lua_pop(L, 1); return false; }
        return true;
    }

    void serverPlayerJoined(int id, const char* name) {
        if (!L || !serverPlayers_) return;
        if (!pushServerPlayerCall("serverPlayer", id, name)) return;
        const int ref = lua_ref(L, -1);
        lua_pop(L, 1);
        callRefsWith(scriptControls().playerAdded, ref, "PlayerAdded");
        notifyPrelude(L, "serverCharacterAdded", id, nullptr);
        lua_unref(L, ref);
    }

    void serverPlayerLeft(int id) {
        if (!L || !serverPlayers_) return;
        if (pushServerPlayerCall("serverPlayerById", id)) {
            const int ref = lua_ref(L, -1);
            lua_pop(L, 1);
            callRefsWith(scriptControls().playerRemoving, ref, "PlayerRemoving");
            lua_unref(L, ref);
        }
        notifyPrelude(L, "serverPlayerRemoved", id, nullptr);
        for (auto it = serverTouching_.begin(); it != serverTouching_.end();)
            it = (it->first == id) ? serverTouching_.erase(it) : std::next(it);
    }

    void serverPlayerMoved(int id, const glm::vec3& feet) {
        if (!L || !serverPlayers_) return;
        if (!pushPreludeField(L, "serverPlayerMoved")) return;
        lua_pushinteger(L, id);
        lua_pushnumber(L, feet.x);
        lua_pushnumber(L, feet.y);
        lua_pushnumber(L, feet.z);
        if (lua_pcall(L, 4, 0, 0) != LUA_OK) lua_pop(L, 1);
        serverPlayerTouches(id, feet);
    }

    // Il server non ha i corpi dei giocatori: Touched lo calcola qui, con una
    // scatola come il personaggio (2 x 5 x 2 sopra i piedi) contro le part
    // che hanno un Touched collegato. Scatta all'ingresso, come in Roblox.
    void serverPlayerTouches(int id, const glm::vec3& feet) {
        if (!workspace) return;
        const glm::vec3 c = feet + glm::vec3(0.f, 2.5f, 0.f);
        const glm::vec3 h(1.f, 2.5f, 1.f);
        for (const auto& kv : scriptControls().partTouched) {
            const int idx = kv.first;
            if (idx < 0 || idx >= (int)workspace->parts.size() || kv.second.empty()) continue;
            const WorkspacePart& p = workspace->parts[(size_t)idx];
            if (!p.canTouch || p.destroyPending) continue;
            // Mezze misure della part ruotata, in assi del mondo.
            const glm::mat3 r(partRotationMatrix(p.rotation));
            const glm::vec3 ph = p.size * 0.5f;
            const glm::vec3 ext(std::fabs(r[0][0]) * ph.x + std::fabs(r[1][0]) * ph.y + std::fabs(r[2][0]) * ph.z,
                                std::fabs(r[0][1]) * ph.x + std::fabs(r[1][1]) * ph.y + std::fabs(r[2][1]) * ph.z,
                                std::fabs(r[0][2]) * ph.x + std::fabs(r[1][2]) * ph.y + std::fabs(r[2][2]) * ph.z);
            const glm::vec3 d = glm::abs(c - p.position);
            const bool overlap = d.x <= h.x + ext.x && d.y <= h.y + ext.y && d.z <= h.z + ext.z;
            const auto key = std::make_pair(id, idx);
            if (!overlap) { serverTouching_.erase(key); continue; }
            if (!serverTouching_.insert(key).second) continue;
            if (!pushServerPlayerCall("serverRootPart", id)) continue;
            const int ref = lua_ref(L, -1);
            lua_pop(L, 1);
            const std::vector<int> refs = kv.second;   // copia: lo script puo' scollegare
            callRefsWith(refs, ref, "Touched");
            lua_unref(L, ref);
        }
    }

    /** Chi ha mandato un evento al server: il Player sul server dedicato, altrimenti il nome. */
    void pushRemoteSender(const IncomingRemoteEvent& in) {
        if (serverPlayers_ && pushPreludeField(L, "serverPlayerByName")) {
            lua_pushstring(L, in.fromName);
            if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
                if (!lua_isnil(L, -1)) return;
                lua_pop(L, 1);
            } else {
                lua_pop(L, 1);
            }
        }
        lua_pushstring(L, in.fromName);
    }

    void firePartChildAdded(int parentIdx, int childIdx) {
        if (!L || !workspace) return;
        auto it = scriptControls().partChildAdded.find(parentIdx);
        if (it == scriptControls().partChildAdded.end() || it->second.empty()) return;
        for (int ref : it->second) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            pushLivePart(L, childIdx);
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "ChildAdded: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    void fireSharedChildAdded(int parentSharedIdx, int childIdx) {
        if (!L || !workspace) return;
        auto it = scriptControls().sharedChildAdded.find(parentSharedIdx);
        if (it == scriptControls().sharedChildAdded.end() || it->second.empty()) return;
        for (int ref : it->second) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            pushSharedInstance(L, childIdx);
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "Shared ChildAdded: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    bool playerLifecycleFired_ = false;
    bool serverPlayers_ = false;                    // server dedicato: Player per peer
    std::set<std::pair<int, int>> serverTouching_;  // (peer, part) gia' a contatto
    void tickPlayerLifecycle() {
        if (playerLifecycleFired_ || !L) return;
        playerLifecycleFired_ = true;
        firePlayerAdded();
        fireCharacterAdded();
    }

    void fireGuiButtonActivated(const std::string& buttonName) {
        if (!L || buttonName.empty()) return;
        scriptControls().lastGuiButton = buttonName;
        auto it = scriptControls().guiButtonActivated.find(buttonName);
        if (it == scriptControls().guiButtonActivated.end() || it->second.empty()) return;
        for (int ref : it->second) {
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(0, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "TextButton Activated error: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    void drainRemoteEvents() {
        if (!L || !net) return;
        for (const auto& in : net->incomingRemotes) {
            // RemoteFunction protocol: payload "RFREQ|<corr>|<data>" / "RFRES|<corr>|<data>"
            // name = remote function name
            if (in.payload.size() >= 5 && in.payload.compare(0, 5, "RFREQ") == 0) {
                // Host riceve InvokeServer dal client
                std::string corr, data;
                size_t p1 = in.payload.find('|');
                size_t p2 = (p1 == std::string::npos) ? std::string::npos : in.payload.find('|', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    corr = in.payload.substr(p1 + 1, p2 - p1 - 1);
                    data = in.payload.substr(p2 + 1);
                }
                std::string result;
                auto it = scriptControls().remoteInvoke.find(in.name);
                if (it != scriptControls().remoteInvoke.end() && it->second != LUA_NOREF) {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, it->second);
                    if (lua_isfunction(L, -1)) {
                        pushRemoteSender(in);   // InvokeServer: il Player sul server dedicato
                        lua_pushstring(L, data.c_str());
                        callStart_ = std::chrono::steady_clock::now();
                        if (timedPcall(2, 1) == LUA_OK) {
                            if (lua_isstring(L, -1)) result = lua_tostring(L, -1);
                            else if (lua_isnumber(L, -1)) {
                                char buf[64];
                                std::snprintf(buf, sizeof(buf), "%.10g", (double)lua_tonumber(L, -1));
                                result = buf;
                            } else if (lua_isboolean(L, -1))
                                result = lua_toboolean(L, -1) ? "true" : "false";
                            lua_pop(L, 1);
                        } else {
                            lastError = safeToString(-1);
                            lua_pop(L, 1);
                        }
                    } else lua_pop(L, 1);
                }
                std::string resPayload = "RFRES|" + corr + "|" + result;
                if (in.fromId)
                    net->fireRemoteEventToClient(in.fromId, in.name, resPayload.c_str());
                continue;
            }
            if (in.payload.size() >= 5 && in.payload.compare(0, 5, "RFRES") == 0) {
                size_t p1 = in.payload.find('|');
                size_t p2 = (p1 == std::string::npos) ? std::string::npos : in.payload.find('|', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    std::string corr = in.payload.substr(p1 + 1, p2 - p1 - 1);
                    std::string data = in.payload.substr(p2 + 1);
                    resumePendingInvoke(corr, data);
                }
                continue;
            }
            auto& map = in.toServer ? scriptControls().remoteServer
                                    : scriptControls().remoteClient;
            auto it = map.find(in.name);
            if (it == map.end()) continue;
            for (int ref : it->second) {
                if (ref == LUA_NOREF) continue;
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
                if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
                if (in.toServer) pushRemoteSender(in); else lua_pushstring(L, in.fromName);
                lua_pushlstring(L, in.payload.data(), in.payload.size());
                callStart_ = std::chrono::steady_clock::now();
                if (timedPcall(2, 0) != LUA_OK) {
                    lastError = safeToString(-1);
                    std::fprintf(stderr, "RemoteEvent error: %s\n", lastError.c_str());
                    lua_pop(L, 1);
                }
            }
        }
        net->incomingRemotes.clear();
    }

private:
    lua_State* L = nullptr;
    std::set<std::string> apiGlobals_;
    int preludeRef_ = LUA_NOREF; // tabella interna del prelude (LuaPrelude.h)
    double runTime_ = 0.0;       // tempo di gioco per RunService.Stepped(tempo, dt)

    // ---- setmetatable / getmetatable per gli script ----

    /**
     * Metatabella del motore = ha almeno una funzione C dentro (__index,
     * __newindex, __add... scritti in C++). Quelle degli script e del
     * prelude sono tutte Luau. Per uno script sono bloccate come in Roblox.
     */
    static bool isEngineMetatable(lua_State* ls, int mtIdx) {
        if (mtIdx < 0) mtIdx = lua_gettop(ls) + 1 + mtIdx;
        bool engine = false;
        lua_pushnil(ls);
        while (lua_next(ls, mtIdx) != 0) {
            if (lua_iscfunction(ls, -1)) engine = true;
            lua_pop(ls, 1);
            if (engine) { lua_pop(ls, 1); break; }
        }
        return engine;
    }

    static int luaSafeGetmetatable(lua_State* ls) {
        luaL_checkany(ls, 1);
        if (!lua_getmetatable(ls, 1)) { lua_pushnil(ls); return 1; }
        lua_pushstring(ls, "__metatable");
        lua_rawget(ls, -2);
        if (!lua_isnil(ls, -1)) return 1; // il valore protetto, come Lua standard
        lua_pop(ls, 1);
        if (isEngineMetatable(ls, -1)) { lua_pushstring(ls, "The metatable is locked"); return 1; }
        return 1;
    }

    static int luaSafeSetmetatable(lua_State* ls) {
        luaL_checktype(ls, 1, LUA_TTABLE);
        const int t = lua_type(ls, 2);
        if (t != LUA_TNIL && t != LUA_TTABLE) return blockyLuaError(ls, "setmetatable: il secondo argomento deve essere una tabella o nil");
        if (lua_getmetatable(ls, 1)) {
            lua_pushstring(ls, "__metatable");
            lua_rawget(ls, -2);
            const bool locked = !lua_isnil(ls, -1);
            lua_pop(ls, 1);
            if (locked || isEngineMetatable(ls, -1))
                return blockyLuaError(ls, "cannot change a protected metatable");
            lua_pop(ls, 1);
        }
        lua_settop(ls, 2);
        lua_setmetatable(ls, 1);
        return 1;
    }

    /** rawset per il solo prelude (passato come argomento del chunk). */
    static int luaPreludeRawset(lua_State* ls) {
        luaL_checktype(ls, 1, LUA_TTABLE);
        lua_settop(ls, 3);
        lua_rawset(ls, 1);
        lua_settop(ls, 1);
        return 1;
    }

    // ---- Prelude Luau (LuaPrelude.h) ----

    /** Compila ed esegue il prelude; installa CFrame, Random e i metodi nuovi di Vector3. */
    bool loadPrelude() {
        preludeRef_ = LUA_NOREF;
        // Stato Lua nuovo: le tracce degli script di prima non esistono piu'.
        charAnim.clear();
        trackRefs_.clear();
        setupCharacterRig();
        const std::string bc = Luau::compile(blk::luaprelude::source());
        bool loaded = luau_load(L, "=BlockyPrelude", bc.data(), bc.size(), 0) == 0;
        if (loaded) {
            blockyPushCFunction(L, luaPreludeRawset); // solo per il prelude: gli script non hanno rawset
            loaded = lua_pcall(L, 1, 1, 0) == LUA_OK;
        }
        if (!loaded) {
            std::fprintf(stderr, "[Lua] prelude non caricato: %s\n", lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
            lua_pop(L, 1);
            return false;
        }
        if (!lua_istable(L, -1)) { lua_pop(L, 1); return false; }
        preludeRef_ = lua_ref(L, -1);
        lua_pop(L, 1);
        return true;
    }

    /** Dopo registerApi: CFrame/Random globali, Vector3 completato, Internal.install(). */
    void installPrelude() {
        if (preludeRef_ == LUA_NOREF) return;
        lua_getref(L, preludeRef_);
        const int pre = lua_gettop(L);
        lua_getfield(L, pre, "install");
        if (lua_isfunction(L, -1) && lua_pcall(L, 0, 0, 0) != LUA_OK) {
            std::fprintf(stderr, "[Lua] prelude install: %s\n", lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
            lua_pop(L, 1);
        } else if (!lua_isfunction(L, -1) && lua_gettop(L) > pre) {
            lua_pop(L, 1);
        }
        lua_getfield(L, pre, "CFrame");
        lua_setglobal(L, "CFrame");
        lua_getfield(L, pre, "Random");
        lua_setglobal(L, "Random");
        ensureVec3Meta(L);
        lua_getfield(L, LUA_REGISTRYINDEX, "BlockyVec3MT");
        const int mt = lua_gettop(L);
        lua_getfield(L, pre, "vec3Index");
        lua_getfield(L, mt, "__index");
        if (lua_pcall(L, 1, 1, 0) == LUA_OK) lua_setfield(L, mt, "__index");
        else lua_pop(L, 1);
        lua_getfield(L, pre, "vec3Div"); lua_setfield(L, mt, "__div");
        lua_getfield(L, pre, "vec3Eq"); lua_setfield(L, mt, "__eq");
        lua_getfield(L, pre, "vec3ToString"); lua_setfield(L, mt, "__tostring");
        lua_pop(L, 2); // mt, prelude

        // Enum.AnimationPriority (Core 0 ... Action4 6), come Roblox.
        lua_getglobal(L, "Enum");
        if (lua_istable(L, -1) && !lua_getreadonly(L, -1)) {
            lua_getfield(L, -1, "AnimationPriority");
            const bool has = !lua_isnil(L, -1);
            lua_pop(L, 1);
            if (!has) {
                static const char* names[] = {"Core", "Idle", "Movement", "Action", "Action2", "Action3", "Action4"};
                lua_newtable(L);
                for (int i = 0; i < 7; ++i) {
                    lua_newtable(L);
                    lua_pushstring(L, names[i]); lua_setfield(L, -2, "Name");
                    lua_pushinteger(L, i); lua_setfield(L, -2, "Value");
                    lua_pushstring(L, "AnimationPriority"); lua_setfield(L, -2, "EnumType");
                    lua_setfield(L, -2, names[i]);
                }
                lua_pushstring(L, "AnimationPriority");
                lua_insert(L, -2);
                lua_rawset(L, -3);
            }
        }
        lua_pop(L, 1);
    }

    /** Mette sullo stack un campo della tabella interna del prelude; falso (e niente sullo stack) se manca. */
    static bool pushPreludeField(lua_State* ls, const char* name) {
        auto* rt = fromState(ls);
        if (!rt || rt->preludeRef_ == LUA_NOREF) return false;
        lua_getref(ls, rt->preludeRef_);
        if (!lua_istable(ls, -1)) { lua_pop(ls, 1); return false; }
        lua_getfield(ls, -1, name);
        lua_remove(ls, -2);
        if (lua_isnil(ls, -1)) { lua_pop(ls, 1); return false; }
        return true;
    }

    /**
     * Connect (o Once) di un evento del C++ con oggetto connessione vero:
     * conn = evento:Connect(f); conn:Disconnect(); conn.Connected.
     * Senza prelude resta la Connect originale.
     */
    static void pushSignalMethod(lua_State* ls, lua_CFunction fn, const char* debugName, int mode) {
        if (!pushPreludeField(ls, "makeConnect")) {
            lua_pushcfunction(ls, fn, debugName);
            return;
        }
        lua_pushcfunction(ls, fn, debugName);
        lua_pushinteger(ls, mode);
        if (lua_pcall(ls, 2, 1, 0) != LUA_OK) {
            lua_pop(ls, 1);
            lua_pushcfunction(ls, fn, debugName);
        }
    }

    // ---- AnimationTrack (Lua) ----

    std::map<int, int> trackRefs_; // id traccia -> tabella AnimationTrack (per gli eventi)

    /** signal:Fire(arg) su un Signal del prelude. */
    void fireLuaSignal(int idx, const char* arg) {
        if (idx < 0) idx = lua_gettop(L) + 1 + idx;
        lua_getfield(L, idx, "Fire");
        if (!lua_isfunction(L, -1)) { lua_pop(L, 1); return; }
        lua_pushvalue(L, idx);
        int n = 1;
        if (arg) { lua_pushstring(L, arg); ++n; }
        if (lua_pcall(L, n, 0, 0) != LUA_OK) {
            std::fprintf(stderr, "[Lua] evento animazione: %s\n", lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
            lua_pop(L, 1);
        }
    }

    static void pushNewSignal(lua_State* ls) {
        if (pushPreludeField(ls, "Signal")) {
            lua_getfield(ls, -1, "new");
            lua_remove(ls, -2);
            if (lua_pcall(ls, 0, 1, 0) == LUA_OK) return;
            lua_pop(ls, 1);
        }
        lua_newtable(ls);
    }

    /**
     * AnimationId -> Action. Formati:
     *   blkanim://nome           prima Action di animations/nome.blkanim
     *   blkanim://nome/Action    l'Action con quel nome
     *   nome                     come blkanim://nome
     * Si cerca nella cartella dati (animazioni salvate dall'Animation Studio)
     * e negli asset del gioco. rbxassetid:// non si puo' scaricare.
     */
    static bool resolveAnimation(const std::string& idIn, blk::anim::Action& out, std::string& err) {
        std::string id = idIn;
        if (id.rfind("rbxassetid://", 0) == 0 || id.rfind("http", 0) == 0) {
            err = "le animazioni di Roblox non si scaricano: salvala dall'Animation Studio e usa \"blkanim://nome\"";
            return false;
        }
        if (id.rfind("blkanim://", 0) == 0) id = id.substr(10);
        std::string file = id, actionName;
        const size_t slash = id.find('/');
        if (slash != std::string::npos) { file = id.substr(0, slash); actionName = id.substr(slash + 1); }
        bool okName = !file.empty() && file.size() <= 64 && file.find("..") == std::string::npos;
        for (char c : file)
            if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == ' ' || c == '.')) okName = false;
        if (!okName) { err = "AnimationId non valido: \"" + idIn + "\""; return false; }
        const std::string candidates[] = {blockyDataDir() + "/animations/" + file + ".blkanim",
                                          resolveScriptPath("animations/" + file + ".blkanim")};
        for (const auto& path : candidates) {
            blk::anim::Armature arm;
            std::vector<blk::anim::Action> acts;
            std::string e;
            if (!blk::anim::loadFile(path, arm, acts, nullptr, e)) continue;
            for (const auto& a : acts)
                if (actionName.empty() || a.name == actionName) { out = a; return true; }
            err = "nel file \"" + file + "\" non c'e' l'animazione \"" + actionName + "\"";
            return false;
        }
        err = "animazione \"" + file + "\" non trovata (salvala dall'Animation Studio)";
        return false;
    }

    static int charTrackId(lua_State* ls, int idx) { return intField(ls, idx, "__trackId"); }

    static int luaCharTrackPlay(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt) return 0;
        rt->charAnim.play(charTrackId(ls, 1), (float)luaL_optnumber(ls, 2, 0.1), (float)luaL_optnumber(ls, 3, 1.0),
                          (float)luaL_optnumber(ls, 4, 1.0));
        return 0;
    }
    static int luaCharTrackStop(lua_State* ls) {
        if (auto* rt = fromState(ls)) rt->charAnim.stop(charTrackId(ls, 1), (float)luaL_optnumber(ls, 2, 0.1));
        return 0;
    }
    static int luaCharTrackAdjustSpeed(lua_State* ls) {
        if (auto* rt = fromState(ls)) rt->charAnim.adjustSpeed(charTrackId(ls, 1), (float)luaL_optnumber(ls, 2, 1.0));
        return 0;
    }
    static int luaCharTrackAdjustWeight(lua_State* ls) {
        if (auto* rt = fromState(ls))
            rt->charAnim.adjustWeight(charTrackId(ls, 1), (float)luaL_optnumber(ls, 2, 1.0), (float)luaL_optnumber(ls, 3, 0.1));
        return 0;
    }
    /** track:GetMarkerReachedSignal("Passo") — un Signal per nome, creato al primo uso. */
    static int luaCharTrackMarkerSignal(lua_State* ls) {
        const char* name = luaL_checkstring(ls, 2);
        lua_getfield(ls, 1, "_markers");
        if (!lua_istable(ls, -1)) return 0;
        lua_getfield(ls, -1, name);
        if (lua_istable(ls, -1)) return 1;
        lua_pop(ls, 1);
        pushNewSignal(ls);
        lua_pushvalue(ls, -1);
        lua_setfield(ls, -3, name);
        return 1;
    }
    static int luaCharTrackTimeOfKeyframe(lua_State* ls) {
        auto* rt = fromState(ls);
        const char* name = luaL_checkstring(ls, 2);
        const blk::anim::CharTrack* t = rt ? rt->charAnim.track(charTrackId(ls, 1)) : nullptr;
        if (t) {
            for (const auto& m : t->action->markers)
                if (m.name == name) { lua_pushnumber(ls, (m.frame - t->action->frameStart) / t->action->fps); return 1; }
            for (const auto& e : t->action->events)
                if (e.name == name) { lua_pushnumber(ls, (e.frame - t->action->frameStart) / t->action->fps); return 1; }
        }
        return blockyLuaError(ls, "GetTimeOfKeyframe: marker \"%s\" non trovato", name);
    }

    static int parsePriority(lua_State* ls, int idx) {
        static const char* names[] = {"Core", "Idle", "Movement", "Action", "Action2", "Action3", "Action4"};
        if (lua_isnumber(ls, idx)) return std::clamp((int)lua_tointeger(ls, idx), 0, 6);
        if (lua_istable(ls, idx)) {
            lua_getfield(ls, idx, "Value");
            const int v = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
            lua_pop(ls, 1);
            if (v >= 0) return std::clamp(v, 0, 6);
            lua_getfield(ls, idx, "Name");
            const char* n = lua_tostring(ls, -1);
            lua_pop(ls, 1);
            for (int i = 0; n && i < 7; ++i) if (std::strcmp(n, names[i]) == 0) return i;
        }
        if (const char* n = lua_tostring(ls, idx))
            for (int i = 0; i < 7; ++i) if (std::strcmp(n, names[i]) == 0) return i;
        return -1;
    }

    static int luaCharTrackIndex(lua_State* ls) {
        auto* rt = fromState(ls);
        const char* k = lua_tostring(ls, 2);
        const blk::anim::CharTrack* t = rt ? rt->charAnim.track(charTrackId(ls, 1)) : nullptr;
        if (!t || !k) return 0;
        if (!std::strcmp(k, "IsPlaying")) { lua_pushboolean(ls, t->playing); return 1; }
        if (!std::strcmp(k, "Length")) { lua_pushnumber(ls, t->action->durationSeconds()); return 1; }
        if (!std::strcmp(k, "Looped")) { lua_pushboolean(ls, t->looped); return 1; }
        if (!std::strcmp(k, "Speed")) { lua_pushnumber(ls, t->speed); return 1; }
        if (!std::strcmp(k, "TimePosition")) { lua_pushnumber(ls, t->time); return 1; }
        if (!std::strcmp(k, "WeightCurrent")) { lua_pushnumber(ls, t->weight); return 1; }
        if (!std::strcmp(k, "WeightTarget")) { lua_pushnumber(ls, t->targetWeight); return 1; }
        if (!std::strcmp(k, "Priority")) {
            lua_getglobal(ls, "Enum");
            if (lua_istable(ls, -1)) {
                lua_getfield(ls, -1, "AnimationPriority");
                if (lua_istable(ls, -1)) {
                    static const char* names[] = {"Core", "Idle", "Movement", "Action", "Action2", "Action3", "Action4"};
                    lua_getfield(ls, -1, names[std::clamp(t->priority, 0, 6)]);
                    return 1;
                }
            }
            lua_pushinteger(ls, t->priority);
            return 1;
        }
        return 0;
    }

    static int luaCharTrackNewIndex(lua_State* ls) {
        auto* rt = fromState(ls);
        const char* k = lua_tostring(ls, 2);
        blk::anim::CharTrack* t = rt ? rt->charAnim.track(charTrackId(ls, 1)) : nullptr;
        if (!t || !k) return 0;
        if (!std::strcmp(k, "Looped")) { t->looped = lua_toboolean(ls, 3) != 0; return 0; }
        if (!std::strcmp(k, "TimePosition")) { rt->charAnim.setTimePosition(t->id, (float)luaL_checknumber(ls, 3)); return 0; }
        if (!std::strcmp(k, "Priority")) {
            const int p = parsePriority(ls, 3);
            if (p < 0) return blockyLuaError(ls, "Priority: Enum.AnimationPriority.Core/Idle/Movement/Action...");
            t->priority = p;
            return 0;
        }
        return blockyLuaError(ls, "AnimationTrack.%s non si puo' cambiare", k);
    }

    static void pushCharTrack(lua_State* ls, int id, const std::string& name) {
        auto* rt = fromState(ls);
        if (rt) {
            const auto it = rt->trackRefs_.find(id);
            if (it != rt->trackRefs_.end()) { lua_getref(ls, it->second); return; }
        }
        lua_newtable(ls);
        lua_pushinteger(ls, id); lua_setfield(ls, -2, "__trackId");
        lua_pushstring(ls, name.c_str()); lua_setfield(ls, -2, "Name");
        lua_pushstring(ls, "AnimationTrack"); lua_setfield(ls, -2, "ClassName");
        blockyPushCFunction(ls, luaCharTrackPlay); lua_setfield(ls, -2, "Play");
        blockyPushCFunction(ls, luaCharTrackStop); lua_setfield(ls, -2, "Stop");
        blockyPushCFunction(ls, luaCharTrackAdjustSpeed); lua_setfield(ls, -2, "AdjustSpeed");
        blockyPushCFunction(ls, luaCharTrackAdjustWeight); lua_setfield(ls, -2, "AdjustWeight");
        blockyPushCFunction(ls, luaCharTrackMarkerSignal); lua_setfield(ls, -2, "GetMarkerReachedSignal");
        blockyPushCFunction(ls, luaCharTrackTimeOfKeyframe); lua_setfield(ls, -2, "GetTimeOfKeyframe");
        for (const char* s : {"Stopped", "Ended", "DidLoop", "KeyframeReached"}) {
            pushNewSignal(ls);
            lua_setfield(ls, -2, s);
        }
        lua_newtable(ls); lua_setfield(ls, -2, "_markers");
        lua_getfield(ls, LUA_REGISTRYINDEX, "BlockyCharTrackMT");
        if (!lua_istable(ls, -1)) {
            lua_pop(ls, 1);
            lua_newtable(ls);
            blockyPushCFunction(ls, luaCharTrackIndex); lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaCharTrackNewIndex); lua_setfield(ls, -2, "__newindex");
            lua_pushstring(ls, "AnimationTrack"); lua_setfield(ls, -2, "__metatable");
            lua_pushvalue(ls, -1);
            lua_setfield(ls, LUA_REGISTRYINDEX, "BlockyCharTrackMT");
        }
        lua_setmetatable(ls, -2);
        if (rt) {
            lua_pushvalue(ls, -1);
            rt->trackRefs_[id] = lua_ref(ls, -1);
            lua_pop(ls, 1);
        }
    }

    /** Humanoid:LoadAnimation(animation) / Animator:LoadAnimation(animation). */
    static int luaLoadCharAnimation(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt) return 0;
        // L'Animation (tabella con AnimationId) o direttamente la stringa.
        std::string id;
        for (int i = 1; i <= lua_gettop(ls) && id.empty(); ++i) {
            if (lua_type(ls, i) == LUA_TSTRING) id = lua_tostring(ls, i);
            else if (lua_istable(ls, i)) {
                lua_getfield(ls, i, "AnimationId");
                if (lua_isstring(ls, -1)) id = lua_tostring(ls, -1);
                lua_pop(ls, 1);
            }
        }
        if (id.empty()) return blockyLuaError(ls, "LoadAnimation(animation): serve un Animation con AnimationId");
        if (rt->trackRefs_.size() >= 256) return blockyLuaError(ls, "LoadAnimation: troppe animazioni caricate (max 256)");
        rt->setupCharacterRig();
        blk::anim::Action a;
        std::string err;
        if (!resolveAnimation(id, a, err)) return blockyLuaError(ls, "LoadAnimation: %s", err.c_str());
        const int tid = rt->charAnim.load(a);
        pushCharTrack(ls, tid, a.name);
        return 1;
    }

    static int luaGetPlayingCharTracks(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_newtable(ls);
        if (!rt) return 1;
        int n = 1;
        for (int id : rt->charAnim.playingTracks()) {
            const blk::anim::CharTrack* t = rt->charAnim.track(id);
            pushCharTrack(ls, id, t ? t->action->name : std::string());
            lua_rawseti(ls, -2, n++);
        }
        return 1;
    }

    // rawset: la tabella game ha gia' il blocco anti-executor su __newindex.
    void writeGameIdentity(int gameIdx) {
        if (gameIdx < 0) gameIdx = lua_gettop(L) + 1 + gameIdx;
        lua_pushstring(L, "PlaceId");
        lua_pushstring(L, placeId.c_str());
        lua_rawset(L, gameIdx);
        lua_pushstring(L, "JobId");
        lua_pushstring(L, jobId.c_str());
        lua_rawset(L, gameIdx);
    }

    // lua_pcall misurato: la funzione da chiamare e' sotto gli argomenti, da
    // li' si legge il chunk (lo script) a cui appartiene.
    int timedPcall(int nargs, int nresults) {
        lua_Debug ar{};
        std::string who = "(C)";
        if (lua_getinfo(L, -(nargs + 1), "s", &ar) && ar.short_src[0]) who = ar.short_src;
        if (who == "BlockyPrelude") {
            // La guardia di Connect (LuaPrelude.h): il costo e' della funzione
            // dello script che avvolge, non del prelude.
            const int fidx = lua_gettop(L) - nargs;
            for (int u = 1; u <= 8; ++u) {
                const char* name = lua_getupvalue(L, fidx, u);
                if (!name) break;
                const bool fn = lua_isfunction(L, -1);
                lua_Debug inner{};
                if (fn && lua_getinfo(L, -1, "s", &inner) && inner.short_src[0]
                    && std::strcmp(inner.short_src, "BlockyPrelude") != 0) {
                    who = inner.short_src;
                    lua_pop(L, 1);
                    break;
                }
                lua_pop(L, 1);
            }
        }
        callStart_ = std::chrono::steady_clock::now();
        const auto t0 = std::chrono::steady_clock::now();
        const int r = lua_pcall(L, nargs, nresults, 0);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        ScriptCost& c = scriptCosts[who];
        c.totalMs += ms;
        c.windowMs += ms;
        c.peakMs = std::max(c.peakMs, ms);
        ++c.calls;
        ++c.windowCalls;
        return r;
    }

    std::vector<int> heartbeatRefs_;
    std::vector<int> steppedRefs_;
    std::vector<int> renderSteppedRefs_;

    void unrefRunServiceListeners(std::vector<int>& refs) {
        if (!L) { refs.clear(); return; }
        for (int ref : refs) {
            if (ref != LUA_NOREF)
                lua_unref(L, ref);
        }
        refs.clear();
    }

    void fireRunServiceEvent(std::vector<int>& refs, float dt, const char* label) {
        if (!L || refs.empty()) return;
        budgetExceeded = false;
        auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < refs.size(); ++i) {
            int ref = refs[i];
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) {
                lua_pop(L, 1);
                continue;
            }
            // Stepped riceve (tempo di gioco, dt), gli altri solo dt.
            const bool stepped = std::strcmp(label, "Stepped") == 0;
            if (stepped) lua_pushnumber(L, runTime_);
            lua_pushnumber(L, dt);
            callStart_ = std::chrono::steady_clock::now();
            if (timedPcall(stepped ? 2 : 1, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "%s error: %s\n", label, lastError.c_str());
                lua_pop(L, 1);
                if (lastError.find("budget") != std::string::npos)
                    budgetExceeded = true;
            }
        }
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - t0).count();
        if (us > budgetUs) {
            budgetExceeded = true;
            lastError = "Lua budget tempo superato (" + std::to_string(us) + "us)";
            unrefRunServiceListeners(refs);
            std::fprintf(stderr, "[Lua] %s — %s disconnesso\n", lastError.c_str(), label);
        }
    }
    std::vector<int> pendingDestroy_;
    std::vector<uint64_t> partPartTouchFired_;
    std::vector<TaskWait> taskWaits_;
    std::vector<ChildWait> childWaits_;
    // ModuleScript require cache: key = "Service/Name" → registry ref of return value.
    std::map<std::string, int> moduleCache_;
    std::vector<std::string> requireStack_; // cycle detection

    void tickTaskWaits(float dt) {
        if (!L || taskWaits_.empty()) return;
        for (size_t i = 0; i < taskWaits_.size();) {
            taskWaits_[i].remaining -= dt;
            if (taskWaits_[i].remaining > 0.f) {
                ++i;
                continue;
            }
            const TaskWait w = taskWaits_[i];
            taskWaits_.erase(taskWaits_.begin() + (int)i);
            lua_rawgeti(L, LUA_REGISTRYINDEX, w.coRef);
            lua_State* co = lua_tothread(L, -1);
            lua_pop(L, 1);
            lua_unref(L, w.coRef);
            if (!co) continue;
            callStart_ = std::chrono::steady_clock::now();
            int nargs = w.nargs;
            if (nargs < 0) {
                // task.wait() restituisce quanto si e' aspettato davvero, come Roblox.
                lua_pushnumber(co, w.total - w.remaining); // remaining <= 0: anche lo sforamento
                nargs = 1;
            }
            // Luau non ha il 4o parametro d'uscita "nres" di Lua 5.4:
            // lo ricaviamo con lua_gettop dopo la resume (i valori
            // restituiti/yieldati restano comunque sullo stack di co).
            int st = lua_resume(co, L, nargs);
            int nres = lua_gettop(co);
            if (st != LUA_OK && st != LUA_YIELD) {
                lastError = lua_tostring(co, -1) ? lua_tostring(co, -1) : "task resume error";
                std::fprintf(stderr, "task error: %s\n", lastError.c_str());
                lua_pop(co, 1);
            } else if (nres > 0) {
                lua_pop(co, nres);
            }
        }
    }

    void destroyPartImmediate(int idx) {
        if (!workspace || idx < 0 || idx >= (int)workspace->parts.size()) return;
        auto& touched = scriptControls().partTouched;
        auto it = touched.find(idx);
        if (it != touched.end()) {
            if (L) {
                for (int ref : it->second)
                    if (ref != LUA_NOREF) lua_unref(L,ref);
            }
            touched.erase(it);
        }
        std::map<int, std::vector<int>> remapped;
        for (auto& kv : touched) {
            if (kv.first > idx)
                remapped[kv.first - 1] = std::move(kv.second);
            else if (kv.first < idx)
                remapped[kv.first] = std::move(kv.second);
        }
        touched.swap(remapped);
        if (!serverTouching_.empty()) {
            std::set<std::pair<int, int>> moved;
            for (const auto& k : serverTouching_)
                if (k.second != idx) moved.insert({k.first, k.second > idx ? k.second - 1 : k.second});
            serverTouching_.swap(moved);
        }
        if (L) notifyPrelude(L, "partRemoved", idx, nullptr);
        workspace->parts.erase(workspace->parts.begin() + idx);
        remapAfterErase(idx);
    }

    // ---- Indici delle part dopo un Destroy ----
    //
    // Il motore conosce le part per posizione nell'elenco: togliere la part
    // idx fa scalare di uno tutte quelle dopo. Tutto cio' che le ricorda per
    // numero va aggiornato, altrimenti punta alla part sbagliata (prima:
    // variabili degli script, vincoli, part figlie, etichette...).

    /** Tabelle Lua che puntano a una part per indice. Elenco "weak": non le tiene vive. */
    static void trackPartTable(lua_State* ls, int tidx) {
        tidx = lua_absindex(ls, tidx);
        lua_getfield(ls, LUA_REGISTRYINDEX, "BlockyLiveParts");
        if (!lua_istable(ls, -1)) {
            lua_pop(ls, 1);
            lua_newtable(ls);
            lua_newtable(ls);
            lua_pushstring(ls, "k");
            lua_setfield(ls, -2, "__mode");
            lua_setmetatable(ls, -2);
            lua_pushvalue(ls, -1);
            lua_setfield(ls, LUA_REGISTRYINDEX, "BlockyLiveParts");
        }
        lua_pushvalue(ls, tidx);
        lua_pushboolean(ls, 1);
        lua_rawset(ls, -3);
        lua_pop(ls, 1);
    }

    void remapPartTables(int removed) {
        if (!L) return;
        lua_getfield(L, LUA_REGISTRYINDEX, "BlockyLiveParts");
        if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            lua_pop(L, 1);   // resta la chiave: la tabella
            if (lua_istable(L, -1)) {
                lua_pushstring(L, "__partIndex");
                lua_rawget(L, -2);
                const bool num = lua_isnumber(L, -1);
                const int i = num ? (int)lua_tointeger(L, -1) : -1;
                lua_pop(L, 1);
                if (num && i >= removed) {
                    lua_pushstring(L, "__partIndex");
                    lua_pushinteger(L, i == removed ? -1 : i - 1);
                    lua_rawset(L, -3);
                }
            }
        }
        lua_pop(L, 1);
    }

    void remapAfterErase(int idx) {
        auto shift = [idx](int& i) {
            if (i == idx) i = -1;
            else if (i > idx) --i;
        };
        for (auto& p : workspace->parts) shift(p.parentIndex);
        partconstraints::onPartErased(*workspace, idx);
        for (auto& l : workspace->labels) shift(l.adorneePart);
        auto& sc = scriptControls();
        {
            std::map<int, std::vector<int>> moved;
            for (auto& kv : sc.partChildAdded) {
                if (kv.first == idx) {
                    if (L) for (int ref : kv.second) if (ref != LUA_NOREF) lua_unref(L, ref);
                    continue;
                }
                moved[kv.first > idx ? kv.first - 1 : kv.first] = std::move(kv.second);
            }
            sc.partChildAdded.swap(moved);
        }
        {
            std::map<int, std::vector<int>> moved;
            for (auto& kv : sc.partClicked) {
                if (kv.first == idx) {
                    if (L) for (int ref : kv.second) if (ref != LUA_NOREF) lua_unref(L, ref);
                    continue;
                }
                moved[kv.first > idx ? kv.first - 1 : kv.first] = std::move(kv.second);
            }
            sc.partClicked.swap(moved);
        }
        for (auto it = sc.debris.begin(); it != sc.debris.end();) {
            shift(it->partIndex);
            it = it->partIndex < 0 ? sc.debris.erase(it) : std::next(it);
        }
        for (auto& a : sc.attachments) shift(a.partIndex);
        for (auto& e : scriptEmitters_) shift(e.partIndex);
        if (proximity) proximity->onPartErased(idx);
        // Attributi e loro listener: chiave "indicePart:nome".
        auto remapKeys = [idx](auto& m) {
            std::remove_reference_t<decltype(m)> moved;
            for (auto& kv : m) {
                const size_t colon = kv.first.find(':');
                if (colon == std::string::npos) { moved.emplace(kv.first, std::move(kv.second)); continue; }
                const int pi = std::atoi(kv.first.c_str());
                if (pi == idx) continue;
                const int ni = pi > idx ? pi - 1 : pi;
                moved.emplace(std::to_string(ni) + kv.first.substr(colon), std::move(kv.second));
            }
            m.swap(moved);
        };
        remapKeys(studio_pro_apis::attributes());
        remapKeys(sc.attributeChanged);
        remapPartTables(idx);
    }

    std::string safeToString(int idx) {
        const char* s = lua_tostring(L, idx);
        return s ? s : "(nil)";
    }

    static LuaRuntime* fromState(lua_State* ls) {
        lua_getfield(ls, LUA_REGISTRYINDEX, "BlockyEngine");
        auto* rt = static_cast<LuaRuntime*>(lua_touserdata(ls, -1));
        lua_pop(ls, 1);
        if (rt) return rt;
        lua_getglobal(ls, "__ROBLOX_ENGINE");
        rt = static_cast<LuaRuntime*>(lua_touserdata(ls, -1));
        lua_pop(ls, 1);
        return rt;
    }

    static int partArgIndex(lua_State* ls) {
        if (lua_gettop(ls) >= 2 && lua_istable(ls, 2)) return 2;
        if (lua_gettop(ls) >= 1 && lua_istable(ls, 1)) return 1;
        return 0;
    }

    static int fnArgIndex(lua_State* ls) {
        if (lua_gettop(ls) >= 2 && lua_isfunction(ls, 2)) return 2;
        if (lua_gettop(ls) >= 1 && lua_isfunction(ls, 1)) return 1;
        return 0;
    }

    static bool readVec3Field(lua_State* ls, int tableIdx, const char* key, glm::vec3& out) {
        if (!lua_istable(ls, tableIdx)) return false;
        lua_getfield(ls, tableIdx, key);
        if (!lua_istable(ls, -1)) {
            lua_pop(ls, 1);
            return false;
        }
        lua_getfield(ls, -1, "x"); out.x = (float)luaL_optnumber(ls, -1, 0); lua_pop(ls, 1);
        lua_getfield(ls, -1, "y"); out.y = (float)luaL_optnumber(ls, -1, 0); lua_pop(ls, 1);
        lua_getfield(ls, -1, "z"); out.z = (float)luaL_optnumber(ls, -1, 0); lua_pop(ls, 1);
        lua_pop(ls, 1);
        return true;
    }

    static bool readColor3Field(lua_State* ls, int tableIdx, const char* key, glm::vec3& out) {
        if (!lua_istable(ls, tableIdx)) return false;
        lua_getfield(ls, tableIdx, key);
        if (!lua_istable(ls, -1)) {
            lua_pop(ls, 1);
            return false;
        }
        lua_getfield(ls, -1, "r"); out.r = (float)luaL_optnumber(ls, -1, 1); lua_pop(ls, 1);
        lua_getfield(ls, -1, "g"); out.g = (float)luaL_optnumber(ls, -1, 1); lua_pop(ls, 1);
        lua_getfield(ls, -1, "b"); out.b = (float)luaL_optnumber(ls, -1, 1); lua_pop(ls, 1);
        lua_pop(ls, 1);
        return true;
    }

    static std::string readStringField(lua_State* ls, int tableIdx, const char* key, const char* def) {
        if (!lua_istable(ls, tableIdx)) return def;
        lua_getfield(ls, tableIdx, key);
        const char* s = lua_tostring(ls, -1);
        std::string out = s ? s : def;
        lua_pop(ls, 1);
        return out;
    }

    static void pushVec3(lua_State* ls, float x, float y, float z) {
        lua_newtable(ls);
        lua_pushnumber(ls, x); lua_setfield(ls, -2, "x");
        lua_pushnumber(ls, y); lua_setfield(ls, -2, "y");
        lua_pushnumber(ls, z); lua_setfield(ls, -2, "z");
        lua_pushnumber(ls, x); lua_setfield(ls, -2, "X");
        lua_pushnumber(ls, y); lua_setfield(ls, -2, "Y");
        lua_pushnumber(ls, z); lua_setfield(ls, -2, "Z");
        ensureVec3Meta(ls);
        lua_getfield(ls, LUA_REGISTRYINDEX, "BlockyVec3MT");
        lua_setmetatable(ls, -2);
    }

    static void pushColor3(lua_State* ls, float r, float g, float b) {
        lua_newtable(ls);
        lua_pushnumber(ls, r); lua_setfield(ls, -2, "r");
        lua_pushnumber(ls, g); lua_setfield(ls, -2, "g");
        lua_pushnumber(ls, b); lua_setfield(ls, -2, "b");
        lua_pushnumber(ls, r); lua_setfield(ls, -2, "R");
        lua_pushnumber(ls, g); lua_setfield(ls, -2, "G");
        lua_pushnumber(ls, b); lua_setfield(ls, -2, "B");
    }

    static void ensureVec3Meta(lua_State* ls) {
        lua_getfield(ls, LUA_REGISTRYINDEX, "BlockyVec3MT");
        if (lua_istable(ls, -1)) { lua_pop(ls, 1); return; }
        lua_pop(ls, 1);
        lua_newtable(ls);
        blockyPushCFunction(ls, luaVec3Add); lua_setfield(ls, -2, "__add");
        blockyPushCFunction(ls, luaVec3Sub); lua_setfield(ls, -2, "__sub");
        blockyPushCFunction(ls, luaVec3Mul); lua_setfield(ls, -2, "__mul");
        blockyPushCFunction(ls, luaVec3Unm); lua_setfield(ls, -2, "__unm");
        blockyPushCFunction(ls, luaVec3Index); lua_setfield(ls, -2, "__index");
        lua_pushstring(ls, "Vector3"); lua_setfield(ls, -2, "__metatable");
        lua_setfield(ls, LUA_REGISTRYINDEX, "BlockyVec3MT");
    }

    static bool readVec3FromStack(lua_State* ls, int idx, glm::vec3& out) {
        if (!lua_istable(ls, idx)) return false;
        return readVec3Arg(ls, idx, out);
    }

    static int luaVec3Add(lua_State* ls) {
        glm::vec3 a{}, b{};
        if (!readVec3FromStack(ls, 1, a) || !readVec3FromStack(ls, 2, b))
            return blockyLuaError(ls, "Vector3 + Vector3");
        pushVec3(ls, a.x + b.x, a.y + b.y, a.z + b.z);
        return 1;
    }
    static int luaVec3Sub(lua_State* ls) {
        glm::vec3 a{}, b{};
        if (!readVec3FromStack(ls, 1, a) || !readVec3FromStack(ls, 2, b))
            return blockyLuaError(ls, "Vector3 - Vector3");
        pushVec3(ls, a.x - b.x, a.y - b.y, a.z - b.z);
        return 1;
    }
    static int luaVec3Mul(lua_State* ls) {
        glm::vec3 a{};
        if (lua_isnumber(ls, 1) && readVec3FromStack(ls, 2, a)) {
            float s = (float)lua_tonumber(ls, 1);
            pushVec3(ls, a.x * s, a.y * s, a.z * s);
            return 1;
        }
        if (readVec3FromStack(ls, 1, a) && lua_isnumber(ls, 2)) {
            float s = (float)lua_tonumber(ls, 2);
            pushVec3(ls, a.x * s, a.y * s, a.z * s);
            return 1;
        }
        glm::vec3 b{};
        if (readVec3FromStack(ls, 1, a) && readVec3FromStack(ls, 2, b)) {
            pushVec3(ls, a.x * b.x, a.y * b.y, a.z * b.z);
            return 1;
        }
        return blockyLuaError(ls, "Vector3 * number");
    }
    static int luaVec3Unm(lua_State* ls) {
        glm::vec3 a{};
        if (!readVec3FromStack(ls, 1, a))
            return blockyLuaError(ls, "-Vector3");
        pushVec3(ls, -a.x, -a.y, -a.z);
        return 1;
    }
    static int luaVector3Dot(lua_State* ls) {
        glm::vec3 a{}, b{};
        if (!readVec3FromStack(ls, 1, a) || !readVec3FromStack(ls, 2, b))
            return blockyLuaError(ls, "Vector3.Dot(a, b)");
        lua_pushnumber(ls, a.x * b.x + a.y * b.y + a.z * b.z);
        return 1;
    }
    static int luaVector3Cross(lua_State* ls) {
        glm::vec3 a{}, b{};
        if (!readVec3FromStack(ls, 1, a) || !readVec3FromStack(ls, 2, b))
            return blockyLuaError(ls, "Vector3.Cross(a, b)");
        pushVec3(ls, a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
        return 1;
    }
    static int luaVector3Lerp(lua_State* ls) {
        glm::vec3 a{}, b{};
        if (!readVec3FromStack(ls, 1, a) || !readVec3FromStack(ls, 2, b))
            return blockyLuaError(ls, "Vector3.Lerp(a, b, t)");
        float t = (float)luaL_optnumber(ls, 3, 0.5);
        pushVec3(ls, a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
        return 1;
    }
    static int luaVec3Index(lua_State* ls) {
        const char* k = luaL_checkstring(ls, 2);
        glm::vec3 a{};
        if (!readVec3FromStack(ls, 1, a)) return 0;
        if (!std::strcmp(k, "Magnitude") || !std::strcmp(k, "magnitude")) {
            lua_pushnumber(ls, std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z));
            return 1;
        }
        if (!std::strcmp(k, "Unit") || !std::strcmp(k, "unit")) {
            float L = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
            if (L < 1e-8f) pushVec3(ls, 0, 0, 0);
            else pushVec3(ls, a.x / L, a.y / L, a.z / L);
            return 1;
        }
        if (!std::strcmp(k, "Dot")) { blockyPushCFunction(ls, luaVector3Dot); return 1; }
        if (!std::strcmp(k, "Cross")) { blockyPushCFunction(ls, luaVector3Cross); return 1; }
        if (!std::strcmp(k, "Lerp")) { blockyPushCFunction(ls, luaVector3Lerp); return 1; }
        lua_pushnil(ls);
        return 1;
    }

    static void pushPart(lua_State* ls, const WorkspacePart& p) {
        lua_newtable(ls);
        lua_pushstring(ls, p.name.c_str());
        lua_setfield(ls, -2, "Name");
        pushVec3(ls, p.position.x, p.position.y, p.position.z);
        lua_setfield(ls, -2, "Position");
        pushVec3(ls, p.size.x, p.size.y, p.size.z);
        lua_setfield(ls, -2, "Size");
        pushVec3(ls, p.rotation.x, p.rotation.y, p.rotation.z);
        lua_setfield(ls, -2, "Orientation");
        pushColor3(ls, p.color.r, p.color.g, p.color.b);
        lua_setfield(ls, -2, "BrickColor");
        partprops::luaFillTable(ls, p);
        lua_pushstring(ls, partMaterialName(p.material));
        lua_setfield(ls, -2, "Material");
        lua_pushstring(ls, partShapeName(p.shape));
        lua_setfield(ls, -2, "Shape");
        lua_pushnumber(ls, p.emissive);
        lua_setfield(ls, -2, "Emissive");
        lua_pushboolean(ls, p.pointLight ? 1 : 0);
        lua_setfield(ls, -2, "PointLight");
        lua_pushnumber(ls, p.lightRange);
        lua_setfield(ls, -2, "LightRange");
        lua_pushnumber(ls, p.lightBrightness);
        lua_setfield(ls, -2, "LightBrightness");
        lua_pushboolean(ls, p.canDestroy ? 1 : 0);
        lua_setfield(ls, -2, "CanDestroy");
        lua_pushnumber(ls, p.health);
        lua_setfield(ls, -2, "Health");
    }

    // ---- Segnali delle part vere (Changed, GetPropertyChangedSignal, Destroying) ----
    //
    // Vivono nel prelude per indice di part (le tabelle delle part si ricreano
    // a ogni accesso, l'indice no); il C++ li fa scattare quando una proprieta'
    // cambia, quando la part viene distrutta e quando gli indici scalano.

    static void pushPartSignal(lua_State* ls, int idx, const char* kind) {
        if (idx < 0 || !pushPreludeField(ls, "partSignal")) { lua_pushnil(ls); return; }
        lua_pushinteger(ls, idx);
        if (kind) lua_pushstring(ls, kind); else lua_pushnil(ls);
        if (lua_pcall(ls, 2, 1, 0) != LUA_OK) { lua_pop(ls, 1); lua_pushnil(ls); }
    }

    static void notifyPrelude(lua_State* ls, const char* fn, int idx, const char* str) {
        if (!pushPreludeField(ls, fn)) return;
        lua_pushinteger(ls, idx);
        if (str) lua_pushstring(ls, str); else lua_pushnil(ls);
        if (lua_pcall(ls, 2, 0, 0) != LUA_OK) lua_pop(ls, 1);
    }

    /** part ha ricevuto un Parent: il prelude la mette tra i figli se il parent e' un Instance Luau. */
    static void notifyPartParented(lua_State* ls, int partTableIdx, int parentIdx) {
        if (partTableIdx < 0) partTableIdx = lua_gettop(ls) + 1 + partTableIdx;
        if (parentIdx < 0) parentIdx = lua_gettop(ls) + 1 + parentIdx;
        if (!pushPreludeField(ls, "partParented")) return;
        lua_pushvalue(ls, partTableIdx);
        lua_pushvalue(ls, parentIdx);
        if (lua_pcall(ls, 2, 0, 0) != LUA_OK) lua_pop(ls, 1);
    }

    static int luaLivePartPropSignal(lua_State* ls) {
        pushPartSignal(ls, partIndexFromSelf(ls, 1), luaL_checkstring(ls, 2));
        return 1;
    }

    // rawset per gli script: come in Roblox, ma non sugli oggetti del motore
    // (aggirerebbe la protezione anti-executor). Stesse regole di setmetatable.
    static int luaSafeRawset(lua_State* ls) {
        luaL_checktype(ls, 1, LUA_TTABLE);
        luaL_checkany(ls, 2);
        if (lua_getreadonly(ls, 1)) return blockyLuaError(ls, "rawset: tabella in sola lettura");
        if (lua_getmetatable(ls, 1)) {
            lua_pushstring(ls, "__metatable");
            lua_rawget(ls, -2);
            const bool locked = !lua_isnil(ls, -1);
            lua_pop(ls, 1);
            if (locked || isEngineMetatable(ls, -1)) return blockyLuaError(ls, "rawset su un oggetto protetto");
            lua_pop(ls, 1);
        }
        lua_settop(ls, 3);
        lua_rawset(ls, 1);
        lua_settop(ls, 1);
        return 1;
    }

    // ---- Part creata con Instance.new, non ancora nel mondo ----
    //
    // Roblox: local p = Instance.new("Part"); p.CFrame = ...; p.Parent = workspace.
    // Finche' non ha un Parent e' una tabella di valori; dare il Parent la
    // aggiunge al mondo e LA STESSA tabella diventa la part viva (gli script
    // continuano a usare la loro variabile).

    static int luaPendingPartNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (key && std::strcmp(key, "CFrame") == 0) {
            glm::vec3 pos{0.f};
            if (readVec3Arg(ls, 3, pos)) {
                pushVec3(ls, pos.x, pos.y, pos.z);
                lua_setfield(ls, 1, "Position");
            }
            if (pushPreludeField(ls, "cframeToEulerYXZ")) {
                lua_pushvalue(ls, 3);
                if (lua_pcall(ls, 1, 3, 0) == LUA_OK) {
                    if (lua_isnumber(ls, -3)) {
                        pushVec3(ls, (float)lua_tonumber(ls, -3), (float)lua_tonumber(ls, -2), (float)lua_tonumber(ls, -1));
                        lua_setfield(ls, 1, "Orientation");
                    }
                    lua_pop(ls, 3);
                } else {
                    lua_pop(ls, 1);
                }
            }
            return 0;
        }
        if (key && std::strcmp(key, "Parent") == 0 && !lua_isnil(ls, 3)) {
            // Nel mondo con i valori messi finora...
            lua_pushcfunction(ls, luaWorkspaceAdd, "luaWorkspaceAdd");
            lua_pushvalue(ls, 1);
            lua_call(ls, 1, 1);
            if (!lua_istable(ls, -1)) { lua_pop(ls, 1); return 0; }
            const int live = lua_gettop(ls);
            // ...poi la tabella dello script prende il posto della part viva:
            // 1) via i valori vecchi (altrimenti, letti per primi, nasconderebbero quelli veri)
            std::vector<std::string> keys;
            lua_pushnil(ls);
            while (lua_next(ls, 1) != 0) {
                lua_pop(ls, 1);
                if (lua_type(ls, -1) == LUA_TSTRING) keys.push_back(lua_tostring(ls, -1));
            }
            for (const auto& k : keys) { lua_pushstring(ls, k.c_str()); lua_pushnil(ls); lua_rawset(ls, 1); }
            // 2) i campi della part viva (__partIndex e metodi)
            lua_pushnil(ls);
            while (lua_next(ls, live) != 0) {
                lua_pushvalue(ls, -2);
                lua_insert(ls, -2);
                lua_rawset(ls, 1);
            }
            // 3) la sua metatabella (Position, CFrame... letti dal mondo)
            if (!lua_getmetatable(ls, live)) lua_pushnil(ls);
            lua_setmetatable(ls, 1);
            trackPartTable(ls, 1);
            notifyPartParented(ls, 1, 3);
            lua_settop(ls, 3);
            return 0;
        }
        lua_rawset(ls, 1);
        return 0;
    }

    static int luaPendingPartIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (key && std::strcmp(key, "CFrame") == 0) {
            glm::vec3 pos{0.f}, rot{0.f};
            readVec3Field(ls, 1, "Position", pos);
            readVec3Field(ls, 1, "Orientation", rot);
            pushCFrameRot(ls, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z);
            return 1;
        }
        return 0;
    }

    static void setPendingPartMeta(lua_State* ls, int idx) {
        if (idx < 0) idx = lua_gettop(ls) + 1 + idx;
        lua_getfield(ls, LUA_REGISTRYINDEX, "BlockyPendingPartMT");
        if (!lua_istable(ls, -1)) {
            lua_pop(ls, 1);
            lua_newtable(ls);
            blockyPushCFunction(ls, luaPendingPartNewIndex);
            lua_setfield(ls, -2, "__newindex");
            blockyPushCFunction(ls, luaPendingPartIndex);
            lua_setfield(ls, -2, "__index");
            lua_pushvalue(ls, -1);
            lua_setfield(ls, LUA_REGISTRYINDEX, "BlockyPendingPartMT");
        }
        lua_setmetatable(ls, idx);
    }

    static int partIndexFromSelf(lua_State* ls, int selfIdx) {
        lua_getfield(ls, selfIdx, "__partIndex");
        int i = (int)lua_tointeger(ls, -1);
        lua_pop(ls, 1);
        return i;
    }

    static WorkspacePart* livePart(lua_State* ls, int selfIdx) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return nullptr;
        int i = partIndexFromSelf(ls, selfIdx);
        if (i < 0 || i >= (int)rt->workspace->parts.size()) return nullptr;
        return &rt->workspace->parts[i];
    }

    // script.Parent per una Label/TextButton: Name/Text/Visible + Connect(fn)
    // per registrare l'azione al click (stessa coda di StarterGui:ConnectButton,
    // ma con il nome del bottone gia' precompilato).
    static WorkspaceLabel* liveLabel(lua_State* ls, int selfIdx) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return nullptr;
        lua_getfield(ls, selfIdx, "__labelIndex");
        int i = (int)lua_tointeger(ls, -1);
        lua_pop(ls, 1);
        if (i < 0 || i >= (int)rt->workspace->labels.size()) return nullptr;
        return &rt->workspace->labels[i];
    }

    static int luaLiveLabelConnect(lua_State* ls) {
        WorkspaceLabel* l = liveLabel(ls, 1);
        if (!l) return 0;
        if (!lua_isfunction(ls, 2))
            return blockyLuaError(ls, "script.Parent:Connect(function)");
        lua_pushvalue(ls, 2);
        int ref = blockyLuaRef(ls);
        scriptControls().guiButtonActivated[l->name].push_back(ref);
        lua_pushinteger(ls, ref);
        return 1;
    }

    static int luaLiveLabelIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key) return 0;
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        if (!lua_isnil(ls, -1)) return 1;
        lua_pop(ls, 1);

        WorkspaceLabel* l = liveLabel(ls, 1);
        if (!l) return 0;
        if (std::strcmp(key, "Text") == 0) {
            lua_pushstring(ls, l->text.c_str());
            return 1;
        }
        if (std::strcmp(key, "Visible") == 0) {
            lua_pushboolean(ls, l->visible);
            return 1;
        }
        if (std::strcmp(key, "Name") == 0) {
            lua_pushstring(ls, l->name.c_str());
            return 1;
        }
        return 0;
    }

    static int luaLiveLabelNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        WorkspaceLabel* l = liveLabel(ls, 1);
        if (!l || !key) { lua_rawset(ls, 1); return 0; }
        if (std::strcmp(key, "Text") == 0) {
            const char* v = lua_tostring(ls, 3);
            l->text = v ? v : "";
            return 0;
        }
        if (std::strcmp(key, "Visible") == 0) {
            l->visible = lua_toboolean(ls, 3);
            return 0;
        }
        if (std::strcmp(key, "Adornee") == 0) {
            if (lua_isnil(ls, 3)) {
                l->adorneePart = -1;
            } else if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1))
                    l->adorneePart = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            return 0;
        }
        if (std::strcmp(key, "StudsOffset") == 0 && lua_istable(ls, 3)) {
            glm::vec3 o{0, l->studsOffsetY, 0};
            readVec3Arg(ls, 3, o);
            l->studsOffsetY = o.y;
            return 0;
        }
        lua_rawset(ls, 1);
        return 0;
    }

    static void pushLiveLabel(lua_State* ls, int labelIndex) {
        lua_newtable(ls);
        lua_pushinteger(ls, labelIndex);
        lua_setfield(ls, -2, "__labelIndex");
        pushSignalMethod(ls, luaLiveLabelConnect, "luaLiveLabelConnect", 0);
        lua_setfield(ls, -2, "Connect");
        pushSignalMethod(ls, luaLiveLabelConnect, "luaLiveLabelConnect", 1);
        lua_setfield(ls, -2, "Once");
        pushSignalMethod(ls, luaLiveLabelConnect, "luaLiveLabelConnect", 2);
        lua_setfield(ls, -2, "Wait");

        auto* rt = fromState(ls);
            if (rt && rt->workspace && labelIndex >= 0
            && labelIndex < (int)rt->workspace->labels.size()) {
            const auto& l = rt->workspace->labels[labelIndex];
            lua_pushstring(ls, l.name.c_str());
            lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, l.className.empty()
                ? (l.isButton() ? "TextButton" : "TextLabel")
                : l.className.c_str());
            lua_setfield(ls, -2, "ClassName");
            if (l.adorneePart >= 0) {
                pushLivePart(ls, l.adorneePart);
                lua_setfield(ls, -2, "Adornee");
            }
        }

        lua_newtable(ls);
        blockyPushCFunction(ls, luaLiveLabelIndex);
        lua_setfield(ls, -2, "__index");
        blockyPushCFunction(ls, luaLiveLabelNewIndex);
        lua_setfield(ls, -2, "__newindex");
        lua_setmetatable(ls, -2);
    }

    static bool readVec3Arg(lua_State* ls, int arg, glm::vec3& out) {
        if (lua_istable(ls, arg)) {
            lua_getfield(ls, arg, "x");
            if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "X"); }
            out.x = (float)luaL_optnumber(ls, -1, 0); lua_pop(ls, 1);
            lua_getfield(ls, arg, "y");
            if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "Y"); }
            out.y = (float)luaL_optnumber(ls, -1, 0); lua_pop(ls, 1);
            lua_getfield(ls, arg, "z");
            if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "Z"); }
            out.z = (float)luaL_optnumber(ls, -1, 0); lua_pop(ls, 1);
            return true;
        }
        if (lua_isnumber(ls, arg)) {
            out.x = (float)lua_tonumber(ls, arg);
            out.y = (float)luaL_optnumber(ls, arg + 1, 0);
            out.z = (float)luaL_optnumber(ls, arg + 2, 0);
            return true;
        }
        return false;
    }

    static void pushLivePart(lua_State* ls, int partIndex) {
        lua_newtable(ls);
        lua_pushinteger(ls, partIndex);
        lua_setfield(ls, -2, "__partIndex");
        trackPartTable(ls, -1);
        blockyPushCFunction(ls, luaLivePartSetPosition);
        lua_setfield(ls, -2, "SetPosition");
        blockyPushCFunction(ls, luaLivePartGetPosition);
        lua_setfield(ls, -2, "GetPosition");
        blockyPushCFunction(ls, luaLivePartDestroy);
        lua_setfield(ls, -2, "Destroy");
        blockyPushCFunction(ls, luaLivePartExplode);
        lua_setfield(ls, -2, "Explode");
        blockyPushCFunction(ls, luaLivePartApplyImpulse);
        lua_setfield(ls, -2, "ApplyImpulse");
        blockyPushCFunction(ls, luaLivePartGetVelocity);
        lua_setfield(ls, -2, "GetVelocity");
        blockyPushCFunction(ls, luaLivePartSetVelocity);
        lua_setfield(ls, -2, "SetVelocity");
        blockyPushCFunction(ls, luaLivePartGetMass);
        lua_setfield(ls, -2, "GetMass");
        blockyPushCFunction(ls, luaPartFindFirstChild);
        lua_setfield(ls, -2, "FindFirstChild");
        blockyPushCFunction(ls, luaPartGetChildren);
        lua_setfield(ls, -2, "GetChildren");
        blockyPushCFunction(ls, luaPartWaitForChild);
        lua_setfield(ls, -2, "WaitForChild");
        blockyPushCFunction(ls, luaLivePartSetAttribute);
        lua_setfield(ls, -2, "SetAttribute");
        blockyPushCFunction(ls, luaLivePartGetAttribute);
        lua_setfield(ls, -2, "GetAttribute");
        blockyPushCFunction(ls, luaPartIsA);
        lua_setfield(ls, -2, "IsA");
        blockyPushCFunction(ls, luaPartClone);
        lua_setfield(ls, -2, "Clone");
        blockyPushCFunction(ls, luaPartGetDescendants);
        lua_setfield(ls, -2, "GetDescendants");
        blockyPushCFunction(ls, luaPartFindFirstChildOfClass);
        lua_setfield(ls, -2, "FindFirstChildOfClass");
        blockyPushCFunction(ls, luaPartFindFirstAncestor);
        lua_setfield(ls, -2, "FindFirstAncestor");
        blockyPushCFunction(ls, luaPartFindFirstAncestorOfClass);
        lua_setfield(ls, -2, "FindFirstAncestorOfClass");
        blockyPushCFunction(ls, luaPartGetPivot);
        lua_setfield(ls, -2, "GetPivot");
        blockyPushCFunction(ls, luaPartPivotTo);
        lua_setfield(ls, -2, "PivotTo");
        blockyPushCFunction(ls, luaPartGetAttributeChangedSignal);
        lua_setfield(ls, -2, "GetAttributeChangedSignal");
        blockyPushCFunction(ls, luaLivePartPropSignal);
        lua_setfield(ls, -2, "GetPropertyChangedSignal");

        auto* rt = fromState(ls);
        if (rt && rt->workspace && partIndex >= 0
            && partIndex < (int)rt->workspace->parts.size()) {
            // Name NON va messo raw: passa da __index/__newindex, cosi' p.Name = x
            // rinomina davvero la part e accende Changed.
            lua_pushstring(ls, "Part");
            lua_setfield(ls, -2, "ClassName");
        }

        lua_newtable(ls);
        blockyPushCFunction(ls, luaLivePartIndex);
        lua_setfield(ls, -2, "__index");
        blockyPushCFunction(ls, luaLivePartNewIndex);
        lua_setfield(ls, -2, "__newindex");
        lua_setmetatable(ls, -2);
    }

    static int luaLivePartGetPosition(lua_State* ls) {
        WorkspacePart* p = livePart(ls, 1);
        if (!p) return 0;
        pushVec3(ls, p->position.x, p->position.y, p->position.z);
        return 1;
    }

    static int luaLivePartSetPosition(lua_State* ls) {
        WorkspacePart* p = livePart(ls, 1);
        if (!p) return 0;
        int arg = 2;
        if (lua_istable(ls, 1) && lua_gettop(ls) >= 2) arg = 2;
        glm::vec3 pos = p->position;
        if (!readVec3Arg(ls, arg, pos))
            return blockyLuaError(ls, "SetPosition(Vector3) oppure SetPosition(x,y,z)");
        p->position = pos;
        return 0;
    }

    static int luaLivePartIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key) return 0;
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        if (!lua_isnil(ls, -1))
            return 1;
        lua_pop(ls, 1);

        WorkspacePart* p = livePart(ls, 1);
        if (!p) return 0;
        if (std::strcmp(key, "Changed") == 0) { pushPartSignal(ls, partIndexFromSelf(ls, 1), nullptr); return 1; }
        if (std::strcmp(key, "Destroying") == 0) { pushPartSignal(ls, partIndexFromSelf(ls, 1), "\x01" "destroying"); return 1; }
        if (std::strcmp(key, "Parent") == 0 && pushPreludeField(ls, "luaParentOfPart")) {
            lua_pushinteger(ls, partIndexFromSelf(ls, 1));
            if (lua_pcall(ls, 1, 1, 0) == LUA_OK) {
                if (!lua_isnil(ls, -1)) return 1;
            }
            lua_pop(ls, 1);
        }
        if (std::strcmp(key, "Position") == 0) {
            pushVec3(ls, p->position.x, p->position.y, p->position.z);
            return 1;
        }
        if (std::strcmp(key, "Size") == 0) {
            pushVec3(ls, p->size.x, p->size.y, p->size.z);
            return 1;
        }
        if (std::strcmp(key, "Orientation") == 0 || std::strcmp(key, "Rotation") == 0) {
            pushVec3(ls, p->rotation.x, p->rotation.y, p->rotation.z);
            return 1;
        }
        if (std::strcmp(key, "CFrame") == 0) {
            pushCFrameRot(ls, p->position.x, p->position.y, p->position.z, p->rotation.x, p->rotation.y, p->rotation.z);
            return 1;
        }
        if (std::strcmp(key, "Name") == 0) {
            lua_pushstring(ls, p->name.c_str());
            return 1;
        }
        if (std::strcmp(key, "Color") == 0 || std::strcmp(key, "BrickColor") == 0) {
            pushColor3(ls, p->color.r, p->color.g, p->color.b);
            return 1;
        }
        if (std::strcmp(key, "Material") == 0) {
            lua_pushstring(ls, partMaterialName(p->material));
            return 1;
        }
        if (std::strcmp(key, "Shape") == 0) {
            lua_pushstring(ls, partShapeName(p->shape));
            return 1;
        }
        if (std::strcmp(key, "Emissive") == 0) {
            lua_pushnumber(ls, p->emissive);
            return 1;
        }
        if (std::strcmp(key, "PointLight") == 0) {
            lua_pushboolean(ls, p->pointLight ? 1 : 0);
            return 1;
        }
        if (std::strcmp(key, "LightRange") == 0) {
            lua_pushnumber(ls, p->lightRange);
            return 1;
        }
        if (std::strcmp(key, "LightBrightness") == 0) {
            lua_pushnumber(ls, p->lightBrightness);
            return 1;
        }
        if (std::strcmp(key, "CanDestroy") == 0) {
            lua_pushboolean(ls, p->canDestroy ? 1 : 0);
            return 1;
        }
        if (std::strcmp(key, "Health") == 0) {
            lua_pushnumber(ls, p->health);
            return 1;
        }
        if (partprops::luaGet(ls, key, *p))
            return 1;
        if (std::strcmp(key, "Touched") == 0) {
            lua_newtable(ls);
            lua_pushinteger(ls, partIndexFromSelf(ls, 1));
            lua_setfield(ls, -2, "__partIndex");
            pushSignalMethod(ls, luaPartTouchedConnect, "luaPartTouchedConnect", 0);
            lua_setfield(ls, -2, "Connect");
            pushSignalMethod(ls, luaPartTouchedConnect, "luaPartTouchedConnect", 1);
            lua_setfield(ls, -2, "Once");
            pushSignalMethod(ls, luaPartTouchedConnect, "luaPartTouchedConnect", 2);
            lua_setfield(ls, -2, "Wait");
            return 1;
        }
        if (std::strcmp(key, "ChildAdded") == 0) {
            lua_newtable(ls);
            lua_pushinteger(ls, partIndexFromSelf(ls, 1));
            lua_setfield(ls, -2, "__partIndex");
            pushSignalMethod(ls, luaPartChildAddedConnect, "luaPartChildAddedConnect", 0);
            lua_setfield(ls, -2, "Connect");
            pushSignalMethod(ls, luaPartChildAddedConnect, "luaPartChildAddedConnect", 1);
            lua_setfield(ls, -2, "Once");
            pushSignalMethod(ls, luaPartChildAddedConnect, "luaPartChildAddedConnect", 2);
            lua_setfield(ls, -2, "Wait");
            return 1;
        }
        if (std::strcmp(key, "Velocity") == 0 || std::strcmp(key, "AssemblyLinearVelocity") == 0) {
            auto* rt = fromState(ls);
            int idx = partIndexFromSelf(ls, 1);
            float x = 0, y = 0, z = 0;
            if (rt && rt->phys.getLinVel && rt->phys.world)
                rt->phys.getLinVel(rt->phys.world, idx, &x, &y, &z);
            pushVec3(ls, x, y, z);
            return 1;
        }
        if (std::strcmp(key, "Mass") == 0) {
            auto* rt = fromState(ls);
            int idx = partIndexFromSelf(ls, 1);
            float m = 0.f;
            if (rt && rt->phys.getMass && rt->phys.world)
                m = rt->phys.getMass(rt->phys.world, idx);
            lua_pushnumber(ls, m);
            return 1;
        }
        if (std::strcmp(key, "Parent") == 0) {
            auto* rt = fromState(ls);
            int idx = partIndexFromSelf(ls, 1);
            if (!rt || !rt->workspace || idx < 0) return 0;
            int pi = rt->workspace->parts[idx].parentIndex;
            if (pi >= 0 && pi < (int)rt->workspace->parts.size()) {
                pushLivePart(ls, pi);
                return 1;
            }
            lua_getglobal(ls, "workspace");
            return 1;
        }
        // Dot-access figlio: part.ChildName
        {
            auto* rt = fromState(ls);
            int idx = partIndexFromSelf(ls, 1);
            if (rt && rt->workspace && idx >= 0) {
                int child = rt->workspace->findPartChild(idx, key);
                if (child >= 0) { pushLivePart(ls, child); return 1; }
            }
        }
        return 0;
    }

    static int luaLivePartNewIndex(lua_State* ls) {
        const int idx = partIndexFromSelf(ls, 1);
        const std::string k = lua_tostring(ls, 2) ? lua_tostring(ls, 2) : "";
        const int r = luaLivePartNewIndexImpl(ls);
        if (!k.empty() && idx >= 0) {
            if (k == "Parent") notifyPartParented(ls, 1, 3);
            notifyPrelude(ls, "firePartChanged", idx, k.c_str());
        }
        return r;
    }

    static int luaLivePartNewIndexImpl(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        WorkspacePart* p = livePart(ls, 1);
        if (!p || !key) return 0;
        if (std::strcmp(key, "Position") == 0) {
            glm::vec3 pos = p->position;
            readVec3Arg(ls, 3, pos);
            p->position = pos;
            return 0;
        }
        if (std::strcmp(key, "Size") == 0) {
            glm::vec3 sz = p->size;
            readVec3Arg(ls, 3, sz);
            p->size = sz;
            return 0;
        }
        if (std::strcmp(key, "Orientation") == 0 || std::strcmp(key, "Rotation") == 0) {
            glm::vec3 rot = p->rotation;
            readVec3Arg(ls, 3, rot);
            p->rotation = rot;
            return 0;
        }
        if (std::strcmp(key, "CFrame") == 0) {
            // Posizione dai campi X/Y/Z, rotazione dalla matrice (gradi YXZ come le Part).
            glm::vec3 pos = p->position;
            readVec3Arg(ls, 3, pos);
            p->position = pos;
            if (pushPreludeField(ls, "cframeToEulerYXZ")) {
                lua_pushvalue(ls, 3);
                if (lua_pcall(ls, 1, 3, 0) == LUA_OK) {
                    if (lua_isnumber(ls, -3))
                        p->rotation = {(float)lua_tonumber(ls, -3), (float)lua_tonumber(ls, -2), (float)lua_tonumber(ls, -1)};
                    lua_pop(ls, 3);
                } else {
                    lua_pop(ls, 1);
                }
            }
            return 0;
        }
        if (partprops::luaSet(ls, key, 3, *p))
            return 0;
        if (std::strcmp(key, "Velocity") == 0 || std::strcmp(key, "AssemblyLinearVelocity") == 0) {
            auto* rt = fromState(ls);
            int idx = partIndexFromSelf(ls, 1);
            glm::vec3 v{0};
            readVec3Arg(ls, 3, v);
            if (rt && rt->phys.setLinVel && rt->phys.world)
                rt->phys.setLinVel(rt->phys.world, idx, v.x, v.y, v.z);
            return 0;
        }
        if (std::strcmp(key, "Name") == 0 && lua_isstring(ls, 3)) {
            p->name = lua_tostring(ls, 3);
            return 0;
        }
        if (std::strcmp(key, "Parent") == 0) {
            auto* rt = fromState(ls);
            int selfIdx = partIndexFromSelf(ls, 1);
            if (!rt || !rt->workspace || selfIdx < 0) return 0;
            int oldParent = p->parentIndex;
            int newParent = -1;
            bool changed = false;
            if (lua_isnil(ls, 3)) {
                newParent = -1;
                changed = (oldParent != -1);
                p->parentIndex = -1;
            } else if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) {
                    int parent = (int)lua_tointeger(ls, -1);
                    if (parent != selfIdx && parent >= -1
                        && parent < (int)rt->workspace->parts.size()) {
                        bool cycle = false;
                        for (int walk = parent; walk >= 0; ) {
                            if (walk == selfIdx) { cycle = true; break; }
                            walk = rt->workspace->parts[walk].parentIndex;
                        }
                        if (!cycle) {
                            newParent = parent;
                            changed = (oldParent != parent);
                            p->parentIndex = parent;
                        }
                    }
                } else {
                    changed = (oldParent != -1);
                    p->parentIndex = -1;
                    newParent = -1;
                }
                lua_pop(ls, 1);
            }
            if (changed && newParent >= 0)
                rt->firePartChildAdded(newParent, selfIdx);
            return 0;
        }
        if (std::strcmp(key, "Color") == 0 || std::strcmp(key, "BrickColor") == 0) {
            glm::vec3 col = p->color;
            if (lua_istable(ls, 3))
                readColor3Arg(ls, 3, col);
            else if (lua_isnumber(ls, 3)) {
                col.x = (float)lua_tonumber(ls, 3);
                col.y = (float)luaL_optnumber(ls, 4, col.x);
                col.z = (float)luaL_optnumber(ls, 5, col.x);
            }
            p->color = glm::clamp(col, glm::vec3(0.f), glm::vec3(1.f));
            return 0;
        }
        if (std::strcmp(key, "Material") == 0) {
            if (lua_isstring(ls, 3)) {
                p->setMaterial(partMaterialFromName(lua_tostring(ls, 3)));
                return 0;
            }
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "Name");
                if (lua_isstring(ls, -1))
                    p->setMaterial(partMaterialFromName(lua_tostring(ls, -1)));
                lua_pop(ls, 1);
                return 0;
            }
            if (lua_isnumber(ls, 3)) {
                int id = (int)lua_tointeger(ls, 3);
                if (id < 0) id = 0;
                if (id >= (int)PartMaterial::Count) id = 0;
                p->setMaterial((PartMaterial)id);
                return 0;
            }
            return 0;
        }
        if (std::strcmp(key, "Shape") == 0) {
            if (lua_isstring(ls, 3))
                p->shape = partShapeFromName(lua_tostring(ls, 3));
            else if (lua_isnumber(ls, 3)) {
                int id = (int)lua_tointeger(ls, 3);
                if (id < 0 || id >= (int)PartShape::Count) id = 0;
                p->shape = (PartShape)id;
            }
            if (p->shape != PartShape::Block)
                p->useStud = false;
            return 0;
        }
        if (std::strcmp(key, "Emissive") == 0) {
            if (lua_isboolean(ls, 3))
                p->emissive = lua_toboolean(ls, 3) ? 1.f : 0.f;
            else
                p->emissive = (float)lua_tonumber(ls, 3);
            if (p->emissive > 0.01f && p->material == PartMaterial::Plastic)
                p->setMaterial(PartMaterial::Neon);
            return 0;
        }
        if (std::strcmp(key, "PointLight") == 0) {
            p->pointLight = lua_toboolean(ls, 3) != 0;
            return 0;
        }
        if (std::strcmp(key, "LightRange") == 0) {
            p->lightRange = (float)lua_tonumber(ls, 3);
            return 0;
        }
        if (std::strcmp(key, "LightBrightness") == 0) {
            p->lightBrightness = (float)lua_tonumber(ls, 3);
            return 0;
        }
        if (std::strcmp(key, "CanDestroy") == 0) {
            p->canDestroy = lua_toboolean(ls, 3) != 0;
            return 0;
        }
        if (std::strcmp(key, "Health") == 0) {
            p->health = (float)lua_tonumber(ls, 3);
            return 0;
        }
        lua_rawset(ls, 1);
        return 0;
    }

    static int luaLivePartApplyImpulse(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = partIndexFromSelf(ls, 1);
        if (!rt || idx < 0) return 0;
        glm::vec3 imp{0};
        readVec3Arg(ls, 2, imp);
        if (rt->phys.applyImpulse && rt->phys.world)
            rt->phys.applyImpulse(rt->phys.world, idx, imp.x, imp.y, imp.z);
        return 0;
    }
    static int luaLivePartGetVelocity(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = partIndexFromSelf(ls, 1);
        float x = 0, y = 0, z = 0;
        if (rt && rt->phys.getLinVel && rt->phys.world)
            rt->phys.getLinVel(rt->phys.world, idx, &x, &y, &z);
        pushVec3(ls, x, y, z);
        return 1;
    }
    static int luaLivePartSetVelocity(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = partIndexFromSelf(ls, 1);
        if (!rt || idx < 0) return 0;
        glm::vec3 v{0};
        readVec3Arg(ls, 2, v);
        if (rt->phys.setLinVel && rt->phys.world)
            rt->phys.setLinVel(rt->phys.world, idx, v.x, v.y, v.z);
        return 0;
    }
    static int luaLivePartGetMass(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = partIndexFromSelf(ls, 1);
        float m = 0.f;
        if (rt && rt->phys.getMass && rt->phys.world)
            m = rt->phys.getMass(rt->phys.world, idx);
        lua_pushnumber(ls, m);
        return 1;
    }

    static int luaLivePartDestroy(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt) return 0;
        int idx = partIndexFromSelf(ls, 1);
        if (idx < 0 || !rt->workspace) return 0;
        if (idx >= (int)rt->workspace->parts.size()) return 0;
        if (!rt->workspace->parts[idx].canDestroy) return 0;
        notifyPrelude(ls, "partDestroying", idx, nullptr);
        rt->workspace->parts[idx].destroyPending = true;
        rt->pendingDestroy_.push_back(idx);
        // Come Roblox: Destroy toglie anche tutti i discendenti.
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            if (i == idx || rt->workspace->parts[(size_t)i].destroyPending) continue;
            int walk = rt->workspace->parts[(size_t)i].parentIndex;
            for (int guard = 0; walk >= 0 && walk < (int)rt->workspace->parts.size() && guard < 64; ++guard) {
                if (walk == idx) {
                    notifyPrelude(ls, "partDestroying", i, nullptr);
                    rt->workspace->parts[(size_t)i].destroyPending = true;
                    rt->pendingDestroy_.push_back(i);
                    break;
                }
                walk = rt->workspace->parts[(size_t)walk].parentIndex;
            }
        }
        // Dopo Destroy: p.Parent == nil e niente piu' accessi alla part (gli indici scaleranno).
        lua_pushstring(ls, "__partIndex");
        lua_pushinteger(ls, -1);
        lua_rawset(ls, 1);
        return 0;
    }

    static int luaLivePartExplode(lua_State* ls) {
        WorkspacePart* p = livePart(ls, 1);
        auto* rt = fromState(ls);
        int idx = partIndexFromSelf(ls, 1);
        if (!p || !rt || !rt->workspace || idx < 0) return 0;
        int count = (int)luaL_optinteger(ls, 2, 16);
        if (count < 4) count = 4;
        if (count > 24) count = 24;
        const glm::vec3 pos = p->position;
        const float radius = 10.f + (float)count * 0.12f;
        const float maxDmg = 40.f + (float)count * 1.0f;
        if (rt->particles)
            rt->particles->spawnExplosion(pos, radius, count);
        rt->applyExplosionPlayerDamage(pos, radius, maxDmg);
        const float dmgRadius = radius * 0.9f;
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            if (i == idx) continue;
            auto& o = rt->workspace->parts[i];
            if (!o.canDestroy) continue;
            float d = glm::length(o.position - pos);
            if (d > dmgRadius) continue;
            float dmg = 80.f * (1.f - d / dmgRadius);
            o.health -= dmg;
            if (o.health <= 0.f)
                rt->pendingDestroy_.push_back(i);
        }
        rt->pendingDestroy_.push_back(idx);
        return 0;
    }

    bool registerApi() {
        lua_pushlightuserdata(L, this);
        lua_setfield(L, LUA_REGISTRYINDEX, "BlockyEngine");
        lua_pushnil(L);
        lua_setglobal(L, "__ROBLOX_ENGINE");

        blockyPushCFunction(L, luaPrint);
        lua_setglobal(L, "print");

        blockyPushCFunction(L, luaAnimateFrame);
        lua_setglobal(L, "AnimateFrame");

        lua_newtable(L);
        blockyPushCFunction(L, luaVector3New);
        lua_setfield(L, -2, "new");
        pushVec3(L, 0, 0, 0); lua_setfield(L, -2, "zero");
        pushVec3(L, 1, 1, 1); lua_setfield(L, -2, "one");
        pushVec3(L, 1, 0, 0); lua_setfield(L, -2, "xAxis");
        pushVec3(L, 0, 1, 0); lua_setfield(L, -2, "yAxis");
        pushVec3(L, 0, 0, 1); lua_setfield(L, -2, "zAxis");
        lua_setglobal(L, "Vector3");

        lua_newtable(L);
        blockyPushCFunction(L, luaCFrameNew);
        lua_setfield(L, -2, "new");
        blockyPushCFunction(L, luaCFrameLookAt);
        lua_setfield(L, -2, "lookAt");
        lua_setglobal(L, "CFrame");

        lua_newtable(L);
        blockyPushCFunction(L, luaRegion3New);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "Region3");

        lua_getglobal(L, "math");
        if (lua_istable(L, -1)) {
            blockyPushCFunction(L, luaMathNoise);
            lua_setfield(L, -2, "noise");
            blockyPushCFunction(L, luaMathClamp);
            lua_setfield(L, -2, "clamp");
            blockyPushCFunction(L, luaMathSign);
            lua_setfield(L, -2, "sign");
            blockyPushCFunction(L, luaMathRound);
            lua_setfield(L, -2, "round");
        }
        lua_pop(L, 1);
        blockyPushCFunction(L, luaTime);
        lua_setglobal(L, "time");

        lua_newtable(L);
        blockyPushCFunction(L, luaColor3New);
        lua_setfield(L, -2, "new");
        blockyPushCFunction(L, luaColor3FromRGB);
        lua_setfield(L, -2, "fromRGB");
        lua_setglobal(L, "Color3");

        lua_newtable(L);
        blockyPushCFunction(L, luaBrickColorNew);
        lua_setfield(L, -2, "new");
        blockyPushCFunction(L, luaBrickColorPalette);
        lua_setfield(L, -2, "palette");
        blockyPushCFunction(L, luaBrickColorRandom);
        lua_setfield(L, -2, "random");
        lua_setglobal(L, "BrickColor");

        lua_newtable(L);
        blockyPushCFunction(L, luaInstanceNew);
        lua_setfield(L, -2, "new");
        lua_setglobal(L, "Instance");
        CrashHandler::note("registerApi: post Instance");

        lua_newtable(L);
        lua_newtable(L);
        for (int mi = 0; mi < (int)PartMaterial::Count; ++mi) {
            const char* nm = partMaterialName((PartMaterial)mi);
            lua_pushstring(L, nm);
            lua_setfield(L, -2, nm);
        }
        // Terrain (Enum.Material in Roblox Studio)
        const char* terrainMat[] = {
            "Grass", "Ground", "Sand", "Rock", "Water", "Snow", "Wood", "LeafyGrass",
            "CrackedLava", "Basalt", "Glacier", "Mud", "Slate", "Limestone", "Air",
            "Cobblestone", "WoodPlanks", "Brick", "Ice", "Granite", "Marble", "Pebble",
            "Sandstone", "Asphalt", "Pavement", "Salt", "Concrete", "DiamondPlate",
            "CorrodedMetal", "Fabric"
        };
        for (const char* nm : terrainMat) {
            lua_pushstring(L, nm);
            lua_setfield(L, -2, nm);
        }
        lua_setfield(L, -2, "Material");
        lua_newtable(L);
        lua_pushinteger(L, 0); lua_setfield(L, -2, "MouseButton1");
        lua_pushinteger(L, 1); lua_setfield(L, -2, "MouseButton2");
        lua_pushinteger(L, 2); lua_setfield(L, -2, "MouseButton3");
        lua_setfield(L, -2, "UserInputType");
        lua_setglobal(L, "Enum");

        lua_newtable(L);
        blockyPushCFunction(L, luaWorkspaceAdd);
        lua_setfield(L, -2, "Add");
        blockyPushCFunction(L, luaWorkspaceFind);
        lua_setfield(L, -2, "FindFirstChild");
        blockyPushCFunction(L, luaWorkspaceChildren);
        lua_setfield(L, -2, "GetChildren");
        blockyPushCFunction(L, luaWorkspaceSetPartPos);
        lua_setfield(L, -2, "SetPartPosition");
        blockyPushCFunction(L, luaWorkspaceFindPath);
        lua_setfield(L, -2, "FindPath");
        blockyPushCFunction(L, luaWorkspaceRebuildNav);
        lua_setfield(L, -2, "RebuildNav");
        blockyPushCFunction(L, luaWorkspaceIsBlocked);
        lua_setfield(L, -2, "IsBlocked");
        blockyPushCFunction(L, luaWorkspaceRaycast);
        lua_setfield(L, -2, "Raycast");
        blockyPushCFunction(L, luaWorkspaceGetPartBoundsInBox);
        lua_setfield(L, -2, "GetPartBoundsInBox");
        blockyPushCFunction(L, luaWorkspaceGetPartsInPart);
        lua_setfield(L, -2, "GetPartsInPart");
        blockyPushCFunction(L, luaWorkspaceFindPartsInRegion3);
        lua_setfield(L, -2, "FindPartsInRegion3");
        blockyPushCFunction(L, luaWorkspaceGetDescendants);
        lua_setfield(L, -2, "GetDescendants");
        blockyPushCFunction(L, luaWorkspaceFindFirstChildOfClass);
        lua_setfield(L, -2, "FindFirstChildOfClass");
        blockyPushCFunction(L, luaWorkspaceWaitForChild);
        lua_setfield(L, -2, "WaitForChild");
        lua_setglobal(L, "workspace");
        lua_getglobal(L, "workspace");
        lua_setglobal(L, "Workspace");
        CrashHandler::note("registerApi: post workspace");

        lua_newtable(L);
        blockyPushCFunction(L, luaEventCreate);
        lua_setfield(L, -2, "Create");
        pushSignalMethod(L, luaEventConnect, "luaEventConnect", 0);
        lua_setfield(L, -2, "Connect");
        pushSignalMethod(L, luaEventConnect, "luaEventConnect", 1);
        lua_setfield(L, -2, "Once");
        pushSignalMethod(L, luaEventConnect, "luaEventConnect", 2);
        lua_setfield(L, -2, "Wait");
        blockyPushCFunction(L, luaEventFire);
        lua_setfield(L, -2, "Fire");
        blockyPushCFunction(L, luaEventClear);
        lua_setfield(L, -2, "DisconnectAll");
        lua_setglobal(L, "Events");

        lua_newtable(L);
        blockyPushCFunction(L, luaHumSetJumpEnabled);
        lua_setfield(L, -2, "SetJumpEnabled");
        blockyPushCFunction(L, luaHumSetWalkEnabled);
        lua_setfield(L, -2, "SetWalkEnabled");
        blockyPushCFunction(L, luaHumSetWalkSpeed);
        lua_setfield(L, -2, "SetWalkSpeed");
        blockyPushCFunction(L, luaHumSetJumpPower);
        lua_setfield(L, -2, "SetJumpPower");
        blockyPushCFunction(L, luaHumGet);
        lua_setfield(L, -2, "Get");
        blockyPushCFunction(L, luaHumGetPosition);
        lua_setfield(L, -2, "GetPosition");
        blockyPushCFunction(L, luaHumTakeDamage);
        lua_setfield(L, -2, "TakeDamage");
        blockyPushCFunction(L, luaHumGetHealth);
        lua_setfield(L, -2, "GetHealth");
        blockyPushCFunction(L, luaHumGetLook);
        lua_setfield(L, -2, "GetLook");
        blockyPushCFunction(L, luaHumSetPosition);
        lua_setfield(L, -2, "SetPosition");
        blockyPushCFunction(L, luaHumGetState);
        lua_setfield(L, -2, "GetState");
        lua_newtable(L);
        pushSignalMethod(L, luaHumDiedConnect, "luaHumDiedConnect", 0);
        lua_setfield(L, -2, "Connect");
        pushSignalMethod(L, luaHumDiedConnect, "luaHumDiedConnect", 1);
        lua_setfield(L, -2, "Once");
        pushSignalMethod(L, luaHumDiedConnect, "luaHumDiedConnect", 2);
        lua_setfield(L, -2, "Wait");
        lua_setfield(L, -2, "Died");
        lua_newtable(L);
        pushSignalMethod(L, luaHumHealthChangedConnect, "luaHumHealthChangedConnect", 0);
        lua_setfield(L, -2, "Connect");
        pushSignalMethod(L, luaHumHealthChangedConnect, "luaHumHealthChangedConnect", 1);
        lua_setfield(L, -2, "Once");
        pushSignalMethod(L, luaHumHealthChangedConnect, "luaHumHealthChangedConnect", 2);
        lua_setfield(L, -2, "Wait");
        lua_setfield(L, -2, "HealthChanged");
        lua_newtable(L);
        pushSignalMethod(L, luaHumStateChangedConnect, "luaHumStateChangedConnect", 0);
        lua_setfield(L, -2, "Connect");
        pushSignalMethod(L, luaHumStateChangedConnect, "luaHumStateChangedConnect", 1);
        lua_setfield(L, -2, "Once");
        pushSignalMethod(L, luaHumStateChangedConnect, "luaHumStateChangedConnect", 2);
        lua_setfield(L, -2, "Wait");
        lua_setfield(L, -2, "StateChanged");
        // Proprieta' vere (Health/MaxHealth/WalkSpeed/JumpPower) accanto ai
        // vecchi Get*/Set* — stessa convenzione di Part (Part.Position),
        // cosi' Humanoid non e' piu' l'unico oggetto che espone solo
        // metodi verbosi per dati che altrove sono proprieta' dirette.
        // I Get*/Set* restano invariati, nessuno script esistente si rompe.
        lua_newtable(L);
        blockyPushCFunction(L, luaHumIndex);
        lua_setfield(L, -2, "__index");
        blockyPushCFunction(L, luaHumNewIndex);
        lua_setfield(L, -2, "__newindex");
        lua_setmetatable(L, -2);
        // Animazioni (Humanoid:LoadAnimation e Humanoid.Animator come Roblox).
        blockyPushCFunction(L, luaLoadCharAnimation);
        lua_setfield(L, -2, "LoadAnimation");
        blockyPushCFunction(L, luaGetPlayingCharTracks);
        lua_setfield(L, -2, "GetPlayingAnimationTracks");
        lua_newtable(L);
        lua_pushstring(L, "Animator");
        lua_setfield(L, -2, "ClassName");
        lua_pushstring(L, "Animator");
        lua_setfield(L, -2, "Name");
        blockyPushCFunction(L, luaLoadCharAnimation);
        lua_setfield(L, -2, "LoadAnimation");
        blockyPushCFunction(L, luaGetPlayingCharTracks);
        lua_setfield(L, -2, "GetPlayingAnimationTracks");
        lua_setfield(L, -2, "Animator");
        lua_setglobal(L, "Humanoid");
        CrashHandler::note("registerApi: post Humanoid");

        studio_pro_apis::registerApis(L);
        CrashHandler::note("registerApi: post StudioProApis");

        lua_newtable(L);
        blockyPushCFunction(L, luaBackpackGive);
        lua_setfield(L, -2, "Give");
        blockyPushCFunction(L, luaBackpackEquipped);
        lua_setfield(L, -2, "Equipped");
        lua_setglobal(L, "Backpack");

        lua_newtable(L);
        blockyPushCFunction(L, luaCameraShake);
        lua_setfield(L, -2, "Shake");
        blockyPushCFunction(L, luaCameraScreenRay);
        lua_setfield(L, -2, "ScreenRay");
        blockyPushCFunction(L, luaCameraScreenPointToRay);
        lua_setfield(L, -2, "ScreenPointToRay");
        blockyPushCFunction(L, luaCameraGetLook);
        lua_setfield(L, -2, "GetLook");
        blockyPushCFunction(L, luaCameraGetCFrame);
        lua_setfield(L, -2, "GetCFrame");
        lua_pushnumber(L, cameraFov ? *cameraFov : 70.0);
        lua_setfield(L, -2, "FieldOfView");
        lua_pushstring(L, cameraType == 2 ? "Attach" : (cameraType == 1 ? "Scriptable" : "Custom"));
        lua_setfield(L, -2, "CameraType");
        lua_newtable(L);
        blockyPushCFunction(L, luaCameraIndex);
        lua_setfield(L, -2, "__index");
        blockyPushCFunction(L, luaCameraNewIndex);
        lua_setfield(L, -2, "__newindex");
        lua_setmetatable(L, -2);
        lua_setglobal(L, "Camera");

        lua_getglobal(L, "workspace");
        if (lua_istable(L, -1)) {
            lua_getglobal(L, "Camera");
            lua_setfield(L, -2, "CurrentCamera");
        }
        lua_pop(L, 1);

        lua_newtable(L);
        blockyPushCFunction(L, luaDisableJump);
        lua_setfield(L, -2, "DisableJump");
        blockyPushCFunction(L, luaEnableJump);
        lua_setfield(L, -2, "EnableJump");
        blockyPushCFunction(L, luaInputIsKeyDown);
        lua_setfield(L, -2, "IsKeyDown");
        blockyPushCFunction(L, luaInputIsSprintHeld);
        lua_setfield(L, -2, "IsSprintHeld");
        blockyPushCFunction(L, luaInputIsMoving);
        lua_setfield(L, -2, "IsMoving");
        blockyPushCFunction(L, luaInputIsMouseButtonPressed);
        lua_setfield(L, -2, "IsMouseButtonPressed");
        lua_setglobal(L, "UserInputService");

        lua_newtable(L);
        blockyPushCFunction(L, luaTaskWait);
        lua_setfield(L, -2, "wait");
        blockyPushCFunction(L, luaTaskSpawn);
        lua_setfield(L, -2, "spawn");
        blockyPushCFunction(L, luaTaskDelay);
        lua_setfield(L, -2, "delay");
        blockyPushCFunction(L, luaTaskDefer);
        lua_setfield(L, -2, "defer");
        blockyPushCFunction(L, luaTaskCancel);
        lua_setfield(L, -2, "cancel");
        lua_setglobal(L, "task");
        blockyPushCFunction(L, luaTaskWait);
        lua_setglobal(L, "wait");
        CrashHandler::note("registerApi: post task");

        lua_newtable(L); // game (chiuso da lua_setglobal(L, "game") in fondo)

        lua_newtable(L); // game.RunService
        blockyPushCFunction(L, luaRunServiceIsStudio);
        lua_setfield(L, -2, "IsStudio");
        blockyPushCFunction(L, luaRunServiceIsClient);
        lua_setfield(L, -2, "IsClient");
        blockyPushCFunction(L, luaRunServiceIsServer);
        lua_setfield(L, -2, "IsServer");
        auto pushRunEvent = [&](lua_CFunction connectFn, const char* evName) {
            lua_newtable(L);
            pushSignalMethod(L, connectFn, "connectFn", 0);
            lua_setfield(L, -2, "Connect");
            pushSignalMethod(L, connectFn, "connectFn", 1);
            lua_setfield(L, -2, "Once");
            pushSignalMethod(L, connectFn, "connectFn", 2);
            lua_setfield(L, -2, "Wait");
            lua_setfield(L, -2, evName);
        };
        pushRunEvent(luaHeartbeatConnect, "Heartbeat");
        pushRunEvent(luaSteppedConnect, "Stepped");
        pushRunEvent(luaRenderSteppedConnect, "RenderStepped");
        lua_setfield(L, -2, "RunService");

        lua_newtable(L);
        blockyPushCFunction(L, luaSoundPlay);
        lua_setfield(L, -2, "Play");
        blockyPushCFunction(L, luaSoundStop);
        lua_setfield(L, -2, "Stop");
        lua_setfield(L, -2, "SoundService");

        lua_newtable(L);
        blockyPushCFunction(L, luaGetDataStore);
        lua_setfield(L, -2, "GetDataStore");
        lua_setfield(L, -2, "DataStoreService");

        lua_newtable(L);
        blockyPushCFunction(L, luaAnimatorPlay);
        lua_setfield(L, -2, "Play");
        blockyPushCFunction(L, luaAnimatorStop);
        lua_setfield(L, -2, "Stop");
        blockyPushCFunction(L, luaAnimatorSetWalk);
        lua_setfield(L, -2, "SetWalkAnim");
        blockyPushCFunction(L, luaAnimatorSetJump);
        lua_setfield(L, -2, "SetJumpAnim");
        blockyPushCFunction(L, luaAnimatorList);
        lua_setfield(L, -2, "List");
        lua_setfield(L, -2, "Animator");

        lua_newtable(L);
        auto pushParticleCmd = [&](lua_CFunction fn, const char* name) {
            lua_newtable(L);
            blockyPushCFunction(L, fn);
            lua_setfield(L, -2, "Explode");
            blockyPushCFunction(L, fn);
            lua_setfield(L, -2, "Emit");
            blockyPushCFunction(L, fn);
            lua_setfield(L, -2, "Play");
            lua_newtable(L);
            blockyPushCFunction(L, fn);
            lua_setfield(L, -2, "__call");
            lua_setmetatable(L, -2);
            lua_setfield(L, -2, name);
        };
        pushParticleCmd(luaParticlesEmit, "Emit");
        pushParticleCmd(luaParticlesSparks, "Sparks");
        pushParticleCmd(luaParticlesSmoke, "Smoke");
        pushParticleCmd(luaParticlesCoins, "Coins");
        blockyPushCFunction(L, luaParticlesExplode);
        lua_setfield(L, -2, "Explode");
        lua_setfield(L, -2, "Particles");
        CrashHandler::note("registerApi: post Particles");

        lua_newtable(L);
        blockyPushCFunction(L, luaLightingGetClockTime);
        lua_setfield(L, -2, "GetClockTime");
        blockyPushCFunction(L, luaLightingSetClockTime);
        lua_setfield(L, -2, "SetClockTime");
        blockyPushCFunction(L, luaLightingGetBrightness);
        lua_setfield(L, -2, "GetBrightness");
        blockyPushCFunction(L, luaLightingSetBrightness);
        lua_setfield(L, -2, "SetBrightness");
        blockyPushCFunction(L, luaLightingGetAmbient);
        lua_setfield(L, -2, "GetAmbient");
        blockyPushCFunction(L, luaLightingSetAmbient);
        lua_setfield(L, -2, "SetAmbient");
        blockyPushCFunction(L, luaLightingGetOutdoorAmbient);
        lua_setfield(L, -2, "GetOutdoorAmbient");
        blockyPushCFunction(L, luaLightingSetOutdoorAmbient);
        lua_setfield(L, -2, "SetOutdoorAmbient");
        blockyPushCFunction(L, luaLightingGetFogColor);
        lua_setfield(L, -2, "GetFogColor");
        blockyPushCFunction(L, luaLightingSetFogColor);
        lua_setfield(L, -2, "SetFogColor");
        blockyPushCFunction(L, luaLightingGetFogStart);
        lua_setfield(L, -2, "GetFogStart");
        blockyPushCFunction(L, luaLightingSetFogStart);
        lua_setfield(L, -2, "SetFogStart");
        blockyPushCFunction(L, luaLightingGetFogEnd);
        lua_setfield(L, -2, "GetFogEnd");
        blockyPushCFunction(L, luaLightingSetFogEnd);
        lua_setfield(L, -2, "SetFogEnd");
        lua_setfield(L, -2, "Lighting");

        lua_newtable(L);
        blockyPushCFunction(L, luaTweenCreate);
        lua_setfield(L, -2, "Create");
        lua_setfield(L, -2, "TweenService");

        lua_newtable(L);
        blockyPushCFunction(L, luaDebrisAddItem);
        lua_setfield(L, -2, "AddItem");
        lua_setfield(L, -2, "Debris");

        lua_newtable(L);
        lua_setfield(L, -2, "Chat");

        lua_newtable(L);
        blockyPushCFunction(L, luaGuiButtonConnect);
        lua_setfield(L, -2, "ConnectButton");
        blockyPushCFunction(L, luaGuiCreateLabel);
        lua_setfield(L, -2, "CreateLabel");
        blockyPushCFunction(L, luaGuiSetLabelText);
        lua_setfield(L, -2, "SetLabelText");
        blockyPushCFunction(L, luaGuiSetLabelSize);
        lua_setfield(L, -2, "SetLabelSize");
        blockyPushCFunction(L, luaGuiSetLabelOffset);
        lua_setfield(L, -2, "SetLabelOffset");
        blockyPushCFunction(L, luaGuiSetLabelColor);
        lua_setfield(L, -2, "SetLabelColor");
        blockyPushCFunction(L, luaGuiSetLabelBg);
        lua_setfield(L, -2, "SetLabelBg");
        blockyPushCFunction(L, luaGuiSetLabelVisible);
        lua_setfield(L, -2, "SetLabelVisible");
        blockyPushCFunction(L, luaGuiFindLabel);
        lua_setfield(L, -2, "FindLabel");
        lua_setfield(L, -2, "StarterGui");
        CrashHandler::note("registerApi: post StarterGui");

        pushServiceContainer(L, SharedService::ReplicatedStorage, "ReplicatedStorage");
        lua_setfield(L, -2, "ReplicatedStorage");

        lua_newtable(L); // Players
        lua_newtable(L); // LocalPlayer
        lua_pushstring(L, "Player");
        lua_setfield(L, -2, "Name");
        lua_pushstring(L, "Player");
        lua_setfield(L, -2, "ClassName");
        lua_newtable(L); // Character
        lua_getglobal(L, "Humanoid");
        lua_setfield(L, -2, "Humanoid");
        lua_pushstring(L, "Model");
        lua_setfield(L, -2, "ClassName");
        lua_setfield(L, -2, "Character");
        // CharacterAdded su LocalPlayer
        lua_newtable(L);
        pushSignalMethod(L, luaCharacterAddedConnect, "luaCharacterAddedConnect", 0);
        lua_setfield(L, -2, "Connect");
        pushSignalMethod(L, luaCharacterAddedConnect, "luaCharacterAddedConnect", 1);
        lua_setfield(L, -2, "Once");
        pushSignalMethod(L, luaCharacterAddedConnect, "luaCharacterAddedConnect", 2);
        lua_setfield(L, -2, "Wait");
        lua_setfield(L, -2, "CharacterAdded");
        // leaderstats — wrapper su Classifica (Monete, Kills, …)
        lua_newtable(L);
        lua_pushstring(L, "leaderstats");
        lua_setfield(L, -2, "Name");
        lua_pushstring(L, "Folder");
        lua_setfield(L, -2, "ClassName");
        blockyPushCFunction(L, luaLeaderstatsCreate);
        lua_setfield(L, -2, "Create");
        lua_newtable(L);
        blockyPushCFunction(L, luaLeaderstatsIndex);
        lua_setfield(L, -2, "__index");
        lua_setmetatable(L, -2);
        lua_setfield(L, -2, "leaderstats");
        lua_setfield(L, -2, "LocalPlayer");
        lua_newtable(L);
        pushSignalMethod(L, luaPlayerAddedConnect, "luaPlayerAddedConnect", 0);
        lua_setfield(L, -2, "Connect");
        pushSignalMethod(L, luaPlayerAddedConnect, "luaPlayerAddedConnect", 1);
        lua_setfield(L, -2, "Once");
        pushSignalMethod(L, luaPlayerAddedConnect, "luaPlayerAddedConnect", 2);
        lua_setfield(L, -2, "Wait");
        lua_setfield(L, -2, "PlayerAdded");
        blockyPushCFunction(L, luaPlayersGetPlayers);
        lua_setfield(L, -2, "GetPlayers");
        blockyPushCFunction(L, luaPlayersGetPlayerFromCharacter);
        lua_setfield(L, -2, "GetPlayerFromCharacter");
        lua_setfield(L, -2, "Players");

        lua_getglobal(L, "Events");
        lua_setfield(L, -2, "Events");

        lua_getglobal(L, "UserInputService");
        lua_setfield(L, -2, "UserInputService");

        blockyPushCFunction(L, luaGetService);
        lua_setfield(L, -2, "GetService");
        writeGameIdentity(-1);

        lua_newtable(L);
        blockyPushCFunction(L, luaCollectionAddTag);
        lua_setfield(L, -2, "AddTag");
        blockyPushCFunction(L, luaCollectionRemoveTag);
        lua_setfield(L, -2, "RemoveTag");
        blockyPushCFunction(L, luaCollectionHasTag);
        lua_setfield(L, -2, "HasTag");
        blockyPushCFunction(L, luaCollectionGetTagged);
        lua_setfield(L, -2, "GetTagged");
        blockyPushCFunction(L, luaCollectionGetTags);
        lua_setfield(L, -2, "GetTags");
        lua_setfield(L, -2, "CollectionService");

        pushServiceContainer(L, SharedService::ServerScriptService, "ServerScriptService");
        lua_setfield(L, -2, "ServerScriptService");
        pushServiceContainer(L, SharedService::ServerStorage, "ServerStorage");
        lua_setfield(L, -2, "ServerStorage");
        lua_newtable(L);
        lua_setfield(L, -2, "StarterPlayer");

        lua_setglobal(L, "game");

        // Alla fine: aggiunge a UserInputService, game, workspace... che prima
        // di qui non esistevano ancora (chiamata piu' in alto non faceva nulla:
        // InputBegan, PlayerRemoving, Debris:AddItem non c'erano mai).
        registerExpandApis(L);
        CrashHandler::note("registerApi: post ExpandApis");

        return true;
    }

    static int luaPrint(lua_State* ls) {
        int n = lua_gettop(ls);
        std::string out;
        for (int i = 1; i <= n; ++i) {
            if (i > 1) out += '\t';
            size_t len = 0;
            const char* s = luaL_tolstring(ls, i, &len);
            if (s) out.append(s, len);
            lua_pop(ls, 1);
        }
        std::fprintf(stdout, "[Lua] %s\n", out.c_str());
        auto* rt = fromState(ls);
        if (rt) {
            rt->printLog.push_back(out);
            if (rt->printLog.size() > 400)
                rt->printLog.erase(rt->printLog.begin(),
                                   rt->printLog.begin() + (rt->printLog.size() - 400));
        }
        return 0;
    }

    static int luaVector3New(lua_State* ls) {
        pushVec3(ls,
            (float)luaL_optnumber(ls, 1, 0),
            (float)luaL_optnumber(ls, 2, 0),
            (float)luaL_optnumber(ls, 3, 0));
        return 1;
    }

    static void pushCFrame(lua_State* ls, float x, float y, float z) { pushCFrameRot(ls, x, y, z, 0.f, 0.f, 0.f); }

    /** CFrame vero (prelude) da posizione e gradi YXZ come le Part; senza prelude solo posizione. */
    static void pushCFrameRot(lua_State* ls, float x, float y, float z, float rxDeg, float ryDeg, float rzDeg) {
        if (pushPreludeField(ls, "cframe")) {
            lua_pushnumber(ls, x); lua_pushnumber(ls, y); lua_pushnumber(ls, z);
            lua_pushnumber(ls, rxDeg); lua_pushnumber(ls, ryDeg); lua_pushnumber(ls, rzDeg);
            if (lua_pcall(ls, 6, 1, 0) == LUA_OK) return;
            lua_pop(ls, 1);
        }
        lua_newtable(ls);
        pushVec3(ls, x, y, z);
        lua_setfield(ls, -2, "Position");
        lua_pushnumber(ls, x); lua_setfield(ls, -2, "X");
        lua_pushnumber(ls, y); lua_setfield(ls, -2, "Y");
        lua_pushnumber(ls, z); lua_setfield(ls, -2, "Z");
        lua_pushnumber(ls, x); lua_setfield(ls, -2, "x");
        lua_pushnumber(ls, y); lua_setfield(ls, -2, "y");
        lua_pushnumber(ls, z); lua_setfield(ls, -2, "z");
    }
    static int luaCFrameNew(lua_State* ls) {
        glm::vec3 p{};
        if (lua_istable(ls, 1))
            readVec3Arg(ls, 1, p);
        else
            p = { (float)luaL_optnumber(ls, 1, 0), (float)luaL_optnumber(ls, 2, 0),
                  (float)luaL_optnumber(ls, 3, 0) };
        pushCFrame(ls, p.x, p.y, p.z);
        return 1;
    }
    static int luaCFrameLookAt(lua_State* ls) {
        glm::vec3 from{}, to{};
        readVec3Arg(ls, 1, from);
        readVec3Arg(ls, 2, to);
        pushCFrame(ls, from.x, from.y, from.z);
        glm::vec3 d = to - from;
        float L = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (L > 1e-6f) {
            pushVec3(ls, d.x / L, d.y / L, d.z / L);
            lua_setfield(ls, -2, "LookVector");
        }
        return 1;
    }
    static int luaRegion3New(lua_State* ls) {
        glm::vec3 a{}, b{};
        readVec3Arg(ls, 1, a);
        readVec3Arg(ls, 2, b);
        glm::vec3 mn{ std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z) };
        glm::vec3 mx{ std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z) };
        lua_newtable(ls);
        pushVec3(ls, mn.x, mn.y, mn.z); lua_setfield(ls, -2, "Min");
        pushVec3(ls, mx.x, mx.y, mx.z); lua_setfield(ls, -2, "Max");
        pushVec3(ls, mx.x - mn.x, mx.y - mn.y, mx.z - mn.z); lua_setfield(ls, -2, "Size");
        pushCFrame(ls, (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f);
        lua_setfield(ls, -2, "CFrame");
        return 1;
    }
    static int luaMathNoise(lua_State* ls) {
        lua_pushnumber(ls, Terrain::noise(
            (float)luaL_optnumber(ls, 1, 0),
            (float)luaL_optnumber(ls, 2, 0),
            (float)luaL_optnumber(ls, 3, 0)));
        return 1;
    }
    static int luaTime(lua_State* ls) {
        lua_pushnumber(ls, glfwGetTime());
        return 1;
    }

    static int luaColor3New(lua_State* ls) {
        pushColor3(ls,
            (float)luaL_optnumber(ls, 1, 1),
            (float)luaL_optnumber(ls, 2, 1),
            (float)luaL_optnumber(ls, 3, 1));
        return 1;
    }

    static int luaColor3FromRGB(lua_State* ls) {
        float r = (float)luaL_optnumber(ls, 1, 255) / 255.f;
        float g = (float)luaL_optnumber(ls, 2, 255) / 255.f;
        float b = (float)luaL_optnumber(ls, 3, 255) / 255.f;
        pushColor3(ls, r, g, b);
        return 1;
    }

    static void pushBrickColor(lua_State* ls, const brickcolor::Entry& e) {
        glm::vec3 c = brickcolor::toLinear(e);
        lua_newtable(ls);
        lua_pushinteger(ls, e.number);
        lua_setfield(ls, -2, "Number");
        lua_pushstring(ls, e.name);
        lua_setfield(ls, -2, "Name");
        pushColor3(ls, c.x, c.y, c.z);
        lua_setfield(ls, -2, "Color");
        lua_pushnumber(ls, c.x); lua_setfield(ls, -2, "r");
        lua_pushnumber(ls, c.y); lua_setfield(ls, -2, "g");
        lua_pushnumber(ls, c.z); lua_setfield(ls, -2, "b");
        lua_pushnumber(ls, c.x); lua_setfield(ls, -2, "R");
        lua_pushnumber(ls, c.y); lua_setfield(ls, -2, "G");
        lua_pushnumber(ls, c.z); lua_setfield(ls, -2, "B");
    }

    static int luaBrickColorNew(lua_State* ls) {
        const brickcolor::Entry* e = nullptr;
        if (lua_isnumber(ls, 1))
            e = brickcolor::findByNumber((int)lua_tointeger(ls, 1));
        else if (lua_isstring(ls, 1))
            e = brickcolor::findByName(lua_tostring(ls, 1));
        if (!e) e = brickcolor::findByName("Medium stone grey");
        if (!e) e = brickcolor::palette() + 0;
        pushBrickColor(ls, *e);
        return 1;
    }

    static int luaBrickColorPalette(lua_State* ls) {
        int n = 0;
        const brickcolor::Entry* all = brickcolor::palette(&n);
        lua_newtable(ls);
        for (int i = 0; i < n; ++i) {
            pushBrickColor(ls, all[i]);
            lua_rawseti(ls, -2, i + 1);
        }
        return 1;
    }

    static int luaBrickColorRandom(lua_State* ls) {
        (void)ls;
        int n = 0;
        const brickcolor::Entry* all = brickcolor::palette(&n);
        if (n <= 0) { lua_pushnil(ls); return 1; }
        static unsigned seed = 1;
        seed = seed * 1103515245u + 12345u;
        pushBrickColor(ls, all[(int)(seed % (unsigned)n)]);
        return 1;
    }

    static int luaInstanceNew(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace)
            return blockyLuaError(ls, "Instance.new: engine non pronto");

        // Argomenti 2 (nome) e 3 (parent) sempre presenti, anche nil: alcuni
        // rami creano la tabella e POI leggono l'argomento 2, che senza questo
        // era la tabella appena creata (Instance.new("Configuration") falliva).
        lua_settop(ls, std::max(lua_gettop(ls), 3));
        const char* className = luaL_optstring(ls, 1, "Part");
        if (std::strcmp(className, "Part") == 0
            || std::strcmp(className, "Ball") == 0
            || std::strcmp(className, "Cylinder") == 0
            || std::strcmp(className, "WedgePart") == 0
            || std::strcmp(className, "Wedge") == 0) {
            WorkspacePart p;
            p.name = className;
            p.size = {4.f, 1.f, 2.f};
            p.color = {0.64f, 0.64f, 0.64f};
            if (std::strcmp(className, "Ball") == 0) {
                p.shape = PartShape::Ball;
                p.size = {4.f, 4.f, 4.f};
                p.useStud = false;
            } else if (std::strcmp(className, "Cylinder") == 0) {
                p.shape = PartShape::Cylinder;
                p.size = {2.f, 4.f, 2.f};
                p.useStud = false;
            } else if (std::strcmp(className, "WedgePart") == 0
                       || std::strcmp(className, "Wedge") == 0) {
                p.shape = PartShape::Wedge;
                p.size = {4.f, 2.f, 4.f};
                p.useStud = false;
            }
            pushPart(ls, p);
            setPendingPartMeta(ls, -1); // part.CFrame / part.Parent = workspace come Roblox
            lua_pushstring(ls, className);
            lua_setfield(ls, -2, "ClassName");
            return 1;
        }
        if (std::strcmp(className, "Animation") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "Animation"); lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, "Animation"); lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, ""); lua_setfield(ls, -2, "AnimationId");
            return 1;
        }
        if (std::strcmp(className, "Sound") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, luaL_optstring(ls, 2, "Sound"));
            lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, "Sound");
            lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, "");
            lua_setfield(ls, -2, "SoundId");
            lua_pushboolean(ls, 0);
            lua_setfield(ls, -2, "Looped");
            lua_pushnumber(ls, 1.0);
            lua_setfield(ls, -2, "Volume");
            blockyPushCFunction(ls, luaSoundPlay);
            lua_setfield(ls, -2, "Play");
            blockyPushCFunction(ls, luaSoundStop);
            lua_setfield(ls, -2, "Stop");
            return 1;
        }
        if (std::strcmp(className, "ParticleEmitter") == 0) {
            auto* rt = fromState(ls);
            int partIdx = -1;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) partIdx = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            int emIdx = -1;
            if (rt) {
                ScriptParticleEmitter e;
                e.partIndex = partIdx;
                e.rate = 10.f;
                e.preset = (int)ParticlePreset::Sparks;
                rt->scriptEmitters_.push_back(e);
                emIdx = (int)rt->scriptEmitters_.size() - 1;
            }
            lua_newtable(ls);
            lua_pushstring(ls, "ParticleEmitter");
            lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "ParticleEmitter"));
            lua_setfield(ls, -2, "Name");
            lua_pushinteger(ls, emIdx);
            lua_setfield(ls, -2, "__emitterIndex");
            lua_pushnumber(ls, 10.0);
            lua_setfield(ls, -2, "Rate");
            lua_pushboolean(ls, 1);
            lua_setfield(ls, -2, "Enabled");
            blockyPushCFunction(ls, luaParticleEmitterEmit);
            lua_setfield(ls, -2, "Emit");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaParticleEmitterNewIndex);
            lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "ClickDetector") == 0) {
            int partIdx = -1;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) partIdx = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            lua_newtable(ls);
            lua_pushstring(ls, "ClickDetector");
            lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, luaL_optstring(ls, 2, "ClickDetector"));
            lua_setfield(ls, -2, "Name");
            lua_pushinteger(ls, partIdx);
            lua_setfield(ls, -2, "__partIndex");
            trackPartTable(ls, -1);
            lua_pushnumber(ls, 32.0);
            lua_setfield(ls, -2, "MaxActivationDistance");
            // MouseClick: un clic del mouse sulla part (main.cpp fa il raggio
            // dal cursore). Prima era lo stesso elenco di Touched: scattava
            // passandoci sopra, non cliccando.
            lua_newtable(ls);
            lua_pushinteger(ls, partIdx);
            lua_setfield(ls, -2, "__partIndex");
            trackPartTable(ls, -1);
            pushSignalMethod(ls, luaPartClickConnect, "luaPartClickConnect", 0);
            lua_setfield(ls, -2, "Connect");
            pushSignalMethod(ls, luaPartClickConnect, "luaPartClickConnect", 1);
            lua_setfield(ls, -2, "Once");
            pushSignalMethod(ls, luaPartClickConnect, "luaPartClickConnect", 2);
            lua_setfield(ls, -2, "Wait");
            lua_setfield(ls, -2, "MouseClick");
            return 1;
        }
        if (std::strcmp(className, "Tool") == 0) {
            const char* nm = luaL_optstring(ls, 2, "Tool");
            auto& it = rt->workspace->addBackpackItem(nm ? nm : "Tool");
            scriptControls().toolEquipped[it.name];
            scriptControls().toolUnequipped[it.name];
            scriptControls().toolActivated[it.name];
            lua_newtable(ls);
            lua_pushstring(ls, it.name.c_str());
            lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, "Tool");
            lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, it.id.c_str());
            lua_setfield(ls, -2, "_toolId");
            lua_pushstring(ls, it.name.c_str());
            lua_setfield(ls, -2, "_toolName");
            lua_newtable(ls);
            lua_pushstring(ls, it.name.c_str());
            lua_setfield(ls, -2, "_toolName");
            lua_pushstring(ls, "Equipped");
            lua_setfield(ls, -2, "_toolEvent");
            pushSignalMethod(ls, luaToolEventConnect, "luaToolEventConnect", 0);
            lua_setfield(ls, -2, "Connect");
            pushSignalMethod(ls, luaToolEventConnect, "luaToolEventConnect", 1);
            lua_setfield(ls, -2, "Once");
            pushSignalMethod(ls, luaToolEventConnect, "luaToolEventConnect", 2);
            lua_setfield(ls, -2, "Wait");
            lua_setfield(ls, -2, "Equipped");
            lua_newtable(ls);
            lua_pushstring(ls, it.name.c_str());
            lua_setfield(ls, -2, "_toolName");
            lua_pushstring(ls, "Unequipped");
            lua_setfield(ls, -2, "_toolEvent");
            pushSignalMethod(ls, luaToolEventConnect, "luaToolEventConnect", 0);
            lua_setfield(ls, -2, "Connect");
            pushSignalMethod(ls, luaToolEventConnect, "luaToolEventConnect", 1);
            lua_setfield(ls, -2, "Once");
            pushSignalMethod(ls, luaToolEventConnect, "luaToolEventConnect", 2);
            lua_setfield(ls, -2, "Wait");
            lua_setfield(ls, -2, "Unequipped");
            lua_newtable(ls);
            lua_pushstring(ls, it.name.c_str());
            lua_setfield(ls, -2, "_toolName");
            lua_pushstring(ls, "Activated");
            lua_setfield(ls, -2, "_toolEvent");
            pushSignalMethod(ls, luaToolEventConnect, "luaToolEventConnect", 0);
            lua_setfield(ls, -2, "Connect");
            pushSignalMethod(ls, luaToolEventConnect, "luaToolEventConnect", 1);
            lua_setfield(ls, -2, "Once");
            pushSignalMethod(ls, luaToolEventConnect, "luaToolEventConnect", 2);
            lua_setfield(ls, -2, "Wait");
            lua_setfield(ls, -2, "Activated");
            blockyPushCFunction(ls, luaToolActivate);
            lua_setfield(ls, -2, "Activate");
            return 1;
        }
        if (std::strcmp(className, "WeldConstraint") == 0
            || std::strcmp(className, "Weld") == 0
            || std::strcmp(className, "Motor6D") == 0) {
            int a = -1, b = -1;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) a = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            if (lua_istable(ls, 4)) {
                lua_getfield(ls, 4, "__partIndex");
                if (lua_isnumber(ls, -1)) b = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            auto& w = rt->workspace->addWeld(a, b);
            w.name = luaL_optstring(ls, 2, className);
            lua_newtable(ls);
            lua_pushinteger(ls, (int)rt->workspace->welds.size() - 1);
            lua_setfield(ls, -2, "__weldIndex");
            lua_pushstring(ls, className);
            lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, w.name.c_str());
            lua_setfield(ls, -2, "Name");
            lua_pushboolean(ls, w.enabled ? 1 : 0);
            lua_setfield(ls, -2, "Enabled");
            if (a >= 0) { pushLivePart(ls, a); lua_setfield(ls, -2, "Part0"); }
            if (b >= 0) { pushLivePart(ls, b); lua_setfield(ls, -2, "Part1"); }
            lua_newtable(ls);
            blockyPushCFunction(ls, luaWeldNewIndex);
            lua_setfield(ls, -2, "__newindex");
            blockyPushCFunction(ls, luaWeldIndex);
            lua_setfield(ls, -2, "__index");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "BillboardGui") == 0
            || std::strcmp(className, "SurfaceGui") == 0) {
            WorkspaceLabel lab;
            lab.name = luaL_optstring(ls, 2, className);
            lab.className = className;
            lab.text = lab.name;
            lab.visible = true;
            lab.fontScale = 1.1f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1))
                    lab.adorneePart = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1))
                    lab.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lab));
            int idx = (int)rt->workspace->labels.size() - 1;
            pushLiveLabel(ls, idx);
            return 1;
        }
        if (std::strcmp(className, "BindableEvent") == 0) {
            const char* nm = luaL_optstring(ls, 2, "Event");
            scriptControls().events[nm];
            lua_newtable(ls);
            lua_pushstring(ls, nm);
            lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, "BindableEvent");
            lua_setfield(ls, -2, "ClassName");
            blockyPushCFunction(ls, luaBindableFire);
            lua_setfield(ls, -2, "Fire");
            lua_newtable(ls);
            lua_pushstring(ls, nm);
            lua_setfield(ls, -2, "_eventName");
            pushSignalMethod(ls, luaBindableConnect, "luaBindableConnect", 0);
            lua_setfield(ls, -2, "Connect");
            pushSignalMethod(ls, luaBindableConnect, "luaBindableConnect", 1);
            lua_setfield(ls, -2, "Once");
            pushSignalMethod(ls, luaBindableConnect, "luaBindableConnect", 2);
            lua_setfield(ls, -2, "Wait");
            lua_setfield(ls, -2, "Event");
            return 1;
        }
        if (std::strcmp(className, "BindableFunction") == 0) {
            const char* nm = luaL_optstring(ls, 2, "Function");
            scriptControls().bindableInvoke[nm] = LUA_NOREF;
            lua_newtable(ls);
            lua_pushstring(ls, nm);
            lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, "BindableFunction");
            lua_setfield(ls, -2, "ClassName");
            blockyPushCFunction(ls, luaBindableInvoke);
            lua_setfield(ls, -2, "Invoke");
            lua_newtable(ls);
            blockyPushCFunction(ls, luaBindableFunctionIndex);
            lua_setfield(ls, -2, "__index");
            blockyPushCFunction(ls, luaBindableFunctionNewIndex);
            lua_setfield(ls, -2, "__newindex");
            lua_setmetatable(ls, -2);
            return 1;
        }
        if (std::strcmp(className, "ProximityPrompt") == 0) {
            const char* nm = luaL_optstring(ls, 2, "Prompt");
            auto* rt = fromState(ls);
            int partIdx = -1;
            float x = 0, y = 0, z = 0, radius = 8.f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__partIndex");
                if (lua_isnumber(ls, -1)) {
                    partIdx = (int)lua_tointeger(ls, -1);
                    if (rt && rt->workspace && partIdx >= 0
                        && partIdx < (int)rt->workspace->parts.size()) {
                        const auto& p = rt->workspace->parts[partIdx];
                        x = p.position.x; y = p.position.y; z = p.position.z;
                    }
                }
                lua_pop(ls, 1);
            }
            if (rt && rt->proximity)
                rt->proximity->add(nm, "Interact", x, y, z, radius, partIdx);
            lua_newtable(ls);
            lua_pushstring(ls, nm);
            lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, "ProximityPrompt");
            lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, nm);
            lua_setfield(ls, -2, "_promptName");
            lua_pushnumber(ls, radius);
            lua_setfield(ls, -2, "MaxActivationDistance");
            lua_newtable(ls);
            lua_pushstring(ls, nm);
            lua_setfield(ls, -2, "_promptName");
            pushSignalMethod(ls, luaProximityTriggeredConnect, "luaProximityTriggeredConnect", 0);
            lua_setfield(ls, -2, "Connect");
            pushSignalMethod(ls, luaProximityTriggeredConnect, "luaProximityTriggeredConnect", 1);
            lua_setfield(ls, -2, "Once");
            pushSignalMethod(ls, luaProximityTriggeredConnect, "luaProximityTriggeredConnect", 2);
            lua_setfield(ls, -2, "Wait");
            lua_setfield(ls, -2, "Triggered");
            return 1;
        }
        if (std::strcmp(className, "UIListLayout") == 0
            || std::strcmp(className, "UIPadding") == 0
            || std::strcmp(className, "UIGridLayout") == 0) {
            auto* rt = fromState(ls);
            if (!rt || !rt->workspace) return 0;
            WorkspaceLabel lay;
            lay.name = luaL_optstring(ls, 2, className);
            lay.className = className;
            lay.visible = false; // layout helper, non disegnato
            lay.sizeW = 0.f;
            lay.sizeH = 0.f;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__labelIndex");
                if (lua_isnumber(ls, -1))
                    lay.parentIndex = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            rt->workspace->labels.push_back(std::move(lay));
            int idx = (int)rt->workspace->labels.size() - 1;
            lua_newtable(ls);
            lua_pushinteger(ls, idx);
            lua_setfield(ls, -2, "__labelIndex");
            lua_pushstring(ls, className);
            lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, rt->workspace->labels[idx].name.c_str());
            lua_setfield(ls, -2, "Name");
            return 1;
        }
        if (std::strcmp(className, "Script") == 0
            || std::strcmp(className, "LocalScript") == 0) {
            const char* nm = luaL_optstring(ls, 2, className);
            SharedService svc = SharedService::ServerScriptService;
            int parentIdx = -1;
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__serviceId");
                if (lua_isnumber(ls, -1))
                    svc = (SharedService)(int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
                lua_getfield(ls, 3, "__sharedIndex");
                if (lua_isnumber(ls, -1))
                    parentIdx = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            auto* rt = fromState(ls);
            if (!rt || !rt->workspace) return 0;
            auto& inst = rt->workspace->addShared(svc, className, nm ? nm : className, parentIdx);
            if (inst.source.empty())
                inst.source = "print(script.Name, \"running\")\n";
            int newIdx = (int)rt->workspace->instances.size() - 1;
            if (parentIdx >= 0) rt->fireSharedChildAdded(parentIdx, newIdx);
            pushSharedInstance(ls, newIdx);
            return 1;
        }
        if (std::strcmp(className, "RemoteEvent") == 0
            || std::strcmp(className, "ModuleScript") == 0
            || std::strcmp(className, "Folder") == 0
            || std::strcmp(className, "RemoteFunction") == 0) {
            const char* nm = luaL_optstring(ls, 2, className);
            SharedService svc = SharedService::ReplicatedStorage;
            int parentIdx = -1;
            // Instance.new("X", parent) — parent puo' essere un service container
            // o un SharedInstance (table con __sharedIndex).
            if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__serviceId");
                if (lua_isnumber(ls, -1))
                    svc = (SharedService)(int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
                lua_getfield(ls, 3, "__sharedIndex");
                if (lua_isnumber(ls, -1))
                    parentIdx = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
            }
            auto* rt = fromState(ls);
            if (!rt || !rt->workspace) return 0;
            auto& inst = rt->workspace->addShared(svc, className, nm ? nm : className, parentIdx);
            if (std::strcmp(className, "RemoteEvent") == 0
                || std::strcmp(className, "RemoteFunction") == 0) {
                scriptControls().remoteServer[inst.name];
                scriptControls().remoteClient[inst.name];
            }
            if (std::strcmp(className, "ModuleScript") == 0 && inst.source.empty()) {
                inst.source = "local M = {}\nreturn M\n";
            }
            int newIdx = (int)rt->workspace->instances.size() - 1;
            if (parentIdx >= 0) rt->fireSharedChildAdded(parentIdx, newIdx);
            pushSharedInstance(ls, newIdx);
            return 1;
        }
        if (int expanded = tryInstanceNewExpand(ls, className))
            return expanded;
        lua_newtable(ls);
        lua_pushstring(ls, className);
        lua_setfield(ls, -2, "ClassName");
        lua_pushstring(ls, className);
        lua_setfield(ls, -2, "Name");
        return 1;
    }

    static bool readColor3Arg(lua_State* ls, int arg, glm::vec3& out) {
        if (!lua_istable(ls, arg)) return false;
        lua_getfield(ls, arg, "r");
        if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "R"); }
        out.x = (float)luaL_optnumber(ls, -1, 1); lua_pop(ls, 1);
        lua_getfield(ls, arg, "g");
        if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "G"); }
        out.y = (float)luaL_optnumber(ls, -1, 1); lua_pop(ls, 1);
        lua_getfield(ls, arg, "b");
        if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "B"); }
        out.z = (float)luaL_optnumber(ls, -1, 1); lua_pop(ls, 1);
        return true;
    }

    static int skipParticleSelfArgs(lua_State* ls, int arg) {
        int top = lua_gettop(ls);
        while (arg <= top && lua_istable(ls, arg)) {
            lua_getfield(ls, arg, "x");
            bool has = lua_isnumber(ls, -1);
            lua_pop(ls, 1);
            if (!has) {
                lua_getfield(ls, arg, "X");
                has = lua_isnumber(ls, -1);
                lua_pop(ls, 1);
            }
            if (has) break;
            ++arg;
        }
        return arg;
    }

    static bool tableHasVec3(lua_State* ls, int idx) {
        if (!lua_istable(ls, idx)) return false;
        lua_getfield(ls, idx, "x");
        bool has = lua_isnumber(ls, -1) != 0;
        lua_pop(ls, 1);
        if (has) return true;
        lua_getfield(ls, idx, "X");
        has = lua_isnumber(ls, -1) != 0;
        lua_pop(ls, 1);
        return has;
    }

    static int luaParticlesPreset(lua_State* ls, ParticlePreset preset) {
        auto* rt = fromState(ls);
        if (!rt || !rt->particles)
            return blockyLuaError(ls, "Particles non pronto");
        int arg = skipParticleSelfArgs(ls, 1);
        glm::vec3 pos{0, 3, 0};
        if (!readVec3Arg(ls, arg, pos))
            return blockyLuaError(ls, "Particles richiede Vector3 posizione");
        int count = -1;
        glm::vec3 col{-1, -1, -1};
        glm::vec3 bias{0, 1, 0};
        auto takeDir = [&](int idx) {
            glm::vec3 v{0, 1, 0};
            if (!readVec3Arg(ls, idx, v)) return;
            float L = glm::length(v);
            if (L > 1e-4f) bias = v / L;
        };
        if (lua_isnumber(ls, arg + 1))
            count = (int)lua_tointeger(ls, arg + 1);
        else if (tableHasVec3(ls, arg + 1))
            takeDir(arg + 1);
        else if (lua_istable(ls, arg + 1))
            readColor3Arg(ls, arg + 1, col);
        if (tableHasVec3(ls, arg + 2))
            takeDir(arg + 2);
        else if (lua_istable(ls, arg + 2))
            readColor3Arg(ls, arg + 2, col);
        if (tableHasVec3(ls, arg + 3))
            takeDir(arg + 3);
        if (count > LuaScriptSecurity::kMaxParticles)
            count = LuaScriptSecurity::kMaxParticles;
        if (preset == ParticlePreset::Coins && count > 8) count = 8;
        rt->particles->emitPreset(preset, pos, count, col, bias);
        return 0;
    }

    static int luaParticlesEmit(lua_State* ls) { return luaParticlesPreset(ls, ParticlePreset::Burst); }
    static int luaParticlesSparks(lua_State* ls) { return luaParticlesPreset(ls, ParticlePreset::Sparks); }
    static int luaParticlesSmoke(lua_State* ls) { return luaParticlesPreset(ls, ParticlePreset::Smoke); }
    static int luaParticlesCoins(lua_State* ls) { return luaParticlesPreset(ls, ParticlePreset::Coins); }

    static int luaParticlesExplode(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->particles)
            return blockyLuaError(ls, "Particles non pronto");
        int arg = skipParticleSelfArgs(ls, 1);
        glm::vec3 pos{0, 3, 0};
        if (!readVec3Arg(ls, arg, pos))
            return blockyLuaError(ls, "Particles:Explode(Vector3 [, count])");
        int count = (int)luaL_optinteger(ls, arg + 1, 16);
        if (count < 4) count = 4;
        if (count > 24) count = 24;
        const float radius = 10.f + (float)count * 0.12f;
        const float maxDmg = 35.f + (float)count * 0.9f;
        rt->particles->spawnExplosion(pos, radius, count);
        rt->applyExplosionPlayerDamage(pos, radius, maxDmg);
        return 0;
    }

    static int luaSoundPlay(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->soundService)
            return blockyLuaError(ls, "SoundService non pronto");
        int arg = 1;
        if (lua_istable(ls, 1) && lua_gettop(ls) >= 2) arg = 2;
        if (lua_istable(ls, arg)) {
            std::string id = readStringField(ls, arg, "SoundId", "");
            bool looped = false;
            lua_getfield(ls, arg, "Looped");
            if (lua_isboolean(ls, -1)) looped = lua_toboolean(ls, -1) != 0;
            lua_pop(ls, 1);
            if (id.empty()) {
                std::fprintf(stderr, "[Lua] SoundService:Play SoundId vuoto\n");
                lua_pushboolean(ls, 0);
                return 1;
            }
            if (!rt->soundService->playPath(id, looped)) {
                std::fprintf(stderr, "[Lua] SoundService:Play fallito: %s\n",
                             rt->soundService->lastError.c_str());
                lua_pushboolean(ls, 0);
                return 1;
            }
            lua_pushboolean(ls, 1);
            return 1;
        }
        const char* path = luaL_checkstring(ls, arg);
        bool looped = lua_toboolean(ls, arg + 1) != 0;
        if (!rt->soundService->playPath(path, looped)) {
            std::fprintf(stderr, "[Lua] SoundService:Play fallito: %s\n",
                         rt->soundService->lastError.c_str());
            lua_pushboolean(ls, 0);
            return 1;
        }
        lua_pushboolean(ls, 1);
        return 1;
    }

    static int luaSoundStop(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->soundService) rt->soundService->stopAll();
        return 0;
    }

    static int luaParticleEmitterEmit(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->particles) return 0;
        int em = -1;
        if (lua_istable(ls, 1)) {
            lua_getfield(ls, 1, "__emitterIndex");
            if (lua_isnumber(ls, -1)) em = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
        }
        int count = (int)luaL_optinteger(ls, lua_istable(ls, 1) ? 2 : 1, 8);
        if (em < 0 || em >= (int)rt->scriptEmitters_.size()) return 0;
        auto& e = rt->scriptEmitters_[em];
        if (e.partIndex < 0 || !rt->workspace || e.partIndex >= (int)rt->workspace->parts.size())
            return 0;
        const auto& p = rt->workspace->parts[e.partIndex];
        rt->particles->emitPreset((ParticlePreset)e.preset,
            p.position + glm::vec3(0, p.size.y * 0.5f, 0), count, e.color, {0, 1, 0});
        return 0;
    }

    static int luaParticleEmitterNewIndex(lua_State* ls) {
        auto* rt = fromState(ls);
        const char* key = lua_tostring(ls, 2);
        if (!rt || !key || !lua_istable(ls, 1)) return 0;
        lua_getfield(ls, 1, "__emitterIndex");
        int em = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        if (em < 0 || em >= (int)rt->scriptEmitters_.size()) return 0;
        auto& e = rt->scriptEmitters_[em];
        if (std::strcmp(key, "Rate") == 0)
            e.rate = std::clamp((float)lua_tonumber(ls, 3), 0.f, 60.f);
        else if (std::strcmp(key, "Enabled") == 0)
            e.enabled = lua_toboolean(ls, 3) != 0;
        else if (std::strcmp(key, "Color") == 0 && lua_istable(ls, 3))
            readColor3Arg(ls, 3, e.color);
        lua_rawset(ls, 1);
        return 0;
    }

    static int luaAnimateFrame(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace)
            return blockyLuaError(ls, "AnimateFrame: workspace non pronto");

        int arg = 1;
        const char* modelName = nullptr;
        if (lua_type(ls, 1) == LUA_TSTRING && lua_gettop(ls) >= 3) {
            modelName = lua_tostring(ls, 1);
            arg = 2;
        }

        int frame = (int)luaL_checkinteger(ls, arg);
        glm::vec3 pos{0, 0, 0};
        if (lua_istable(ls, arg + 1)) {
            lua_getfield(ls, arg + 1, "X");
            pos.x = (float)luaL_optnumber(ls, -1, 0);
            lua_pop(ls, 1);
            lua_getfield(ls, arg + 1, "Y");
            pos.y = (float)luaL_optnumber(ls, -1, 0);
            lua_pop(ls, 1);
            lua_getfield(ls, arg + 1, "Z");
            pos.z = (float)luaL_optnumber(ls, -1, 0);
            lua_pop(ls, 1);
            if (pos.x == 0 && pos.y == 0 && pos.z == 0) {
                readVec3Field(ls, arg + 1, "x", pos);
            }
        } else {
            pos.x = (float)luaL_optnumber(ls, arg + 1, 0);
            pos.y = (float)luaL_optnumber(ls, arg + 2, 0);
            pos.z = (float)luaL_optnumber(ls, arg + 3, 0);
        }

        WorkspaceModel* target = nullptr;
        if (modelName && modelName[0]) {
            for (auto& m : rt->workspace->models) {
                if (m.name == modelName) { target = &m; break; }
            }
        }
        if (!target && !rt->workspace->models.empty())
            target = &rt->workspace->models[0];
        if (!target)
            return blockyLuaError(ls, "AnimateFrame: nessun modello nel Workspace");

        if (!target->rig.hasAnimRig())
            target->rig.simpleRig = true;
        int idx = target->anim.animateFrame(frame, pos, target->rotation, target->scale);
        std::fprintf(stdout, "[Lua] AnimateFrame(%d, %.2f,%.2f,%.2f) -> %s key#%d\n",
            frame, pos.x, pos.y, pos.z, target->name.c_str(), idx);
        lua_pushinteger(ls, idx);
        return 1;
    }

    static int luaAnimatorPlay(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->animator)
            return blockyLuaError(ls, "Animator non pronto");
        int arg = 1;
        if (lua_istable(ls, 1) && lua_gettop(ls) >= 2) arg = 2;
        const char* name = luaL_optstring(ls, arg, "Wave");
        rt->animator->play(name);
        return 0;
    }

    static int luaAnimatorStop(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->animator) rt->animator->stop();
        return 0;
    }

    static int luaAnimatorSetWalk(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->animator)
            return blockyLuaError(ls, "Animator non pronto");
        int arg = 1;
        if (lua_istable(ls, 1) && lua_gettop(ls) >= 2) arg = 2;
        const char* id = luaL_checkstring(ls, arg);
        rt->animator->setWalkAnimId(id);
        return 0;
    }

    static int luaAnimatorSetJump(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->animator)
            return blockyLuaError(ls, "Animator non pronto");
        int arg = 1;
        if (lua_istable(ls, 1) && lua_gettop(ls) >= 2) arg = 2;
        const char* id = luaL_checkstring(ls, arg);
        rt->animator->setJumpAnimId(id);
        return 0;
    }

    static int luaAnimatorList(lua_State* ls) {
        lua_newtable(ls);
        auto ids = animLibrary().listIds();
        for (int i = 0; i < (int)ids.size(); ++i) {
            lua_pushstring(ls, ids[i].c_str());
            lua_rawseti(ls, -2, i + 1);
        }
        return 1;
    }

    static int luaDisableJump(lua_State*) {
        scriptControls().jumpEnabled = false;
        return 0;
    }
    static int luaEnableJump(lua_State*) {
        scriptControls().jumpEnabled = true;
        return 0;
    }

    static int glfwKeyFromName(const char* name) {
        if (!name) return -1;
        std::string k = name;
        for (char& c : k)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (k == "w") return GLFW_KEY_W;
        if (k == "a") return GLFW_KEY_A;
        if (k == "s") return GLFW_KEY_S;
        if (k == "d") return GLFW_KEY_D;
        if (k == "space") return GLFW_KEY_SPACE;
        if (k == "leftshift" || k == "lshift" || k == "shift") return GLFW_KEY_LEFT_SHIFT;
        if (k == "rightshift" || k == "rshift") return GLFW_KEY_RIGHT_SHIFT;
        if (k == "leftcontrol" || k == "lctrl" || k == "leftctrl" || k == "ctrl"
            || k == "control") return GLFW_KEY_LEFT_CONTROL;
        if (k == "rightcontrol" || k == "rctrl" || k == "rightctrl") return GLFW_KEY_RIGHT_CONTROL;
        if (k == "e") return GLFW_KEY_E;
        if (k == "q") return GLFW_KEY_Q;
        if (k == "f") return GLFW_KEY_F;
        if (k == "r") return GLFW_KEY_R;
        if (k == "c") return GLFW_KEY_C;
        if (k == "v") return GLFW_KEY_V;
        if (k.size() == 1 && k[0] >= '1' && k[0] <= '9')
            return GLFW_KEY_1 + (k[0] - '1');
        if (k == "0") return GLFW_KEY_0;
        return -1;
    }

    static int luaInputIsKeyDown(lua_State* ls) {
        int arg = (lua_gettop(ls) >= 2 && (lua_isstring(ls, 2) || lua_isnumber(ls, 2))) ? 2 : 1;
        const char* key = luaL_checkstring(ls, arg);
        GLFWwindow* win = glfwGetCurrentContext();
        int code = glfwKeyFromName(key);
        bool down = false;
        if (win && code >= 0)
            down = glfwGetKey(win, code) == GLFW_PRESS;
        lua_pushboolean(ls, down ? 1 : 0);
        return 1;
    }

    static int luaInputIsSprintHeld(lua_State* ls) {
        lua_pushboolean(ls, scriptControls().sprintHeld ? 1 : 0);
        return 1;
    }

    static int luaInputIsMoving(lua_State* ls) {
        lua_pushboolean(ls, scriptControls().moveHeld ? 1 : 0);
        return 1;
    }
    static int luaInputIsMouseButtonPressed(lua_State* ls) {
        int arg = 1;
        if (lua_istable(ls, 1) && lua_gettop(ls) >= 2) arg = 2;
        int btn = 0;
        if (lua_istable(ls, arg)) {
            lua_getfield(ls, arg, "Value");
            if (lua_isnumber(ls, -1)) btn = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
        } else if (lua_isnumber(ls, arg)) {
            btn = (int)lua_tointeger(ls, arg);
        } else if (lua_isstring(ls, arg)) {
            const char* s = lua_tostring(ls, arg);
            if (s && (s[0] == 'r' || s[0] == 'R' || s[0] == '2')) btn = 1;
            else if (s && (s[0] == 'm' || s[0] == 'M' || s[0] == '3')) btn = 2;
        }
        GLFWwindow* win = glfwGetCurrentContext();
        int code = GLFW_MOUSE_BUTTON_LEFT;
        if (btn == 1) code = GLFW_MOUSE_BUTTON_RIGHT;
        if (btn == 2) code = GLFW_MOUSE_BUTTON_MIDDLE;
        bool down = win && glfwGetMouseButton(win, code) == GLFW_PRESS;
        lua_pushboolean(ls, down ? 1 : 0);
        return 1;
    }

    static WorkspaceLabel* findLabelByName(LuaRuntime* rt, const char* name) {
        if (!rt || !rt->workspace || !name) return nullptr;
        for (auto& l : rt->workspace->labels)
            if (l.name == name) return &l;
        return nullptr;
    }

    static int luaGuiCreateLabel(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace)
            return blockyLuaError(ls, "StarterGui:CreateLabel: engine non pronto");
        WorkspaceLabel lab;
        int base = 1;
        if (lua_istable(ls, 1) && lua_istable(ls, 2))
            base = 2;
        else if (lua_istable(ls, 1) && !lua_isstring(ls, 1))
            base = 1;
        else if (lua_isstring(ls, 1) || (lua_gettop(ls) >= 2 && lua_isstring(ls, 2))) {
            int ni = lua_isstring(ls, 1) ? 1 : 2;
            lab.name = lua_tostring(ls, ni);
            lab.text = luaL_optstring(ls, ni + 1, lab.name.c_str());
            lab.anchorX = (float)luaL_optnumber(ls, ni + 2, 0.5);
            lab.anchorY = (float)luaL_optnumber(ls, ni + 3, 0.9);
            lab.className = "TextLabel";
            lab.bgColor = {0.f, 0.f, 0.f};
            lab.bgTransparency = 0.35f;
            lab.color = {1.f, 1.f, 1.f};
            rt->workspace->labels.push_back(lab);
            lua_pushstring(ls, lab.name.c_str());
            return 1;
        }
        if (!lua_istable(ls, base))
            return blockyLuaError(ls, "CreateLabel(name,text,ax,ay) o CreateLabel({...})");
        auto getStr = [&](const char* k, const char* def) -> std::string {
            lua_getfield(ls, base, k);
            std::string v = lua_isstring(ls, -1) ? lua_tostring(ls, -1) : def;
            lua_pop(ls, 1);
            return v;
        };
        auto getNum = [&](const char* k, float def) -> float {
            lua_getfield(ls, base, k);
            float v = lua_isnumber(ls, -1) ? (float)lua_tonumber(ls, -1) : def;
            lua_pop(ls, 1);
            return v;
        };
        auto getBool = [&](const char* k, bool def) -> bool {
            lua_getfield(ls, base, k);
            bool v = lua_isboolean(ls, -1) ? (lua_toboolean(ls, -1) != 0) : def;
            lua_pop(ls, 1);
            return v;
        };
        auto getCol = [&](const char* k, glm::vec3 def) -> glm::vec3 {
            lua_getfield(ls, base, k);
            glm::vec3 c = def;
            if (lua_istable(ls, -1)) {
                lua_rawgeti(ls, -1, 1); if (lua_isnumber(ls, -1)) c.x = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
                lua_rawgeti(ls, -1, 2); if (lua_isnumber(ls, -1)) c.y = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
                lua_rawgeti(ls, -1, 3); if (lua_isnumber(ls, -1)) c.z = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
                lua_getfield(ls, -1, "R"); if (lua_isnumber(ls, -1)) c.x = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
                lua_getfield(ls, -1, "G"); if (lua_isnumber(ls, -1)) c.y = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
                lua_getfield(ls, -1, "B"); if (lua_isnumber(ls, -1)) c.z = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
            }
            lua_pop(ls, 1);
            return c;
        };
        lab.name = getStr("Name", "Label");
        lab.text = getStr("Text", "");
        lab.className = getStr("ClassName", "TextLabel");
        lab.anchorX = getNum("AnchorX", 0.5f);
        lab.anchorY = getNum("AnchorY", 0.9f);
        lab.offsetX = getNum("OffsetX", 0.f);
        lab.offsetY = getNum("OffsetY", 0.f);
        lab.sizeW = getNum("SizeW", 0.f);
        lab.sizeH = getNum("SizeH", 0.f);
        lab.fontScale = getNum("FontScale", 1.f);
        lab.bgTransparency = getNum("BackgroundTransparency", 0.35f);
        lab.color = getCol("TextColor", {1.f, 1.f, 1.f});
        lab.bgColor = getCol("BackgroundColor", {0.f, 0.f, 0.f});
        lab.visible = getBool("Visible", true);
        if (auto* existing = findLabelByName(rt, lab.name.c_str())) {
            *existing = lab;
        } else {
            rt->workspace->labels.push_back(lab);
        }
        lua_pushstring(ls, lab.name.c_str());
        return 1;
    }

    static int luaGuiSetLabelText(lua_State* ls) {
        auto* rt = fromState(ls);
        int ni = lua_isstring(ls, 1) ? 1 : 2;
        const char* name = luaL_checkstring(ls, ni);
        const char* text = luaL_checkstring(ls, ni + 1);
        if (auto* l = findLabelByName(rt, name)) l->text = text;
        return 0;
    }

    static int luaGuiSetLabelSize(lua_State* ls) {
        auto* rt = fromState(ls);
        int ni = lua_isstring(ls, 1) ? 1 : 2;
        const char* name = luaL_checkstring(ls, ni);
        float w = (float)luaL_checknumber(ls, ni + 1);
        float h = (float)luaL_optnumber(ls, ni + 2, -1.0);
        if (auto* l = findLabelByName(rt, name)) {
            l->sizeW = w;
            if (h > 0.f) l->sizeH = h;
        }
        return 0;
    }

    static int luaGuiSetLabelOffset(lua_State* ls) {
        auto* rt = fromState(ls);
        int ni = lua_isstring(ls, 1) ? 1 : 2;
        const char* name = luaL_checkstring(ls, ni);
        float ox = (float)luaL_checknumber(ls, ni + 1);
        float oy = (float)luaL_optnumber(ls, ni + 2, 0.0);
        if (auto* l = findLabelByName(rt, name)) {
            l->offsetX = ox;
            l->offsetY = oy;
        }
        return 0;
    }

    static int luaGuiSetLabelColor(lua_State* ls) {
        auto* rt = fromState(ls);
        int ni = lua_isstring(ls, 1) ? 1 : 2;
        const char* name = luaL_checkstring(ls, ni);
        float r = (float)luaL_checknumber(ls, ni + 1);
        float g = (float)luaL_checknumber(ls, ni + 2);
        float b = (float)luaL_checknumber(ls, ni + 3);
        if (auto* l = findLabelByName(rt, name))
            l->color = glm::clamp(glm::vec3(r, g, b), glm::vec3(0.f), glm::vec3(1.f));
        return 0;
    }

    static int luaGuiSetLabelBg(lua_State* ls) {
        auto* rt = fromState(ls);
        int ni = lua_isstring(ls, 1) ? 1 : 2;
        const char* name = luaL_checkstring(ls, ni);
        float r = (float)luaL_checknumber(ls, ni + 1);
        float g = (float)luaL_checknumber(ls, ni + 2);
        float b = (float)luaL_checknumber(ls, ni + 3);
        float t = (float)luaL_optnumber(ls, ni + 4, 0.0);
        if (auto* l = findLabelByName(rt, name)) {
            l->bgColor = glm::clamp(glm::vec3(r, g, b), glm::vec3(0.f), glm::vec3(1.f));
            l->bgTransparency = std::clamp(t, 0.f, 1.f);
        }
        return 0;
    }

    static int luaGuiSetLabelVisible(lua_State* ls) {
        auto* rt = fromState(ls);
        int ni = lua_isstring(ls, 1) ? 1 : 2;
        const char* name = luaL_checkstring(ls, ni);
        bool vis = lua_toboolean(ls, ni + 1) != 0;
        if (auto* l = findLabelByName(rt, name)) l->visible = vis;
        return 0;
    }

    static int luaGuiFindLabel(lua_State* ls) {
        auto* rt = fromState(ls);
        int ni = lua_isstring(ls, 1) ? 1 : 2;
        const char* name = luaL_checkstring(ls, ni);
        lua_pushboolean(ls, findLabelByName(rt, name) != nullptr ? 1 : 0);
        return 1;
    }

    static int luaHumIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        if (!lua_isnil(ls, -1))
            return 1;
        lua_pop(ls, 1);
        if (!key) return 0;

        LuaRuntime* rt = fromState(ls);
        if (std::strcmp(key, "Health") == 0) {
            lua_pushnumber(ls, (rt && rt->playerHumanoid) ? rt->playerHumanoid->health : 0.f);
            return 1;
        }
        if (std::strcmp(key, "MaxHealth") == 0) {
            lua_pushnumber(ls, (rt && rt->playerHumanoid) ? rt->playerHumanoid->maxHealth : 100.f);
            return 1;
        }
        if (std::strcmp(key, "WalkSpeed") == 0) {
            lua_pushnumber(ls, scriptControls().walkSpeedOverride);
            return 1;
        }
        if (std::strcmp(key, "JumpPower") == 0) {
            lua_pushnumber(ls, scriptControls().jumpPowerOverride);
            return 1;
        }
        if (std::strcmp(key, "GetState") == 0) {
            blockyPushCFunction(ls, luaHumGetState);
            return 1;
        }
        return 0;
    }

    static int luaLivePartSetAttribute(lua_State* ls) {
        return studio_pro_apis::luaSetAttribute(ls);
    }
    static int luaLivePartGetAttribute(lua_State* ls) {
        return studio_pro_apis::luaGetAttribute(ls);
    }

    static int luaHumNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key) return 0;
        if (std::strcmp(key, "Health") == 0) {
            LuaRuntime* rt = fromState(ls);
            if (rt && rt->playerHumanoid) {
                float v = (float)luaL_optnumber(ls, 3, rt->playerHumanoid->health);
                rt->playerHumanoid->health = std::clamp(v, 0.f, rt->playerHumanoid->maxHealth);
            }
            return 0;
        }
        if (std::strcmp(key, "WalkSpeed") == 0) {
            float v = (float)luaL_optnumber(ls, 3, 16.0);
            scriptControls().walkSpeedOverride = std::clamp(v, 1.f, LuaScriptSecurity::kMaxWalkSpeed);
            return 0;
        }
        if (std::strcmp(key, "JumpPower") == 0) {
            float v = (float)luaL_optnumber(ls, 3, 50.0);
            scriptControls().jumpPowerOverride = std::clamp(v, 0.f, LuaScriptSecurity::kMaxJumpPower);
            return 0;
        }
        lua_rawset(ls, 1);
        return 0;
    }

    static int luaHumSetJumpEnabled(lua_State* ls) {
        bool v = true;
        if (lua_gettop(ls) >= 2) v = lua_toboolean(ls, 2) != 0;
        else if (lua_gettop(ls) >= 1) v = lua_toboolean(ls, 1) != 0;
        scriptControls().jumpEnabled = v;
        return 0;
    }
    static int luaHumSetWalkEnabled(lua_State* ls) {
        bool v = true;
        if (lua_gettop(ls) >= 2) v = lua_toboolean(ls, 2) != 0;
        else if (lua_gettop(ls) >= 1) v = lua_toboolean(ls, 1) != 0;
        scriptControls().walkEnabled = v;
        return 0;
    }
    static int luaHumSetWalkSpeed(lua_State* ls) {
        int arg = (lua_gettop(ls) >= 2) ? 2 : 1;
        float v = (float)luaL_optnumber(ls, arg, 16.0);
        v = std::clamp(v, 1.f, LuaScriptSecurity::kMaxWalkSpeed);
        scriptControls().walkSpeedOverride = v;
        return 0;
    }
    static int luaHumSetJumpPower(lua_State* ls) {
        int arg = (lua_gettop(ls) >= 2) ? 2 : 1;
        float v = (float)luaL_optnumber(ls, arg, 50.0);
        v = std::clamp(v, 0.f, LuaScriptSecurity::kMaxJumpPower);
        scriptControls().jumpPowerOverride = v;
        return 0;
    }
    static int luaHumGet(lua_State* ls) {
        lua_newtable(ls);
        lua_pushboolean(ls, scriptControls().jumpEnabled ? 1 : 0);
        lua_setfield(ls, -2, "JumpEnabled");
        lua_pushboolean(ls, scriptControls().walkEnabled ? 1 : 0);
        lua_setfield(ls, -2, "WalkEnabled");
        lua_pushnumber(ls, scriptControls().walkSpeedOverride);
        lua_setfield(ls, -2, "WalkSpeed");
        lua_pushnumber(ls, scriptControls().jumpPowerOverride);
        lua_setfield(ls, -2, "JumpPower");
        return 1;
    }


    static int luaHumGetPosition(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        glm::vec3 p = (rt && rt->playerPos) ? *rt->playerPos : glm::vec3(0.f);
        pushVec3(ls, p.x, p.y, p.z);
        return 1;
    }

    static int luaHumTakeDamage(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        int arg = (lua_gettop(ls) >= 2) ? 2 : 1;
        float dmg = (float)luaL_optnumber(ls, arg, 0.0);
        if (rt && rt->playerHumanoid && dmg > 0.f) {
            dmg = std::clamp(dmg, 0.f, 100.f);
            rt->playerHumanoid->takeDamage(dmg);
        }
        return 0;
    }

    static int luaHumGetState(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        const char* st = (rt && rt->playerHumanoid) ? rt->playerHumanoid->getState() : "Running";
        lua_pushstring(ls, st);
        return 1;
    }

    static int luaHumDiedConnect(lua_State* ls) {
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "Humanoid.Died:Connect(function)");
        lua_pushvalue(ls, fidx);
        scriptControls().humanoidDied.push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaHumHealthChangedConnect(lua_State* ls) {
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "Humanoid.HealthChanged:Connect(function)");
        lua_pushvalue(ls, fidx);
        scriptControls().humanoidHealthChanged.push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaHumStateChangedConnect(lua_State* ls) {
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "Humanoid.StateChanged:Connect(function)");
        lua_pushvalue(ls, fidx);
        scriptControls().humanoidStateChanged.push_back(blockyLuaRef(ls));
        return 0;
    }

    static int luaPlayerAddedConnect(lua_State* ls) {
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "Players.PlayerAdded:Connect(function)");
        lua_pushvalue(ls, fidx);
        scriptControls().playerAdded.push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaCharacterAddedConnect(lua_State* ls) {
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "CharacterAdded:Connect(function)");
        lua_pushvalue(ls, fidx);
        scriptControls().characterAdded.push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaPlayersGetPlayers(lua_State* ls) {
        lua_newtable(ls);
        auto* rt = fromState(ls);
        if (rt) {
            rt->pushLocalPlayer(ls);
            if (!lua_isnil(ls, -1))
                lua_rawseti(ls, -2, 1);
            else
                lua_pop(ls, 1);
        }
        return 1;
    }
    static int luaPlayersGetPlayerFromCharacter(lua_State* ls) {
        // Single-player: any Character maps to LocalPlayer
        auto* rt = fromState(ls);
        if (rt) { rt->pushLocalPlayer(ls); return 1; }
        lua_pushnil(ls);
        return 1;
    }

    static int luaPartChildAddedConnect(lua_State* ls) {
        int partIdx = -1;
        if (lua_istable(ls, 1)) {
            lua_getfield(ls, 1, "__partIndex");
            if (lua_isnumber(ls, -1)) partIdx = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
        }
        int fidx = fnArgIndex(ls);
        if (!fidx || partIdx < 0)
            return blockyLuaError(ls, "ChildAdded:Connect(fn) su Part");
        lua_pushvalue(ls, fidx);
        scriptControls().partChildAdded[partIdx].push_back(blockyLuaRef(ls));
        return 0;
    }
    static int luaSharedChildAddedConnect(lua_State* ls) {
        int idx = -1;
        if (lua_istable(ls, 1)) {
            lua_getfield(ls, 1, "__sharedIndex");
            if (lua_isnumber(ls, -1)) idx = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
        }
        int fidx = fnArgIndex(ls);
        if (!fidx || idx < 0)
            return blockyLuaError(ls, "ChildAdded:Connect(fn) su Folder/Instance");
        lua_pushvalue(ls, fidx);
        scriptControls().sharedChildAdded[idx].push_back(blockyLuaRef(ls));
        return 0;
    }

    static int luaToolEventConnect(lua_State* ls) {
        std::string name = "Tool";
        std::string ev = "Equipped";
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "_toolName", "Tool");
            ev = readStringField(ls, 1, "_toolEvent", "Equipped");
        }
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "Tool.Event:Connect(function)");
        lua_pushvalue(ls, fidx);
        int ref = blockyLuaRef(ls);
        if (ev == "Unequipped")
            scriptControls().toolUnequipped[name].push_back(ref);
        else if (ev == "Activated")
            scriptControls().toolActivated[name].push_back(ref);
        else
            scriptControls().toolEquipped[name].push_back(ref);
        return 0;
    }
    static int luaToolActivate(lua_State* ls) {
        auto* rt = fromState(ls);
        std::string name = "Tool";
        std::string id;
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "_toolName", "Tool");
            id = readStringField(ls, 1, "_toolId", "");
        }
        if (rt) rt->fireItemActivated(id.empty() ? name : id, name);
        return 0;
    }

    static int luaWeldIndex(lua_State* ls) {
        auto* rt = fromState(ls);
        const char* key = lua_tostring(ls, 2);
        if (!rt || !rt->workspace || !key) return 0;
        lua_getfield(ls, 1, "__weldIndex");
        int wi = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        if (wi < 0 || wi >= (int)rt->workspace->welds.size()) return 0;
        auto& w = rt->workspace->welds[wi];
        if (std::strcmp(key, "Enabled") == 0) { lua_pushboolean(ls, w.enabled); return 1; }
        if (std::strcmp(key, "Part0") == 0) {
            if (w.part0 >= 0) pushLivePart(ls, w.part0); else lua_pushnil(ls);
            return 1;
        }
        if (std::strcmp(key, "Part1") == 0) {
            if (w.part1 >= 0) pushLivePart(ls, w.part1); else lua_pushnil(ls);
            return 1;
        }
        if (std::strcmp(key, "C0") == 0 || std::strcmp(key, "C1") == 0) {
            pushVec3(ls, w.relPos.x, w.relPos.y, w.relPos.z);
            return 1;
        }
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        return 1;
    }
    static int luaWeldNewIndex(lua_State* ls) {
        auto* rt = fromState(ls);
        const char* key = lua_tostring(ls, 2);
        if (!rt || !rt->workspace || !key) return 0;
        lua_getfield(ls, 1, "__weldIndex");
        int wi = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        if (wi < 0 || wi >= (int)rt->workspace->welds.size()) return 0;
        auto& w = rt->workspace->welds[wi];
        if (std::strcmp(key, "Enabled") == 0) {
            w.enabled = lua_toboolean(ls, 3) != 0;
        } else if (std::strcmp(key, "Part0") == 0 && lua_istable(ls, 3)) {
            lua_getfield(ls, 3, "__partIndex");
            if (lua_isnumber(ls, -1)) w.part0 = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
        } else if (std::strcmp(key, "Part1") == 0 && lua_istable(ls, 3)) {
            lua_getfield(ls, 3, "__partIndex");
            if (lua_isnumber(ls, -1)) w.part1 = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
            if (w.part0 >= 0 && w.part1 >= 0
                && w.part0 < (int)rt->workspace->parts.size()
                && w.part1 < (int)rt->workspace->parts.size()) {
                w.relPos = rt->workspace->parts[w.part1].position
                         - rt->workspace->parts[w.part0].position;
                w.relRot = rt->workspace->parts[w.part1].rotation
                         - rt->workspace->parts[w.part0].rotation;
            }
        } else if ((std::strcmp(key, "C0") == 0 || std::strcmp(key, "C1") == 0)
                   && lua_istable(ls, 3)) {
            readVec3Arg(ls, 3, w.relPos);
        }
        lua_pushvalue(ls, 2);
        lua_pushvalue(ls, 3);
        lua_rawset(ls, 1);
        return 0;
    }

    static int luaLeaderstatValueIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key || std::strcmp(key, "Value") != 0) return 0;
        auto* rt = fromState(ls);
        lua_getfield(ls, 1, "__stat");
        const char* stat = lua_isstring(ls, -1) ? lua_tostring(ls, -1) : "";
        lua_pop(ls, 1);
        double v = 0.0;
        if (rt && rt->classifica && stat[0])
            v = rt->classifica->get(stat);
        lua_pushnumber(ls, v);
        return 1;
    }
    static int luaLeaderstatValueNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key || std::strcmp(key, "Value") != 0) return 0;
        auto* rt = fromState(ls);
        lua_getfield(ls, 1, "__stat");
        const char* stat = lua_isstring(ls, -1) ? lua_tostring(ls, -1) : "";
        lua_pop(ls, 1);
        if (rt && rt->classifica && stat[0])
            rt->classifica->set(stat, (double)luaL_optnumber(ls, 3, 0.0));
        return 0;
    }
    static void pushLeaderstatValue(lua_State* ls, const char* stat) {
        lua_newtable(ls);
        lua_pushstring(ls, stat);
        lua_setfield(ls, -2, "__stat");
        lua_pushstring(ls, "IntValue");
        lua_setfield(ls, -2, "ClassName");
        lua_pushstring(ls, stat);
        lua_setfield(ls, -2, "Name");
        lua_newtable(ls);
        blockyPushCFunction(ls, luaLeaderstatValueIndex);
        lua_setfield(ls, -2, "__index");
        blockyPushCFunction(ls, luaLeaderstatValueNewIndex);
        lua_setfield(ls, -2, "__newindex");
        lua_setmetatable(ls, -2);
    }
    static int luaLeaderstatsIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key) return 0;
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        if (!lua_isnil(ls, -1)) return 1;
        lua_pop(ls, 1);
        if (std::strcmp(key, "Create") == 0 || std::strcmp(key, "Name") == 0
            || std::strcmp(key, "ClassName") == 0)
            return 0;
        // Auto-create IntValue proxy for any stat name
        pushLeaderstatValue(ls, key);
        lua_pushvalue(ls, 2);
        lua_pushvalue(ls, -2);
        lua_rawset(ls, 1);
        return 1;
    }
    static int luaLeaderstatsCreate(lua_State* ls) {
        const char* stat = luaL_checkstring(ls, lua_istable(ls, 1) ? 2 : 1);
        auto* rt = fromState(ls);
        if (rt && rt->classifica)
            rt->classifica->set(stat, 0.0);
        pushLeaderstatValue(ls, stat);
        return 1;
    }

    static int luaProximityTriggeredConnect(lua_State* ls) {
        std::string name = "Prompt";
        if (lua_istable(ls, 1))
            name = readStringField(ls, 1, "_promptName", "Prompt");
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "Triggered:Connect(function)");
        lua_pushvalue(ls, fidx);
        scriptControls().proximityTriggered[name].push_back(blockyLuaRef(ls));
        return 0;
    }

    static int luaHumGetHealth(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        float hp = (rt && rt->playerHumanoid) ? rt->playerHumanoid->health : 0.f;
        lua_pushnumber(ls, hp);
        return 1;
    }

    static int luaHumSetPosition(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        if (!rt || !rt->playerPos) return 0;
        glm::vec3 p = *rt->playerPos;
        if (lua_istable(ls, 1) || lua_istable(ls, 2)) {
            int t = lua_istable(ls, 2) ? 2 : 1;
            lua_getfield(ls, t, "X"); if (lua_isnumber(ls, -1)) p.x = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
            lua_getfield(ls, t, "Y"); if (lua_isnumber(ls, -1)) p.y = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
            lua_getfield(ls, t, "Z"); if (lua_isnumber(ls, -1)) p.z = (float)lua_tonumber(ls, -1); lua_pop(ls, 1);
        } else {
            int b = lua_isnumber(ls, 1) ? 1 : 2;
            p.x = (float)luaL_optnumber(ls, b, p.x);
            p.y = (float)luaL_optnumber(ls, b + 1, p.y);
            p.z = (float)luaL_optnumber(ls, b + 2, p.z);
        }
        *rt->playerPos = p;
        return 0;
    }

    static int luaBackpackGive(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        int arg = (lua_type(ls, 1) == LUA_TTABLE) ? 2 : 1;
        const char* name = luaL_optstring(ls, arg, "Item");
        const char* meshPath = luaL_optstring(ls, arg + 1, "");
        if (rt && rt->workspace) {
            auto& it = rt->workspace->addBackpackItem(name, 4);
            // Stessa pipeline gia' usata dall'import di un'arma in Avatar
            // Editor (SelectionAndInput.h) — render in mano (PlayingHudPanel.h),
            // salvataggio (PlaceExporter/GameHub) e controllo file
            // (SceneIntegrity) leggono gia' questo campo, mancava solo il
            // modo di impostarlo da script.
            if (meshPath && meshPath[0])
                it.meshPath = resolveScriptPath(meshPath);
        }
        return 0;
    }

    static int luaBackpackEquipped(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        std::string name;
        if (rt && rt->workspace) {
            int e = rt->workspace->equippedBackpack;
            if (e >= 0 && e < (int)rt->workspace->backpack.size())
                name = rt->workspace->backpack[e].name;
        }
        lua_pushstring(ls, name.c_str());
        return 1;
    }

    static int luaCameraShake(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        if (!rt || !rt->cameraShake) return 0;
        const float magnitude = (float)luaL_optnumber(ls, 1, 4.0);
        const float duration = (float)luaL_optnumber(ls, 2, 0.3);
        rt->cameraShake(magnitude, duration);
        return 0;
    }

    static bool fillAimRay(LuaRuntime* rt, glm::vec3& o, glm::vec3& d) {
        o = glm::vec3(0.f, 2.2f, 0.f);
        d = glm::vec3(0.f, 0.f, -1.f);
        if (rt && rt->getAimRay && rt->getAimRay(o, d))
            return true;
        if (rt && rt->playerPos)
            o = *rt->playerPos + glm::vec3(0.f, 2.2f, 0.f);
        if (rt && rt->playerYaw) {
            float r = *rt->playerYaw * 3.14159265f / 180.f;
            d = glm::vec3(std::sin(r), 0.f, std::cos(r));
        }
        return false;
    }

    static int luaCameraScreenRay(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        glm::vec3 o, d;
        fillAimRay(rt, o, d);
        pushVec3(ls, o.x, o.y, o.z);
        pushVec3(ls, d.x, d.y, d.z);
        return 2;
    }

    static int luaCameraScreenPointToRay(lua_State* ls) {
        (void)luaL_optnumber(ls, 1, 0);
        (void)luaL_optnumber(ls, 2, 0);
        LuaRuntime* rt = fromState(ls);
        glm::vec3 o, d;
        fillAimRay(rt, o, d);
        lua_newtable(ls);
        pushVec3(ls, o.x, o.y, o.z);
        lua_setfield(ls, -2, "Origin");
        pushVec3(ls, d.x, d.y, d.z);
        lua_setfield(ls, -2, "Direction");
        return 1;
    }

    static int luaCameraGetLook(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        glm::vec3 o, d;
        fillAimRay(rt, o, d);
        pushVec3(ls, d.x, d.y, d.z);
        return 1;
    }

    static int luaCameraGetCFrame(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        glm::vec3 o, d;
        fillAimRay(rt, o, d);
        if (pushPreludeField(ls, "CFrame")) {
            lua_getfield(ls, -1, "lookAt");
            lua_remove(ls, -2);
            pushVec3(ls, o.x, o.y, o.z);
            pushVec3(ls, o.x + d.x, o.y + d.y, o.z + d.z);
            if (lua_pcall(ls, 2, 1, 0) == LUA_OK) return 1;
            lua_pop(ls, 1);
        }
        lua_newtable(ls);
        pushVec3(ls, o.x, o.y, o.z);
        lua_setfield(ls, -2, "Position");
        pushVec3(ls, d.x, d.y, d.z);
        lua_setfield(ls, -2, "LookVector");
        return 1;
    }

    static int luaCameraIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key) return 0;
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        if (!lua_isnil(ls, -1)) return 1;
        lua_pop(ls, 1);
        auto* rt = fromState(ls);
        if (std::strcmp(key, "FieldOfView") == 0) {
            lua_pushnumber(ls, (rt && rt->cameraFov) ? *rt->cameraFov : 70.0);
            return 1;
        }
        if (std::strcmp(key, "CameraType") == 0) {
            int t = rt ? rt->cameraType : 0;
            lua_pushstring(ls, t == 2 ? "Attach" : (t == 1 ? "Scriptable" : "Custom"));
            return 1;
        }
        if (std::strcmp(key, "CameraSubject") == 0) {
            lua_getglobal(ls, "Humanoid");
            return 1;
        }
        if (std::strcmp(key, "CFrame") == 0)
            return luaCameraGetCFrame(ls);
        return 0;
    }

    static int luaCameraNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        auto* rt = fromState(ls);
        if (!key || !rt) return 0;
        if (std::strcmp(key, "FieldOfView") == 0 && rt->cameraFov) {
            *rt->cameraFov = std::clamp((float)luaL_optnumber(ls, 3, 70.0), 40.f, 120.f);
            return 0;
        }
        if (std::strcmp(key, "CameraType") == 0 && lua_isstring(ls, 3)) {
            const char* t = lua_tostring(ls, 3);
            if (std::strcmp(t, "Scriptable") == 0) rt->cameraType = 1;
            else if (std::strcmp(t, "Attach") == 0 || std::strcmp(t, "FirstPerson") == 0) {
                rt->cameraType = 2;
                if (rt->firstPersonFlag) *rt->firstPersonFlag = true;
            } else {
                rt->cameraType = 0;
                if (rt->firstPersonFlag) *rt->firstPersonFlag = false;
            }
            return 0;
        }
        lua_rawset(ls, 1);
        return 0;
    }

    static int luaPathCreatePath(lua_State* ls) {
        lua_newtable(ls);
        blockyPushCFunction(ls, luaPathComputeAsync);
        lua_setfield(ls, -2, "ComputeAsync");
        blockyPushCFunction(ls, luaWorkspaceFindPath);
        lua_setfield(ls, -2, "GetWaypoints"); // alias se ComputeAsync non usato
        return 1;
    }

    static int luaPathComputeAsync(lua_State* ls) {
        // path:ComputeAsync(from, to) -> waypoints table with .Status
        auto* rt = fromState(ls);
        int a = lua_istable(ls, 1) ? 2 : 1;
        glm::vec3 from{0}, to{0};
        if (!readVec3Arg(ls, a, from) || !readVec3Arg(ls, a + 1, to))
            return blockyLuaError(ls, "ComputeAsync(Vector3 from, Vector3 to)");
        if (!rt || !rt->nav) {
            lua_newtable(ls);
            lua_pushstring(ls, "NoPath");
            lua_setfield(ls, -2, "Status");
            return 1;
        }
        if (!rt->nav->ready() && rt->navRebuild) rt->navRebuild();
        const std::vector<glm::vec3> path = rt->nav->findPath(from, to);
        lua_newtable(ls);
        if (path.empty()) {
            lua_pushstring(ls, "NoPath");
            lua_setfield(ls, -2, "Status");
            return 1;
        }
        for (size_t i = 0; i < path.size(); ++i) {
            pushVec3(ls, path[i].x, path[i].y, path[i].z);
            lua_rawseti(ls, -2, (int)i + 1);
        }
        lua_pushstring(ls, "Success");
        lua_setfield(ls, -2, "Status");
        return 1;
    }

    static int luaHumGetLook(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        float yawDeg = (rt && rt->playerYaw) ? *rt->playerYaw : 0.f;
        float r = yawDeg * 3.14159265f / 180.f;
        float fx = std::sin(r), fz = std::cos(r);
        pushVec3(ls, fx, 0.f, fz);
        return 1;
    }

    static int luaEventCreate(lua_State* ls) {
        const char* name = luaL_checkstring(ls, 1);
        scriptControls().events[name];
        lua_pushstring(ls, name);
        return 1;
    }
    static int luaEventConnect(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt) return 0;
        const char* name = luaL_checkstring(ls, 1);
        int fidx = 2;
        if (!lua_isfunction(ls, fidx))
            return blockyLuaError(ls, "Events.Connect(name, function)");
        lua_pushvalue(ls, fidx);
        int ref = blockyLuaRef(ls);
        scriptControls().events[name].push_back(ref);
        lua_pushinteger(ls, ref);
        return 1;
    }

    static int luaGuiButtonConnect(lua_State* ls) {
        int nameIdx = 1;
        int fnIdx = 2;
        if (lua_gettop(ls) >= 3 && lua_isfunction(ls, 3)) {
            nameIdx = 2;
            fnIdx = 3;
        }
        const char* name = luaL_checkstring(ls, nameIdx);
        if (!lua_isfunction(ls, fnIdx))
            return blockyLuaError(ls, "StarterGui:ConnectButton(name, function)");
        lua_pushvalue(ls, fnIdx);
        int ref = blockyLuaRef(ls);
        scriptControls().guiButtonActivated[name].push_back(ref);
        lua_pushinteger(ls, ref);
        return 1;
    }
    static int luaEventFire(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->L) return 0;
        const char* name = luaL_checkstring(ls, 1);
        auto it = scriptControls().events.find(name);
        if (it == scriptControls().events.end()) return 0;
        int nargs = lua_gettop(ls) - 1;
        for (int ref : it->second) {
            lua_rawgeti(ls, LUA_REGISTRYINDEX, ref);
            for (int i = 0; i < nargs; ++i)
                lua_pushvalue(ls, 2 + i);
            rt->callStart_ = std::chrono::steady_clock::now();
            if (lua_pcall(ls, nargs, 0, 0) != LUA_OK) {
                std::fprintf(stderr, "[Events] %s: %s\n", name, lua_tostring(ls, -1));
                lua_pop(ls, 1);
            }
        }
        return 0;
    }
    static int luaEventClear(lua_State* ls) {
        auto* rt = fromState(ls);
        const char* name = luaL_optstring(ls, 1, "");
        if (!name[0]) {
            if (rt) {
                for (auto& kv : scriptControls().events)
                    for (int r : kv.second)
                        lua_unref(ls,r);
            }
            scriptControls().events.clear();
            return 0;
        }
        auto it = scriptControls().events.find(name);
        if (it != scriptControls().events.end()) {
            for (int r : it->second)
                lua_unref(ls,r);
            scriptControls().events.erase(it);
        }
        return 0;
    }

    static int luaBindableInvoke(lua_State* ls) {
        std::string name = "Function";
        int arg = 1;
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "Name", "Function");
            arg = 2;
        }
        const char* payload = luaL_optstring(ls, arg, "");
        auto it = scriptControls().bindableInvoke.find(name);
        if (it == scriptControls().bindableInvoke.end() || it->second == LUA_NOREF) {
            lua_pushnil(ls);
            return 1;
        }
        lua_rawgeti(ls, LUA_REGISTRYINDEX, it->second);
        if (!lua_isfunction(ls, -1)) { lua_pop(ls, 1); lua_pushnil(ls); return 1; }
        lua_pushstring(ls, payload ? payload : "");
        auto* rt = fromState(ls);
        if (rt) rt->callStart_ = std::chrono::steady_clock::now();
        if (lua_pcall(ls, 1, 1, 0) != LUA_OK) {
            const char* err = lua_tostring(ls, -1);
            if (rt) rt->lastError = err ? err : "BindableFunction error";
            lua_pop(ls, 1);
            lua_pushnil(ls);
            return 1;
        }
        return 1;
    }
    static int luaBindableFunctionIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key) return 0;
        lua_pushvalue(ls, 2);
        lua_rawget(ls, 1);
        if (!lua_isnil(ls, -1)) return 1;
        lua_pop(ls, 1);
        if (std::strcmp(key, "Invoke") == 0) {
            blockyPushCFunction(ls, luaBindableInvoke);
            return 1;
        }
        return 0;
    }
    static int luaBindableFunctionNewIndex(lua_State* ls) {
        const char* key = lua_tostring(ls, 2);
        if (!key) return 0;
        if (std::strcmp(key, "OnInvoke") == 0 && lua_isfunction(ls, 3)) {
            std::string name = readStringField(ls, 1, "Name", "Function");
            auto it = scriptControls().bindableInvoke.find(name);
            if (it != scriptControls().bindableInvoke.end() && it->second != LUA_NOREF)
                lua_unref(ls, it->second);
            lua_pushvalue(ls, 3);
            scriptControls().bindableInvoke[name] = blockyLuaRef(ls);
        }
        return 0;
    }

    static int luaBindableFire(lua_State* ls) {
        std::string name = "Event";
        if (lua_istable(ls, 1))
            name = readStringField(ls, 1, "Name", "Event");
        lua_pushstring(ls, name.c_str());
        lua_insert(ls, 1);
        if (lua_istable(ls, 2)) {
            lua_remove(ls, 2);
        }
        return luaEventFire(ls);
    }
    static int luaBindableConnect(lua_State* ls) {
        std::string name = "Event";
        if (lua_istable(ls, 1))
            name = readStringField(ls, 1, "_eventName", "Event");
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "Connect richiede function");
        lua_pushstring(ls, name.c_str());
        lua_pushvalue(ls, fidx);
        lua_remove(ls, 1);
        auto* rt = fromState(ls);
        if (!rt) return 0;
        lua_pushvalue(ls, fidx);
        int ref = blockyLuaRef(ls);
        scriptControls().events[name].push_back(ref);
        lua_pushinteger(ls, ref);
        return 1;
    }

    // ---- DataStoreService: dati che restano tra una sessione e l'altra ----
    // Prima di questo blocco GetService("DataStoreService") tornava una
    // tabella vuota (stub silenzioso): nessuno script poteva salvare niente.
    // Vedi engine/DataStore.h per il file su disco e i limiti di sicurezza.

    /** Nome dello store dalla tabella self (chiamata con ':'). */
    static std::string dsStoreOf(lua_State* ls) {
        if (!lua_istable(ls, 1)) return {};
        lua_getfield(ls, 1, "__store");
        std::string n = lua_isstring(ls, -1) ? lua_tostring(ls, -1) : "";
        lua_pop(ls, 1);
        return n;
    }

    static int luaDataStoreSetAsync(lua_State* ls) {
        const std::string store = dsStoreOf(ls);
        if (store.empty()) return blockyLuaError(ls, "usa store:SetAsync(chiave, valore)");
        const std::string key = blk::datastore::sanitizeKeyName(luaL_checkstring(ls, 2));
        if (key.empty()) return blockyLuaError(ls, "SetAsync: chiave non valida");

        blk::datastore::Value v;
        if (lua_isboolean(ls, 3)) {
            v.isNumber = true;
            v.number = lua_toboolean(ls, 3) ? 1.0 : 0.0;
        } else if (lua_isnumber(ls, 3)) {
            v.isNumber = true;
            v.number = (double)lua_tonumber(ls, 3);
        } else if (lua_isstring(ls, 3)) {
            v.isNumber = false;
            v.text = lua_tostring(ls, 3);
            if (v.text.size() > blk::datastore::kMaxStringLen)
                v.text.resize(blk::datastore::kMaxStringLen);
        } else {
            return blockyLuaError(ls, "SetAsync: il valore deve essere numero, testo o bool");
        }

        auto& s = blk::datastore::db().store(store);
        if (s.find(key) == s.end() && s.size() >= blk::datastore::kMaxKeysPerStore)
            return blockyLuaError(ls, "SetAsync: troppe chiavi nello store '%s'", store.c_str());
        s[key] = v;
        blk::datastore::db().save(store);
        return 0;
    }

    static int luaDataStoreGetAsync(lua_State* ls) {
        const std::string store = dsStoreOf(ls);
        if (store.empty()) return blockyLuaError(ls, "usa store:GetAsync(chiave)");
        const std::string key = blk::datastore::sanitizeKeyName(luaL_checkstring(ls, 2));
        auto& s = blk::datastore::db().store(store);
        auto it = s.find(key);
        if (it == s.end()) { lua_pushnil(ls); return 1; }
        if (it->second.isNumber) lua_pushnumber(ls, it->second.number);
        else lua_pushstring(ls, it->second.text.c_str());
        return 1;
    }

    static int luaDataStoreIncrementAsync(lua_State* ls) {
        const std::string store = dsStoreOf(ls);
        if (store.empty()) return blockyLuaError(ls, "usa store:IncrementAsync(chiave, quanto)");
        const std::string key = blk::datastore::sanitizeKeyName(luaL_checkstring(ls, 2));
        if (key.empty()) return blockyLuaError(ls, "IncrementAsync: chiave non valida");
        const double delta = lua_isnumber(ls, 3) ? (double)lua_tonumber(ls, 3) : 1.0;

        auto& s = blk::datastore::db().store(store);
        auto it = s.find(key);
        if (it == s.end() && s.size() >= blk::datastore::kMaxKeysPerStore)
            return blockyLuaError(ls, "IncrementAsync: troppe chiavi nello store '%s'", store.c_str());
        blk::datastore::Value& v = s[key];
        if (!v.isNumber) { v.isNumber = true; v.number = 0.0; v.text.clear(); }
        v.number += delta;
        blk::datastore::db().save(store);
        lua_pushnumber(ls, v.number);
        return 1;
    }

    static int luaDataStoreRemoveAsync(lua_State* ls) {
        const std::string store = dsStoreOf(ls);
        if (store.empty()) return blockyLuaError(ls, "usa store:RemoveAsync(chiave)");
        const std::string key = blk::datastore::sanitizeKeyName(luaL_checkstring(ls, 2));
        auto& s = blk::datastore::db().store(store);
        s.erase(key);
        blk::datastore::db().save(store);
        return 0;
    }

    // UpdateAsync(key, function(old) return new end)
    static int luaDataStoreUpdateAsync(lua_State* ls) {
        const std::string store = dsStoreOf(ls);
        if (store.empty()) return blockyLuaError(ls, "usa store:UpdateAsync(chiave, fn)");
        const std::string key = blk::datastore::sanitizeKeyName(luaL_checkstring(ls, 2));
        if (key.empty()) return blockyLuaError(ls, "UpdateAsync: chiave non valida");
        if (!lua_isfunction(ls, 3))
            return blockyLuaError(ls, "UpdateAsync: serve function(oldValue)");
        auto& s = blk::datastore::db().store(store);
        auto it = s.find(key);
        if (it == s.end()) lua_pushnil(ls);
        else if (it->second.isNumber) lua_pushnumber(ls, it->second.number);
        else lua_pushstring(ls, it->second.text.c_str());
        lua_pushvalue(ls, 3);
        lua_insert(ls, -2);
        auto* rt = fromState(ls);
        if (rt) rt->callStart_ = std::chrono::steady_clock::now();
        if (lua_pcall(ls, 1, 1, 0) != LUA_OK) {
            const char* err = lua_tostring(ls, -1);
            return blockyLuaError(ls, "UpdateAsync: %s", err ? err : "?");
        }
        if (lua_isnil(ls, -1)) {
            s.erase(key);
            blk::datastore::db().save(store);
            return 1;
        }
        blk::datastore::Value v;
        if (lua_isboolean(ls, -1)) {
            v.isNumber = true;
            v.number = lua_toboolean(ls, -1) ? 1.0 : 0.0;
        } else if (lua_isnumber(ls, -1)) {
            v.isNumber = true;
            v.number = (double)lua_tonumber(ls, -1);
        } else if (lua_isstring(ls, -1)) {
            v.isNumber = false;
            v.text = lua_tostring(ls, -1);
            if (v.text.size() > blk::datastore::kMaxStringLen)
                v.text.resize(blk::datastore::kMaxStringLen);
        } else {
            return blockyLuaError(ls, "UpdateAsync: ritorna numero/testo/bool/nil");
        }
        if (s.find(key) == s.end() && s.size() >= blk::datastore::kMaxKeysPerStore)
            return blockyLuaError(ls, "UpdateAsync: troppe chiavi");
        s[key] = v;
        blk::datastore::db().save(store);
        return 1;
    }

    static int luaGetDataStore(lua_State* ls) {
        const int arg = lua_istable(ls, 1) ? 2 : 1;
        const std::string name = blk::datastore::sanitizeKeyName(luaL_checkstring(ls, arg));
        if (name.empty()) return blockyLuaError(ls, "GetDataStore: nome non valido");

        lua_newtable(ls);
        lua_pushstring(ls, name.c_str());
        lua_setfield(ls, -2, "__store");
        blockyPushCFunction(ls, luaDataStoreSetAsync);
        lua_setfield(ls, -2, "SetAsync");
        blockyPushCFunction(ls, luaDataStoreGetAsync);
        lua_setfield(ls, -2, "GetAsync");
        blockyPushCFunction(ls, luaDataStoreIncrementAsync);
        lua_setfield(ls, -2, "IncrementAsync");
        blockyPushCFunction(ls, luaDataStoreRemoveAsync);
        lua_setfield(ls, -2, "RemoveAsync");
        blockyPushCFunction(ls, luaDataStoreUpdateAsync);
        lua_setfield(ls, -2, "UpdateAsync");
        return 1;
    }

    static int luaPhysicsCollisionGroupSetCollidable(lua_State* ls) {
        auto* rt = fromState(ls);
        int a = (int)luaL_checkinteger(ls, 2);
        int b = (int)luaL_checkinteger(ls, 3);
        bool on = lua_toboolean(ls, 4) != 0;
        if (rt && rt->phys.setGroupsCollidable && rt->phys.world)
            rt->phys.setGroupsCollidable(rt->phys.world, a, b, on);
        return 0;
    }
    static int luaPhysicsCollisionGroupsAreCollidable(lua_State* ls) {
        auto* rt = fromState(ls);
        int a = (int)luaL_checkinteger(ls, 2);
        int b = (int)luaL_checkinteger(ls, 3);
        bool on = true;
        if (rt && rt->phys.groupsCollidable && rt->phys.world)
            on = rt->phys.groupsCollidable(rt->phys.world, a, b);
        lua_pushboolean(ls, on ? 1 : 0);
        return 1;
    }
    static int luaPhysicsCreateCollisionGroup(lua_State* ls) {
        // Nome opzionale: ritorna un id 0..31 (semplice registry locale).
        static int nextGroup = 1;
        int id = nextGroup;
        if (nextGroup < 31) ++nextGroup;
        lua_pushinteger(ls, id);
        return 1;
    }

    static int luaGetService(lua_State* ls) {
        const char* name = luaL_checkstring(ls, 2);
        if (lua_isstring(ls, 1) && !lua_istable(ls, 1))
            name = lua_tostring(ls, 1);
        std::string n = name ? name : "";
        for (char& c : n)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        auto fromGame = [&](const char* field) -> int {
            lua_getglobal(ls, "game");
            lua_getfield(ls, -1, field);
            return 1;
        };
        if (n == "runservice") return fromGame("RunService");
        if (n == "soundservice") return fromGame("SoundService");
        if (n == "particles") return fromGame("Particles");
        if (n == "players") return fromGame("Players");
        if (n == "animator") return fromGame("Animator");
        if (n == "workspace") { lua_getglobal(ls, "workspace"); return 1; }
        if (n == "replicatedstorage") return fromGame("ReplicatedStorage");
        if (n == "events") return fromGame("Events");
        if (n == "serverstorage") return fromGame("ServerStorage");
        if (n == "serverscriptservice") return fromGame("ServerScriptService");
        if (n == "lighting" || n == "lightingservice") return fromGame("Lighting");
        if (n == "tweenservice") return fromGame("TweenService");
        if (n == "debris") return fromGame("Debris");
        if (n == "chat" || n == "textchatservice") return fromGame("Chat");
        if (n == "startergui") return fromGame("StarterGui");
        if (n == "userinputservice") return fromGame("UserInputService");
        if (n == "datastoreservice") return fromGame("DataStoreService");
        if (n == "collectionservice") return fromGame("CollectionService");
        if (n == "starterplayer") return fromGame("StarterPlayer");
        if (n == "httpservice") { lua_getglobal(ls, "HttpService"); return 1; }
        if (n == "contextactionservice") { lua_getglobal(ls, "ContextActionService"); return 1; }
        if (n == "teleportservice") { lua_getglobal(ls, "TeleportService"); return 1; }
        // Live gameplay globals (registered by systems / CombatService bridge).
        auto fromGlobal = [&](const char* gname) -> int {
            lua_getglobal(ls, gname);
            if (!lua_isnil(ls, -1)) return 1;
            lua_pop(ls, 1);
            return 0;
        };
        if (n == "proximitypromptservice" || n == "proximityprompt") {
            if (fromGlobal("Prossimita")) return 1;
            if (fromGlobal("ProximityPromptService")) return 1;
        }
        if (n == "teams" || n == "teamservice") {
            if (fromGlobal("Teams")) return 1;
        }
        if (n == "combatservice" || n == "combat") {
            if (fromGlobal("CombatService")) return 1;
        }
        if (n == "inventoryservice" || n == "inventory" || n == "inventario") {
            if (fromGlobal("Inventario")) return 1;
        }
        if (n == "npcservice" || n == "npc" || n == "mobs") {
            if (fromGlobal("Mobs")) return 1;
        }
        if (n == "questservice" || n == "quests" || n == "missioni") {
            if (fromGlobal("Missioni")) return 1;
        }
        if (n == "currencyservice" || n == "currency") {
            if (fromGlobal("CurrencyService")) return 1;
        }
        if (n == "projectileservice" || n == "proiettili") {
            if (fromGlobal("Proiettili")) return 1;
        }
        if (n == "healthservice" || n == "salute") {
            if (fromGlobal("Salute")) return 1;
        }
        if (n == "vehicleservice" || n == "veicoli") {
            if (fromGlobal("Veicoli")) return 1;
        }
        if (n == "craftingservice" || n == "crafting") {
            if (fromGlobal("Crafting")) return 1;
        }
        if (n == "dialogueservice" || n == "dialoghi") {
            if (fromGlobal("Dialoghi")) return 1;
        }
        if (n == "zoneservice" || n == "zone") {
            if (fromGlobal("Zone")) return 1;
        }
        if (n == "staminaservice" || n == "stamina") {
            if (fromGlobal("Stamina")) return 1;
        }
        if (n == "pathfindingservice") {
            lua_newtable(ls);
            blockyPushCFunction(ls, luaPathCreatePath);
            lua_setfield(ls, -2, "CreatePath");
            lua_getglobal(ls, "workspace");
            if (lua_istable(ls, -1)) {
                lua_getfield(ls, -1, "FindPath");
                lua_setfield(ls, -3, "ComputeAsync");
                lua_getfield(ls, -1, "IsBlocked");
                lua_setfield(ls, -3, "IsBlocked");
                lua_getfield(ls, -1, "RebuildNav");
                lua_setfield(ls, -3, "Rebuild");
            }
            lua_pop(ls, 1);
            lua_pushstring(ls, "PathfindingService");
            lua_setfield(ls, -2, "Name");
            return 1;
        }
        // Platform services: always return a table so creators never need C++.
        if (n == "physicsservice") {
            lua_newtable(ls);
            lua_pushstring(ls, "PhysicsService");
            lua_setfield(ls, -2, "Name");
            blockyPushCFunction(ls, luaPhysicsCollisionGroupSetCollidable);
            lua_setfield(ls, -2, "CollisionGroupSetCollidable");
            blockyPushCFunction(ls, luaPhysicsCollisionGroupsAreCollidable);
            lua_setfield(ls, -2, "CollisionGroupsAreCollidable");
            blockyPushCFunction(ls, luaPhysicsCreateCollisionGroup);
            lua_setfield(ls, -2, "CreateCollisionGroup");
            return 1;
        }
        if (n == "marketplaceservice"
            || n == "guiservice"
            || n == "messagingservice"
            || n == "localizationservice"
            || n == "audioservice"
            || n == "analyticsservice"
            || n == "assetservice"
            || n == "replication" || n == "replicationservice"
            || n == "startercharacter"
            || n == "starterpack"
            || n == "terrain") {
            lua_newtable(ls);
            lua_pushstring(ls, name ? name : "");
            lua_setfield(ls, -2, "Name");
            return 1;
        }
        return blockyLuaError(ls, "GetService: servizio sconosciuto '%s'", name ? name : "?");
    }

    static int luaLightingGetClockTime(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_pushnumber(ls, rt && rt->lighting ? rt->lighting->clockTime : 14.0);
        return 1;
    }
    static int luaLightingSetClockTime(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->lighting) {
            rt->lighting->clockTime = (float)luaL_checknumber(ls, lua_istable(ls, 1) ? 2 : 1);
            rt->lighting->clampAll();
        }
        return 0;
    }
    static int luaLightingGetBrightness(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_pushnumber(ls, rt && rt->lighting ? rt->lighting->brightness : 2.0);
        return 1;
    }
    static int luaLightingSetBrightness(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->lighting) {
            rt->lighting->brightness = (float)luaL_checknumber(ls, lua_istable(ls, 1) ? 2 : 1);
            rt->lighting->clampAll();
        }
        return 0;
    }
    static int luaLightingGetAmbient(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_pushnumber(ls, rt && rt->lighting ? rt->lighting->ambient : 0.42);
        return 1;
    }
    static int luaLightingSetAmbient(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->lighting) {
            rt->lighting->ambient = (float)luaL_checknumber(ls, lua_istable(ls, 1) ? 2 : 1);
            rt->lighting->clampAll();
        }
        return 0;
    }
    static int luaLightingGetOutdoorAmbient(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->lighting)
            pushVec3(ls, rt->lighting->outdoorAmbient.x, rt->lighting->outdoorAmbient.y,
                     rt->lighting->outdoorAmbient.z);
        else
            pushVec3(ls, 0.38f, 0.42f, 0.52f);
        return 1;
    }
    static int luaLightingSetOutdoorAmbient(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->lighting) return 0;
        int arg = lua_istable(ls, 1) && !lua_isnumber(ls, 2) ? 2 : 1;
        if (lua_istable(ls, arg)) {
            glm::vec3 c = rt->lighting->outdoorAmbient;
            lua_getfield(ls, arg, "r");
            if (lua_isnumber(ls, -1)) {
                c.r = (float)lua_tonumber(ls, -1);
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "g");
                c.g = (float)luaL_checknumber(ls, -1);
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "b");
                c.b = (float)luaL_checknumber(ls, -1);
                lua_pop(ls, 1);
            } else {
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "x");
                if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "X"); }
                c.x = (float)luaL_optnumber(ls, -1, c.x);
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "y");
                if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "Y"); }
                c.y = (float)luaL_optnumber(ls, -1, c.y);
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "z");
                if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "Z"); }
                c.z = (float)luaL_optnumber(ls, -1, c.z);
                lua_pop(ls, 1);
            }
            rt->lighting->outdoorAmbient = c;
        }
        rt->lighting->clampAll();
        return 0;
    }
    static int luaLightingGetFogColor(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->lighting)
            pushVec3(ls, rt->lighting->fogColor.x, rt->lighting->fogColor.y, rt->lighting->fogColor.z);
        else
            pushVec3(ls, 0.55f, 0.65f, 0.78f);
        return 1;
    }
    static int luaLightingSetFogColor(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->lighting) return 0;
        int arg = lua_istable(ls, 1) && !lua_isnumber(ls, 2) ? 2 : 1;
        if (lua_istable(ls, arg)) {
            glm::vec3 c = rt->lighting->fogColor;
            lua_getfield(ls, arg, "r");
            if (lua_isnumber(ls, -1)) {
                c.r = (float)lua_tonumber(ls, -1);
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "g");
                c.g = (float)luaL_checknumber(ls, -1);
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "b");
                c.b = (float)luaL_checknumber(ls, -1);
                lua_pop(ls, 1);
            } else {
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "x");
                if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "X"); }
                c.x = (float)luaL_optnumber(ls, -1, c.x);
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "y");
                if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "Y"); }
                c.y = (float)luaL_optnumber(ls, -1, c.y);
                lua_pop(ls, 1);
                lua_getfield(ls, arg, "z");
                if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, arg, "Z"); }
                c.z = (float)luaL_optnumber(ls, -1, c.z);
                lua_pop(ls, 1);
            }
            rt->lighting->fogColor = c;
        }
        rt->lighting->clampAll();
        return 0;
    }
    static int luaLightingGetFogStart(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_pushnumber(ls, rt && rt->lighting ? rt->lighting->fogStart : 200.0);
        return 1;
    }
    static int luaLightingSetFogStart(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->lighting) {
            rt->lighting->fogStart = (float)luaL_checknumber(ls, lua_istable(ls, 1) ? 2 : 1);
            rt->lighting->clampAll();
        }
        return 0;
    }
    static int luaLightingGetFogEnd(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_pushnumber(ls, rt && rt->lighting ? rt->lighting->fogEnd : 800.0);
        return 1;
    }
    static int luaLightingSetFogEnd(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->lighting) {
            rt->lighting->fogEnd = (float)luaL_checknumber(ls, lua_istable(ls, 1) ? 2 : 1);
            rt->lighting->clampAll();
        }
        return 0;
    }
    static int luaMathClamp(lua_State* ls) {
        float v = (float)luaL_checknumber(ls, 1);
        float lo = (float)luaL_checknumber(ls, 2);
        float hi = (float)luaL_checknumber(ls, 3);
        if (lo > hi) std::swap(lo, hi);
        lua_pushnumber(ls, std::clamp(v, lo, hi));
        return 1;
    }
    static int luaMathSign(lua_State* ls) {
        float v = (float)luaL_checknumber(ls, 1);
        lua_pushnumber(ls, v > 0.f ? 1.0 : (v < 0.f ? -1.0 : 0.0));
        return 1;
    }
    static int luaMathRound(lua_State* ls) {
        float v = (float)luaL_checknumber(ls, 1);
        lua_pushnumber(ls, std::round(v));
        return 1;
    }
    static int luaRunServiceIsStudio(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_pushboolean(ls, rt && rt->runIsStudio ? 1 : 0);
        return 1;
    }
    static int luaRunServiceIsClient(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_pushboolean(ls, rt && rt->runIsClient ? 1 : 0);
        return 1;
    }
    static int luaRunServiceIsServer(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_pushboolean(ls, !rt || rt->runIsServer ? 1 : 0);
        return 1;
    }
    static int partIndexFromInstance(lua_State* ls, int idx) {
        if (!lua_istable(ls, idx)) return -1;
        lua_getfield(ls, idx, "__partIndex");
        int pi = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        if (pi < 0) {
            lua_getfield(ls, idx, "_partIdx");
            pi = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
            lua_pop(ls, 1);
        }
        return pi;
    }
    static int collectionInstanceArg(lua_State* ls) {
        if (lua_istable(ls, 1) && lua_istable(ls, 2)) return 2;
        return 1;
    }
    static int luaCollectionAddTag(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->tags) return blockyLuaError(ls, "CollectionService: motore non pronto");
        int inst = collectionInstanceArg(ls);
        int pi = partIndexFromInstance(ls, inst);
        if (pi < 0) return blockyLuaError(ls, "CollectionService:AddTag(instance, tag)");
        rt->tags->add(luaL_checkstring(ls, inst + 1), pi);
        return 0;
    }
    static int luaCollectionRemoveTag(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->tags) return 0;
        int inst = collectionInstanceArg(ls);
        int pi = partIndexFromInstance(ls, inst);
        if (pi < 0) return 0;
        rt->tags->remove(luaL_checkstring(ls, inst + 1), pi);
        return 0;
    }
    static int luaCollectionHasTag(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->tags) { lua_pushboolean(ls, 0); return 1; }
        int inst = collectionInstanceArg(ls);
        int pi = partIndexFromInstance(ls, inst);
        if (pi < 0) { lua_pushboolean(ls, 0); return 1; }
        lua_pushboolean(ls, rt->tags->has(luaL_checkstring(ls, inst + 1), pi) ? 1 : 0);
        return 1;
    }
    static int luaCollectionGetTagged(lua_State* ls) {
        auto* rt = fromState(ls);
        const char* tag = luaL_checkstring(ls, lua_istable(ls, 1) ? 2 : 1);
        lua_newtable(ls);
        if (!rt || !rt->tags || !rt->workspace) return 1;
        auto ids = rt->tags->get(tag);
        int n = 1;
        for (int id : ids) {
            if (id < 0 || id >= (int)rt->workspace->parts.size()) continue;
            lua_pushinteger(ls, n++);
            pushLivePart(ls, id);
            lua_settable(ls, -3);
        }
        return 1;
    }
    static int luaCollectionGetTags(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_newtable(ls);
        if (!rt || !rt->tags) return 1;
        int inst = collectionInstanceArg(ls);
        int pi = partIndexFromInstance(ls, inst);
        if (pi < 0) return 1;
        int n = 1;
        for (const std::string& tag : rt->tags->allTagNames()) {
            if (!rt->tags->has(tag, pi)) continue;
            lua_pushinteger(ls, n++);
            lua_pushstring(ls, tag.c_str());
            lua_settable(ls, -3);
        }
        return 1;
    }
    static int luaTweenPlayStub(lua_State*) { return 0; }
    static int luaTweenCancelStub(lua_State*) { return 0; }
    static int luaTweenCreate(lua_State* ls) {
        lua_newtable(ls);
        blockyPushCFunction(ls, luaTweenPlayStub);
        lua_setfield(ls, -2, "Play");
        blockyPushCFunction(ls, luaTweenCancelStub);
        lua_setfield(ls, -2, "Cancel");
        return 1;
    }
    static int luaDebrisAddItem(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return 0;
        int selfOff = lua_istable(ls, 1) ? 1 : 0;
        float life = (float)luaL_optnumber(ls, selfOff + 2, 1.0);
        if (lua_istable(ls, selfOff + 1)) {
            lua_getfield(ls, selfOff + 1, "_partIdx");
            if (lua_isnumber(ls, -1)) {
                int idx = (int)lua_tointeger(ls, -1);
                // Prima cancellava subito, ignorando il tempo e senza
                // aggiornare gli indici: ora come Debris normale.
                if (idx >= 0 && idx < (int)rt->workspace->parts.size())
                    scriptControls().debris.push_back({idx, std::max(0.f, life)});
            }
            lua_pop(ls, 1);
        }
        return 0;
    }


    static int intField(lua_State* ls, int selfIdx, const char* key) {
        lua_getfield(ls, selfIdx, key);
        const int v = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        return v;
    }

    static WorkspaceModel* liveModel(lua_State* ls, int selfIdx) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return nullptr;
        const int i = intField(ls, selfIdx, "__modelIndex");
        if (i < 0 || i >= (int)rt->workspace->models.size()) return nullptr;
        return &rt->workspace->models[(size_t)i];
    }

    static int luaTrackPlay(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        const int id = intField(ls, 1, "__trackId");
        if (AnimTrack* t = m->animator.get(id)) {
            for (AnimTrack& other : m->animator.tracks) other.playing = false;
            t->playing = true;
        }
        return 0;
    }

    static int luaTrackStop(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        m->animator.stop(intField(ls, 1, "__trackId"));
        return 0;
    }

    static int luaTrackIsPlaying(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        const AnimTrack* t = m ? m->animator.get(intField(ls, 1, "__trackId")) : nullptr;
        lua_pushboolean(ls, t && t->playing);
        return 1;
    }

    static int luaTrackAdjustSpeed(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        if (AnimTrack* t = m->animator.get(intField(ls, 1, "__trackId")))
            t->speed = std::max(0.f, (float)luaL_optnumber(ls, 2, 1.0));
        return 0;
    }

    static int luaTrackSetLooped(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        if (AnimTrack* t = m->animator.get(intField(ls, 1, "__trackId")))
            t->looped = lua_toboolean(ls, 2) != 0;
        return 0;
    }

    static void pushAnimTrack(lua_State* ls, int modelIndex, int trackId) {
        lua_newtable(ls);
        lua_pushinteger(ls, modelIndex); lua_setfield(ls, -2, "__modelIndex");
        lua_pushinteger(ls, trackId);    lua_setfield(ls, -2, "__trackId");
        blockyPushCFunction(ls, luaTrackPlay);        lua_setfield(ls, -2, "Play");
        blockyPushCFunction(ls, luaTrackStop);        lua_setfield(ls, -2, "Stop");
        blockyPushCFunction(ls, luaTrackIsPlaying);   lua_setfield(ls, -2, "IsPlaying");
        blockyPushCFunction(ls, luaTrackAdjustSpeed); lua_setfield(ls, -2, "AdjustSpeed");
        blockyPushCFunction(ls, luaTrackSetLooped);   lua_setfield(ls, -2, "SetLooped");
    }

    static int luaAnimatorLoad(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        const std::string wanted = luaL_checkstring(ls, 2);

        std::string clip = wanted;
        auto it = m->clipAlias.find(wanted);
        if (it != m->clipAlias.end() && !it->second.empty()) clip = it->second;

        const int modelIndex = intField(ls, 1, "__modelIndex");
        pushAnimTrack(ls, modelIndex, m->animator.load(clip));
        return 1;
    }

    static int luaModelSetClipAlias(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        m->clipAlias[luaL_checkstring(ls, 2)] = luaL_checkstring(ls, 3);
        return 0;
    }

    static int luaAnimatorStopAll(lua_State* ls) {
        if (WorkspaceModel* m = liveModel(ls, 1)) m->animator.stopAll();
        return 0;
    }

    static void pushAnimator(lua_State* ls, int modelIndex) {
        lua_newtable(ls);
        lua_pushinteger(ls, modelIndex); lua_setfield(ls, -2, "__modelIndex");
        blockyPushCFunction(ls, luaAnimatorLoad);    lua_setfield(ls, -2, "LoadAnimation");
        blockyPushCFunction(ls, luaAnimatorStopAll); lua_setfield(ls, -2, "StopAll");
    }

    static int luaModelGetPosition(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        pushVec3(ls, m->position.x, m->position.y, m->position.z);
        return 1;
    }

    static int luaModelSetPosition(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        glm::vec3 p = m->position;
        if (!readVec3Arg(ls, 2, p))
            return blockyLuaError(ls, "SetPosition(Vector3) oppure SetPosition(x,y,z)");
        m->position = p;
        return 0;
    }

    static int luaModelSetRotation(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        glm::vec3 r = m->rotation;
        if (!readVec3Arg(ls, 2, r))
            return blockyLuaError(ls, "SetRotation(Vector3) oppure SetRotation(x,y,z)");
        m->rotation = r;
        return 0;
    }

    static int luaModelSetYaw(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        m->rotation.y = (float)luaL_checknumber(ls, 2);
        return 0;
    }

    static int luaModelSetCanCollide(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        if (!m) return 0;
        m->canCollide = lua_toboolean(ls, 2) != 0;
        return 0;
    }

    static int luaModelGetCanCollide(lua_State* ls) {
        WorkspaceModel* m = liveModel(ls, 1);
        lua_pushboolean(ls, m && m->canCollide);
        return 1;
    }

    static void pushLiveModel(lua_State* ls, int modelIndex) {
        auto* rt = fromState(ls);
        lua_newtable(ls);
        lua_pushinteger(ls, modelIndex);
        lua_setfield(ls, -2, "__modelIndex");

        blockyPushCFunction(ls, luaModelGetPosition);   lua_setfield(ls, -2, "GetPosition");
        blockyPushCFunction(ls, luaModelSetPosition);   lua_setfield(ls, -2, "SetPosition");
        blockyPushCFunction(ls, luaModelSetRotation);   lua_setfield(ls, -2, "SetRotation");
        blockyPushCFunction(ls, luaModelSetYaw);        lua_setfield(ls, -2, "SetYaw");
        blockyPushCFunction(ls, luaModelSetCanCollide); lua_setfield(ls, -2, "SetCanCollide");
        blockyPushCFunction(ls, luaModelGetCanCollide); lua_setfield(ls, -2, "GetCanCollide");
        blockyPushCFunction(ls, luaModelSetClipAlias);  lua_setfield(ls, -2, "SetClipAlias");
        blockyPushCFunction(ls, luaModelGetPivot);      lua_setfield(ls, -2, "GetPivot");
        blockyPushCFunction(ls, luaModelPivotTo);       lua_setfield(ls, -2, "PivotTo");
        blockyPushCFunction(ls, luaModelIsA);           lua_setfield(ls, -2, "IsA");

        pushAnimator(ls, modelIndex);
        lua_setfield(ls, -2, "Animator");

        if (rt && rt->workspace && modelIndex >= 0
            && modelIndex < (int)rt->workspace->models.size()) {
            lua_pushstring(ls, rt->workspace->models[(size_t)modelIndex].name.c_str());
            lua_setfield(ls, -2, "Name");
        }
        lua_pushstring(ls, "Model");
        lua_setfield(ls, -2, "ClassName");
    }

    static int luaWorkspaceFindPath(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->nav) { lua_pushnil(ls); return 1; }

        const int base = lua_istable(ls, 1) ? 2 : 1;
        glm::vec3 from(0.f), to(0.f);
        if (!readVec3Arg(ls, base, from) || !readVec3Arg(ls, base + 1, to))
            return blockyLuaError(ls, "FindPath(Vector3 da, Vector3 a)");

        if (!rt->nav->ready() && rt->navRebuild) rt->navRebuild();

        const std::vector<glm::vec3> path = rt->nav->findPath(from, to);
        if (path.empty()) { lua_pushnil(ls); return 1; }

        lua_newtable(ls);
        for (size_t i = 0; i < path.size(); ++i) {
            pushVec3(ls, path[i].x, path[i].y, path[i].z);
            lua_rawseti(ls, -2, (int)i + 1);
        }
        return 1;
    }

    static int luaWorkspaceRebuildNav(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->navRebuild) rt->navRebuild();
        return 0;
    }

    static int luaWorkspaceIsBlocked(lua_State* ls) {
        auto* rt = fromState(ls);
        glm::vec3 p(0.f);
        const int base = lua_istable(ls, 1) ? 2 : 1;
        if (!rt || !rt->nav || !readVec3Arg(ls, base, p)) {
            lua_pushboolean(ls, 0);
            return 1;
        }
        if (!rt->nav->ready() && rt->navRebuild) rt->navRebuild();
        lua_pushboolean(ls, rt->nav->worldBlocked(p.x, p.z) ? 1 : 0);
        return 1;
    }

    static int luaWorkspaceFind(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return 0;
        const char* name = luaL_checkstring(ls, lua_istable(ls, 1) ? 2 : 1);
        // Solo figli diretti di Workspace (parentIndex < 0).
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            if (rt->workspace->parts[i].parentIndex < 0 && !rt->workspace->parts[i].destroyPending
                && rt->workspace->parts[i].name == name) {
                pushLivePart(ls, i);
                return 1;
            }
        }
        for (int i = 0; i < (int)rt->workspace->models.size(); ++i) {
            if (rt->workspace->models[i].name == name) {
                pushLiveModel(ls, i);
                return 1;
            }
        }
        lua_pushnil(ls);
        return 1;
    }
    static int luaWorkspaceChildren(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_newtable(ls);
        if (!rt || !rt->workspace) return 1;
        int i = 1;
        for (int pi = 0; pi < (int)rt->workspace->parts.size(); ++pi) {
            if (rt->workspace->parts[pi].parentIndex >= 0 || rt->workspace->parts[pi].destroyPending) continue;
            pushLivePart(ls, pi);
            lua_rawseti(ls, -2, i++);
        }
        for (int mi = 0; mi < (int)rt->workspace->models.size(); ++mi) {
            pushLiveModel(ls, mi);
            lua_rawseti(ls, -2, i++);
        }
        return 1;
    }

    static int luaPartFindFirstChild(lua_State* ls) {
        auto* rt = fromState(ls);
        int parent = partIndexFromSelf(ls, 1);
        const char* name = luaL_checkstring(ls, 2);
        if (!rt || !rt->workspace || parent < 0) { lua_pushnil(ls); return 1; }
        int child = rt->workspace->findPartChild(parent, name);
        if (child >= 0) pushLivePart(ls, child);
        else lua_pushnil(ls);
        return 1;
    }
    static int luaPartGetChildren(lua_State* ls) {
        auto* rt = fromState(ls);
        int parent = partIndexFromSelf(ls, 1);
        lua_newtable(ls);
        if (!rt || !rt->workspace || parent < 0) return 1;
        int n = 1;
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            if (rt->workspace->parts[i].parentIndex == parent) {
                pushLivePart(ls, i);
                lua_rawseti(ls, -2, n++);
            }
        }
        return 1;
    }
    static int beginChildWait(lua_State* ls, int kind, int parentOrSvc, const char* name, float timeout) {
        auto* rt = fromState(ls);
        if (!rt) return 0;
        ChildWait probe;
        probe.kind = kind;
        probe.parentOrSvc = parentOrSvc;
        probe.name = name ? name : "";
        if (rt->tryResolveChildWait(probe, ls))
            return 1;
        if (lua_pushthread(ls)) {
            lua_pop(ls, 1);
            // Non in coroutine: ritorna nil subito (come FindFirstChild).
            lua_pushnil(ls);
            return 1;
        }
        if ((int)rt->childWaits_.size() >= 64)
            return blockyLuaError(ls, "WaitForChild: troppi wait attivi");
        lua_pushthread(ls);
        lua_xmove(ls, rt->L, 1);
        int ref = blockyLuaRef(rt->L);
        ChildWait w;
        w.coRef = ref;
        w.remaining = timeout > 0.f ? timeout : 5.f;
        if (w.remaining > 120.f) w.remaining = 120.f;
        w.kind = kind;
        w.parentOrSvc = parentOrSvc;
        w.name = name ? name : "";
        rt->childWaits_.push_back(std::move(w));
        return lua_yield(ls, 0);
    }
    static int luaPartWaitForChild(lua_State* ls) {
        int parent = partIndexFromSelf(ls, 1);
        const char* name = luaL_checkstring(ls, 2);
        float timeout = (float)luaL_optnumber(ls, 3, 5.0);
        return beginChildWait(ls, 3, parent, name, timeout);
    }

    static int luaWorkspaceAdd(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return 0;

        int idx = partArgIndex(ls);
        if (!idx) return 0;

        std::string name = readStringField(ls, idx, "Name", "ScriptedPart");
        glm::vec3 pos{0, 1, 0};
        glm::vec3 size{4, 1, 2};
        glm::vec3 rot{0, 0, 0};
        glm::vec3 col{0.64f, 0.64f, 0.64f};
        readVec3Field(ls, idx, "Position", pos);
        readVec3Field(ls, idx, "Size", size);
        if (!readVec3Field(ls, idx, "Orientation", rot))
            readVec3Field(ls, idx, "Rotation", rot);
        if (!readColor3Field(ls, idx, "BrickColor", col))
            readColor3Field(ls, idx, "Color", col);

        auto& part = rt->workspace->addPart(name, pos, size, col);
        part.rotation = rot;
        partprops::luaReadTable(ls, idx, part);
        lua_getfield(ls, idx, "Material");
        if (lua_isstring(ls, -1))
            part.setMaterial(partMaterialFromName(lua_tostring(ls, -1)));
        lua_pop(ls, 1);
        lua_getfield(ls, idx, "Emissive");
        if (lua_isnumber(ls, -1)) part.emissive = (float)lua_tonumber(ls, -1);
        lua_pop(ls, 1);
        lua_getfield(ls, idx, "PointLight");
        if (lua_isboolean(ls, -1)) part.pointLight = lua_toboolean(ls, -1) != 0;
        lua_pop(ls, 1);
        lua_getfield(ls, idx, "LightRange");
        if (lua_isnumber(ls, -1)) part.lightRange = (float)lua_tonumber(ls, -1);
        lua_pop(ls, 1);
        lua_getfield(ls, idx, "LightBrightness");
        if (lua_isnumber(ls, -1)) part.lightBrightness = (float)lua_tonumber(ls, -1);
        lua_pop(ls, 1);
        lua_getfield(ls, idx, "CanDestroy");
        if (lua_isboolean(ls, -1)) part.canDestroy = lua_toboolean(ls, -1) != 0;
        lua_pop(ls, 1);
        lua_getfield(ls, idx, "Health");
        if (lua_isnumber(ls, -1)) {
            part.health = (float)lua_tonumber(ls, -1);
            part.maxHealth = std::max(part.maxHealth, part.health);
        }
        lua_pop(ls, 1);
        pushLivePart(ls, (int)rt->workspace->parts.size() - 1);
        return 1;
    }

    static int luaWorkspaceSetPartPos(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return 0;
        int base = 1;
        if (lua_istable(ls, 1) && lua_type(ls, 2) == LUA_TSTRING)
            base = 2;
        const char* name = luaL_checkstring(ls, base);
        glm::vec3 pos{0, 0, 0};
        if (lua_istable(ls, base + 1)) {
            lua_getfield(ls, base + 1, "x");
            if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, base + 1, "X"); }
            pos.x = (float)luaL_optnumber(ls, -1, 0); lua_pop(ls, 1);
            lua_getfield(ls, base + 1, "y");
            if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, base + 1, "Y"); }
            pos.y = (float)luaL_optnumber(ls, -1, 0); lua_pop(ls, 1);
            lua_getfield(ls, base + 1, "z");
            if (lua_isnil(ls, -1)) { lua_pop(ls, 1); lua_getfield(ls, base + 1, "Z"); }
            pos.z = (float)luaL_optnumber(ls, -1, 0); lua_pop(ls, 1);
        } else {
            pos.x = (float)luaL_checknumber(ls, base + 1);
            pos.y = (float)luaL_checknumber(ls, base + 2);
            pos.z = (float)luaL_checknumber(ls, base + 3);
        }
        for (auto& p : rt->workspace->parts) {
            if (p.name == name) {
                p.position = pos;
                break;
            }
        }
        return 0;
    }

    static int luaPartClickConnect(lua_State* ls) {
        int partIdx = -1;
        if (lua_istable(ls, 1)) {
            lua_getfield(ls, 1, "__partIndex");
            partIdx = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
            lua_pop(ls, 1);
        }
        int fidx = fnArgIndex(ls);
        if (!fidx || partIdx < 0) return blockyLuaError(ls, "MouseClick:Connect(fn) su un ClickDetector dentro una Part");
        auto& list = scriptControls().partClicked[partIdx];
        if ((int)list.size() >= LuaScriptSecurity::kMaxTouchedPerPart)
            return blockyLuaError(ls, "MouseClick: troppi listener (max %d)", LuaScriptSecurity::kMaxTouchedPerPart);
        lua_pushvalue(ls, fidx);
        int ref = blockyLuaRef(ls);
        list.push_back(ref);
        lua_pushinteger(ls, ref);
        return 1;
    }

    static int luaPartTouchedConnect(lua_State* ls) {
        int partIdx = -1;
        if (lua_istable(ls, 1)) {
            lua_getfield(ls, 1, "__partIndex");
            partIdx = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
        }
        int fidx = fnArgIndex(ls);
        if (!fidx || partIdx < 0) return blockyLuaError(ls, "Touched:Connect(fn) su Part");
        auto& list = scriptControls().partTouched[partIdx];
        if ((int)list.size() >= LuaScriptSecurity::kMaxTouchedPerPart)
            return blockyLuaError(ls, "Touched: troppi listener (max %d)", LuaScriptSecurity::kMaxTouchedPerPart);
        lua_pushvalue(ls, fidx);
        int ref = blockyLuaRef(ls);
        list.push_back(ref);
        lua_pushinteger(ls, ref);
        return 1;
    }

    static int luaWorkspaceRaycast(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return 0;
        int base = 1;
        if (lua_istable(ls, 1) && lua_istable(ls, 2)) {
            lua_getfield(ls, 1, "Raycast");
            bool isSelf = lua_isfunction(ls, -1) || lua_isnil(ls, -1);
            lua_pop(ls, 1);
            lua_getfield(ls, 1, "x");
            bool hasX = lua_isnumber(ls, -1);
            lua_pop(ls, 1);
            if (!hasX) base = 2;
            (void)isSelf;
        }
        glm::vec3 origin{0}, dir{0, -1, 0};
        if (!readVec3Arg(ls, base, origin))
            return blockyLuaError(ls, "Raycast(origin, direction)");
        if (!readVec3Arg(ls, base + 1, dir))
            return blockyLuaError(ls, "Raycast(origin, direction)");
        float maxDist = (float)luaL_optnumber(ls, base + 2, 500.0);
        float len = glm::length(dir);
        if (len < 1e-6f) { lua_pushnil(ls); return 1; }
        dir /= len;

        float bestT = maxDist;
        int bestI = -1;
        glm::vec3 bestHit{0};
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            const auto& p = rt->workspace->parts[i];
            glm::mat4 inv = glm::inverse(partHitboxMatrix(p));
            glm::vec3 o = glm::vec3(inv * glm::vec4(origin, 1.f));
            glm::vec3 d = glm::vec3(inv * glm::vec4(dir, 0.f));
            float tmin = 0.f, tmax = maxDist;
            bool hit = true;
            for (int a = 0; a < 3; ++a) {
                float oA = o[a], dA = d[a];
                if (std::fabs(dA) < 1e-8f) {
                    if (oA < -0.5f || oA > 0.5f) { hit = false; break; }
                    continue;
                }
                float t1 = (-0.5f - oA) / dA;
                float t2 = (0.5f - oA) / dA;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) { hit = false; break; }
            }
            if (!hit || tmin < 0.f || tmin > bestT) continue;
            bestT = tmin;
            bestI = i;
            bestHit = origin + dir * tmin;
        }
        VoxelHit vh;
        if (rt->voxelRaycast && rt->voxelRaycast(origin, dir, maxDist, vh) && vh.ok
            && vh.dist < bestT) {
            lua_newtable(ls);
            lua_pushnil(ls); lua_setfield(ls, -2, "Instance");
            pushVec3(ls, vh.pos.x, vh.pos.y, vh.pos.z);
            lua_setfield(ls, -2, "Position");
            pushVec3(ls, vh.normal.x, vh.normal.y, vh.normal.z);
            lua_setfield(ls, -2, "Normal");
            lua_pushnumber(ls, vh.dist);
            lua_setfield(ls, -2, "Distance");
            lua_pushstring(ls, vh.material ? vh.material : "Air");
            lua_setfield(ls, -2, "Material");
            return 1;
        }
        if (bestI < 0) { lua_pushnil(ls); return 1; }
        lua_newtable(ls);
        pushLivePart(ls, bestI);
        lua_setfield(ls, -2, "Instance");
        pushVec3(ls, bestHit.x, bestHit.y, bestHit.z);
        lua_setfield(ls, -2, "Position");
        lua_pushnumber(ls, bestT);
        lua_setfield(ls, -2, "Distance");
        return 1;
    }

    static bool partOverlapsAabb(const WorkspacePart& p, const glm::vec3& bmin, const glm::vec3& bmax) {
        glm::vec3 h = partHitboxSize(p) * 0.5f;
        glm::vec3 pmin = p.position - h, pmax = p.position + h;
        return pmin.x <= bmax.x && pmax.x >= bmin.x
            && pmin.y <= bmax.y && pmax.y >= bmin.y
            && pmin.z <= bmax.z && pmax.z >= bmin.z;
    }

    static int luaWorkspaceGetPartBoundsInBox(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) { lua_newtable(ls); return 1; }
        int b = 1;
        if (lua_istable(ls, 1)) {
            lua_getfield(ls, 1, "GetPartBoundsInBox");
            bool self = lua_isfunction(ls, -1) || lua_isnil(ls, -1);
            lua_pop(ls, 1);
            lua_getfield(ls, 1, "x");
            bool hasX = lua_isnumber(ls, -1);
            lua_pop(ls, 1);
            if (self && !hasX) b = 2;
        }
        glm::vec3 center{}, size{4, 4, 4};
        if (!readVec3Arg(ls, b, center)) { lua_newtable(ls); return 1; }
        if (lua_istable(ls, b)) {
            lua_getfield(ls, b, "Position");
            if (lua_istable(ls, -1)) readVec3Arg(ls, lua_gettop(ls), center);
            lua_pop(ls, 1);
        }
        readVec3Arg(ls, b + 1, size);
        glm::vec3 h = size * 0.5f;
        glm::vec3 bmin = center - h, bmax = center + h;
        lua_newtable(ls);
        int n = 1;
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            if (!partOverlapsAabb(rt->workspace->parts[i], bmin, bmax)) continue;
            lua_pushinteger(ls, n++);
            pushLivePart(ls, i);
            lua_settable(ls, -3);
        }
        return 1;
    }
    static int luaWorkspaceGetPartsInPart(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) { lua_newtable(ls); return 1; }
        int idx = -1;
        int arg = lua_istable(ls, 1) ? 2 : 1;
        if (lua_istable(ls, arg)) {
            lua_getfield(ls, arg, "__partIndex");
            if (lua_isnumber(ls, -1)) idx = (int)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
        }
        lua_newtable(ls);
        if (idx < 0 || idx >= (int)rt->workspace->parts.size()) return 1;
        glm::vec3 h = partHitboxSize(rt->workspace->parts[idx]) * 0.5f;
        glm::vec3 bmin = rt->workspace->parts[idx].position - h;
        glm::vec3 bmax = rt->workspace->parts[idx].position + h;
        int n = 1;
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            if (i == idx) continue;
            if (!partOverlapsAabb(rt->workspace->parts[i], bmin, bmax)) continue;
            lua_pushinteger(ls, n++);
            pushLivePart(ls, i);
            lua_settable(ls, -3);
        }
        return 1;
    }
    static int luaWorkspaceFindPartsInRegion3(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) { lua_newtable(ls); return 1; }
        int b = lua_istable(ls, 1) ? 2 : 1;
        glm::vec3 mn{}, mx{};
        if (lua_istable(ls, b)) {
            lua_getfield(ls, b, "Min");
            if (lua_istable(ls, -1)) readVec3Arg(ls, lua_gettop(ls), mn);
            lua_pop(ls, 1);
            lua_getfield(ls, b, "Max");
            if (lua_istable(ls, -1)) readVec3Arg(ls, lua_gettop(ls), mx);
            lua_pop(ls, 1);
        }
        lua_newtable(ls);
        int n = 1;
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            if (!partOverlapsAabb(rt->workspace->parts[i], mn, mx)) continue;
            lua_pushinteger(ls, n++);
            pushLivePart(ls, i);
            lua_settable(ls, -3);
        }
        return 1;
    }

    static int luaRemoteConnect(lua_State* ls) {
        std::string name = "Remote";
        bool isServer = true;
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "_remoteName", "Remote");
            // Il nome ATTUALE del remote (puo' essere stato rinominato dopo la creazione).
            auto* rt = fromState(ls);
            const int si = intField(ls, 1, "__sharedIndex");
            if (rt && rt->workspace && si >= 0 && si < (int)rt->workspace->instances.size())
                name = rt->workspace->instances[(size_t)si].name;
            lua_getfield(ls, 1, "_isServer");
            isServer = lua_toboolean(ls, -1) != 0;
            lua_pop(ls, 1);
        }
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "Connect richiede function");
        lua_pushvalue(ls, fidx);
        int ref = blockyLuaRef(ls);
        if (isServer)
            scriptControls().remoteServer[name].push_back(ref);
        else
            scriptControls().remoteClient[name].push_back(ref);
        lua_pushinteger(ls, ref);
        return 1;
    }

    static int luaRemoteFireServer(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->net) return blockyLuaError(ls, "RemoteEvent: rete offline");
        std::string name = "Remote";
        int arg = 1;
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "Name", "Remote");
            arg = 2;
        }
        const char* payload = luaL_optstring(ls, arg, "");
        rt->net->sendRemoteEventToServer(name.c_str(), payload);
        return 0;
    }

    static int luaRemoteFireAllClients(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->net) return blockyLuaError(ls, "RemoteEvent: rete offline");
        std::string name = "Remote";
        int arg = 1;
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "Name", "Remote");
            arg = 2;
        }
        const char* payload = luaL_optstring(ls, arg, "");
        if (rt->net->role != NetRole::Host)
            return blockyLuaError(ls, "FireAllClients solo su Host");
        rt->net->fireRemoteEventAllClients(name.c_str(), payload);
        return 0;
    }

    // FireClient(remote, player, payload) — player = nome stringa o peer id number
    static int luaRemoteFireClient(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->net) return blockyLuaError(ls, "RemoteEvent: rete offline");
        if (rt->net->role != NetRole::Host)
            return blockyLuaError(ls, "FireClient solo su Host (server)");
        std::string name = "Remote";
        int arg = 1;
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "Name", "Remote");
            arg = 2;
        }
        uint8_t peerId = 0;
        if (lua_isnumber(ls, arg)) {
            peerId = (uint8_t)lua_tointeger(ls, arg);
        } else if (lua_isstring(ls, arg)) {
            peerId = rt->net->findPeerIdByName(lua_tostring(ls, arg));
            if (peerId == 0)
                return blockyLuaError(ls, "FireClient: giocatore '%s' non trovato",
                                     lua_tostring(ls, arg));
        } else if (lua_istable(ls, arg)) {
            // Player-like table: .Name o .UserId
            lua_getfield(ls, arg, "UserId");
            if (lua_isnumber(ls, -1))
                peerId = (uint8_t)lua_tointeger(ls, -1);
            lua_pop(ls, 1);
            if (peerId == 0) {
                lua_getfield(ls, arg, "Name");
                if (lua_isstring(ls, -1))
                    peerId = rt->net->findPeerIdByName(lua_tostring(ls, -1));
                lua_pop(ls, 1);
            }
            if (peerId == 0)
                return blockyLuaError(ls, "FireClient: Player senza Name/UserId validi");
        } else {
            return blockyLuaError(ls, "FireClient(player, payload): player = id|nome|Player");
        }
        const char* payload = luaL_optstring(ls, arg + 1, "");
        if (payload && std::strlen(payload) > (size_t)kRemoteExMaxPayload)
            return blockyLuaError(ls, "FireClient: payload troppo lungo (max %d)",
                                 kRemoteExMaxPayload);
        rt->net->fireRemoteEventToClient(peerId, name.c_str(), payload);
        return 0;
    }

    static int luaRunEventConnect(lua_State* ls, std::vector<int>* refs, const char* label) {
        auto* rt = fromState(ls);
        if (!rt || !refs) return blockyLuaError(ls, "%s:Connect: engine non pronto", label);
        int idx = fnArgIndex(ls);
        if (!idx) return blockyLuaError(ls, "%s:Connect richiede una funzione", label);
        // Gli slot staccati con Disconnect si riusano: connettere/staccare
        // in continuazione (equip/unequip) non deve finire i posti.
        size_t slot = refs->size();
        int active = 0;
        for (size_t i = 0; i < refs->size(); ++i) {
            if ((*refs)[i] == LUA_NOREF) { if (slot == refs->size()) slot = i; }
            else ++active;
        }
        if (active >= LuaScriptSecurity::kMaxHeartbeat)
            return blockyLuaError(ls, "%s: troppi listener (max %d)", label,
                              LuaScriptSecurity::kMaxHeartbeat);
        lua_pushvalue(ls, idx);
        const int ref = blockyLuaRef(ls);
        if (slot < refs->size()) (*refs)[slot] = ref;
        else refs->push_back(ref);
        // Restituisce chi lo stacca davvero: Connection:Disconnect lo chiama (LuaPrelude.h).
        lua_pushlightuserdata(ls, refs);
        lua_pushinteger(ls, ref);
        lua_pushcclosure(ls, luaRunEventDrop, "luaRunEventDrop", 2);
        return 1;
    }

    static int luaRunEventDrop(lua_State* ls) {
        auto* refs = static_cast<std::vector<int>*>(lua_tolightuserdata(ls, lua_upvalueindex(1)));
        const int ref = (int)lua_tointeger(ls, lua_upvalueindex(2));
        auto* rt = fromState(ls);
        if (!refs || !rt) return 0;
        for (int& r : *refs)
            if (r == ref) {
                lua_unref(ls, r);
                r = LUA_NOREF;
                break;
            }
        return 0;
    }
    static int luaHeartbeatConnect(lua_State* ls) {
        auto* rt = fromState(ls);
        return luaRunEventConnect(ls, rt ? &rt->heartbeatRefs_ : nullptr, "Heartbeat");
    }
    static int luaSteppedConnect(lua_State* ls) {
        auto* rt = fromState(ls);
        return luaRunEventConnect(ls, rt ? &rt->steppedRefs_ : nullptr, "Stepped");
    }
    static int luaRenderSteppedConnect(lua_State* ls) {
        auto* rt = fromState(ls);
        return luaRunEventConnect(ls, rt ? &rt->renderSteppedRefs_ : nullptr, "RenderStepped");
    }

    static int luaWorkspaceGetDescendants(lua_State* ls) {
        auto* rt = fromState(ls);
        lua_newtable(ls);
        if (!rt || !rt->workspace) return 1;
        int n = 1;
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            lua_pushinteger(ls, n++);
            pushLivePart(ls, i);
            lua_settable(ls, -3);
        }
        return 1;
    }

    static int luaWorkspaceFindFirstChildOfClass(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace) return 0;
        int nameArg = lua_istable(ls, 1) ? 2 : 1;
        const char* cls = luaL_checkstring(ls, nameArg);
        std::string c = cls ? cls : "";
        for (char& ch : c)
            if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        if (c == "PART") {
            if (!rt->workspace->parts.empty()) {
                pushLivePart(ls, 0);
                return 1;
            }
            return 0;
        }
        if (c == "TERRAIN") {
            lua_getglobal(ls, "workspace");
            lua_getfield(ls, -1, "Terrain");
            lua_remove(ls, -2);
            return lua_istable(ls, -1) ? 1 : (lua_pop(ls, 1), 0);
        }
        return 0;
    }

    static void pushSharedInstance(lua_State* ls, int index) {
        auto* rt = fromState(ls);
        lua_newtable(ls);
        lua_pushinteger(ls, index);
        lua_setfield(ls, -2, "__sharedIndex");
        if (rt && rt->workspace && index >= 0
            && index < (int)rt->workspace->instances.size()) {
            const auto& s = rt->workspace->instances[index];
            // Name NON come campo: con un campo vero `obj.Name = "x"` cambiava solo
            // la tabella e il motore restava col nome vecchio. Lo legge __index.
            lua_pushstring(ls, s.className.c_str());
            lua_setfield(ls, -2, "ClassName");
            lua_pushinteger(ls, (int)s.service);
            lua_setfield(ls, -2, "__serviceId");
            if (s.className == "RemoteEvent") {
                blockyPushCFunction(ls, luaRemoteFireServer);
                lua_setfield(ls, -2, "FireServer");
                blockyPushCFunction(ls, luaRemoteFireAllClients);
                lua_setfield(ls, -2, "FireAllClients");
                blockyPushCFunction(ls, luaRemoteFireClient);
                lua_setfield(ls, -2, "FireClient");
                lua_newtable(ls);
                lua_pushstring(ls, s.name.c_str());
                lua_setfield(ls, -2, "_remoteName");
                lua_pushinteger(ls, index);
                lua_setfield(ls, -2, "__sharedIndex");
                lua_pushboolean(ls, 1);
                lua_setfield(ls, -2, "_isServer");
                pushSignalMethod(ls, luaRemoteConnect, "luaRemoteConnect", 0);
                lua_setfield(ls, -2, "Connect");
                pushSignalMethod(ls, luaRemoteConnect, "luaRemoteConnect", 1);
                lua_setfield(ls, -2, "Once");
                pushSignalMethod(ls, luaRemoteConnect, "luaRemoteConnect", 2);
                lua_setfield(ls, -2, "Wait");
                lua_setfield(ls, -2, "OnServerEvent");
                lua_newtable(ls);
                lua_pushstring(ls, s.name.c_str());
                lua_setfield(ls, -2, "_remoteName");
                lua_pushinteger(ls, index);
                lua_setfield(ls, -2, "__sharedIndex");
                lua_pushboolean(ls, 0);
                lua_setfield(ls, -2, "_isServer");
                pushSignalMethod(ls, luaRemoteConnect, "luaRemoteConnect", 0);
                lua_setfield(ls, -2, "Connect");
                pushSignalMethod(ls, luaRemoteConnect, "luaRemoteConnect", 1);
                lua_setfield(ls, -2, "Once");
                pushSignalMethod(ls, luaRemoteConnect, "luaRemoteConnect", 2);
                lua_setfield(ls, -2, "Wait");
                lua_setfield(ls, -2, "OnClientEvent");
            }
            if (s.className == "RemoteFunction") {
                blockyPushCFunction(ls, luaRemoteInvokeServer);
                lua_setfield(ls, -2, "InvokeServer");
                blockyPushCFunction(ls, luaRemoteInvokeClient);
                lua_setfield(ls, -2, "InvokeClient");
            }
            // Source NON come campo (come Name): `mod.Source = "..."` deve arrivare
            // al motore, e lo legge __index.
            if (s.className == "Folder" || s.className == "ModuleScript"
                || s.className == "RemoteEvent" || s.className == "RemoteFunction"
                || s.className == "Script" || s.className == "LocalScript") {
                blockyPushCFunction(ls, luaSharedFindFirstChild);
                lua_setfield(ls, -2, "FindFirstChild");
                blockyPushCFunction(ls, luaSharedGetChildren);
                lua_setfield(ls, -2, "GetChildren");
                blockyPushCFunction(ls, luaSharedWaitForChild);
                lua_setfield(ls, -2, "WaitForChild");
            }
        }
        lua_newtable(ls);
        blockyPushCFunction(ls, luaSharedIndex);
        lua_setfield(ls, -2, "__index");
        blockyPushCFunction(ls, luaSharedNewIndex);
        lua_setfield(ls, -2, "__newindex");
        lua_setmetatable(ls, -2);
    }

    static void pushServiceContainer(lua_State* ls, SharedService svc, const char* name) {
        lua_newtable(ls);
        lua_pushstring(ls, name);
        lua_setfield(ls, -2, "Name");
        lua_pushstring(ls, name);
        lua_setfield(ls, -2, "ClassName");
        lua_pushinteger(ls, (int)svc);
        lua_setfield(ls, -2, "__serviceId");
        blockyPushCFunction(ls, luaServiceFindFirstChild);
        lua_setfield(ls, -2, "FindFirstChild");
        blockyPushCFunction(ls, luaServiceGetChildren);
        lua_setfield(ls, -2, "GetChildren");
        blockyPushCFunction(ls, luaServiceWaitForChild);
        lua_setfield(ls, -2, "WaitForChild");
    }

    static int sharedIndexFromSelf(lua_State* ls, int selfIdx) {
        if (!lua_istable(ls, selfIdx)) return -1;
        lua_getfield(ls, selfIdx, "__sharedIndex");
        int idx = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : -1;
        lua_pop(ls, 1);
        return idx;
    }

    static SharedService serviceFromSelf(lua_State* ls, int selfIdx) {
        if (!lua_istable(ls, selfIdx)) return SharedService::ReplicatedStorage;
        lua_getfield(ls, selfIdx, "__serviceId");
        int id = lua_isnumber(ls, -1) ? (int)lua_tointeger(ls, -1) : 0;
        lua_pop(ls, 1);
        if (id < 0 || id > 2) id = 0;
        return (SharedService)id;
    }

    static int luaSharedIndex(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = sharedIndexFromSelf(ls, 1);
        const char* key = lua_tostring(ls, 2);
        if (!rt || !rt->workspace || idx < 0 || !key) return 0;
        if (idx >= (int)rt->workspace->instances.size()) return 0;
        auto& s = rt->workspace->instances[idx];
        if (std::strcmp(key, "Name") == 0) { lua_pushstring(ls, s.name.c_str()); return 1; }
        if (std::strcmp(key, "ClassName") == 0) { lua_pushstring(ls, s.className.c_str()); return 1; }
        if (std::strcmp(key, "Source") == 0) { lua_pushstring(ls, s.source.c_str()); return 1; }
        if (std::strcmp(key, "Parent") == 0) {
            if (s.parentIndex >= 0) pushSharedInstance(ls, s.parentIndex);
            else {
                const char* sn = s.service == SharedService::ServerStorage ? "ServerStorage"
                    : s.service == SharedService::ServerScriptService ? "ServerScriptService"
                    : "ReplicatedStorage";
                lua_getglobal(ls, "game");
                lua_getfield(ls, -1, sn);
                lua_remove(ls, -2);
            }
            return 1;
        }
        if (std::strcmp(key, "ChildAdded") == 0) {
            lua_newtable(ls);
            lua_pushinteger(ls, idx);
            lua_setfield(ls, -2, "__sharedIndex");
            pushSignalMethod(ls, luaSharedChildAddedConnect, "luaSharedChildAddedConnect", 0);
            lua_setfield(ls, -2, "Connect");
            pushSignalMethod(ls, luaSharedChildAddedConnect, "luaSharedChildAddedConnect", 1);
            lua_setfield(ls, -2, "Once");
            pushSignalMethod(ls, luaSharedChildAddedConnect, "luaSharedChildAddedConnect", 2);
            lua_setfield(ls, -2, "Wait");
            return 1;
        }
        // Child by name: rs.MyModule style
        int child = rt->workspace->findSharedChild(s.service, idx, key);
        if (child >= 0) { pushSharedInstance(ls, child); return 1; }
        return 0;
    }

    static int luaSharedNewIndex(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = sharedIndexFromSelf(ls, 1);
        const char* key = lua_tostring(ls, 2);
        if (!rt || !rt->workspace || idx < 0 || !key) return 0;
        if (idx >= (int)rt->workspace->instances.size()) return 0;
        auto& s = rt->workspace->instances[idx];
        if (std::strcmp(key, "Name") == 0 && lua_isstring(ls, 3)) {
            const std::string old = s.name;
            s.name = lua_tostring(ls, 3);
            // I remote sono indicizzati per nome: chi era gia' collegato segue il nome nuovo.
            if (old != s.name && (s.className == "RemoteEvent" || s.className == "RemoteFunction")) {
                auto move = [&](auto& m) {
                    auto it = m.find(old);
                    if (it == m.end()) return;
                    auto v = std::move(it->second);
                    m.erase(it);
                    m[s.name] = std::move(v);
                };
                move(scriptControls().remoteServer);
                move(scriptControls().remoteClient);
                move(scriptControls().remoteInvoke);
            }
        }
        else if (std::strcmp(key, "Source") == 0 && lua_isstring(ls, 3))
            s.source = lua_tostring(ls, 3);
        else if (std::strcmp(key, "Parent") == 0) {
            int old = s.parentIndex;
            int neu = -1;
            if (lua_isnil(ls, 3)) {
                s.parentIndex = -1;
            } else if (lua_istable(ls, 3)) {
                lua_getfield(ls, 3, "__sharedIndex");
                if (lua_isnumber(ls, -1)) neu = (int)lua_tointeger(ls, -1);
                lua_pop(ls, 1);
                if (neu == idx) neu = old;
                s.parentIndex = neu;
            }
            if (neu >= 0 && neu != old)
                rt->fireSharedChildAdded(neu, idx);
        }
        else if (std::strcmp(key, "OnServerInvoke") == 0 && lua_isfunction(ls, 3)
                 && s.className == "RemoteFunction") {
            auto it = scriptControls().remoteInvoke.find(s.name);
            if (it != scriptControls().remoteInvoke.end() && it->second != LUA_NOREF)
                lua_unref(ls, it->second);
            lua_pushvalue(ls, 3);
            scriptControls().remoteInvoke[s.name] = blockyLuaRef(ls);
        } else if (std::strcmp(key, "OnClientInvoke") == 0 && lua_isfunction(ls, 3)
                   && s.className == "RemoteFunction") {
            auto it = scriptControls().remoteInvokeClient.find(s.name);
            if (it != scriptControls().remoteInvokeClient.end() && it->second != LUA_NOREF)
                lua_unref(ls, it->second);
            lua_pushvalue(ls, 3);
            scriptControls().remoteInvokeClient[s.name] = blockyLuaRef(ls);
        }
        return 0;
    }

    static int luaRemoteInvokeServer(lua_State* ls) {
        auto* rt = fromState(ls);
        std::string name = "Remote";
        int arg = 1;
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "Name", "Remote");
            arg = 2;
        }
        const char* payload = luaL_optstring(ls, arg, "");
        // Client online: invia RFREQ e yield finche' arriva RFRES
        if (rt && rt->net && rt->net->role == NetRole::Client) {
            if (lua_pushthread(ls)) {
                lua_pop(ls, 1);
                // non in coroutine — fallback sync impossibile, ritorna nil
                lua_pushnil(ls);
                return 1;
            }
            int id = scriptControls().nextInvokeId++;
            char corr[32];
            std::snprintf(corr, sizeof(corr), "%d", id);
            std::string wire = std::string("RFREQ|") + corr + "|" + (payload ? payload : "");
            rt->net->sendRemoteEventToServer(name.c_str(), wire.c_str());
            lua_pushthread(ls);
            lua_xmove(ls, rt->L, 1);
            int ref = blockyLuaRef(rt->L);
            ScriptControls::PendingInvoke p;
            p.coRef = ref;
            p.corrId = corr;
            p.remaining = 10.f;
            scriptControls().pendingInvokes.push_back(std::move(p));
            return lua_yield(ls, 0);
        }
        // Host / Offline: sync OnServerInvoke
        auto it = scriptControls().remoteInvoke.find(name);
        if (it == scriptControls().remoteInvoke.end() || it->second == LUA_NOREF) {
            lua_pushnil(ls);
            return 1;
        }
        lua_rawgeti(ls, LUA_REGISTRYINDEX, it->second);
        if (!lua_isfunction(ls, -1)) { lua_pop(ls, 1); lua_pushnil(ls); return 1; }
        if (rt && rt->net && rt->net->localName[0])
            lua_pushstring(ls, rt->net->localName);
        else
            lua_pushstring(ls, "Player");
        lua_pushstring(ls, payload ? payload : "");
        if (rt) rt->callStart_ = std::chrono::steady_clock::now();
        if (lua_pcall(ls, 2, 1, 0) != LUA_OK) {
            const char* err = lua_tostring(ls, -1);
            if (rt) rt->lastError = err ? err : "InvokeServer error";
            std::fprintf(stderr, "InvokeServer error: %s\n", err ? err : "?");
            lua_pop(ls, 1);
            lua_pushnil(ls);
            return 1;
        }
        return 1;
    }

    static int luaRemoteInvokeClient(lua_State* ls) {
        auto* rt = fromState(ls);
        std::string name = "Remote";
        int arg = 1;
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "Name", "Remote");
            arg = 2;
        }
        // player arg opzionale, poi payload
        const char* payload = "";
        if (lua_isstring(ls, arg) && !lua_isnumber(ls, arg)) {
            // potrebbe essere player name o payload — se c'e' arg+1 e' payload
            if (lua_gettop(ls) >= arg + 1)
                payload = luaL_optstring(ls, arg + 1, "");
            else
                payload = luaL_optstring(ls, arg, "");
        } else {
            payload = luaL_optstring(ls, arg + 1, "");
        }
        // Locale: OnClientInvoke
        auto it = scriptControls().remoteInvokeClient.find(name);
        if (it != scriptControls().remoteInvokeClient.end() && it->second != LUA_NOREF) {
            lua_rawgeti(ls, LUA_REGISTRYINDEX, it->second);
            if (lua_isfunction(ls, -1)) {
                lua_pushstring(ls, payload ? payload : "");
                if (rt) rt->callStart_ = std::chrono::steady_clock::now();
                if (lua_pcall(ls, 1, 1, 0) != LUA_OK) {
                    lua_pop(ls, 1);
                    lua_pushnil(ls);
                }
                return 1;
            }
            lua_pop(ls, 1);
        }
        // Rete: FireClient con RFREQ (client risponde con RFRES se ha OnClientInvoke via drain)
        if (rt && rt->net && rt->net->role == NetRole::Host) {
            uint8_t peer = 0;
            if (lua_isnumber(ls, arg)) peer = (uint8_t)lua_tointeger(ls, arg);
            else if (lua_isstring(ls, arg) && lua_gettop(ls) >= arg + 1)
                peer = rt->net->findPeerIdByName(lua_tostring(ls, arg));
            if (peer) {
                int id = scriptControls().nextInvokeId++;
                char corr[32];
                std::snprintf(corr, sizeof(corr), "c%d", id);
                std::string wire = std::string("RFREQ|") + corr + "|" + (payload ? payload : "");
                rt->net->fireRemoteEventToClient(peer, name.c_str(), wire.c_str());
            }
        }
        lua_pushnil(ls);
        return 1;
    }

    static int luaSharedFindFirstChild(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = sharedIndexFromSelf(ls, 1);
        const char* name = luaL_checkstring(ls, 2);
        if (!rt || !rt->workspace || idx < 0) { lua_pushnil(ls); return 1; }
        const auto& s = rt->workspace->instances[idx];
        int child = rt->workspace->findSharedChild(s.service, idx, name);
        if (child >= 0) pushSharedInstance(ls, child);
        else lua_pushnil(ls);
        return 1;
    }

    static int luaSharedGetChildren(lua_State* ls) {
        auto* rt = fromState(ls);
        int idx = sharedIndexFromSelf(ls, 1);
        lua_newtable(ls);
        if (!rt || !rt->workspace || idx < 0) return 1;
        const auto& s = rt->workspace->instances[idx];
        int n = 1;
        for (int i = 0; i < (int)rt->workspace->instances.size(); ++i) {
            if (rt->workspace->instances[i].service == s.service
                && rt->workspace->instances[i].parentIndex == idx) {
                pushSharedInstance(ls, i);
                lua_rawseti(ls, -2, n++);
            }
        }
        return 1;
    }

    static int luaSharedWaitForChild(lua_State* ls) {
        int idx = sharedIndexFromSelf(ls, 1);
        const char* name = luaL_checkstring(ls, 2);
        float timeout = (float)luaL_optnumber(ls, 3, 5.0);
        return beginChildWait(ls, 2, idx, name, timeout);
    }

    static int luaServiceFindFirstChild(lua_State* ls) {
        auto* rt = fromState(ls);
        SharedService svc = serviceFromSelf(ls, 1);
        const char* name = luaL_checkstring(ls, 2);
        if (!rt || !rt->workspace) { lua_pushnil(ls); return 1; }
        int child = rt->workspace->findSharedChild(svc, -1, name);
        if (child >= 0) pushSharedInstance(ls, child);
        else lua_pushnil(ls);
        return 1;
    }

    static int luaServiceGetChildren(lua_State* ls) {
        auto* rt = fromState(ls);
        SharedService svc = serviceFromSelf(ls, 1);
        lua_newtable(ls);
        if (!rt || !rt->workspace) return 1;
        int n = 1;
        for (int i = 0; i < (int)rt->workspace->instances.size(); ++i) {
            if (rt->workspace->instances[i].service == svc
                && rt->workspace->instances[i].parentIndex < 0) {
                pushSharedInstance(ls, i);
                lua_rawseti(ls, -2, n++);
            }
        }
        return 1;
    }

    static int luaServiceWaitForChild(lua_State* ls) {
        SharedService svc = serviceFromSelf(ls, 1);
        const char* name = luaL_checkstring(ls, 2);
        float timeout = (float)luaL_optnumber(ls, 3, 5.0);
        return beginChildWait(ls, 1, (int)svc, name, timeout);
    }

    static int luaWorkspaceWaitForChild(lua_State* ls) {
        const char* name = luaL_checkstring(ls, lua_istable(ls, 1) ? 2 : 1);
        int toArg = lua_istable(ls, 1) ? 3 : 2;
        float timeout = (float)luaL_optnumber(ls, toArg, 5.0);
        return beginChildWait(ls, 0, -1, name, timeout);
    }

    static int luaRequire(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace || !rt->L) return 0;
        std::string path;
        if (lua_isstring(ls, 1)) {
            path = lua_tostring(ls, 1);
        } else if (lua_istable(ls, 1)) {
            int idx = sharedIndexFromSelf(ls, 1);
            if (idx >= 0 && idx < (int)rt->workspace->instances.size())
                path = rt->workspace->instances[idx].name;
        }
        if (path.empty())
            return blockyLuaError(ls, "require(ModuleScript | \"Name\")");

        // Strip optional prefixes: game.ReplicatedStorage.X / ReplicatedStorage.X
        auto strip = [](std::string& p, const char* pref) {
            size_t n = std::strlen(pref);
            if (p.size() > n && p.compare(0, n, pref) == 0) p = p.substr(n);
        };
        strip(path, "game.ReplicatedStorage.");
        strip(path, "ReplicatedStorage.");
        strip(path, "game.ServerStorage.");
        strip(path, "ServerStorage.");

        int modIdx = -1;
        SharedService svc = SharedService::ReplicatedStorage;
        // Support "Folder/Child" path
        std::string leaf = path;
        int parent = -1;
        size_t slash = path.find('/');
        if (slash == std::string::npos) slash = path.find('.');
        if (slash != std::string::npos) {
            std::string folder = path.substr(0, slash);
            leaf = path.substr(slash + 1);
            parent = rt->workspace->findSharedChild(svc, -1, folder);
            if (parent < 0) {
                svc = SharedService::ServerStorage;
                parent = rt->workspace->findSharedChild(svc, -1, folder);
            }
            if (parent < 0)
                return blockyLuaError(ls, "require: modulo non trovato '%s'", path.c_str());
            modIdx = rt->workspace->findSharedChild(svc, parent, leaf);
        } else {
            modIdx = rt->workspace->findSharedChild(SharedService::ReplicatedStorage, -1, leaf);
            if (modIdx < 0)
                modIdx = rt->workspace->findSharedChild(SharedService::ServerStorage, -1, leaf);
            if (modIdx < 0)
                modIdx = rt->workspace->findSharedChild(SharedService::ServerScriptService, -1, leaf);
        }
        if (modIdx < 0)
            return blockyLuaError(ls, "require: ModuleScript '%s' non trovato in ReplicatedStorage", path.c_str());

        const auto& mod = rt->workspace->instances[modIdx];
        if (mod.className != "ModuleScript")
            return blockyLuaError(ls, "require: '%s' non e' un ModuleScript", path.c_str());

        std::string cacheKey = std::to_string((int)mod.service) + "/" + std::to_string(modIdx);
        auto it = rt->moduleCache_.find(cacheKey);
        if (it != rt->moduleCache_.end()) {
            lua_rawgeti(ls, LUA_REGISTRYINDEX, it->second);
            return 1;
        }
        for (const auto& s : rt->requireStack_)
            if (s == cacheKey)
                return blockyLuaError(ls, "require: ciclo su '%s'", path.c_str());

        rt->requireStack_.push_back(cacheKey);
        std::string chunk = mod.source;
        if (chunk.find("return") == std::string::npos)
            chunk = "local module = {}\n" + chunk + "\nreturn module\n";
        const std::string bc = Luau::compile(chunk);
        std::string cname = "=" + mod.name;
        if (luau_load(rt->L, cname.c_str(), bc.data(), bc.size(), 0) != 0) {
            rt->requireStack_.pop_back();
            rt->lastError = rt->safeToString(-1);
            lua_pop(rt->L, 1);
            return blockyLuaError(ls, "require compile: %s", rt->lastError.c_str());
        }
        rt->callStart_ = std::chrono::steady_clock::now();
        if (lua_pcall(rt->L, 0, 1, 0) != LUA_OK) {
            rt->requireStack_.pop_back();
            rt->lastError = rt->safeToString(-1);
            lua_pop(rt->L, 1);
            return blockyLuaError(ls, "require: %s", rt->lastError.c_str());
        }
        // Cache return value
        lua_pushvalue(rt->L, -1);
        int ref = blockyLuaRef(rt->L); // pops copy
        rt->moduleCache_[cacheKey] = ref;
        rt->requireStack_.pop_back();
        lua_xmove(rt->L, ls, 1);
        return 1;
    }

    static int luaTaskWait(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt) return 0;
        float sec = (float)luaL_optnumber(ls, 1, 0);
        if (sec < 0.f) sec = 0.f;
        if (sec > 60.f) sec = 60.f;
        if (lua_pushthread(ls)) {
            lua_pop(ls, 1);
            return blockyLuaError(ls, "task.wait() va usato dentro task.spawn(function() ... end)");
        }
        if ((int)rt->taskWaits_.size() >= LuaScriptSecurity::kMaxTaskWaits)
            return blockyLuaError(ls, "task.wait: troppi wait attivi");
        lua_pushthread(ls);
        lua_xmove(ls, rt->L, 1);
        int ref = blockyLuaRef(rt->L);
        rt->taskWaits_.push_back({ref, sec, sec, -1});
        return lua_yield(ls, 0);
    }

    // ---- task (come Roblox) ----
    //   task.spawn(f | thread, ...)  parte subito, restituisce il thread
    //   task.defer(f | thread, ...)  parte al prossimo giro dello scheduler
    //   task.delay(sec, f, ...)      parte dopo sec secondi
    //   task.cancel(thread)          non riprende piu' (se stava aspettando)
    //   task.wait(sec)               restituisce il tempo atteso davvero

    // Primo argomento utile: con task:spawn(...) (due punti per errore) il primo e' la tabella task.
    static int taskArgBase(lua_State* ls) {
        return (lua_istable(ls, 1) && lua_gettop(ls) >= 2 && (lua_isfunction(ls, 2) || lua_isthread(ls, 2) || lua_isnumber(ls, 2))) ? 2 : 1;
    }

    /**
     * Prepara il thread da far partire: nuovo (con la funzione) o quello
     * passato. Lascia il thread in cima a rt->L e mette funzione/argomenti
     * sul suo stack. Ritorna il numero di argomenti, -1 se il valore non va.
     */
    static int prepareTaskThread(lua_State* ls, LuaRuntime* rt, int fnIdx, lua_State*& co) {
        const int top = lua_gettop(ls);
        if (lua_isthread(ls, fnIdx)) {
            co = lua_tothread(ls, fnIdx);
            lua_pushvalue(ls, fnIdx);
            if (ls != rt->L) lua_xmove(ls, rt->L, 1);
        } else if (lua_isfunction(ls, fnIdx)) {
            co = lua_newthread(rt->L);
            lua_pushvalue(ls, fnIdx);
            lua_xmove(ls, co, 1);
        } else {
            return -1;
        }
        const int nargs = top - fnIdx;
        for (int i = fnIdx + 1; i <= top; ++i) lua_pushvalue(ls, i);
        if (nargs > 0) lua_xmove(ls, co, nargs);
        return nargs;
    }

    /** Il thread in cima a rt->L diventa il valore restituito a chi ha chiamato. */
    static int returnTaskThread(lua_State* ls, LuaRuntime* rt) {
        if (ls != rt->L) lua_xmove(rt->L, ls, 1);
        return 1;
    }

    static int luaTaskSpawn(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->L) return 0;
        lua_State* co = nullptr;
        const int nargs = prepareTaskThread(ls, rt, taskArgBase(ls), co);
        if (nargs < 0) return blockyLuaError(ls, "task.spawn(funzione o thread, ...)");
        rt->callStart_ = std::chrono::steady_clock::now();
        int st = lua_resume(co, rt->L, nargs);
        int nres = lua_gettop(co);
        if (st != LUA_OK && st != LUA_YIELD) {
            const char* err = lua_tostring(co, -1);
            std::string msg = err ? err : "error";
            lua_pop(rt->L, 1);
            return blockyLuaError(ls, "task.spawn: %s", msg.c_str());
        }
        if (nres > 0) lua_pop(co, nres);
        return returnTaskThread(ls, rt);
    }

    /** Mette in coda un thread nuovo che parte tra `sec` secondi (0 = al prossimo giro). */
    static int scheduleTask(lua_State* ls, float sec, int fnIdx, const char* usage) {
        auto* rt = fromState(ls);
        if (!rt || !rt->L) return 0;
        if ((int)rt->taskWaits_.size() >= LuaScriptSecurity::kMaxTaskWaits)
            return blockyLuaError(ls, "%s: troppi task in attesa", usage);
        lua_State* co = nullptr;
        const int nargs = prepareTaskThread(ls, rt, fnIdx, co);
        if (nargs < 0) return blockyLuaError(ls, "%s", usage);
        lua_pushvalue(rt->L, -1);
        const int ref = blockyLuaRef(rt->L);
        // Un thread passato a defer che sta gia' aspettando altro riparte col
        // suo stack; uno nuovo parte con funzione + argomenti.
        rt->taskWaits_.push_back({ref, sec, sec, lua_status(co) == LUA_YIELD ? 0 : nargs});
        return returnTaskThread(ls, rt);
    }

    static int luaTaskDelay(lua_State* ls) {
        const int base = taskArgBase(ls);
        float sec = (float)luaL_optnumber(ls, base, 0);
        sec = std::clamp(sec, 0.f, 3600.f);
        return scheduleTask(ls, sec, base + 1, "task.delay(secondi, funzione, ...)");
    }

    static int luaTaskDefer(lua_State* ls) {
        return scheduleTask(ls, 0.f, taskArgBase(ls), "task.defer(funzione o thread, ...)");
    }

    static int luaTaskCancel(lua_State* ls) {
        auto* rt = fromState(ls);
        const int base = taskArgBase(ls);
        if (!rt || !rt->L || !lua_isthread(ls, base)) return blockyLuaError(ls, "task.cancel(thread)");
        lua_State* target = lua_tothread(ls, base);
        auto drop = [&](auto& list) {
            for (size_t i = 0; i < list.size();) {
                lua_rawgeti(rt->L, LUA_REGISTRYINDEX, list[i].coRef);
                lua_State* co = lua_tothread(rt->L, -1);
                lua_pop(rt->L, 1);
                if (co == target) {
                    lua_unref(rt->L, list[i].coRef);
                    list.erase(list.begin() + (long)i);
                } else {
                    ++i;
                }
            }
        };
        drop(rt->taskWaits_);
        drop(rt->childWaits_);
        return 0;
    }

#include "LuaRuntimeExpand.inl"
};
