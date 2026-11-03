#pragma once
#include "systems/CameraShake.h"
#include "lua.h"
#include "lualib.h"
namespace camerashakelua {
inline CameraShake* self(lua_State* L) {
    return static_cast<CameraShake*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}
inline int add(lua_State* L) {
    self(L)->add((float)luaL_optnumber(L, 1, 0.35));
    return 0;
}
inline int trauma(lua_State* L) {
    lua_pushnumber(L, self(L)->trauma());
    return 1;
}
inline int offset(lua_State* L) {
    lua_newtable(L);
    lua_pushnumber(L, self(L)->offsetX()); lua_setfield(L, -2, "x");
    lua_pushnumber(L, self(L)->offsetY()); lua_setfield(L, -2, "y");
    lua_pushnumber(L, self(L)->roll()); lua_setfield(L, -2, "roll");
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, add, "Add", "CameraShake.Add");
    addFn(L, ctx, trauma, "Trauma", "CameraShake.Trauma");
    addFn(L, ctx, offset, "Offset", "CameraShake.Offset");
    lua_setglobal(L, "CameraShake");
}
}
