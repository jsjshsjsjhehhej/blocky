#pragma once

#include "systems/Missioni.h"
#include "lua.h"
#include "lualib.h"

namespace missionilua {

inline Missioni* self(lua_State* L) {
    return static_cast<Missioni*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline int argBase(lua_State* L) { return lua_istable(L, 1) ? 2 : 1; }

inline int start(lua_State* L) {
    const int b = argBase(L);
    lua_pushboolean(L, self(L)->start(luaL_checkstring(L, b), luaL_optstring(L, b + 1, ""),
                                      (int)luaL_optinteger(L, b + 2, 0)) ? 1 : 0);
    return 1;
}

inline int addStep(lua_State* L) {
    const int b = argBase(L);
    lua_pushboolean(L, self(L)->addStep(luaL_checkstring(L, b), luaL_checkstring(L, b + 1),
                                        luaL_optstring(L, b + 2, "")) ? 1 : 0);
    return 1;
}

inline int advance(lua_State* L) {
    const int b = argBase(L);
    lua_pushboolean(L, self(L)->advance(luaL_checkstring(L, b), luaL_checkstring(L, b + 1)) ? 1 : 0);
    return 1;
}

inline int complete(lua_State* L) {
    lua_pushboolean(L, self(L)->complete(luaL_checkstring(L, argBase(L))) ? 1 : 0);
    return 1;
}

inline int isDone(lua_State* L) {
    const auto* q = self(L)->get(luaL_checkstring(L, argBase(L)));
    lua_pushboolean(L, (q && q->completed) ? 1 : 0);
    return 1;
}

inline int active(lua_State* L) {
    lua_pushinteger(L, self(L)->activeCount());
    return 1;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, start, "Start", "Missioni.Start");
    addFn(L, ctx, addStep, "AddStep", "Missioni.AddStep");
    addFn(L, ctx, advance, "Advance", "Missioni.Advance");
    addFn(L, ctx, complete, "Complete", "Missioni.Complete");
    addFn(L, ctx, isDone, "IsDone", "Missioni.IsDone");
    addFn(L, ctx, active, "Active", "Missioni.Active");
    lua_setglobal(L, "Missioni");
}

} // namespace missionilua
