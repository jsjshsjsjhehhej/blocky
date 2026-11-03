#pragma once

#include "systems/Meteo.h"
#include "lua.h"
#include "lualib.h"

namespace meteolua {

inline Meteo* self(lua_State* L) {
    return static_cast<Meteo*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline int setHours(lua_State* L) {
    self(L)->setHours((float)luaL_checknumber(L, 1));
    return 0;
}

inline int hours(lua_State* L) {
    lua_pushnumber(L, self(L)->hours());
    return 1;
}

inline int setSpeed(lua_State* L) {
    self(L)->setSpeed((float)luaL_optnumber(L, 1, 0.0));
    return 0;
}

inline int setWeather(lua_State* L) {
    self(L)->setWeather(Meteo::parseWeather(luaL_checkstring(L, 1)));
    return 0;
}

inline int weather(lua_State* L) {
    lua_pushstring(L, Meteo::weatherName(self(L)->weather()));
    return 1;
}

inline int isNight(lua_State* L) {
    lua_pushboolean(L, self(L)->isNight() ? 1 : 0);
    return 1;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, setHours, "SetHours", "Meteo.SetHours");
    addFn(L, ctx, hours, "Hours", "Meteo.Hours");
    addFn(L, ctx, setSpeed, "SetSpeed", "Meteo.SetSpeed");
    addFn(L, ctx, setWeather, "SetWeather", "Meteo.SetWeather");
    addFn(L, ctx, weather, "Weather", "Meteo.Weather");
    addFn(L, ctx, isNight, "IsNight", "Meteo.IsNight");
    lua_setglobal(L, "Meteo");
}

} // namespace meteolua
