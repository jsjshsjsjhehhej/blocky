#pragma once
#include "systems/Stamina.h"
#include "lua.h"
#include "lualib.h"
namespace staminalua {
inline Stamina* self(lua_State* L) {
    return static_cast<Stamina*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int add(lua_State* L) {
    lua_pushboolean(L, self(L)->add(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 100)) ? 1 : 0);
    return 1;
}
inline int use(lua_State* L) {
    lua_pushboolean(L, self(L)->use(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 10)) ? 1 : 0);
    return 1;
}
inline int value(lua_State* L) {
    lua_pushnumber(L, self(L)->value(luaL_checkstring(L, 1)));
    return 1;
}
inline int fraction(lua_State* L) {
    lua_pushnumber(L, self(L)->fraction(luaL_checkstring(L, 1)));
    return 1;
}
inline int ready(lua_State* L) {
    lua_pushboolean(L, self(L)->ready(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 1)) ? 1 : 0);
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, add, "Add", "Stamina.Add");
    addFn(L, ctx, use, "Use", "Stamina.Use");
    addFn(L, ctx, value, "Value", "Stamina.Value");
    addFn(L, ctx, fraction, "Fraction", "Stamina.Fraction");
    addFn(L, ctx, ready, "Ready", "Stamina.Ready");
    lua_setglobal(L, "Stamina");
}
}
