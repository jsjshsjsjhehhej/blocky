#pragma once
#include "systems/Debris.h"
#include "lua.h"
#include "lualib.h"
namespace debrislua {
inline Debris* self(lua_State* L) {
    return static_cast<Debris*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, field);
}
inline int add(lua_State* L) {
    lua_pushboolean(L, self(L)->add(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 3.0)) ? 1 : 0);
    return 1;
}
inline int take(lua_State* L) {
    const std::string n = self(L)->takeExpired();
    if (n.empty()) lua_pushnil(L);
    else lua_pushstring(L, n.c_str());
    return 1;
}
inline int pending(lua_State* L) {
    lua_pushinteger(L, self(L)->pending());
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, add, "Add", "Debris.Add");
    addFn(L, ctx, take, "TakeExpired", "Debris.TakeExpired");
    addFn(L, ctx, pending, "Pending", "Debris.Pending");
    lua_setglobal(L, "Debris");
}
} // namespace debrislua
