#pragma once
#include "systems/Cooldown.h"
#include "lua.h"
#include "lualib.h"
namespace cooldownlua {
inline Cooldown* self(lua_State* L) {
    return static_cast<Cooldown*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, field);
}
inline int start(lua_State* L) {
    lua_pushboolean(L, self(L)->start(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 1.0)) ? 1 : 0);
    return 1;
}
inline int ready(lua_State* L) {
    lua_pushboolean(L, self(L)->ready(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int left(lua_State* L) {
    lua_pushnumber(L, self(L)->left(luaL_checkstring(L, 1)));
    return 1;
}
inline int fraction(lua_State* L) {
    lua_pushnumber(L, self(L)->fraction(luaL_checkstring(L, 1)));
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, start, "Start", "Cooldown.Start");
    addFn(L, ctx, ready, "Ready", "Cooldown.Ready");
    addFn(L, ctx, left, "Left", "Cooldown.Left");
    addFn(L, ctx, fraction, "Fraction", "Cooldown.Fraction");
    lua_setglobal(L, "Cooldown");
}
} // namespace cooldownlua
