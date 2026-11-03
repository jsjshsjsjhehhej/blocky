#pragma once

#include "systems/Cassa.h"
#include "lua.h"
#include "lualib.h"

namespace cassalua {
inline Cassa* self(lua_State* L) {
    return static_cast<Cassa*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int xyz(lua_State* L, int& x, int& y, int& z) {
    int b = lua_istable(L, 1) ? 2 : 1;
    x = (int)luaL_checknumber(L, b);
    y = (int)luaL_checknumber(L, b + 1);
    z = (int)luaL_checknumber(L, b + 2);
    return b;
}
inline int open(lua_State* L) {
    int x, y, z; xyz(L, x, y, z);
    lua_pushboolean(L, self(L)->open(x, y, z) ? 1 : 0);
    return 1;
}
inline int close(lua_State* L) {
    int x, y, z; xyz(L, x, y, z);
    lua_pushboolean(L, self(L)->close(x, y, z) ? 1 : 0);
    return 1;
}
inline int put(lua_State* L) {
    int x, y, z; int b = xyz(L, x, y, z);
    lua_pushinteger(L, self(L)->put(x, y, z, luaL_checkstring(L, b + 3),
        luaL_optstring(L, b + 4, luaL_checkstring(L, b + 3)),
        (int)luaL_optinteger(L, b + 5, 1)));
    return 1;
}
inline int take(lua_State* L) {
    int x, y, z; int b = xyz(L, x, y, z);
    lua_pushinteger(L, self(L)->take(x, y, z, luaL_checkstring(L, b + 3),
        (int)luaL_optinteger(L, b + 4, 1)));
    return 1;
}
inline int count(lua_State* L) {
    int x, y, z; int b = xyz(L, x, y, z);
    lua_pushinteger(L, self(L)->count(x, y, z, luaL_checkstring(L, b + 3)));
    return 1;
}
inline int list(lua_State* L) {
    int x, y, z; xyz(L, x, y, z);
    const Cassa::Box* box = self(L)->get(x, y, z);
    lua_newtable(L);
    if (!box) return 1;
    int i = 1;
    for (int s = 0; s < Cassa::kSlots; ++s) {
        if (box->slots[s].id.empty()) continue;
        lua_pushinteger(L, i++);
        lua_newtable(L);
        lua_pushstring(L, box->slots[s].id.c_str()); lua_setfield(L, -2, "id");
        lua_pushstring(L, box->slots[s].name.c_str()); lua_setfield(L, -2, "name");
        lua_pushinteger(L, box->slots[s].count); lua_setfield(L, -2, "count");
        lua_settable(L, -3);
    }
    return 1;
}
inline int nchests(lua_State* L) {
    lua_pushinteger(L, self(L)->chestCount());
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, open, "Open", "Cassa.Open");
    addFn(L, ctx, close, "Close", "Cassa.Close");
    addFn(L, ctx, put, "Put", "Cassa.Put");
    addFn(L, ctx, take, "Take", "Cassa.Take");
    addFn(L, ctx, count, "Count", "Cassa.Count");
    addFn(L, ctx, list, "List", "Cassa.List");
    addFn(L, ctx, nchests, "ChestCount", "Cassa.ChestCount");
    lua_setglobal(L, "Cassa");
    lua_getglobal(L, "Cassa");
    lua_setglobal(L, "Chest");
}
}
