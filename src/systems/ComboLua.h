#pragma once
#include "systems/Combo.h"
#include "lua.h"
#include "lualib.h"
namespace combolua {
inline Combo* self(lua_State* L) {
    return static_cast<Combo*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int hit(lua_State* L) {
    lua_pushinteger(L, self(L)->hit());
    return 1;
}
inline int count(lua_State* L) {
    lua_pushinteger(L, self(L)->count());
    return 1;
}
inline int best(lua_State* L) {
    lua_pushinteger(L, self(L)->best());
    return 1;
}
inline int left(lua_State* L) {
    lua_pushnumber(L, self(L)->left());
    return 1;
}
inline int setTimeout(lua_State* L) {
    self(L)->setTimeout((float)luaL_optnumber(L, 1, 1.4));
    return 0;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, hit, "Hit", "Combo.Hit");
    addFn(L, ctx, count, "Count", "Combo.Count");
    addFn(L, ctx, best, "Best", "Combo.Best");
    addFn(L, ctx, left, "Left", "Combo.Left");
    addFn(L, ctx, setTimeout, "SetTimeout", "Combo.SetTimeout");
    lua_setglobal(L, "Combo");
}
}
