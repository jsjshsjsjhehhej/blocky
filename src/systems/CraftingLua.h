#pragma once
#include "systems/Crafting.h"
#include "lua.h"
#include "lualib.h"
namespace craftinglua {
inline Crafting* self(lua_State* L) {
    return static_cast<Crafting*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, field);
}
inline int addRecipe(lua_State* L) {
    lua_pushboolean(L, self(L)->addRecipe(luaL_checkstring(L, 1), luaL_checkstring(L, 2),
                                          luaL_optstring(L, 3, ""), (int)luaL_optinteger(L, 4, 1)) ? 1 : 0);
    return 1;
}
inline int addIngredient(lua_State* L) {
    lua_pushboolean(L, self(L)->addIngredient(luaL_checkstring(L, 1), luaL_checkstring(L, 2),
                                              (int)luaL_optinteger(L, 3, 1)) ? 1 : 0);
    return 1;
}
inline int getOut(lua_State* L) {
    const auto* r = self(L)->get(luaL_checkstring(L, 1));
    if (!r) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushstring(L, r->outId.c_str()); lua_setfield(L, -2, "id");
    lua_pushstring(L, r->outName.c_str()); lua_setfield(L, -2, "name");
    lua_pushinteger(L, r->outCount); lua_setfield(L, -2, "count");
    return 1;
}
inline int needs(lua_State* L) {
    const auto* r = self(L)->get(luaL_checkstring(L, 1));
    lua_newtable(L);
    if (!r) return 1;
    for (int i = 0; i < r->inCount; ++i) {
        lua_pushinteger(L, i + 1);
        lua_newtable(L);
        lua_pushstring(L, r->ins[i].id.c_str()); lua_setfield(L, -2, "id");
        lua_pushinteger(L, r->ins[i].n); lua_setfield(L, -2, "count");
        lua_settable(L, -3);
    }
    return 1;
}
inline int canCraft(lua_State* L) {
    lua_pushboolean(L, self(L)->canCraft(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int craft(lua_State* L) {
    lua_pushboolean(L, self(L)->tryCraft(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int count(lua_State* L) {
    lua_pushinteger(L, self(L)->count());
    return 1;
}
inline int list(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& r : self(L)->all()) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, r.id.c_str());
        lua_settable(L, -3);
    }
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, addRecipe, "AddRecipe", "Crafting.AddRecipe");
    addFn(L, ctx, addIngredient, "AddIngredient", "Crafting.AddIngredient");
    addFn(L, ctx, getOut, "GetOut", "Crafting.GetOut");
    addFn(L, ctx, needs, "Needs", "Crafting.Needs");
    addFn(L, ctx, canCraft, "CanCraft", "Crafting.CanCraft");
    addFn(L, ctx, craft, "Craft", "Crafting.Craft");
    addFn(L, ctx, count, "Count", "Crafting.Count");
    addFn(L, ctx, list, "List", "Crafting.List");
    lua_setglobal(L, "Crafting");
}
} // namespace craftinglua
