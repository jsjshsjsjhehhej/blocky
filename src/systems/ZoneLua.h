#pragma once
#include "systems/Zone.h"
#include "lua.h"
#include "lualib.h"
namespace zonelua {
inline Zone* self(lua_State* L) {
    return static_cast<Zone*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, field);
}
inline int scan(lua_State* L) {
    self(L)->scan((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2),
                  (float)luaL_checknumber(L, 3));
    return 0;
}
inline int add(lua_State* L) {
    lua_pushboolean(L, self(L)->add(luaL_checkstring(L, 1),
                                    (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
                                    (float)luaL_checknumber(L, 4), (float)luaL_optnumber(L, 5, 4),
                                    (float)luaL_optnumber(L, 6, 4), (float)luaL_optnumber(L, 7, 4)) ? 1 : 0);
    return 1;
}
inline int contains(lua_State* L) {
    lua_pushboolean(L, self(L)->contains(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int entered(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& n : self(L)->entered()) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, n.c_str());
        lua_settable(L, -3);
    }
    return 1;
}
inline int exited(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& n : self(L)->exited()) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, n.c_str());
        lua_settable(L, -3);
    }
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, add, "Add", "Zone.Add");
    addFn(L, ctx, scan, "Scan", "Zone.Scan");
    addFn(L, ctx, contains, "Contains", "Zone.Contains");
    addFn(L, ctx, entered, "Entered", "Zone.Entered");
    addFn(L, ctx, exited, "Exited", "Zone.Exited");
    lua_setglobal(L, "Zone");
}
} // namespace zonelua
