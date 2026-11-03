#pragma once

// Lato Lua del generatore (vedi PartProps.def): part.X in lettura e
// scrittura, e i campi della tabella passata a Instance.new/Workspace:Add.

#include "PartProps.h"

#include "lua.h"

#include <cstring>

namespace partprops {

inline void pushLua(lua_State* ls, bool v) { lua_pushboolean(ls, v ? 1 : 0); }
inline void pushLua(lua_State* ls, float v) { lua_pushnumber(ls, v); }

// Ritorna false se il valore Lua non e' del tipo giusto (la proprieta' non cambia).
inline bool readLua(lua_State* ls, int idx, bool& out) {
    if (!lua_isboolean(ls, idx)) return false;
    out = lua_toboolean(ls, idx) != 0;
    return true;
}
inline bool readLua(lua_State* ls, int idx, float& out) {
    if (!lua_isnumber(ls, idx)) return false;
    out = (float)lua_tonumber(ls, idx);
    return true;
}

// part.X: true se la chiave era una delle proprieta' generate.
inline bool luaGet(lua_State* ls, const char* key, const WorkspacePart& p) {
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help) \
    if (std::strcmp(key, luaName) == 0) { pushLua(ls, p.field); return true; }
#include "PartProps.def"
#undef PART_PROP
    return false;
}

// part.X = valore (il valore e' in posizione idx dello stack).
inline bool luaSet(lua_State* ls, const char* key, int idx, WorkspacePart& p) {
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help)           \
    if (std::strcmp(key, luaName) == 0) {                                   \
        T v = p.field;                                                      \
        if (readLua(ls, idx, v)) p.field = clampProp(v, mn, mx);            \
        return true;                                                        \
    }
#include "PartProps.def"
#undef PART_PROP
    return false;
}

// Tutte le proprieta' come campi della tabella in cima allo stack.
inline void luaFillTable(lua_State* ls, const WorkspacePart& p) {
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help) \
    pushLua(ls, p.field); lua_setfield(ls, -2, luaName);
#include "PartProps.def"
#undef PART_PROP
}

// Legge dalla tabella in posizione tableIdx i campi presenti.
inline void luaReadTable(lua_State* ls, int tableIdx, WorkspacePart& p) {
#define PART_PROP(T, field, luaName, jsonKey, def, mn, mx, help) \
    {                                                            \
        lua_getfield(ls, tableIdx, luaName);                     \
        T v = p.field;                                           \
        if (readLua(ls, -1, v)) p.field = clampProp(v, mn, mx);  \
        lua_pop(ls, 1);                                          \
    }
#include "PartProps.def"
#undef PART_PROP
}

} // namespace partprops
