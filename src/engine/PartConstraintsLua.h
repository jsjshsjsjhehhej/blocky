#pragma once

// API Lua dei vincoli: la tabella globale "Constraints". Le part si passano
// come oggetti (workspace:FindFirstChild, script.Parent...) o per nome.
//
//   local w = Constraints.Weld(a, b)                -- fisse fra loro
//   Constraints.Hinge(telaio, porta)                -- cerniera verticale a meta' strada
//   Constraints.Hinge(a, b, Vector3.new(1, 0, 0))   -- asse orizzontale
//   Constraints.Rope("Soffitto", "Lampadario", 8)   -- corda lunga 8 stud
//   Constraints.Spring(a, b, 4, 0.2)                -- molla: rigidita' 4 Hz, smorzamento 0.2
//   Constraints.Rod(a, b)                           -- asta rigida, lunga com'e' adesso
//   Constraints.SetEnabled(w, false)   Constraints.Remove(w)   Constraints.Count()
//
// Ogni funzione che crea ritorna il nome unico del vincolo: e' quello che si
// passa a SetEnabled/Remove. Vincoli creati in partita li applica la fisica
// dal frame dopo.

#include "PartConstraints.h"

#include "lua.h"
#include "lualib.h"

#include <cstring>

namespace partconstraintslua {

inline DataModel* world(lua_State* L) {
    return static_cast<DataModel*>(lua_touserdata(L, lua_upvalueindex(1)));
}

// Una part: oggetto Lua del motore (ha __partIndex) oppure il suo nome.
inline int checkPart(lua_State* L, int idx) {
    DataModel& dm = *world(L);
    if (lua_istable(L, idx)) {
        lua_getfield(L, idx, "__partIndex");
        const int i = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : -1;
        lua_pop(L, 1);
        if (partconstraints::validPart(dm, i)) return i;
    } else if (lua_isstring(L, idx)) {
        const char* name = lua_tostring(L, idx);
        for (int i = 0; i < (int)dm.parts.size(); ++i)
            if (dm.parts[(size_t)i].name == name) return i;
        luaL_error(L, "Constraints: nessuna part chiamata '%s'", name);
    }
    luaL_error(L, "Constraints: argomento %d deve essere una part o il nome di una part", idx);
    return -1;
}

inline WeldConstraint* checkConstraint(lua_State* L, int idx) {
    const char* name = luaL_checkstring(L, idx);
    for (auto& c : world(L)->welds)
        if (c.name == name) return &c;
    luaL_error(L, "Constraints: nessun vincolo chiamato '%s'", name);
    return nullptr;
}

inline int make(lua_State* L, ConstraintKind kind) {
    DataModel& dm = *world(L);
    const int a = checkPart(L, 1), b = checkPart(L, 2);
    if (a == b) luaL_error(L, "Constraints: le due part devono essere diverse");
    const int k = partconstraints::create(dm, kind, a, b);
    if (k < 0) luaL_error(L, "Constraints: impossibile creare il vincolo");
    WeldConstraint& c = dm.welds[(size_t)k];
    switch (kind) {
    case ConstraintKind::Rope:
    case ConstraintKind::Rod:
        if (lua_isnumber(L, 3)) c.length = std::max(0.f, (float)lua_tonumber(L, 3));
        break;
    case ConstraintKind::Spring:
        if (lua_isnumber(L, 3)) c.stiffness = std::clamp((float)lua_tonumber(L, 3), 0.1f, 20.f);
        if (lua_isnumber(L, 4)) c.damping = std::clamp((float)lua_tonumber(L, 4), 0.f, 1.f);
        if (lua_isnumber(L, 5)) c.length = std::max(0.f, (float)lua_tonumber(L, 5));
        break;
    case ConstraintKind::Hinge:
        if (lua_istable(L, 3)) {
            // Asse dato in coordinate del mondo: si porta nello spazio di Part0.
            glm::vec3 axis(0.f);
            const char* keys[3] = {"X", "Y", "Z"};
            for (int i = 0; i < 3; ++i) {
                lua_getfield(L, 3, keys[i]);
                axis[i] = (float)luaL_optnumber(L, -1, 0.0);
                lua_pop(L, 1);
            }
            if (glm::length(axis) > 1e-4f) {
                const glm::mat3 r0(partRotationMatrix(dm.parts[(size_t)a].rotation));
                c.axis = glm::normalize(glm::transpose(r0) * axis);
            }
        }
        break;
    case ConstraintKind::Weld:
    case ConstraintKind::BallSocket:
    case ConstraintKind::Prismatic:
    case ConstraintKind::Cylindrical:
    case ConstraintKind::NoCollision:
    case ConstraintKind::AlignPosition:
    case ConstraintKind::AlignOrientation:
    case ConstraintKind::LinearVelocity:
    case ConstraintKind::AngularVelocity:
    case ConstraintKind::VectorForce:
    case ConstraintKind::Torque:
    case ConstraintKind::Count:
        break;
    }
    lua_pushstring(L, c.name.c_str());
    return 1;
}

inline int weld(lua_State* L) { return make(L, ConstraintKind::Weld); }
inline int hinge(lua_State* L) { return make(L, ConstraintKind::Hinge); }
inline int rope(lua_State* L) { return make(L, ConstraintKind::Rope); }
inline int spring(lua_State* L) { return make(L, ConstraintKind::Spring); }
inline int rod(lua_State* L) { return make(L, ConstraintKind::Rod); }
inline int ball(lua_State* L) { return make(L, ConstraintKind::BallSocket); }
inline int prismatic(lua_State* L) { return make(L, ConstraintKind::Prismatic); }
inline int cylindrical(lua_State* L) { return make(L, ConstraintKind::Cylindrical); }
inline int noCollision(lua_State* L) { return make(L, ConstraintKind::NoCollision); }
inline int alignPos(lua_State* L) { return make(L, ConstraintKind::AlignPosition); }
inline int alignOri(lua_State* L) { return make(L, ConstraintKind::AlignOrientation); }
inline int linVel(lua_State* L) { return make(L, ConstraintKind::LinearVelocity); }
inline int angVel(lua_State* L) { return make(L, ConstraintKind::AngularVelocity); }
inline int vecForce(lua_State* L) { return make(L, ConstraintKind::VectorForce); }
inline int torque(lua_State* L) { return make(L, ConstraintKind::Torque); }

inline int setEnabled(lua_State* L) {
    WeldConstraint* c = checkConstraint(L, 1);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    c->enabled = lua_toboolean(L, 2) != 0;
    return 0;
}

inline int remove(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    auto& ws = world(L)->welds;
    for (size_t k = 0; k < ws.size(); ++k)
        if (ws[k].name == name) {
            ws.erase(ws.begin() + (long)k);
            lua_pushboolean(L, 1);
            return 1;
        }
    lua_pushboolean(L, 0);
    return 1;
}

inline int count(lua_State* L) {
    lua_pushinteger(L, (int)world(L)->welds.size());
    return 1;
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* debugName) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, debugName, 1);
    lua_setfield(L, -2, field);
}

// ctx = il DataModel del gioco.
inline void registerLua(lua_State* L, void* ctx) {
    lua_newtable(L);
    addFn(L, ctx, weld, "Weld", "Constraints.Weld");
    addFn(L, ctx, hinge, "Hinge", "Constraints.Hinge");
    addFn(L, ctx, rope, "Rope", "Constraints.Rope");
    addFn(L, ctx, spring, "Spring", "Constraints.Spring");
    addFn(L, ctx, rod, "Rod", "Constraints.Rod");
    addFn(L, ctx, ball, "BallSocket", "Constraints.BallSocket");
    addFn(L, ctx, prismatic, "Prismatic", "Constraints.Prismatic");
    addFn(L, ctx, cylindrical, "Cylindrical", "Constraints.Cylindrical");
    addFn(L, ctx, noCollision, "NoCollision", "Constraints.NoCollision");
    addFn(L, ctx, alignPos, "AlignPosition", "Constraints.AlignPosition");
    addFn(L, ctx, alignOri, "AlignOrientation", "Constraints.AlignOrientation");
    addFn(L, ctx, linVel, "LinearVelocity", "Constraints.LinearVelocity");
    addFn(L, ctx, angVel, "AngularVelocity", "Constraints.AngularVelocity");
    addFn(L, ctx, vecForce, "VectorForce", "Constraints.VectorForce");
    addFn(L, ctx, torque, "Torque", "Constraints.Torque");
    addFn(L, ctx, setEnabled, "SetEnabled", "Constraints.SetEnabled");
    addFn(L, ctx, remove, "Remove", "Constraints.Remove");
    addFn(L, ctx, count, "Count", "Constraints.Count");
    lua_setglobal(L, "Constraints");
}

} // namespace partconstraintslua
