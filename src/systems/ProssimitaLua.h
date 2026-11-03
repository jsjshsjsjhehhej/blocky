#pragma once
#include "systems/Prossimita.h"
#include "lua.h"
#include "lualib.h"
namespace prossimitalua {
inline Prossimita* self(lua_State* L) {
    return static_cast<Prossimita*>(lua_touserdata(L, lua_upvalueindex(1)));
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
    lua_pushboolean(L, self(L)->add(luaL_checkstring(L, 1), luaL_optstring(L, 2, "Usa"),
                                    (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
                                    (float)luaL_checknumber(L, 5), (float)luaL_optnumber(L, 6, 6)) ? 1 : 0);
    return 1;
}
inline int nearest(lua_State* L) {
    lua_pushstring(L, self(L)->nearest().c_str());
    return 1;
}
inline int activate(lua_State* L) {
    self(L)->requestActivate();
    const std::string n = self(L)->takeActivated();
    if (n.empty()) lua_pushnil(L);
    else lua_pushstring(L, n.c_str());
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, add, "Add", "Prossimita.Add");
    addFn(L, ctx, scan, "Scan", "Prossimita.Scan");
    addFn(L, ctx, nearest, "Nearest", "Prossimita.Nearest");
    addFn(L, ctx, activate, "Activate", "Prossimita.Activate");
    lua_setglobal(L, "Prossimita");
}
} // namespace prossimitalua
