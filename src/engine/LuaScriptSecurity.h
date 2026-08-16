#pragma once

/**
 * Blocky Lua — sicurezza anti-executor (client).
 * - Congela API engine (game/workspace/...)
 * - Blocca overwrite di globali protetti
 * - Limiti connessioni / particelle / stats
 * Nota: client non è inviolabile; multiplayer → authority server.
 */

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstring>
#include <string>

namespace LuaScriptSecurity {

inline constexpr int kMaxHeartbeat = 64;
inline constexpr int kMaxTouchedPerPart = 16;
inline constexpr int kMaxRemoteListeners = 32;
inline constexpr int kMaxParticles = 128;
inline constexpr int kMaxTaskWaits = 128;
inline constexpr float kMaxWalkSpeed = 28.f;
inline constexpr float kMaxJumpPower = 70.f;
inline constexpr int kInstructionBudget = 2000000; // più stretto vs executor busy-loop

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
    return luaL_error(ls, "API Blocky protetta (anti-executor): non modificare '%s'",
                      k ? k : "?");
}

inline int luaGlobalNewIndex(lua_State* ls) {
    // __newindex(_G, key, value) — blocca overwrite engine, permette variabili utente
    const char* k = lua_tostring(ls, 2);
    // "script" si aggiorna a ogni Part script (bindScriptParent usa rawset; qui per sicurezza)
    if (k && std::strcmp(k, "script") == 0) {
        lua_rawset(ls, 1);
        return 0;
    }
    if (isLockedGlobal(k))
        return luaL_error(ls, "Globale protetto: '%s' (anti-executor)", k ? k : "?");
    lua_rawset(ls, 1); // C rawset (Lua rawset è tolto)
    return 0;
}

/** Metatable read-only su tabella in cima allo stack (o a idx). */
inline void protectTable(lua_State* L, int idx) {
    if (idx < 0) idx = lua_gettop(L) + 1 + idx;
    if (!lua_istable(L, idx)) return;
    lua_newtable(L);
    lua_pushcfunction(L, luaProtectedNewIndex);
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

/** Dopo registerApi: congela superficie + _G selettivo. */
inline void lockApiSurface(lua_State* L) {
    if (!L) return;
    const char* names[] = {
        "game", "workspace", "Workspace", "Vector3", "Color3", "Instance",
        "Humanoid", "Events", "UserInputService", "task", nullptr
    };
    for (int i = 0; names[i]; ++i)
        protectGlobal(L, names[i]);

    // game.RunService / Particles / ... già dentro game — proteggi sotto-tabelle
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
            // Heartbeat sotto RunService
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

    lua_pushglobaltable(L);
    lua_newtable(L);
    lua_pushcfunction(L, luaGlobalNewIndex);
    lua_setfield(L, -2, "__newindex");
    lua_pushstring(L, "BlockyGlobalLock");
    lua_setfield(L, -2, "__metatable");
    lua_setmetatable(L, -2);
    lua_pop(L, 1);
}

/** Path trusted: script engine in assets/ (dev / builtin). */
inline bool isTrustedAssetScript(const std::string& path) {
    auto has = [&](const char* s) {
        return path.find(s) != std::string::npos;
    };
    return has("assets/scripts/") || has("assets\\scripts\\")
        || has("/scripts/") || has("\\scripts\\");
}

} // namespace LuaScriptSecurity
