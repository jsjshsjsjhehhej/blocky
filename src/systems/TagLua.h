#pragma once

#include "systems/Tag.h"
#include "lua.h"
#include "lualib.h"

namespace taglua {

inline Tag* self(lua_State* L) {
    return static_cast<Tag*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline int add(lua_State* L) {
    lua_pushboolean(L, self(L)->add(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2)) ? 1 : 0);
    return 1;
}

inline int remove(lua_State* L) {
    lua_pushboolean(L, self(L)->remove(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2)) ? 1 : 0);
    return 1;
}

inline int has(lua_State* L) {
    lua_pushboolean(L, self(L)->has(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2)) ? 1 : 0);
    return 1;
}

inline int get(lua_State* L) {
    auto ids = self(L)->get(luaL_checkstring(L, 1));
    lua_newtable(L);
    for (int i = 0; i < (int)ids.size(); ++i) {
        lua_pushinteger(L, i + 1);
        lua_pushinteger(L, ids[(size_t)i]);
        lua_settable(L, -3);
    }
    return 1;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, add, "Add", "Tag.Add");
    addFn(L, ctx, remove, "Remove", "Tag.Remove");
    addFn(L, ctx, has, "Has", "Tag.Has");
    addFn(L, ctx, get, "Get", "Tag.Get");
    lua_setglobal(L, "Tag");
}

} // namespace taglua
