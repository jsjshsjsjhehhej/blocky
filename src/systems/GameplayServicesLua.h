#pragma once

/**
 * English / Roblox-style service facades for real game creation.
 * Wraps Hitbox + Proiettili + Salute + Inventario + Mobs + Missioni
 * so creators use game:GetService("CombatService") etc.
 */

#include "systems/Hitbox.h"
#include "systems/Proiettili.h"
#include "systems/Salute.h"
#include "systems/Mobs.h"

#include "lua.h"
#include "lualib.h"

#include <cstring>
#include <map>
#include <string>

/** Place currencies (Coins, Gems, …) — fully scriptable. */
struct Currency {
    std::map<std::string, int> bal;
    void reset() { bal.clear(); }
    void update(float) {}
    int get(const std::string& id) const {
        auto it = bal.find(id);
        return it == bal.end() ? 0 : it->second;
    }
    int add(const std::string& id, int n) {
        if (id.empty() || n == 0) return get(id);
        int& v = bal[id];
        v += n;
        if (v < 0) v = 0;
        if (v > 999999999) v = 999999999;
        return v;
    }
    bool spend(const std::string& id, int n) {
        if (n <= 0) return true;
        if (get(id) < n) return false;
        bal[id] -= n;
        return true;
    }
};

namespace gameplayservices {

struct CombatBundle {
    Hitbox* hitbox = nullptr;
    Proiettili* shots = nullptr;
    Salute* salute = nullptr;
    Mobs* mobs = nullptr;
};

inline CombatBundle* combatSelf(lua_State* L) {
    return static_cast<CombatBundle*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline void addFn(lua_State* L, void* ctx, lua_CFunction fn, const char* field, const char* dn) {
    lua_pushlightuserdata(L, ctx);
    lua_pushcclosure(L, fn, dn, 1);
    lua_setfield(L, -2, field);
}

inline int combatHitscan(lua_State* L) {
    auto* b = combatSelf(L);
    if (!b || !b->hitbox) { lua_pushnil(L); return 1; }
    int a = lua_istable(L, 1) ? 2 : 1;
    auto h = b->hitbox->ray(
        (float)luaL_checknumber(L, a), (float)luaL_checknumber(L, a + 1), (float)luaL_checknumber(L, a + 2),
        (float)luaL_checknumber(L, a + 3), (float)luaL_checknumber(L, a + 4), (float)luaL_checknumber(L, a + 5),
        (float)luaL_optnumber(L, a + 6, 200), Hitbox::parseGroup(luaL_optstring(L, a + 7, "world")));
    if (!h.ok) { lua_pushnil(L); return 1; }
    lua_newtable(L);
    lua_pushstring(L, h.name.c_str()); lua_setfield(L, -2, "Name");
    lua_pushnumber(L, h.t); lua_setfield(L, -2, "Distance");
    lua_pushnumber(L, h.x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, h.y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, h.z); lua_setfield(L, -2, "z");
    return 1;
}

inline int combatShoot(lua_State* L) {
    auto* b = combatSelf(L);
    if (!b || !b->shots) { lua_pushboolean(L, 0); return 1; }
    int a = lua_istable(L, 1) ? 2 : 1;
    // Shoot(name, x,y,z, vx,vy,vz [, life, radius])
    lua_pushboolean(L, b->shots->spawn(
        luaL_optstring(L, a, "bullet"),
        (float)luaL_checknumber(L, a + 1), (float)luaL_checknumber(L, a + 2), (float)luaL_checknumber(L, a + 3),
        (float)luaL_checknumber(L, a + 4), (float)luaL_checknumber(L, a + 5), (float)luaL_checknumber(L, a + 6),
        (float)luaL_optnumber(L, a + 7, 1.5), (float)luaL_optnumber(L, a + 8, 0.25)) ? 1 : 0);
    return 1;
}

inline int combatDamage(lua_State* L) {
    auto* b = combatSelf(L);
    int a = lua_istable(L, 1) ? 2 : 1;
    const char* target = luaL_checkstring(L, a);
    float dmg = (float)luaL_checknumber(L, a + 1);
    bool ok = false;
    if (b && b->salute && target && std::strcmp(target, "Player") != 0)
        ok = b->salute->damage(target, dmg);
    if (b && b->mobs && !ok)
        ok = b->mobs->hurt(target, dmg);
    // Player damage via global Humanoid.TakeDamage if name is Player / LocalPlayer
    if (!ok && target && (std::strcmp(target, "Player") == 0 || std::strcmp(target, "LocalPlayer") == 0)) {
        lua_getglobal(L, "Humanoid");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "TakeDamage");
            if (lua_isfunction(L, -1)) {
                lua_pushnumber(L, dmg);
                if (lua_pcall(L, 1, 0, 0) == LUA_OK) ok = true;
                else lua_pop(L, 1);
            } else lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

inline int combatRegisterHitbox(lua_State* L) {
    auto* b = combatSelf(L);
    if (!b || !b->hitbox) { lua_pushboolean(L, 0); return 1; }
    int a = lua_istable(L, 1) ? 2 : 1;
    // RegisterHitbox(name, x,y,z, sx,sy,sz [, group])
    lua_pushboolean(L, b->hitbox->addBox(
        luaL_checkstring(L, a),
        (float)luaL_checknumber(L, a + 1), (float)luaL_checknumber(L, a + 2), (float)luaL_checknumber(L, a + 3),
        (float)luaL_optnumber(L, a + 4, 2), (float)luaL_optnumber(L, a + 5, 2), (float)luaL_optnumber(L, a + 6, 2),
        0.f, Hitbox::parseGroup(luaL_optstring(L, a + 7, "world"))) ? 1 : 0);
    return 1;
}

inline int combatSpawnMob(lua_State* L) {
    auto* b = combatSelf(L);
    if (!b || !b->mobs) { lua_pushboolean(L, 0); return 1; }
    int a = lua_istable(L, 1) ? 2 : 1;
    lua_pushboolean(L, b->mobs->spawn(luaL_checkstring(L, a),
        (float)luaL_checknumber(L, a + 1), (float)luaL_checknumber(L, a + 2),
        (float)luaL_checknumber(L, a + 3)) ? 1 : 0);
    return 1;
}

inline void registerCombat(lua_State* L, CombatBundle* bundle) {
    lua_newtable(L);
    addFn(L, bundle, combatHitscan, "Hitscan", "CombatService.Hitscan");
    addFn(L, bundle, combatShoot, "Shoot", "CombatService.Shoot");
    addFn(L, bundle, combatDamage, "Damage", "CombatService.Damage");
    addFn(L, bundle, combatRegisterHitbox, "RegisterHitbox", "CombatService.RegisterHitbox");
    addFn(L, bundle, combatSpawnMob, "SpawnMob", "CombatService.SpawnMob");
    lua_setglobal(L, "CombatService");
    lua_getglobal(L, "CombatService");
    lua_setglobal(L, "Combat");
}

inline Currency* currencySelf(lua_State* L) {
    return static_cast<Currency*>(lua_touserdata(L, lua_upvalueindex(1)));
}

inline int curGet(lua_State* L) {
    int a = lua_istable(L, 1) ? 2 : 1;
    lua_pushinteger(L, currencySelf(L)->get(luaL_optstring(L, a, "Coins")));
    return 1;
}
inline int curAdd(lua_State* L) {
    int a = lua_istable(L, 1) ? 2 : 1;
    lua_pushinteger(L, currencySelf(L)->add(luaL_optstring(L, a, "Coins"), (int)luaL_checkinteger(L, a + 1)));
    return 1;
}
inline int curSpend(lua_State* L) {
    int a = lua_istable(L, 1) ? 2 : 1;
    lua_pushboolean(L, currencySelf(L)->spend(luaL_optstring(L, a, "Coins"), (int)luaL_checkinteger(L, a + 1)) ? 1 : 0);
    return 1;
}

inline void registerCurrency(lua_State* L, Currency* c) {
    lua_newtable(L);
    addFn(L, c, curGet, "Get", "CurrencyService.Get");
    addFn(L, c, curAdd, "Add", "CurrencyService.Add");
    addFn(L, c, curSpend, "Spend", "CurrencyService.Spend");
    lua_setglobal(L, "CurrencyService");
    lua_getglobal(L, "CurrencyService");
    lua_setglobal(L, "Currency");
}

/** Alias globals so GetService / game.X find English names. */
inline void registerAliases(lua_State* L) {
    auto alias = [&](const char* from, const char* to) {
        lua_getglobal(L, from);
        if (!lua_isnil(L, -1))
            lua_setglobal(L, to);
        else
            lua_pop(L, 1);
    };
    alias("Inventario", "InventoryService");
    alias("Inventario", "Inventory");
    alias("Mobs", "NpcService");
    alias("Mobs", "Npc");
    alias("Missioni", "QuestService");
    alias("Missioni", "Quests");
    alias("Hitbox", "PhysicsHitbox");
    alias("Proiettili", "ProjectileService");
    alias("Salute", "HealthService");
    alias("Veicoli", "VehicleService");
    alias("Teams", "TeamService");
    alias("Zone", "ZoneService");
    alias("Dialoghi", "DialogueService");
    alias("Crafting", "CraftingService");
    alias("Stamina", "StaminaService");
    alias("CombatService", "CombatService"); // ensure present
    alias("Prossimita", "ProximityPromptService");
    alias("Prossimita", "ProximityPrompt");
    alias("Classifica", "LeaderboardService");
    alias("Classifica", "Leaderstats");
}

} // namespace gameplayservices
