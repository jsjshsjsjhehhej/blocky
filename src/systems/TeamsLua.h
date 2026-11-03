#pragma once

#include "systems/Teams.h"
#include "lua.h"
#include "lualib.h"

namespace teamslua {

inline Teams* self(lua_State* L) {
    return static_cast<Teams*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline int create(lua_State* L) {
    lua_pushboolean(L, self(L)->create(luaL_checkstring(L, 1),
                                       (float)luaL_optnumber(L, 2, 0.2),
                                       (float)luaL_optnumber(L, 3, 0.6),
                                       (float)luaL_optnumber(L, 4, 1.0)) ? 1 : 0);
    return 1;
}

inline int join(lua_State* L) {
    lua_pushboolean(L, self(L)->join(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}

inline int leave(lua_State* L) {
    self(L)->leave();
    return 0;
}

inline int mine(lua_State* L) {
    lua_pushstring(L, self(L)->mine().c_str());
    return 1;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, create, "Create", "Teams.Create");
    addFn(L, ctx, join, "Join", "Teams.Join");
    addFn(L, ctx, leave, "Leave", "Teams.Leave");
    addFn(L, ctx, mine, "Mine", "Teams.Mine");
    lua_setglobal(L, "Teams");
}

} // namespace teamslua
