#pragma once


#include "lua.h"
#include "lualib.h"

#include <cstring>
#include <string>

// ---- Adattatori Luau condivisi: qui perche' LuaRuntime.h include questo
// file PRIMA che la classe LuaRuntime esista (quindi i suoi shim privati
// non sono ancora visibili a questo punto). Stesso nome/comportamento
// delle funzioni di Lua 5.4 che il codice gia' chiamava ovunque. ----

// lua_pushcfunction di Luau vuole un terzo argomento (nome di debug, usato
// negli stack trace) rispetto a Lua 5.4 — #fn lo ricava automaticamente
// dal nome della funzione passata.
#define blockyPushCFunction(L, fn) lua_pushcfunction(L, fn, #fn)

// luaL_error di Luau e' void (macro luaL_errorL) invece che int come in
// Lua 5.4 — "return blockyLuaError(...)" smetteva di compilare. Questo
// adattatore torna a restituire int (mai davvero, dato che dentro solleva
// comunque l'errore Lua che salta fuori).
template <typename... Args>
static int blockyLuaError(lua_State* ls, const char* fmt, Args... args) {
    luaL_error(ls, fmt, args...);
    return 0;
}

namespace LuaScriptSecurity {

inline constexpr int kMaxHeartbeat = 64;
inline constexpr int kMaxTouchedPerPart = 16;
inline constexpr int kMaxRemoteListeners = 32;
inline constexpr int kMaxParticles = 128;
inline constexpr int kMaxTaskWaits = 128;
inline constexpr float kMaxWalkSpeed = 28.f;
inline constexpr float kMaxJumpPower = 70.f;
// Luau non ha un hook "ogni N istruzioni" come Lua 5.4 (lua_sethook +
// LUA_MASKCOUNT) — al suo posto un callback "interrupt" che scatta ai
// safepoint (salti di loop, chiamate, GC). Invece di contare istruzioni,
// il limite duro ora e' in tempo reale: se un singolo script gira oltre
// questa soglia senza restituire il controllo, viene fermato — stessa
// funzione di prima (bloccare un "while true do end"), granularita' diversa.
inline constexpr int kScriptHardTimeoutMs = 50;

inline bool isLockedGlobal(const char* k) {
    if (!k) return false;
    static const char* locked[] = {
        "game", "workspace", "Workspace", "Vector3", "Color3", "Instance",
        "Humanoid", "Events", "UserInputService", "task", "wait",
        "print", "AnimateFrame", "script", "_G",
        "dofile", "loadfile", "load", "require", "collectgarbage",
        "rawset", "rawget", "setmetatable", "getmetatable",
        "coroutine", "debug", "io", "os", "package",
        nullptr
    };
    for (int i = 0; locked[i]; ++i)
        if (std::strcmp(k, locked[i]) == 0) return true;
    return false;
}

inline int luaProtectedNewIndex(lua_State* ls) {
    const char* k = lua_tostring(ls, 2);
    return blockyLuaError(ls, "API Blocky protetta (anti-executor): non modificare '%s'",
                      k ? k : "?");
}

inline int luaGlobalNewIndex(lua_State* ls) {
    const char* k = lua_tostring(ls, 2);
    if (k && std::strcmp(k, "script") == 0) {
        lua_rawset(ls, 1);
        return 0;
    }
    if (isLockedGlobal(k))
        return blockyLuaError(ls, "Globale protetto: '%s' (anti-executor)", k ? k : "?");
    lua_rawset(ls, 1);
    return 0;
}

inline void protectTable(lua_State* L, int idx) {
    if (idx < 0) idx = lua_gettop(L) + 1 + idx;
    if (!lua_istable(L, idx)) return;
    lua_newtable(L);
    blockyPushCFunction(L, luaProtectedNewIndex);
    lua_setfield(L, -2, "__newindex");
    lua_pushstring(L, "BlockyProtected");
    lua_setfield(L, -2, "__metatable");
    lua_setmetatable(L, idx);
}

inline void protectGlobal(lua_State* L, const char* name) {
    lua_getglobal(L, name);
    if (lua_istable(L, -1))
        protectTable(L, -1);
    lua_pop(L, 1);
}

inline void lockApiSurface(lua_State* L) {
    if (!L) return;
    const char* names[] = {
        "game", "workspace", "Workspace", "Vector3", "Color3", "Instance",
        "Humanoid", "Events", "UserInputService", "task", nullptr
    };
    for (int i = 0; names[i]; ++i)
        protectGlobal(L, names[i]);

    lua_getglobal(L, "game");
    if (lua_istable(L, -1)) {
        const char* sub[] = {
            "RunService", "SoundService", "Animator", "Particles",
            "Players", "Events", nullptr
        };
        for (int i = 0; sub[i]; ++i) {
            lua_getfield(L, -1, sub[i]);
            if (lua_istable(L, -1))
                protectTable(L, -1);
            lua_pop(L, 1);
            if (std::strcmp(sub[i], "RunService") == 0) {
                lua_getfield(L, -1, "RunService");
                if (lua_istable(L, -1)) {
                    lua_getfield(L, -1, "Heartbeat");
                    if (lua_istable(L, -1))
                        protectTable(L, -1);
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
        }
    }
    lua_pop(L, 1);

    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_newtable(L);
    blockyPushCFunction(L, luaGlobalNewIndex);
    lua_setfield(L, -2, "__newindex");
    lua_pushstring(L, "BlockyGlobalLock");
    lua_setfield(L, -2, "__metatable");
    lua_setmetatable(L, -2);
    lua_pop(L, 1);
}

inline bool isTrustedAssetScript(const std::string& path) {
    auto has = [&](const char* s) {
        return path.find(s) != std::string::npos;
    };
    return has("assets/scripts/") || has("assets\\scripts\\")
        || has("/scripts/") || has("\\scripts\\");
}

}
