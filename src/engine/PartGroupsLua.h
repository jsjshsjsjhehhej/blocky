#pragma once

// API Lua dei Model (Ctrl+G in Studio): la tabella globale "Models".
//
//   Models.Exists("Auto")                      -> true/false
//   Models.Names()                             -> {"Auto", "Casa", ...}
//   Models.Count("Auto")                       -> numero di part (anche annidate)
//   Models.GetPivot("Auto")                    -> Vector3 del centro (come GetPivot)
//   Models.MoveTo("Auto", Vector3.new(0,5,0))  -> sposta tutto il Model
//   Models.Translate("Auto", Vector3.new(0,1,0))
//   Models.Scale("Auto", 2)                    -> grande il doppio, attorno al centro
//   Models.SetAnchored("Auto", false)          -> tutto il Model cade (fisica)
//   Models.SetColor("Auto", Color3.new(1,0,0))
//   Models.SetDragDetector("Auto", true)       -> in Play il clic sposta tutto il Model
//
// Se piu' Model hanno lo stesso nome vale il primo, come FindFirstChild.

#include "PartGroups.h"

#include "lua.h"
#include "lualib.h"

namespace partgroupslua {

inline DataModel* world(lua_State* L) {
    return static_cast<DataModel*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline int checkGroup(lua_State* L, int idx) {
    const char* name = luaL_checkstring(L, idx);
    const int id = partgroups::findByName(*world(L), name);
    if (!id) luaL_error(L, "Models: nessun Model chiamato '%s'", name);
    return id;
}

// Vector3 o Color3 di Blocky: tabella con X/Y/Z oppure R/G/B.
inline glm::vec3 checkVec(lua_State* L, int idx, const char* a, const char* b, const char* c) {
    luaL_checktype(L, idx, LUA_TTABLE);
    glm::vec3 v(0.f);
    const char* keys[3] = {a, b, c};
    for (int i = 0; i < 3; ++i) {
        lua_getfield(L, idx, keys[i]);
        v[i] = (float)luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
    return v;
}

inline void pushVec(lua_State* L, const glm::vec3& v) {
    // Stessa forma dei Vector3 del motore (x/y/z e X/Y/Z); senza metatabella,
    // basta per leggere i campi o passarlo a Vector3.new.
    lua_newtable(L);
    const char* lower[3] = {"x", "y", "z"};
    const char* upper[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) {
        lua_pushnumber(L, v[i]); lua_setfield(L, -2, lower[i]);
        lua_pushnumber(L, v[i]); lua_setfield(L, -2, upper[i]);
    }
}

inline int exists(lua_State* L) {
    lua_pushboolean(L, partgroups::findByName(*world(L), luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}

inline int names(lua_State* L) {
    lua_newtable(L);
    int n = 0;
    for (const auto& g : world(L)->groups) {
        lua_pushstring(L, g.name.c_str());
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

inline int count(lua_State* L) {
    lua_pushinteger(L, (int)partgroups::memberParts(*world(L), checkGroup(L, 1)).size());
    return 1;
}

inline int getPivot(lua_State* L) {
    pushVec(L, partgroups::pivot(*world(L), checkGroup(L, 1)));
    return 1;
}

inline int moveTo(lua_State* L) {
    const int id = checkGroup(L, 1);
    partgroups::moveTo(*world(L), id, checkVec(L, 2, "X", "Y", "Z"));
    return 0;
}

inline int translate(lua_State* L) {
    const int id = checkGroup(L, 1);
    const glm::vec3 d = checkVec(L, 2, "X", "Y", "Z");
    DataModel& dm = *world(L);
    for (int i : partgroups::memberParts(dm, id)) dm.parts[(size_t)i].position += d;
    return 0;
}

// Models.Scale("Auto", 2) -> tutto il Model grande il doppio, attorno al centro.
inline int scale(lua_State* L) {
    const int id = checkGroup(L, 1);
    const float f = (float)luaL_checknumber(L, 2);
    if (!(f > 0.01f && f < 100.f)) luaL_error(L, "Models.Scale: fattore fra 0.01 e 100");
    partgroups::scaleParts(*world(L), partgroups::memberParts(*world(L), id), f);
    return 0;
}

inline int setAnchored(lua_State* L) {
    const int id = checkGroup(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    const bool v = lua_toboolean(L, 2) != 0;
    DataModel& dm = *world(L);
    for (int i : partgroups::memberParts(dm, id)) dm.parts[(size_t)i].anchored = v;
    return 0;
}

inline int setDragDetector(lua_State* L) {
    const int id = checkGroup(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    if (PartGroup* g = partgroups::find(*world(L), id)) {
        if (g->folder) luaL_error(L, "Models.SetDragDetector: '%s' e' una Folder", g->name.c_str());
        g->dragDetector = lua_toboolean(L, 2) != 0;
    }
    return 0;
}

inline int setColor(lua_State* L) {
    const int id = checkGroup(L, 1);
    const glm::vec3 c = glm::clamp(checkVec(L, 2, "R", "G", "B"), glm::vec3(0.f), glm::vec3(1.f));
    DataModel& dm = *world(L);
    for (int i : partgroups::memberParts(dm, id)) dm.parts[(size_t)i].color = c;
    return 0;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

// ctx = il DataModel del gioco.
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, exists, "Exists", "Models.Exists");
    addFn(L, ctx, names, "Names", "Models.Names");
    addFn(L, ctx, count, "Count", "Models.Count");
    addFn(L, ctx, getPivot, "GetPivot", "Models.GetPivot");
    addFn(L, ctx, moveTo, "MoveTo", "Models.MoveTo");
    addFn(L, ctx, translate, "Translate", "Models.Translate");
    addFn(L, ctx, scale, "Scale", "Models.Scale");
    addFn(L, ctx, setAnchored, "SetAnchored", "Models.SetAnchored");
    addFn(L, ctx, setColor, "SetColor", "Models.SetColor");
    addFn(L, ctx, setDragDetector, "SetDragDetector", "Models.SetDragDetector");
    lua_setglobal(L, "Models");
}

} // namespace partgroupslua
