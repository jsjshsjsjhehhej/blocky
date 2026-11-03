#pragma once

#include "systems/Inventario.h"
#include "lua.h"
#include "lualib.h"

namespace inventariolua {

inline Inventario* self(lua_State* L) {
    return static_cast<Inventario*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline int argBase(lua_State* L) { return lua_istable(L, 1) ? 2 : 1; }

inline int add(lua_State* L) {
    const int b = argBase(L);
    const char* id = luaL_checkstring(L, b);
    const char* name = luaL_optstring(L, b + 1, id);
    const int n = (int)luaL_optinteger(L, b + 2, 1);
    lua_pushinteger(L, self(L)->add(id, name, n));
    return 1;
}

inline int remove(lua_State* L) {
    const int b = argBase(L);
    lua_pushinteger(L, self(L)->remove(luaL_checkstring(L, b), (int)luaL_optinteger(L, b + 1, 1)));
    return 1;
}

inline int count(lua_State* L) {
    lua_pushinteger(L, self(L)->count(luaL_checkstring(L, argBase(L))));
    return 1;
}

inline int has(lua_State* L) {
    const int b = argBase(L);
    lua_pushboolean(L, self(L)->has(luaL_checkstring(L, b), (int)luaL_optinteger(L, b + 1, 1)) ? 1 : 0);
    return 1;
}

inline int select(lua_State* L) {
    self(L)->select((int)luaL_optinteger(L, argBase(L), 0));
    return 0;
}
inline int selected(lua_State* L) {
    lua_pushinteger(L, self(L)->selected());
    lua_pushstring(L, self(L)->selectedId());
    return 2;
}
inline int selectedId(lua_State* L) {
    lua_pushstring(L, self(L)->selectedId());
    return 1;
}
inline int slotAt(lua_State* L) {
    const Inventario::Slot* s = self(L)->slotAt((int)luaL_checkinteger(L, 1));
    if (!s) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushstring(L, s->id.c_str()); lua_setfield(L, -2, "id");
    lua_pushstring(L, s->name.c_str()); lua_setfield(L, -2, "name");
    lua_pushinteger(L, s->count); lua_setfield(L, -2, "count");
    return 1;
}
inline int list(lua_State* L) {
    lua_newtable(L);
    int i = 1;
    for (const auto& s : self(L)->slots()) {
        lua_pushinteger(L, i++);
        lua_newtable(L);
        lua_pushstring(L, s.id.c_str()); lua_setfield(L, -2, "id");
        lua_pushstring(L, s.name.c_str()); lua_setfield(L, -2, "name");
        lua_pushinteger(L, s.count); lua_setfield(L, -2, "count");
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
    addFn(L, ctx, add, "Add", "Inventario.Add");
    addFn(L, ctx, remove, "Remove", "Inventario.Remove");
    addFn(L, ctx, count, "Count", "Inventario.Count");
    addFn(L, ctx, has, "Has", "Inventario.Has");
    addFn(L, ctx, list, "List", "Inventario.List");
    addFn(L, ctx, select, "Select", "Inventario.Select");
    addFn(L, ctx, selected, "Selected", "Inventario.Selected");
    addFn(L, ctx, selectedId, "SelectedId", "Inventario.SelectedId");
    addFn(L, ctx, slotAt, "Slot", "Inventario.Slot");
    lua_setglobal(L, "Inventario");
    lua_getglobal(L, "Inventario");
    lua_setglobal(L, "Inventory");
}

} // namespace inventariolua
