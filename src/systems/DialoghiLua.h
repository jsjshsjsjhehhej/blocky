#pragma once
#include "systems/Dialoghi.h"
#include "lua.h"
#include "lualib.h"
namespace dialoghilua {
inline Dialoghi* self(lua_State* L) {
    return static_cast<Dialoghi*>(lua_touserdata(L, lua_upvalueindex(1)));
}
inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, field);
}
inline int addNode(lua_State* L) {
    lua_pushboolean(L, self(L)->addNode(luaL_checkstring(L, 1), luaL_optstring(L, 2, "NPC"),
                                        luaL_checkstring(L, 3)) ? 1 : 0);
    return 1;
}
inline int link(lua_State* L) {
    lua_pushboolean(L, self(L)->link(luaL_checkstring(L, 1), luaL_checkstring(L, 2)) ? 1 : 0);
    return 1;
}
inline int addChoice(lua_State* L) {
    lua_pushboolean(L, self(L)->addChoice(luaL_checkstring(L, 1), luaL_checkstring(L, 2),
                                          luaL_checkstring(L, 3)) ? 1 : 0);
    return 1;
}
inline int start(lua_State* L) {
    lua_pushboolean(L, self(L)->start(luaL_checkstring(L, 1), luaL_optstring(L, 2, "")) ? 1 : 0);
    return 1;
}
inline int next(lua_State* L) {
    lua_pushboolean(L, self(L)->next() ? 1 : 0);
    return 1;
}
inline int choose(lua_State* L) {
    lua_pushboolean(L, self(L)->choose((int)luaL_checkinteger(L, 1) - 1) ? 1 : 0);
    return 1;
}
inline int isOpen(lua_State* L) {
    lua_pushboolean(L, self(L)->open() ? 1 : 0);
    return 1;
}
inline int text(lua_State* L) {
    const auto* n = self(L)->current();
    lua_pushstring(L, n ? n->text.c_str() : "");
    return 1;
}
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, addNode, "AddNode", "Dialoghi.AddNode");
    addFn(L, ctx, link, "Link", "Dialoghi.Link");
    addFn(L, ctx, addChoice, "AddChoice", "Dialoghi.AddChoice");
    addFn(L, ctx, start, "Start", "Dialoghi.Start");
    addFn(L, ctx, next, "Next", "Dialoghi.Next");
    addFn(L, ctx, choose, "Choose", "Dialoghi.Choose");
    addFn(L, ctx, isOpen, "IsOpen", "Dialoghi.IsOpen");
    addFn(L, ctx, text, "Text", "Dialoghi.Text");
    lua_setglobal(L, "Dialoghi");
}
} // namespace dialoghilua
