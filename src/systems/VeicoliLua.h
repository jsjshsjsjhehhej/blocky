#pragma once

// API Lua di Veicoli: la tabella globale "Veicoli" negli script.
//
//   if Veicoli.IsDriving() then ... end
//   print(Veicoli.Speed())      -- stud/s, negativa in retromarcia
//
// Un'auto si crea dalla part stessa: part.VehicleSeat = true con
// part.Anchored = false (anche da script).

#include "systems/Veicoli.h"

#include "lua.h"
#include "lualib.h"

namespace veicolilua {

inline Veicoli* self(lua_State* L) {
    return static_cast<Veicoli*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline int isDriving(lua_State* L) {
    lua_pushboolean(L, self(L)->seated() ? 1 : 0);
    return 1;
}

inline int speed(lua_State* L) {
    lua_pushnumber(L, self(L)->speed());
    return 1;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, isDriving, "IsDriving", "Veicoli.IsDriving");
    addFn(L, ctx, speed, "Speed", "Veicoli.Speed");
    lua_setglobal(L, "Veicoli");
}

} // namespace veicolilua
