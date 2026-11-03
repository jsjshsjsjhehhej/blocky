#pragma once
#include "systems/Forno.h"
#include "lua.h"
#include "lualib.h"
namespace fornolua {
inline Forno* self(lua_State* L) {
    return static_cast<Forno*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int add(lua_State* L) {
    lua_pushboolean(L, self(L)->add(luaL_checkstring(L, lua_istable(L, 1) ? 2 : 1)) ? 1 : 0);
    return 1;
}
inline int put(lua_State* L) {
    int b = lua_istable(L, 1) ? 2 : 1;
    lua_pushboolean(L, self(L)->put(luaL_checkstring(L, b), luaL_checkstring(L, b + 1),
                                    lua_toboolean(L, b + 2) != 0) ? 1 : 0);
    return 1;
}
inline int take(lua_State* L) {
    int b = lua_istable(L, 1) ? 2 : 1;
    std::string o = self(L)->take(luaL_checkstring(L, b));
    if (o.empty()) lua_pushnil(L);
    else lua_pushstring(L, o.c_str());
    return 1;
}
inline int addRecipe(lua_State* L) {
    int b = lua_istable(L, 1) ? 2 : 1;
    lua_pushboolean(L, self(L)->addRecipe(luaL_checkstring(L, b), luaL_checkstring(L, b + 1),
                                          (int)luaL_optinteger(L, b + 2, 1),
                                          (float)luaL_optnumber(L, b + 3, 8)) ? 1 : 0);
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, add, "Add", "Forno.Add");
    addFn(L, ctx, put, "Put", "Forno.Put");
    addFn(L, ctx, take, "Take", "Forno.Take");
    addFn(L, ctx, addRecipe, "AddRecipe", "Forno.AddRecipe");
    lua_setglobal(L, "Forno");
    lua_getglobal(L, "Forno");
    lua_setglobal(L, "Furnace");
}
}
