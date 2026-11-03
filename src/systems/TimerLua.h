#pragma once
#include "systems/Timer.h"
#include "lua.h"
#include "lualib.h"
namespace timerlua {
inline Timer* self(lua_State* L) {
    return static_cast<Timer*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx); lua_pushcclosure(L, fn, dn, 1); lua_setfield(L, -2, f);
}
inline int start(lua_State* L) {
    lua_pushboolean(L, self(L)->start(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 1),
                                      lua_toboolean(L, 3) != 0) ? 1 : 0);
    return 1;
}
inline int done(lua_State* L) {
    lua_pushboolean(L, self(L)->done(luaL_checkstring(L, 1)) ? 1 : 0); return 1;
}
inline int left(lua_State* L) {
    lua_pushnumber(L, self(L)->left(luaL_checkstring(L, 1))); return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, start, "Start", "Timer.Start");
    addFn(L, ctx, done, "Done", "Timer.Done");
    addFn(L, ctx, left, "Left", "Timer.Left");
    lua_setglobal(L, "Timer");
}
}
