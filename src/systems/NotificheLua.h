#pragma once

#include "systems/Notifiche.h"
#include "lua.h"
#include "lualib.h"

namespace notifichelua {

inline Notifiche* self(lua_State* L) {
    return static_cast<Notifiche*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline int push(lua_State* L) {
    self(L)->push(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 2.5));
    return 0;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, push, "Push", "Notifiche.Push");
    lua_setglobal(L, "Notifiche");
}

} // namespace notifichelua
