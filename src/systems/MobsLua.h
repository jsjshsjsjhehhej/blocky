#pragma once
#include "systems/Mobs.h"
#include "lua.h"
#include "lualib.h"
#include <cstdlib>
namespace mobslua {
inline Mobs* self(lua_State* L) {
    return static_cast<Mobs*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int argBase(lua_State* L) {
    // Colon call: self table first. Dot call with string kind: arg 1 is string.
    if (lua_istable(L, 1) && !lua_isstring(L, 1)) return 2;
    return 1;
}
inline int spawn(lua_State* L) {
    int b = argBase(L);
    lua_pushboolean(L, self(L)->spawn(luaL_checkstring(L, b),
        (float)luaL_checknumber(L, b + 1), (float)luaL_checknumber(L, b + 2),
        (float)luaL_checknumber(L, b + 3)) ? 1 : 0);
    return 1;
}
inline int spawnItem(lua_State* L) {
    int b = lua_istable(L, 1) ? 2 : 1;
    lua_pushboolean(L, self(L)->spawnItem(
        (float)luaL_checknumber(L, b), (float)luaL_checknumber(L, b + 1),
        (float)luaL_checknumber(L, b + 2), luaL_checkstring(L, b + 3),
        (int)luaL_optinteger(L, b + 4, 1)) ? 1 : 0);
    return 1;
}
inline int hurt(lua_State* L) {
    int b = lua_istable(L, 1) ? 2 : 1;
    lua_pushboolean(L, self(L)->hurt(luaL_checkstring(L, b), (float)luaL_checknumber(L, b + 1)) ? 1 : 0);
    return 1;
}
inline int pickup(lua_State* L) {
    int b = lua_istable(L, 1) ? 2 : 1;
    std::string s = self(L)->pickupNear(
        (float)luaL_checknumber(L, b), (float)luaL_checknumber(L, b + 1),
        (float)luaL_checknumber(L, b + 2), (float)luaL_optnumber(L, b + 3, 2));
    if (s.empty()) { lua_pushnil(L); return 1; }
    auto colon = s.find(':');
    lua_newtable(L);
    lua_pushstring(L, s.substr(0, colon).c_str()); lua_setfield(L, -2, "id");
    lua_pushinteger(L, colon == std::string::npos ? 1 : std::atoi(s.c_str() + colon + 1));
    lua_setfield(L, -2, "count");
    return 1;
}
inline int spawnAround(lua_State* L) {
    int b = lua_istable(L, 1) ? 2 : 1;
    lua_pushinteger(L, self(L)->spawnAround((int)luaL_optnumber(L, b, 4),
                                            lua_toboolean(L, b + 1) != 0));
    return 1;
}
inline int count(lua_State* L) {
    lua_pushinteger(L, self(L)->count());
    return 1;
}
inline int takeDmg(lua_State* L) {
    lua_pushnumber(L, self(L)->takePlayerDamage());
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, spawn, "Spawn", "Mobs.Spawn");
    addFn(L, ctx, spawnItem, "SpawnItem", "Mobs.SpawnItem");
    addFn(L, ctx, hurt, "Hurt", "Mobs.Hurt");
    addFn(L, ctx, pickup, "Pickup", "Mobs.Pickup");
    addFn(L, ctx, spawnAround, "SpawnAround", "Mobs.SpawnAround");
    addFn(L, ctx, count, "Count", "Mobs.Count");
    addFn(L, ctx, takeDmg, "TakePlayerDamage", "Mobs.TakePlayerDamage");
    lua_setglobal(L, "Mobs");
}
}
