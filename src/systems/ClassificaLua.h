#pragma once

// API Lua di Classifica: la tabella globale "Classifica" negli script.
//
//   Classifica.Set("Monete", 10)            -- la mia riga
//   local n = Classifica.Add("Monete", 5)   -- ritorna il nuovo valore
//   Classifica.Get("Monete")                -- il mio valore (0 se non c'e')
//   Classifica.Get("Monete", "Mario")       -- quello di un altro giocatore
//   Classifica.Remove("Monete")             -- toglie la colonna
//
// Ognuno scrive solo la propria riga (le altre arrivano dalla rete).
// Nomi colonna: max 12 caratteri, lettere/numeri/spazio/_, max 6 colonne.

#include "systems/Classifica.h"

#include "lua.h"
#include "lualib.h"

namespace classificalua {

inline Classifica* self(lua_State* L) {
    return static_cast<Classifica*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline std::string optPlayer(lua_State* L, int idx) {
    const char* p = lua_isstring(L, idx) ? lua_tostring(L, idx) : nullptr;
    return p ? p : "";
}

inline int set(lua_State* L) {
    const std::string stat = luaL_checkstring(L, 1);
    const double v = luaL_checknumber(L, 2);
    if (!self(L)->set(stat, v))
        luaL_error(L, "Classifica.Set: nome '%s' non valido (max 12 caratteri, lettere/numeri) o troppe colonne (max 6)",
                   stat.c_str());
    return 0;
}

inline int add(lua_State* L) {
    const std::string stat = luaL_checkstring(L, 1);
    const double delta = luaL_optnumber(L, 2, 1.0);
    double out = 0.0;
    if (!self(L)->add(stat, delta, out))
        luaL_error(L, "Classifica.Add: nome '%s' non valido o troppe colonne", stat.c_str());
    lua_pushnumber(L, out);
    return 1;
}

inline int get(lua_State* L) {
    lua_pushnumber(L, self(L)->get(luaL_checkstring(L, 1), optPlayer(L, 2)));
    return 1;
}

inline int remove(lua_State* L) {
    lua_pushboolean(L, self(L)->removeStat(luaL_checkstring(L, 1)) ? 1 : 0);
    return 1;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, set, "Set", "Classifica.Set");
    addFn(L, ctx, add, "Add", "Classifica.Add");
    addFn(L, ctx, get, "Get", "Classifica.Get");
    addFn(L, ctx, remove, "Remove", "Classifica.Remove");
    lua_setglobal(L, "Classifica");
}

} // namespace classificalua
