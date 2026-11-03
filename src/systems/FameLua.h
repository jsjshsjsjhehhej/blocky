#pragma once

#include "systems/Fame.h"
#include "lua.h"
#include "lualib.h"

namespace famelua {
inline Fame* self(lua_State* L) {
    return static_cast<Fame*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int eat(lua_State* L) {
    lua_pushboolean(L, self(L)->eat(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int hunger(lua_State* L) {
    lua_pushnumber(L, self(L)->hunger());
    return 1;
}
inline int sat(lua_State* L) {
    lua_pushnumber(L, self(L)->saturation());
    return 1;
}
inline int set(lua_State* L) {
    self(L)->set((float)luaL_checknumber(L, 1));
    return 0;
}
inline int isFood(lua_State* L) {
    lua_pushboolean(L, self(L)->isFood(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int foodValue(lua_State* L) {
    lua_pushinteger(L, self(L)->foodValue(luaL_checkstring(L, 1)));
    return 1;
}
inline int starving(lua_State* L) {
    lua_pushboolean(L, self(L)->starving() ? 1 : 0);
    return 1;
}
inline int takeDmg(lua_State* L) {
    lua_pushnumber(L, self(L)->takeStarveDamage());
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, eat, "Eat", "Fame.Eat");
    addFn(L, ctx, hunger, "Hunger", "Fame.Hunger");
    addFn(L, ctx, sat, "Saturation", "Fame.Saturation");
    addFn(L, ctx, set, "Set", "Fame.Set");
    addFn(L, ctx, isFood, "IsFood", "Fame.IsFood");
    addFn(L, ctx, foodValue, "FoodValue", "Fame.FoodValue");
    addFn(L, ctx, starving, "Starving", "Fame.Starving");
    addFn(L, ctx, takeDmg, "TakeStarveDamage", "Fame.TakeStarveDamage");
    lua_setglobal(L, "Fame");
    lua_getglobal(L, "Fame");
    lua_setglobal(L, "Hunger");
}
}
