#pragma once

// API Luau come workspace.Terrain in Roblox Studio (solo nomi/metodi ufficiali).

#include "systems/Terrain.h"
#include "lua.h"
#include "lualib.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace terrainlua {

inline Terrain* self(lua_State* L) {
    return static_cast<Terrain*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* f, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, f);
}

inline bool readVec(lua_State* L, int i, glm::vec3& o) {
    if (lua_istable(L, i)) {
        lua_getfield(L, i, "x");
        if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_getfield(L, i, "X"); }
        o.x = (float)luaL_optnumber(L, -1, 0); lua_pop(L, 1);
        lua_getfield(L, i, "y");
        if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_getfield(L, i, "Y"); }
        o.y = (float)luaL_optnumber(L, -1, 0); lua_pop(L, 1);
        lua_getfield(L, i, "z");
        if (lua_isnil(L, -1)) { lua_pop(L, 1); lua_getfield(L, i, "Z"); }
        o.z = (float)luaL_optnumber(L, -1, 0); lua_pop(L, 1);
        return true;
    }
    if (lua_isnumber(L, i)) {
        o.x = (float)lua_tonumber(L, i);
        o.y = (float)luaL_optnumber(L, i + 1, 0);
        o.z = (float)luaL_optnumber(L, i + 2, 0);
        return true;
    }
    return false;
}

inline int argBase(lua_State* L) {
    return lua_istable(L, 1) && self(L) ? 2 : 1;
}

inline uint8_t readMaterial(lua_State* L, int i) {
    if (lua_isnumber(L, i))
        return (uint8_t)std::clamp((int)lua_tointeger(L, i), 0, (int)Terrain::Count - 1);
    if (lua_isstring(L, i))
        return Terrain::materialFromEnum(lua_tostring(L, i));
    if (lua_istable(L, i)) {
        lua_getfield(L, i, "Name");
        if (lua_isstring(L, -1)) {
            std::string s = lua_tostring(L, -1);
            lua_pop(L, 1);
            return Terrain::materialFromEnum(s);
        }
        lua_pop(L, 1);
    }
    return Terrain::Grass;
}

inline glm::vec3 cframePos(lua_State* L, int i) {
    glm::vec3 p{};
    if (lua_istable(L, i)) {
        lua_getfield(L, i, "Position");
        if (lua_istable(L, -1)) readVec(L, lua_gettop(L), p);
        lua_pop(L, 1);
        if (p.x == 0.f && p.y == 0.f && p.z == 0.f) readVec(L, i, p);
    }
    return p;
}

inline int clear(lua_State* L) {
    self(L)->reset();
    return 0;
}

inline int worldToCell(lua_State* L) {
    glm::vec3 p{};
    if (!readVec(L, argBase(L), p)) return 0;
    auto c = self(L)->worldToCell(p);
    lua_pushnumber(L, c.x);
    lua_pushnumber(L, c.y);
    lua_pushnumber(L, c.z);
    return 3;
}

inline int cellCenterToWorld(lua_State* L) {
    int b = argBase(L);
    glm::vec3 cell{};
    if (!readVec(L, b, cell)) return 0;
    auto w = self(L)->cellToWorld((int)cell.x, (int)cell.y, (int)cell.z);
    lua_pushnumber(L, w.x);
    lua_pushnumber(L, w.y);
    lua_pushnumber(L, w.z);
    return 3;
}

inline int fillBlock(lua_State* L) {
    int b = argBase(L);
    glm::vec3 pos = cframePos(L, b);
    glm::vec3 size{4, 4, 4};
    readVec(L, b + 1, size);
    uint8_t id = readMaterial(L, b + 2);
    lua_pushinteger(L, self(L)->fillBlockWorld(pos, size, id));
    return 1;
}

inline int fillBall(lua_State* L) {
    int b = argBase(L);
    glm::vec3 pos{};
    readVec(L, b, pos);
    float r = (float)luaL_optnumber(L, b + 1, 4);
    uint8_t id = readMaterial(L, b + 2);
    lua_pushinteger(L, self(L)->fillBallWorld(pos, r, id));
    return 1;
}

inline int readVoxels(lua_State* L) {
    int b = argBase(L);
    glm::vec3 mn{}, mx{};
    if (lua_istable(L, b)) {
        lua_getfield(L, b, "Min");
        if (lua_istable(L, -1)) readVec(L, lua_gettop(L), mn);
        lua_pop(L, 1);
        lua_getfield(L, b, "Max");
        if (lua_istable(L, -1)) readVec(L, lua_gettop(L), mx);
        lua_pop(L, 1);
    }
    (void)luaL_optinteger(L, b + 1, 4);
    auto a = self(L)->worldToCell(mn);
    auto c = self(L)->worldToCell(mx);
    if (a.x > c.x) std::swap(a.x, c.x);
    if (a.y > c.y) std::swap(a.y, c.y);
    if (a.z > c.z) std::swap(a.z, c.z);
    lua_newtable(L);
    lua_newtable(L);
    int idx = 1;
    for (int y = a.y; y <= c.y; ++y)
        for (int z = a.z; z <= c.z; ++z)
            for (int x = a.x; x <= c.x; ++x) {
                uint8_t id = self(L)->get(x, y, z);
                lua_pushinteger(L, idx++);
                lua_pushstring(L, Terrain::enumMaterialName(id));
                lua_settable(L, -3);
            }
    lua_setfield(L, -2, "materials");
    return 1;
}

inline int writeVoxels(lua_State* L) {
    int b = argBase(L);
    glm::vec3 mn{}, mx{};
    if (lua_istable(L, b)) {
        lua_getfield(L, b, "Min");
        if (lua_istable(L, -1)) readVec(L, lua_gettop(L), mn);
        lua_pop(L, 1);
        lua_getfield(L, b, "Max");
        if (lua_istable(L, -1)) readVec(L, lua_gettop(L), mx);
        lua_pop(L, 1);
    }
    auto a = self(L)->worldToCell(mn);
    auto c = self(L)->worldToCell(mx);
    if (a.x > c.x) std::swap(a.x, c.x);
    if (a.y > c.y) std::swap(a.y, c.y);
    if (a.z > c.z) std::swap(a.z, c.z);
    int matIdx = b + 2;
    if (!lua_istable(L, matIdx)) return 0;
    lua_pushvalue(L, matIdx);
    int idx = 1;
    int n = 0;
    for (int y = a.y; y <= c.y; ++y)
        for (int z = a.z; z <= c.z; ++z)
            for (int x = a.x; x <= c.x; ++x) {
                lua_rawgeti(L, -1, idx++);
                uint8_t id = Terrain::Air;
                if (lua_isstring(L, -1)) id = Terrain::materialFromEnum(lua_tostring(L, -1));
                else if (lua_isnumber(L, -1)) id = (uint8_t)lua_tointeger(L, -1);
                lua_pop(L, 1);
                if (self(L)->set(x, y, z, id)) ++n;
            }
    lua_pop(L, 1);
    lua_pushinteger(L, n);
    return 1;
}

inline void attachToWorkspace(lua_State* L, void* ctx) {
    lua_getglobal(L, "workspace");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_newtable(L);
    lua_pushstring(L, "Terrain");
    lua_setfield(L, -2, "ClassName");
    addFn(L, ctx, clear, "Clear", "Terrain.Clear");
    addFn(L, ctx, worldToCell, "WorldToCell", "Terrain.WorldToCell");
    addFn(L, ctx, cellCenterToWorld, "CellCenterToWorld", "Terrain.CellCenterToWorld");
    addFn(L, ctx, fillBlock, "FillBlock", "Terrain.FillBlock");
    addFn(L, ctx, fillBall, "FillBall", "Terrain.FillBall");
    addFn(L, ctx, readVoxels, "ReadVoxels", "Terrain.ReadVoxels");
    addFn(L, ctx, writeVoxels, "WriteVoxels", "Terrain.WriteVoxels");
    lua_setfield(L, -2, "Terrain");
    lua_pop(L, 1);
}

inline void registerLua(lua_State* L, void* ctx) {
    attachToWorkspace(L, ctx);
}

} // namespace terrainlua
