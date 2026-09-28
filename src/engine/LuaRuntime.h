#pragma once

#include "lua.h"
#include "lualib.h"
#include <Luau/Compiler.h>

#include "DataModel.h"
#include "DataStore.h"
#include "GameHub.h"
#include "Humanoid.h"
#include "NavGrid.h"
#include "SoundService.h"
#include "Animator.h"
#include "LightingService.h"
#include "render/Particles.h"
#include "SafeBytecode.h"
#include "ScriptControls.h"
#include "AnimLibrary.h"
#include "LanNet.h"
#include "LuaScriptSecurity.h"
#include "platform/CrashHandler.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
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
    std::string lastError;
    bool ok = false;

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
        if (elapsed > LuaScriptSecurity::kScriptHardTimeoutMs)
            blockyLuaError(ls, "Lua budget: troppe istruzioni (script fermato)");
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
        lua_pushnil(L); lua_setglobal(L, "dofile");
        lua_pushnil(L); lua_setglobal(L, "loadfile");
        lua_pushnil(L); lua_setglobal(L, "load");
        lua_pushnil(L); lua_setglobal(L, "require");
        lua_pushnil(L); lua_setglobal(L, "collectgarbage");
        lua_pushnil(L); lua_setglobal(L, "rawset");
        lua_pushnil(L); lua_setglobal(L, "setmetatable");
        lua_pushnil(L); lua_setglobal(L, "getmetatable");
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
        CrashHandler::note("LuaRuntime::init: pre registerApi");
        if (!registerApi()) {
            shutdown();
            return false;
        }
        CrashHandler::note("LuaRuntime::init: post registerApi");
        LuaScriptSecurity::lockApiSurface(L);
        CrashHandler::note("LuaRuntime::init: post lockApiSurface");
        initializing_ = false;
        ok = true;
        lastError.clear();
        budgetExceeded = false;
        return true;
    }

    void shutdown() {
        if (L) {
            for (int ref : heartbeatRefs_) {
                if (ref != LUA_NOREF)
                    lua_unref(L,ref);
            }
            heartbeatRefs_.clear();
            for (auto& w : taskWaits_) {
                if (w.coRef != LUA_NOREF)
                    lua_unref(L,w.coRef);
            }
            taskWaits_.clear();
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
        callStart_ = std::chrono::steady_clock::now();
        if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
            lastError = safeToString(-1);
            std::fprintf(stderr, "Lua error: %s\n", lastError.c_str());
            lua_pop(L, 1);
            return false;
        }
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
        return runString(code);
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
        for (int ref : heartbeatRefs_) {
            if (ref != LUA_NOREF)
                lua_unref(L,ref);
        }
        heartbeatRefs_.clear();
        pendingDestroy_.clear();
        for (auto& w : taskWaits_) {
            if (L && w.coRef != LUA_NOREF)
                lua_unref(L,w.coRef);
        }
        taskWaits_.clear();
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
        unrefMapS(scriptControls().remoteServer);
        unrefMapS(scriptControls().remoteClient);
        unrefMapS(scriptControls().guiButtonActivated);
        scriptControls().lastGuiButton.clear();
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
        if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
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

    void fireHeartbeat(float dt) {
        if (!L) return;
        if (!heartbeatRefs_.empty()) {
        budgetExceeded = false;
        auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < heartbeatRefs_.size(); ++i) {
            int ref = heartbeatRefs_[i];
            if (ref == LUA_NOREF) continue;
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            if (!lua_isfunction(L, -1)) {
                lua_pop(L, 1);
                continue;
            }
            lua_pushnumber(L, dt);
            callStart_ = std::chrono::steady_clock::now();
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "Heartbeat error: %s\n", lastError.c_str());
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
            for (int ref : heartbeatRefs_) {
                if (ref != LUA_NOREF)
                    lua_unref(L,ref);
            }
            heartbeatRefs_.clear();
            std::fprintf(stderr, "[Lua] %s — Heartbeat disconnesso\n", lastError.c_str());
        }
        }
        tickTaskWaits(dt);
        flushPendingDestroy();
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
            if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "Touched error: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

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
            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "%s error: %s\n", eventName, lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }
    void fireItemActivated(const std::string& itemId, const std::string& itemName) {
        fireItemEvent("ItemActivated", itemId, itemName);
    }
    void fireItemEquipped(const std::string& itemId, const std::string& itemName) {
        fireItemEvent("ItemEquipped", itemId, itemName);
    }
    void fireItemUnequipped(const std::string& itemId, const std::string& itemName) {
        fireItemEvent("ItemUnequipped", itemId, itemName);
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
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                lastError = safeToString(-1);
                std::fprintf(stderr, "TextButton Activated error: %s\n", lastError.c_str());
                lua_pop(L, 1);
            }
        }
    }

    void drainRemoteEvents() {
        if (!L || !net) return;
        for (const auto& in : net->incomingRemotes) {
            auto& map = in.toServer ? scriptControls().remoteServer
                                    : scriptControls().remoteClient;
            auto it = map.find(in.name);
            if (it == map.end()) continue;
            for (int ref : it->second) {
                if (ref == LUA_NOREF) continue;
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
                if (!lua_isfunction(L, -1)) { lua_pop(L, 1); continue; }
                lua_pushstring(L, in.fromName);
                lua_pushstring(L, in.payload);
                callStart_ = std::chrono::steady_clock::now();
                if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
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
    std::vector<int> heartbeatRefs_;
    std::vector<int> pendingDestroy_;
    std::vector<TaskWait> taskWaits_;

    void tickTaskWaits(float dt) {
        if (!L || taskWaits_.empty()) return;
        for (size_t i = 0; i < taskWaits_.size();) {
            taskWaits_[i].remaining -= dt;
            if (taskWaits_[i].remaining > 0.f) {
                ++i;
                continue;
            }
            int ref = taskWaits_[i].coRef;
            taskWaits_.erase(taskWaits_.begin() + (int)i);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            lua_State* co = lua_tothread(L, -1);
            lua_pop(L, 1);
            lua_unref(L,ref);
            if (!co) continue;
            callStart_ = std::chrono::steady_clock::now();
            // Luau non ha il 4o parametro d'uscita "nres" di Lua 5.4:
            // lo ricaviamo con lua_gettop dopo la resume (i valori
            // restituiti/yieldati restano comunque sullo stack di co).
            int st = lua_resume(co, L, 0);
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
        workspace->parts.erase(workspace->parts.begin() + idx);
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
        lua_pushboolean(ls, p.canCollide ? 1 : 0);
        lua_setfield(ls, -2, "CanCollide");
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
        lua_rawset(ls, 1);
        return 0;
    }

    static void pushLiveLabel(lua_State* ls, int labelIndex) {
        lua_newtable(ls);
        lua_pushinteger(ls, labelIndex);
        lua_setfield(ls, -2, "__labelIndex");
        blockyPushCFunction(ls, luaLiveLabelConnect);
        lua_setfield(ls, -2, "Connect");

        auto* rt = fromState(ls);
        if (rt && rt->workspace && labelIndex >= 0
            && labelIndex < (int)rt->workspace->labels.size()) {
            const auto& l = rt->workspace->labels[labelIndex];
            lua_pushstring(ls, l.name.c_str());
            lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, l.isButton() ? "TextButton" : "TextLabel");
            lua_setfield(ls, -2, "ClassName");
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
        blockyPushCFunction(ls, luaLivePartSetPosition);
        lua_setfield(ls, -2, "SetPosition");
        blockyPushCFunction(ls, luaLivePartGetPosition);
        lua_setfield(ls, -2, "GetPosition");
        blockyPushCFunction(ls, luaLivePartDestroy);
        lua_setfield(ls, -2, "Destroy");
        blockyPushCFunction(ls, luaLivePartExplode);
        lua_setfield(ls, -2, "Explode");

        auto* rt = fromState(ls);
        if (rt && rt->workspace && partIndex >= 0
            && partIndex < (int)rt->workspace->parts.size()) {
            const auto& p = rt->workspace->parts[partIndex];
            lua_pushstring(ls, p.name.c_str());
            lua_setfield(ls, -2, "Name");
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
        if (std::strcmp(key, "Touched") == 0) {
            lua_newtable(ls);
            lua_pushinteger(ls, partIndexFromSelf(ls, 1));
            lua_setfield(ls, -2, "__partIndex");
            blockyPushCFunction(ls, luaPartTouchedConnect);
            lua_setfield(ls, -2, "Connect");
            return 1;
        }
        return 0;
    }

    static int luaLivePartNewIndex(lua_State* ls) {
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
        if (std::strcmp(key, "CanCollide") == 0) {
            p->canCollide = lua_toboolean(ls, 3) != 0;
            return 0;
        }
        if (std::strcmp(key, "Name") == 0 && lua_isstring(ls, 3)) {
            p->name = lua_tostring(ls, 3);
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

    static int luaLivePartDestroy(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt) return 0;
        int idx = partIndexFromSelf(ls, 1);
        if (idx < 0 || !rt->workspace) return 0;
        if (idx >= (int)rt->workspace->parts.size()) return 0;
        if (!rt->workspace->parts[idx].canDestroy) return 0;
        rt->pendingDestroy_.push_back(idx);
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
        lua_setglobal(L, "Vector3");

        lua_newtable(L);
        blockyPushCFunction(L, luaColor3New);
        lua_setfield(L, -2, "new");
        blockyPushCFunction(L, luaColor3FromRGB);
        lua_setfield(L, -2, "fromRGB");
        lua_setglobal(L, "Color3");

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
        lua_setfield(L, -2, "Material");
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
        lua_setglobal(L, "workspace");
        lua_getglobal(L, "workspace");
        lua_setglobal(L, "Workspace");
        CrashHandler::note("registerApi: post workspace");

        lua_newtable(L);
        blockyPushCFunction(L, luaEventCreate);
        lua_setfield(L, -2, "Create");
        blockyPushCFunction(L, luaEventConnect);
        lua_setfield(L, -2, "Connect");
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
        lua_setglobal(L, "Humanoid");
        CrashHandler::note("registerApi: post Humanoid");

        lua_newtable(L);
        blockyPushCFunction(L, luaBackpackGive);
        lua_setfield(L, -2, "Give");
        blockyPushCFunction(L, luaBackpackEquipped);
        lua_setfield(L, -2, "Equipped");
        lua_setglobal(L, "Backpack");

        lua_newtable(L);
        blockyPushCFunction(L, luaCameraShake);
        lua_setfield(L, -2, "Shake");
        lua_setglobal(L, "Camera");

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
        lua_setglobal(L, "UserInputService");

        lua_newtable(L);
        blockyPushCFunction(L, luaTaskWait);
        lua_setfield(L, -2, "wait");
        blockyPushCFunction(L, luaTaskSpawn);
        lua_setfield(L, -2, "spawn");
        blockyPushCFunction(L, luaTaskDelay);
        lua_setfield(L, -2, "delay");
        lua_setglobal(L, "task");
        blockyPushCFunction(L, luaTaskWait);
        lua_setglobal(L, "wait");
        CrashHandler::note("registerApi: post task");

        lua_newtable(L);
        lua_newtable(L);
        lua_newtable(L);
        blockyPushCFunction(L, luaHeartbeatConnect);
        lua_setfield(L, -2, "Connect");
        lua_setfield(L, -2, "Heartbeat");
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

        lua_getglobal(L, "Events");
        lua_setfield(L, -2, "ReplicatedStorage");

        lua_newtable(L);
        lua_newtable(L);
        lua_newtable(L);
        lua_getglobal(L, "Humanoid");
        lua_setfield(L, -2, "Humanoid");
        lua_setfield(L, -2, "Character");
        lua_setfield(L, -2, "LocalPlayer");
        lua_setfield(L, -2, "Players");

        lua_getglobal(L, "Events");
        lua_setfield(L, -2, "Events");

        lua_getglobal(L, "UserInputService");
        lua_setfield(L, -2, "UserInputService");

        blockyPushCFunction(L, luaGetService);
        lua_setfield(L, -2, "GetService");

        lua_setglobal(L, "game");

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
        return 0;
    }

    static int luaVector3New(lua_State* ls) {
        pushVec3(ls,
            (float)luaL_optnumber(ls, 1, 0),
            (float)luaL_optnumber(ls, 2, 0),
            (float)luaL_optnumber(ls, 3, 0));
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

    static int luaInstanceNew(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->workspace)
            return blockyLuaError(ls, "Instance.new: engine non pronto");

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
            return 1;
        }
        if (std::strcmp(className, "Sound") == 0) {
            lua_newtable(ls);
            lua_pushstring(ls, "Sound");
            lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, "Sound");
            lua_setfield(ls, -2, "ClassName");
            lua_pushstring(ls, "");
            lua_setfield(ls, -2, "SoundId");
            lua_pushboolean(ls, 0);
            lua_setfield(ls, -2, "Looped");
            lua_pushnumber(ls, 1.0);
            lua_setfield(ls, -2, "Volume");
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
            blockyPushCFunction(ls, luaBindableConnect);
            lua_setfield(ls, -2, "Connect");
            lua_setfield(ls, -2, "Event");
            return 1;
        }
        if (std::strcmp(className, "RemoteEvent") == 0) {
            const char* nm = luaL_optstring(ls, 2, "Remote");
            scriptControls().remoteServer[nm];
            scriptControls().remoteClient[nm];
            lua_newtable(ls);
            lua_pushstring(ls, nm);
            lua_setfield(ls, -2, "Name");
            lua_pushstring(ls, "RemoteEvent");
            lua_setfield(ls, -2, "ClassName");
            blockyPushCFunction(ls, luaRemoteFireServer);
            lua_setfield(ls, -2, "FireServer");
            blockyPushCFunction(ls, luaRemoteFireAllClients);
            lua_setfield(ls, -2, "FireAllClients");
            lua_newtable(ls);
            lua_pushstring(ls, nm);
            lua_setfield(ls, -2, "_remoteName");
            lua_pushboolean(ls, 1);
            lua_setfield(ls, -2, "_isServer");
            blockyPushCFunction(ls, luaRemoteConnect);
            lua_setfield(ls, -2, "Connect");
            lua_setfield(ls, -2, "OnServerEvent");
            lua_newtable(ls);
            lua_pushstring(ls, nm);
            lua_setfield(ls, -2, "_remoteName");
            lua_pushboolean(ls, 0);
            lua_setfield(ls, -2, "_isServer");
            blockyPushCFunction(ls, luaRemoteConnect);
            lua_setfield(ls, -2, "Connect");
            lua_setfield(ls, -2, "OnClientEvent");
            return 1;
        }
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
        if (lua_isnumber(ls, arg + 1))
            count = (int)lua_tointeger(ls, arg + 1);
        else if (lua_istable(ls, arg + 1))
            readColor3Arg(ls, arg + 1, col);
        if (lua_istable(ls, arg + 2))
            readColor3Arg(ls, arg + 2, col);
        if (count > LuaScriptSecurity::kMaxParticles)
            count = LuaScriptSecurity::kMaxParticles;
        if (preset == ParticlePreset::Coins && count > 8) count = 8;
        rt->particles->emitPreset(preset, pos, count, col);
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
            if (id.empty()) return blockyLuaError(ls, "Sound.SoundId vuoto");
            if (!rt->soundService->playPath(id, looped))
                return blockyLuaError(ls, "%s", rt->soundService->lastError.c_str());
            return 0;
        }
        const char* path = luaL_checkstring(ls, arg);
        bool looped = lua_toboolean(ls, arg + 1) != 0;
        if (!rt->soundService->playPath(path, looped))
            return blockyLuaError(ls, "%s", rt->soundService->lastError.c_str());
        return 0;
    }

    static int luaSoundStop(lua_State* ls) {
        auto* rt = fromState(ls);
        if (rt && rt->soundService) rt->soundService->stopAll();
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
        return 0;
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
        lua_newtable(ls);
        lua_pushnumber(ls, p.x); lua_setfield(ls, -2, "X");
        lua_pushnumber(ls, p.y); lua_setfield(ls, -2, "Y");
        lua_pushnumber(ls, p.z); lua_setfield(ls, -2, "Z");
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

    static int luaHumGetLook(lua_State* ls) {
        LuaRuntime* rt = fromState(ls);
        float yawDeg = (rt && rt->playerYaw) ? *rt->playerYaw : 0.f;
        float r = yawDeg * 3.14159265f / 180.f;
        float fx = std::sin(r), fz = std::cos(r);
        lua_newtable(ls);
        lua_pushnumber(ls, fx); lua_setfield(ls, -2, "X");
        lua_pushnumber(ls, 0.0); lua_setfield(ls, -2, "Y");
        lua_pushnumber(ls, fz); lua_setfield(ls, -2, "Z");
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
        if (n == "replicatedstorage" || n == "events") return fromGame("ReplicatedStorage");
        if (n == "lighting" || n == "lightingservice") return fromGame("Lighting");
        if (n == "tweenservice") return fromGame("TweenService");
        if (n == "debris") return fromGame("Debris");
        if (n == "chat" || n == "textchatservice") return fromGame("Chat");
        if (n == "startergui") return fromGame("StarterGui");
        if (n == "userinputservice") return fromGame("UserInputService");
        if (n == "datastoreservice") return fromGame("DataStoreService");
        if (n == "marketplaceservice" || n == "teleportservice" || n == "httpservice"
            || n == "contextactionservice" || n == "collectionservice"
            || n == "pathfindingservice" || n == "physicsservice"
            || n == "guiservice" || n == "proximitypromptservice"
            || n == "messagingservice") {
            lua_newtable(ls);
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
        (void)life;
        if (lua_istable(ls, selfOff + 1)) {
            lua_getfield(ls, selfOff + 1, "_partIdx");
            if (lua_isnumber(ls, -1)) {
                int idx = (int)lua_tointeger(ls, -1);
                if (idx >= 0 && idx < (int)rt->workspace->parts.size()) {
                    rt->workspace->parts.erase(rt->workspace->parts.begin() + idx);
                }
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
        for (int i = 0; i < (int)rt->workspace->parts.size(); ++i) {
            if (rt->workspace->parts[i].name == name) {
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
            pushLivePart(ls, pi);
            lua_rawseti(ls, -2, i++);
        }
        for (int mi = 0; mi < (int)rt->workspace->models.size(); ++mi) {
            pushLiveModel(ls, mi);
            lua_rawseti(ls, -2, i++);
        }
        return 1;
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
        lua_getfield(ls, idx, "CanCollide");
        if (lua_isboolean(ls, -1))
            part.canCollide = lua_toboolean(ls, -1) != 0;
        lua_pop(ls, 1);
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
            glm::mat4 inv = glm::inverse(partModelMatrix(p));
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

    static int luaRemoteConnect(lua_State* ls) {
        std::string name = "Remote";
        bool isServer = true;
        if (lua_istable(ls, 1)) {
            name = readStringField(ls, 1, "_remoteName", "Remote");
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

    static int luaHeartbeatConnect(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt) return blockyLuaError(ls, "Heartbeat:Connect: engine non pronto");

        int idx = fnArgIndex(ls);
        if (!idx) return blockyLuaError(ls, "Heartbeat:Connect richiede una funzione");
        if ((int)rt->heartbeatRefs_.size() >= LuaScriptSecurity::kMaxHeartbeat)
            return blockyLuaError(ls, "Heartbeat: troppi listener (max %d, anti-executor)",
                              LuaScriptSecurity::kMaxHeartbeat);

        lua_pushvalue(ls, idx);
        rt->heartbeatRefs_.push_back(blockyLuaRef(ls));
        return 0;
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
        rt->taskWaits_.push_back({ref, sec});
        return lua_yield(ls, 0);
    }

    static int luaTaskSpawn(lua_State* ls) {
        auto* rt = fromState(ls);
        if (!rt || !rt->L) return 0;
        int fidx = fnArgIndex(ls);
        if (!fidx) return blockyLuaError(ls, "task.spawn(function)");
        lua_State* co = lua_newthread(rt->L);
        lua_pushvalue(ls, fidx);
        lua_xmove(ls, co, 1);
        rt->callStart_ = std::chrono::steady_clock::now();
        int st = lua_resume(co, rt->L, 0);
        int nres = lua_gettop(co);
        if (st != LUA_OK && st != LUA_YIELD) {
            const char* err = lua_tostring(co, -1);
            std::string msg = err ? err : "error";
            lua_pop(rt->L, 1);
            return blockyLuaError(ls, "task.spawn: %s", msg.c_str());
        }
        if (nres > 0) lua_pop(co, nres);
        lua_pop(rt->L, 1);
        return 0;
    }

    static int luaTaskDelayWorker(lua_State* ls) {
        float s = (float)lua_tonumber(ls, lua_upvalueindex(1));
        blockyPushCFunction(ls, luaTaskWait);
        lua_pushnumber(ls, s);
        lua_call(ls, 1, 0);
        lua_pushvalue(ls, lua_upvalueindex(2));
        lua_call(ls, 0, 0);
        return 0;
    }

    static int luaTaskDelay(lua_State* ls) {
        int base = 1;
        if (lua_istable(ls, 1) && lua_gettop(ls) >= 3) base = 2;
        float sec = (float)luaL_optnumber(ls, base, 0);
        if (!lua_isfunction(ls, base + 1))
            return blockyLuaError(ls, "task.delay(sec, function)");
        lua_pushnumber(ls, sec);
        lua_pushvalue(ls, base + 1);
        lua_pushcclosure(ls, luaTaskDelayWorker, "luaTaskDelayWorker", 2);
        blockyPushCFunction(ls, luaTaskSpawn);
        lua_insert(ls, -2);
        lua_call(ls, 1, 0);
        return 0;
    }
};
