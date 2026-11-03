#pragma once
#include "systems/Salute.h"
#include "lua.h"
#include "lualib.h"
namespace salutelua {
inline Salute* self(lua_State* L) {
    return static_cast<Salute*>(lua_touserdata(L, lua_upvalueindex(1)));
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
inline int damage(lua_State* L) {
    lua_pushboolean(L, self(L)->damage(luaL_checkstring(L, 1), (float)luaL_checknumber(L, 2)) ? 1 : 0);
    return 1;
}
inline int heal(lua_State* L) {
    lua_pushboolean(L, self(L)->heal(luaL_checkstring(L, 1), (float)luaL_checknumber(L, 2)) ? 1 : 0);
    return 1;
}
inline int revive(lua_State* L) {
    lua_pushboolean(L, self(L)->revive(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 0)) ? 1 : 0);
    return 1;
}
inline int hp(lua_State* L) {
    lua_pushnumber(L, self(L)->hp(luaL_checkstring(L, 1)));
    return 1;
}
inline int dead(lua_State* L) {
    lua_pushboolean(L, self(L)->dead(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int fraction(lua_State* L) {
    lua_pushnumber(L, self(L)->fraction(luaL_checkstring(L, 1)));
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, add, "Add", "Salute.Add");
    addFn(L, ctx, damage, "Damage", "Salute.Damage");
    addFn(L, ctx, heal, "Heal", "Salute.Heal");
    addFn(L, ctx, revive, "Revive", "Salute.Revive");
    addFn(L, ctx, hp, "Hp", "Salute.Hp");
    addFn(L, ctx, dead, "Dead", "Salute.Dead");
    addFn(L, ctx, fraction, "Fraction", "Salute.Fraction");
    lua_setglobal(L, "Salute");
}
}
