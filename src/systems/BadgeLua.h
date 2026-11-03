#pragma once
#include "systems/Badge.h"
#include "lua.h"
#include "lualib.h"
namespace badgelua {
inline Badge* self(lua_State* L) {
    return static_cast<Badge*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx); lua_pushcclosure(L, fn, dn, 1); lua_setfield(L, -2, f);
}
inline int award(lua_State* L) {
    lua_pushboolean(L, self(L)->award(luaL_checkstring(L, 1)) ? 1 : 0); return 1;
}
inline int has(lua_State* L) {
    lua_pushboolean(L, self(L)->has(luaL_checkstring(L, 1)) ? 1 : 0); return 1;
}
inline int count(lua_State* L) { lua_pushinteger(L, self(L)->count()); return 1; }
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, award, "Award", "Badge.Award");
    addFn(L, ctx, has, "Has", "Badge.Has");
    addFn(L, ctx, count, "Count", "Badge.Count");
    lua_setglobal(L, "Badge");
}
}
