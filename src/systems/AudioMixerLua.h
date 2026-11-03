#pragma once

// API Lua di AudioMixer: la tabella globale "AudioMixer" negli script.
//
//   local id = AudioMixer.Play("sounds/shoot.wav")              -- ovunque, volume pieno
//   AudioMixer.Play("sounds/music.mp3", 0.5, true)              -- volume 0.5, in loop
//   AudioMixer.PlayAt("sounds/groan.wav", part.Position)        -- 3D: piu' forte da vicino
//   AudioMixer.PlayAt("sounds/hit.wav", Vector3.new(0, 2, 0), 0.8)
//   AudioMixer.Stop(id)   AudioMixer.StopAll()
//   AudioMixer.IsPlaying(id)
//   AudioMixer.SetVolume(0.5)                                   -- volume generale
//
// I suoni si sommano: uno nuovo non ferma piu' quello di prima.

#include "systems/AudioMixer.h"

#include "lua.h"
#include "lualib.h"

#include <string>

namespace audiomixerlua {

inline AudioMixer* self(lua_State* L) {
    return static_cast<AudioMixer*>(lua_touserdata(L, lua_upvalueindex(1)));
}

// Uno script non deve poter aprire file fuori dalle cartelle del gioco.
inline std::string scriptPath(lua_State* L, int idx) {
    std::string p = luaL_checkstring(L, idx);
    if (p.find("..") != std::string::npos || (!p.empty() && (p[0] == '/' || p[0] == '\\'))
        || p.find(':') != std::string::npos)
        luaL_error(L, "AudioMixer: percorso non ammesso '%s' (usa per esempio \"sounds/jump.wav\")", p.c_str());
    AudioMixer* m = self(L);
    return m->resolveScriptPath ? m->resolveScriptPath(p) : p;
}

inline bool readVec3(lua_State* L, int idx, glm::vec3& out) {
    if (!lua_istable(L, idx)) return false;
    const char* names[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) {
        lua_getfield(L, idx, names[i]);
        if (!lua_isnumber(L, -1)) { lua_pop(L, 1); return false; }
        out[i] = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    return true;
}

inline int play(lua_State* L) {
    const std::string path = scriptPath(L, 1);
    const float vol = (float)luaL_optnumber(L, 2, 1.0);
    const bool loop = lua_toboolean(L, 3) != 0;
    const int id = self(L)->play(path, vol, loop);
    if (!id) luaL_error(L, "AudioMixer.Play: %s", self(L)->lastError().c_str());
    lua_pushinteger(L, id);
    return 1;
}

// PlayAt(path, Vector3 [, volume [, loop]])  oppure  PlayAt(path, x, y, z [, volume [, loop]])
inline int playAt(lua_State* L) {
    const std::string path = scriptPath(L, 1);
    glm::vec3 pos(0.f);
    int next = 3;
    if (!readVec3(L, 2, pos)) {
        pos = {(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)};
        next = 5;
    }
    const float vol = (float)luaL_optnumber(L, next, 1.0);
    const bool loop = lua_toboolean(L, next + 1) != 0;
    const int id = self(L)->playAt(path, pos, vol, loop);
    if (!id) luaL_error(L, "AudioMixer.PlayAt: %s", self(L)->lastError().c_str());
    lua_pushinteger(L, id);
    return 1;
}

inline int stop(lua_State* L) {
    self(L)->stop((int)luaL_checkinteger(L, 1));
    return 0;
}

inline int stopAll(lua_State* L) {
    self(L)->stopAll();
    return 0;
}

inline int isPlaying(lua_State* L) {
    lua_pushboolean(L, self(L)->isPlaying((int)luaL_checkinteger(L, 1)) ? 1 : 0);
    return 1;
}

inline int setVolume(lua_State* L) {
    self(L)->setMasterVolume((float)luaL_checknumber(L, 1));
    return 0;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, play, "Play", "AudioMixer.Play");
    addFn(L, ctx, playAt, "PlayAt", "AudioMixer.PlayAt");
    addFn(L, ctx, stop, "Stop", "AudioMixer.Stop");
    addFn(L, ctx, stopAll, "StopAll", "AudioMixer.StopAll");
    addFn(L, ctx, isPlaying, "IsPlaying", "AudioMixer.IsPlaying");
    addFn(L, ctx, setVolume, "SetVolume", "AudioMixer.SetVolume");
    lua_setglobal(L, "AudioMixer");
}

} // namespace audiomixerlua
