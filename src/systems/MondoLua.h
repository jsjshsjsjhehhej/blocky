#pragma once

#include "systems/Mondo.h"
#include "lua.h"
#include "lualib.h"

namespace mondolua {
inline Mondo* self(lua_State* L) {
    return static_cast<Mondo*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int setSpawn(lua_State* L) {
    int b = lua_istable(L, 1) ? 2 : 1;
    self(L)->setSpawn((float)luaL_checknumber(L, b),
        (float)luaL_checknumber(L, b + 1), (float)luaL_checknumber(L, b + 2));
    return 0;
}
inline int spawn(lua_State* L) {
    lua_newtable(L);
    lua_pushnumber(L, self(L)->spawnX()); lua_setfield(L, -2, "x");
    lua_pushnumber(L, self(L)->spawnY()); lua_setfield(L, -2, "y");
    lua_pushnumber(L, self(L)->spawnZ()); lua_setfield(L, -2, "z");
    lua_pushnumber(L, self(L)->spawnX()); lua_setfield(L, -2, "X");
    lua_pushnumber(L, self(L)->spawnY()); lua_setfield(L, -2, "Y");
    lua_pushnumber(L, self(L)->spawnZ()); lua_setfield(L, -2, "Z");
    return 1;
}
inline int sleep(lua_State* L) {
    lua_pushboolean(L, self(L)->sleep() ? 1 : 0);
    return 1;
}
inline int day(lua_State* L) {
    lua_pushinteger(L, self(L)->day());
    return 1;
}
inline int setName(lua_State* L) {
    lua_pushboolean(L, self(L)->setName(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int name(lua_State* L) {
    lua_pushstring(L, self(L)->name().c_str());
    return 1;
}
inline int seed(lua_State* L) {
    lua_pushinteger(L, self(L)->seed());
    return 1;
}
inline int playTime(lua_State* L) {
    lua_pushnumber(L, self(L)->playTime());
    return 1;
}
inline int spawnFrom(lua_State* L) {
    self(L)->spawnFromTerrain();
    return 0;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, setSpawn, "SetSpawn", "Mondo.SetSpawn");
    addFn(L, ctx, spawn, "Spawn", "Mondo.Spawn");
    addFn(L, ctx, sleep, "Sleep", "Mondo.Sleep");
    addFn(L, ctx, day, "Day", "Mondo.Day");
    addFn(L, ctx, setName, "SetName", "Mondo.SetName");
    addFn(L, ctx, name, "Name", "Mondo.Name");
    addFn(L, ctx, seed, "Seed", "Mondo.Seed");
    addFn(L, ctx, playTime, "PlayTime", "Mondo.PlayTime");
    addFn(L, ctx, spawnFrom, "SpawnFromTerrain", "Mondo.SpawnFromTerrain");
    lua_setglobal(L, "Mondo");
    lua_getglobal(L, "Mondo");
    lua_setglobal(L, "World");
}
}
