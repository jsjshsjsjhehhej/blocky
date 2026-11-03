#pragma once

#include "systems/GameKit.h"
#include "lua.h"
#include "lualib.h"

#include <cstring>

namespace gamekitlua {

inline gamekit::GameKit* self(lua_State* L) {
    return static_cast<gamekit::GameKit*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, field);
}

inline int reset(lua_State* L) {
    self(L)->reset();
    return 0;
}

inline int addPad(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* kind = luaL_optstring(L, 2, "Checkpoint");
    float x = (float)luaL_checknumber(L, 3);
    float y = (float)luaL_checknumber(L, 4);
    float z = (float)luaL_checknumber(L, 5);
    float hx = (float)luaL_optnumber(L, 6, 2);
    float hy = (float)luaL_optnumber(L, 7, 1);
    float hz = (float)luaL_optnumber(L, 8, 2);
    float strength = (float)luaL_optnumber(L, 9, 1);
    int order = (int)luaL_optinteger(L, 10, 0);
    lua_pushboolean(L, self(L)->addPad(name, gamekit::GameKit::padKindFromName(kind),
                                       x, y, z, hx, hy, hz, strength, order) ? 1 : 0);
    return 1;
}

inline int removePad(lua_State* L) {
    lua_pushboolean(L, self(L)->removePad(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}

inline int setTeleportDest(lua_State* L) {
    lua_pushboolean(L, self(L)->setTeleportDest(
        luaL_checkstring(L, 1),
        (float)luaL_checknumber(L, 2),
        (float)luaL_checknumber(L, 3),
        (float)luaL_checknumber(L, 4)) ? 1 : 0);
    return 1;
}

inline int addDoor(lua_State* L) {
    lua_pushboolean(L, self(L)->addDoor(
        luaL_checkstring(L, 1),
        (float)luaL_checknumber(L, 2),
        (float)luaL_checknumber(L, 3),
        (float)luaL_checknumber(L, 4),
        lua_toboolean(L, 5) != 0,
        luaL_optstring(L, 6, "")) ? 1 : 0);
    return 1;
}

inline int openDoor(lua_State* L) {
    lua_pushboolean(L, self(L)->openDoor(luaL_checkstring(L, 1), lua_toboolean(L, 2) != 0) ? 1 : 0);
    return 1;
}

inline int closeDoor(lua_State* L) {
    lua_pushboolean(L, self(L)->closeDoor(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}

inline int addWave(lua_State* L) {
    lua_pushboolean(L, self(L)->addWave(
        luaL_optstring(L, 1, "Wave"),
        (int)luaL_optinteger(L, 2, 5),
        (float)luaL_optnumber(L, 3, 2),
        (float)luaL_checknumber(L, 4),
        (float)luaL_checknumber(L, 5),
        (float)luaL_checknumber(L, 6),
        (float)luaL_optnumber(L, 7, 50)) ? 1 : 0);
    return 1;
}

inline int startMatch(lua_State* L) {
    self(L)->startMatch((float)luaL_optnumber(L, 1, 120), (float)luaL_optnumber(L, 2, 3));
    return 0;
}

inline int endMatch(lua_State* L) {
    self(L)->endMatch(lua_toboolean(L, 1) != 0);
    return 0;
}

inline int giveKey(lua_State* L) {
    self(L)->giveKey(luaL_optstring(L, 1, "key"));
    return 0;
}

inline int announce(lua_State* L) {
    self(L)->pushAnnounce(luaL_checkstring(L, 1),
                          (float)luaL_optnumber(L, 2, 2.5),
                          (int)luaL_optinteger(L, 3, 0));
    return 0;
}

inline int buildObby(lua_State* L) {
    self(L)->buildObbyKit((float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0));
    return 0;
}

inline int buildArena(lua_State* L) {
    self(L)->buildArenaKit((float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0));
    return 0;
}

inline int buildTycoon(lua_State* L) {
    self(L)->buildTycoonPads((float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0));
    return 0;
}

inline int getCoins(lua_State* L) {
    lua_pushinteger(L, self(L)->stats.coins);
    return 1;
}

inline int setCoins(lua_State* L) {
    self(L)->stats.coins = (int)luaL_checkinteger(L, 1);
    return 0;
}

inline int getPhase(lua_State* L) {
    switch (self(L)->phase) {
    case gamekit::MatchPhase::Countdown: lua_pushstring(L, "Countdown"); break;
    case gamekit::MatchPhase::Playing: lua_pushstring(L, "Playing"); break;
    case gamekit::MatchPhase::Ended: lua_pushstring(L, "Ended"); break;
    default: lua_pushstring(L, "Idle"); break;
    }
    return 1;
}

inline int getTimeLeft(lua_State* L) {
    lua_pushnumber(L, self(L)->matchTimeLeft);
    return 1;
}

inline int getTriggered(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& n : self(L)->triggeredPads) {
        lua_pushstring(L, n.c_str());
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

inline int enableWaves(lua_State* L) {
    self(L)->wavesEnabled = lua_toboolean(L, 1) != 0;
    return 0;
}

inline int padCount(lua_State* L) {
    lua_pushinteger(L, self(L)->padCount());
    return 1;
}

inline int statsTable(lua_State* L) {
    auto& s = self(L)->stats;
    lua_newtable(L);
    lua_pushinteger(L, s.coins); lua_setfield(L, -2, "Coins");
    lua_pushinteger(L, s.kills); lua_setfield(L, -2, "Kills");
    lua_pushinteger(L, s.deaths); lua_setfield(L, -2, "Deaths");
    lua_pushinteger(L, s.checkpoints); lua_setfield(L, -2, "Checkpoints");
    lua_pushnumber(L, s.surviveTime); lua_setfield(L, -2, "SurviveTime");
    lua_pushnumber(L, s.bestTime); lua_setfield(L, -2, "BestTime");
    lua_pushinteger(L, s.wins); lua_setfield(L, -2, "Wins");
    return 1;
}

inline int setWinCheckpoint(lua_State* L) {
    self(L)->winCheckpointOrder = (int)luaL_checkinteger(L, 1);
    return 0;
}

inline int setKillY(lua_State* L) {
    self(L)->killY = (float)luaL_checknumber(L, 1);
    return 0;
}

inline int getCheckpoint(lua_State* L) {
    auto* k = self(L);
    if (!k->hasCheckpoint) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushnumber(L, k->cpX); lua_setfield(L, -2, "X");
    lua_pushnumber(L, k->cpY); lua_setfield(L, -2, "Y");
    lua_pushnumber(L, k->cpZ); lua_setfield(L, -2, "Z");
    lua_pushinteger(L, k->lastCheckpoint); lua_setfield(L, -2, "Order");
    return 1;
}

inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, reset, "Reset", "gk_reset");
    addFn(L, ctx, addPad, "AddPad", "gk_addPad");
    addFn(L, ctx, removePad, "RemovePad", "gk_removePad");
    addFn(L, ctx, setTeleportDest, "SetTeleportDest", "gk_setTp");
    addFn(L, ctx, addDoor, "AddDoor", "gk_addDoor");
    addFn(L, ctx, openDoor, "OpenDoor", "gk_openDoor");
    addFn(L, ctx, closeDoor, "CloseDoor", "gk_closeDoor");
    addFn(L, ctx, addWave, "AddWave", "gk_addWave");
    addFn(L, ctx, startMatch, "StartMatch", "gk_start");
    addFn(L, ctx, endMatch, "EndMatch", "gk_end");
    addFn(L, ctx, giveKey, "GiveKey", "gk_key");
    addFn(L, ctx, announce, "Announce", "gk_ann");
    addFn(L, ctx, buildObby, "BuildObbyKit", "gk_obby");
    addFn(L, ctx, buildArena, "BuildArenaKit", "gk_arena");
    addFn(L, ctx, buildTycoon, "BuildTycoonPads", "gk_tycoon");
    addFn(L, ctx, getCoins, "GetCoins", "gk_coins");
    addFn(L, ctx, setCoins, "SetCoins", "gk_setCoins");
    addFn(L, ctx, getPhase, "GetPhase", "gk_phase");
    addFn(L, ctx, getTimeLeft, "GetTimeLeft", "gk_time");
    addFn(L, ctx, getTriggered, "GetTriggeredPads", "gk_trig");
    addFn(L, ctx, enableWaves, "EnableWaves", "gk_waves");
    addFn(L, ctx, padCount, "PadCount", "gk_padCount");
    addFn(L, ctx, statsTable, "GetStats", "gk_stats");
    addFn(L, ctx, setWinCheckpoint, "SetWinCheckpoint", "gk_winCp");
    addFn(L, ctx, setKillY, "SetKillY", "gk_killY");
    addFn(L, ctx, getCheckpoint, "GetCheckpoint", "gk_getCp");
    lua_setglobal(L, "GameKit");
    lua_getglobal(L, "game");
    if (lua_istable(L, -1)) {
        lua_getglobal(L, "GameKit");
        lua_setfield(L, -2, "GameKit");
    }
    lua_pop(L, 1);
}

} // namespace gamekitlua
