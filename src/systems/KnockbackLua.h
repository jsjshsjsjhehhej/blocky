#pragma once
#include "systems/Knockback.h"
#include "lua.h"
#include "lualib.h"
namespace knockbacklua {
inline Knockback* self(lua_State* L) {
    return static_cast<Knockback*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int add(lua_State* L) {
    lua_pushboolean(L, self(L)->add(luaL_checkstring(L, 1),
        (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
        (float)luaL_checknumber(L, 4)) ? 1 : 0);
    return 1;
}
inline int fromHit(lua_State* L) {
    lua_pushboolean(L, self(L)->fromHit(luaL_checkstring(L, 1),
        (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
        (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5),
        (float)luaL_checknumber(L, 6), (float)luaL_checknumber(L, 7),
        (float)luaL_optnumber(L, 8, 12)) ? 1 : 0);
    return 1;
}
inline int vel(lua_State* L) {
    const char* n = luaL_checkstring(L, 1);
    lua_newtable(L);
    lua_pushnumber(L, self(L)->vx(n)); lua_setfield(L, -2, "x");
    lua_pushnumber(L, self(L)->vy(n)); lua_setfield(L, -2, "y");
    lua_pushnumber(L, self(L)->vz(n)); lua_setfield(L, -2, "z");
    return 1;
}
inline int active(lua_State* L) {
    lua_pushboolean(L, self(L)->active(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, add, "Add", "Knockback.Add");
    addFn(L, ctx, fromHit, "FromHit", "Knockback.FromHit");
    addFn(L, ctx, vel, "Vel", "Knockback.Vel");
    addFn(L, ctx, active, "Active", "Knockback.Active");
    lua_setglobal(L, "Knockback");
}
}
