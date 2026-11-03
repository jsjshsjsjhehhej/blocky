#pragma once
#include "systems/Hitbox.h"
#include "lua.h"
#include "lualib.h"
namespace hitboxlua {
inline Hitbox* self(lua_State* L) {
    return static_cast<Hitbox*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, field);
}
inline uint32_t grp(lua_State* L, int i) {
    return Hitbox::parseGroup(luaL_optstring(L, i, "world"));
}
inline int box(lua_State* L) {
    lua_pushboolean(L, self(L)->addBox(luaL_checkstring(L, 1),
        (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
        (float)luaL_optnumber(L, 5, 1), (float)luaL_optnumber(L, 6, 1), (float)luaL_optnumber(L, 7, 1),
        (float)luaL_optnumber(L, 8, 0), grp(L, 9)) ? 1 : 0);
    return 1;
}
inline int sphere(lua_State* L) {
    lua_pushboolean(L, self(L)->addSphere(luaL_checkstring(L, 1),
        (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
        (float)luaL_optnumber(L, 5, 1), grp(L, 6)) ? 1 : 0);
    return 1;
}
inline int capsule(lua_State* L) {
    lua_pushboolean(L, self(L)->addCapsule(luaL_checkstring(L, 1),
        (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
        (float)luaL_optnumber(L, 5, 1), (float)luaL_optnumber(L, 6, 2),
        (float)luaL_optnumber(L, 7, 0), grp(L, 8)) ? 1 : 0);
    return 1;
}
inline int move(lua_State* L) {
    lua_pushboolean(L, self(L)->move(luaL_checkstring(L, 1),
        (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
        (float)luaL_optnumber(L, 5, 0)) ? 1 : 0);
    return 1;
}
inline int setLife(lua_State* L) {
    lua_pushboolean(L, self(L)->setLife(luaL_checkstring(L, 1), (float)luaL_checknumber(L, 2)) ? 1 : 0);
    return 1;
}
inline int setMask(lua_State* L) {
    lua_pushboolean(L, self(L)->setMask(luaL_checkstring(L, 1), grp(L, 2)) ? 1 : 0);
    return 1;
}
inline int setEnabled(lua_State* L) {
    lua_pushboolean(L, self(L)->setEnabled(luaL_checkstring(L, 1), lua_toboolean(L, 2) != 0) ? 1 : 0);
    return 1;
}
inline int count(lua_State* L) {
    lua_pushinteger(L, self(L)->count());
    return 1;
}
inline int remove(lua_State* L) {
    lua_pushboolean(L, self(L)->remove(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}
inline int hits(lua_State* L) {
    lua_pushboolean(L, self(L)->hits(luaL_checkstring(L, 1), luaL_checkstring(L, 2)) ? 1 : 0);
    return 1;
}
inline int overlap(lua_State* L) {
    auto names = self(L)->overlapNames(luaL_checkstring(L, 1));
    lua_newtable(L);
    int i = 1;
    for (const auto& n : names) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, n.c_str());
        lua_settable(L, -3);
    }
    return 1;
}
inline int ray(lua_State* L) {
    auto h = self(L)->ray(
        (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
        (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6),
        (float)luaL_optnumber(L, 7, 200), grp(L, 8));
    if (!h.ok) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushstring(L, h.name.c_str()); lua_setfield(L, -2, "Name");
    lua_pushnumber(L, h.t); lua_setfield(L, -2, "Distance");
    lua_pushnumber(L, h.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, h.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, h.z); lua_setfield(L, -2, "z");
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, box, "Box", "Hitbox.Box");
    addFn(L, ctx, sphere, "Sphere", "Hitbox.Sphere");
    addFn(L, ctx, capsule, "Capsule", "Hitbox.Capsule");
    addFn(L, ctx, move, "Move", "Hitbox.Move");
    addFn(L, ctx, setLife, "SetLife", "Hitbox.SetLife");
    addFn(L, ctx, setMask, "SetMask", "Hitbox.SetMask");
    addFn(L, ctx, setEnabled, "Enable", "Hitbox.Enable");
    addFn(L, ctx, count, "Count", "Hitbox.Count");
    addFn(L, ctx, remove, "Remove", "Hitbox.Remove");
    addFn(L, ctx, hits, "Hits", "Hitbox.Hits");
    addFn(L, ctx, overlap, "Overlap", "Hitbox.Overlap");
    addFn(L, ctx, ray, "Ray", "Hitbox.Ray");
    lua_setglobal(L, "Hitbox");
}
} // namespace hitboxlua
