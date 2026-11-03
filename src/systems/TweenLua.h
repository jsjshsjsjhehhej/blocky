#pragma once

#include "systems/Tween.h"
#include "lua.h"
#include "lualib.h"

namespace tweenlua {

inline Tween* self(lua_State* L) {
    return static_cast<Tween*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline Tween::Ease easeArg(lua_State* L, int idx) {
    const char* s = luaL_optstring(L, idx, "linear");
    std::string n = s ? s : "linear";
    for (char& c : n)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return Tween::parseEase(n);
}

inline int number(lua_State* L) {
    const float a = (float)luaL_checknumber(L, 1);
    const float b = (float)luaL_checknumber(L, 2);
    const float d = (float)luaL_optnumber(L, 3, 1.0);
    lua_pushinteger(L, self(L)->number(a, b, d, easeArg(L, 4)));
    return 1;
}

inline int vector(lua_State* L) {
    auto num = [&](int i) { return (float)luaL_checknumber(L, i); };
    lua_pushinteger(L, self(L)->vector(num(1), num(2), num(3), num(4), num(5), num(6),
                                       (float)luaL_optnumber(L, 7, 1.0), easeArg(L, 8)));
    return 1;
}

inline int value(lua_State* L) {
    lua_pushnumber(L, self(L)->value((int)luaL_checkinteger(L, 1)));
    return 1;
}

inline int value3(lua_State* L) {
    float x, y, z;
    self(L)->value3((int)luaL_checkinteger(L, 1), x, y, z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

inline int isDone(lua_State* L) {
    lua_pushboolean(L, self(L)->isDone((int)luaL_checkinteger(L, 1)) ? 1 : 0);
    return 1;
}

inline int cancel(lua_State* L) {
    lua_pushboolean(L, self(L)->cancel((int)luaL_checkinteger(L, 1)) ? 1 : 0);
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
    addFn(L, ctx, number, "Number", "Tween.Number");
    addFn(L, ctx, vector, "Vector", "Tween.Vector");
    addFn(L, ctx, value, "Value", "Tween.Value");
    addFn(L, ctx, value3, "Value3", "Tween.Value3");
    addFn(L, ctx, isDone, "IsDone", "Tween.IsDone");
    addFn(L, ctx, cancel, "Cancel", "Tween.Cancel");
    addFn(L, ctx, active, "Active", "Tween.Active");
    lua_setglobal(L, "Tween");
}

} // namespace tweenlua
