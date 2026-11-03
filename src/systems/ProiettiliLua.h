#pragma once
#include "systems/Proiettili.h"
#include "lua.h"
#include "lualib.h"
namespace proiettililua {
inline Proiettili* self(lua_State* L) {
    return static_cast<Proiettili*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int spawn(lua_State* L) {
    lua_pushboolean(L, self(L)->spawn(luaL_optstring(L, 1, ""),
        (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
        (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5),
        (float)luaL_checknumber(L, 6), (float)luaL_checknumber(L, 7),
        (float)luaL_optnumber(L, 8, 2), (float)luaL_optnumber(L, 9, 0.2)) ? 1 : 0);
    return 1;
}
inline int kill(lua_State* L) {
    lua_pushboolean(L, self(L)->kill(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int pos(lua_State* L) {
    float x = 0, y = 0, z = 0;
    if (!self(L)->pos(luaL_checkstring(L, 1), x, y, z)) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushnumber(L, x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, z); lua_setfield(L, -2, "z");
    return 1;
}
inline int hits(lua_State* L) {
    auto names = self(L)->hits((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2),
                               (float)luaL_checknumber(L, 3), (float)luaL_optnumber(L, 4, 1));
    lua_newtable(L);
    int i = 1;
    for (const auto& n : names) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, n.c_str());
        lua_settable(L, -3);
    }
    return 1;
}
inline int count(lua_State* L) {
    lua_pushinteger(L, self(L)->count());
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, spawn, "Spawn", "Proiettili.Spawn");
    addFn(L, ctx, kill, "Kill", "Proiettili.Kill");
    addFn(L, ctx, pos, "Pos", "Proiettili.Pos");
    addFn(L, ctx, hits, "Hits", "Proiettili.Hits");
    addFn(L, ctx, count, "Count", "Proiettili.Count");
    lua_setglobal(L, "Proiettili");
}
}
