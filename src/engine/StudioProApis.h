#pragma once

/**
 * Studio Pro Luau APIs — HttpService JSON, ContextAction, Teleport,
 * Marketplace, Enum, Color3/UDim2/Rect/NumberRange helpers.
 * Registrato da LuaRuntime::registerApi via studio_pro_apis::registerApis.
 */

#include "ScriptControls.h"

#include "lua.h"
#include "lualib.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace studio_pro_apis {

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

inline std::map<std::string, std::string>& attributes() {
    static std::map<std::string, std::string> m;
    return m;
}

inline std::map<std::string, bool>& contextBound() {
    static std::map<std::string, bool> m;
    return m;
}

struct ContextAction {
    std::string name;
    int callbackRef = -1; // LUA_NOREF
    int priority = 2000;
    bool createTouchButton = false;
    std::vector<std::string> keys;
};
inline std::vector<ContextAction>& contextActions() {
    static std::vector<ContextAction> v;
    return v;
}

inline std::vector<int>& teleportFailedRefs() {
    static std::vector<int> v;
    return v;
}

inline std::map<std::string, int>& processReceiptHooks() {
    static std::map<std::string, int> m;
    return m;
}

inline double& sessionStart() {
    static double t = (double)std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return t;
}

inline void pushC(lua_State* L, lua_CFunction fn, const char* name) {
    lua_pushcfunction(L, fn, name);
}

// ---------------------------------------------------------------------------
// JSON — recursive encode / decode
// ---------------------------------------------------------------------------

inline void jsonEscapeAppend(std::string& o, const char* s) {
    o.push_back('"');
    if (!s) { o.push_back('"'); return; }
    for (const char* p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { o.push_back('\\'); o.push_back((char)c); }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            o += buf;
        } else o.push_back((char)c);
    }
    o.push_back('"');
}

inline void jsonEncodeValue(lua_State* L, int idx, std::string& o, int depth);

inline int absIndex(lua_State* L, int idx) {
    return (idx > 0 || idx <= LUA_REGISTRYINDEX) ? idx : lua_gettop(L) + idx + 1;
}

inline bool tableIsArray(lua_State* L, int idx) {
    size_t n = lua_objlen(L, idx);
    if (n == 0) {
        lua_pushnil(L);
        if (lua_next(L, idx) != 0) { lua_pop(L, 2); return false; }
        return true; // empty → array
    }
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (!lua_isnumber(L, -2)) { lua_pop(L, 2); return false; }
        double k = lua_tonumber(L, -2);
        if (k != (double)(int)k || k < 1.0 || k > (double)n + 0.1) {
            lua_pop(L, 2);
            return false;
        }
        lua_pop(L, 1);
    }
    return true;
}

inline void jsonEncodeValue(lua_State* L, int idx, std::string& o, int depth) {
    if (depth > 32) { o += "null"; return; }
    idx = absIndex(L, idx);
    int t = lua_type(L, idx);
    if (t == LUA_TNIL) { o += "null"; return; }
    if (t == LUA_TBOOLEAN) { o += lua_toboolean(L, idx) ? "true" : "false"; return; }
    if (t == LUA_TNUMBER) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.14g", (double)lua_tonumber(L, idx));
        o += buf;
        return;
    }
    if (t == LUA_TSTRING) {
        jsonEscapeAppend(o, lua_tostring(L, idx));
        return;
    }
    if (t != LUA_TTABLE) { o += "null"; return; }
    if (tableIsArray(L, idx)) {
        o += "[";
        size_t n = lua_objlen(L, idx);
        for (size_t i = 1; i <= n; ++i) {
            if (i > 1) o += ",";
            lua_rawgeti(L, idx, (int)i);
            jsonEncodeValue(L, -1, o, depth + 1);
            lua_pop(L, 1);
        }
        o += "]";
        return;
    }
    o += "{";
    bool first = true;
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        if (!first) o += ",";
        first = false;
        if (lua_isstring(L, -2)) {
            jsonEscapeAppend(o, lua_tostring(L, -2));
        } else if (lua_isnumber(L, -2)) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "\"%.14g\"", (double)lua_tonumber(L, -2));
            o += buf;
        } else {
            jsonEscapeAppend(o, "?");
        }
        o += ":";
        jsonEncodeValue(L, -1, o, depth + 1);
        lua_pop(L, 1);
    }
    o += "}";
}

inline int luaJsonEncode(lua_State* L) {
    std::string o;
    jsonEncodeValue(L, 1, o, 0);
    lua_pushlstring(L, o.data(), o.size());
    return 1;
}

inline void skipWs(const char*& p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
}

inline bool jsonParseValue(lua_State* L, const char*& p, int depth);

inline bool jsonParseString(lua_State* L, const char*& p) {
    if (*p != '"') return false;
    ++p;
    std::string s;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            ++p;
            if (*p == 'n') s.push_back('\n');
            else if (*p == 'r') s.push_back('\r');
            else if (*p == 't') s.push_back('\t');
            else if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) {
                // skip unicode escape — push raw nibble chars lightly
                p += 4;
                s.push_back('?');
            } else s.push_back(*p);
            ++p;
        } else {
            s.push_back(*p++);
        }
    }
    if (*p == '"') ++p;
    lua_pushlstring(L, s.data(), s.size());
    return true;
}

inline bool jsonParseNumber(lua_State* L, const char*& p) {
    char* end = nullptr;
    double n = std::strtod(p, &end);
    if (end == p) return false;
    p = end;
    lua_pushnumber(L, n);
    return true;
}

inline bool jsonParseObject(lua_State* L, const char*& p, int depth) {
    if (*p != '{') return false;
    ++p;
    lua_newtable(L);
    skipWs(p);
    if (*p == '}') { ++p; return true; }
    while (*p) {
        skipWs(p);
        if (!jsonParseString(L, p)) { lua_pop(L, 1); return false; }
        skipWs(p);
        if (*p != ':') { lua_pop(L, 2); return false; }
        ++p;
        skipWs(p);
        if (!jsonParseValue(L, p, depth + 1)) { lua_pop(L, 2); return false; }
        lua_settable(L, -3);
        skipWs(p);
        if (*p == ',') { ++p; continue; }
        if (*p == '}') { ++p; break; }
        lua_pop(L, 1);
        return false;
    }
    return true;
}

inline bool jsonParseArray(lua_State* L, const char*& p, int depth) {
    if (*p != '[') return false;
    ++p;
    lua_newtable(L);
    skipWs(p);
    if (*p == ']') { ++p; return true; }
    int i = 1;
    while (*p) {
        skipWs(p);
        if (!jsonParseValue(L, p, depth + 1)) { lua_pop(L, 1); return false; }
        lua_rawseti(L, -2, i++);
        skipWs(p);
        if (*p == ',') { ++p; continue; }
        if (*p == ']') { ++p; break; }
        lua_pop(L, 1);
        return false;
    }
    return true;
}

inline bool jsonParseValue(lua_State* L, const char*& p, int depth) {
    if (depth > 32) { lua_pushnil(L); return true; }
    skipWs(p);
    if (*p == 'n' && std::strncmp(p, "null", 4) == 0) { p += 4; lua_pushnil(L); return true; }
    if (*p == 't' && std::strncmp(p, "true", 4) == 0) { p += 4; lua_pushboolean(L, 1); return true; }
    if (*p == 'f' && std::strncmp(p, "false", 5) == 0) { p += 5; lua_pushboolean(L, 0); return true; }
    if (*p == '"') return jsonParseString(L, p);
    if (*p == '{') return jsonParseObject(L, p, depth);
    if (*p == '[') return jsonParseArray(L, p, depth);
    if (*p == '-' || (*p >= '0' && *p <= '9')) return jsonParseNumber(L, p);
    return false;
}

inline int luaJsonDecode(lua_State* L) {
    const char* s = luaL_optstring(L, 1, "");
    const char* p = s ? s : "";
    skipWs(p);
    if (!jsonParseValue(L, p, 0)) {
        lua_newtable(L);
    }
    return 1;
}

inline int luaUrlEncode(lua_State* L) {
    const char* s = luaL_optstring(L, 1, "");
    std::string o;
    for (const char* p = s; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            o.push_back((char)c);
        } else if (c == ' ') {
            o.push_back('+');
        } else {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            o += buf;
        }
    }
    lua_pushlstring(L, o.data(), o.size());
    return 1;
}

inline int luaUrlDecode(lua_State* L) {
    const char* s = luaL_optstring(L, 1, "");
    std::string o;
    for (const char* p = s; *p; ++p) {
        if (*p == '+') o.push_back(' ');
        else if (*p == '%' && p[1] && p[2]) {
            char hex[3] = { p[1], p[2], 0 };
            o.push_back((char)std::strtol(hex, nullptr, 16));
            p += 2;
        } else o.push_back(*p);
    }
    lua_pushlstring(L, o.data(), o.size());
    return 1;
}

inline int luaGenerateGUID(lua_State* L) {
    bool braces = true;
    if (lua_isboolean(L, 1)) braces = lua_toboolean(L, 1) != 0;
    else if (lua_istable(L, 1) && lua_isboolean(L, 2)) braces = lua_toboolean(L, 2) != 0;
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> d(0, 15);
    char hex[] = "0123456789abcdef";
    char g[48];
    int n = 0;
    if (braces) g[n++] = '{';
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) g[n++] = '-';
        g[n++] = hex[d(rng)];
    }
    if (braces) g[n++] = '}';
    g[n] = 0;
    lua_pushlstring(L, g, (size_t)n);
    return 1;
}

inline int luaHttpGetAsync(lua_State* L) {
    luaL_error(L, "HttpService:GetAsync disabilitato in sandbox (usa JSONEncode/Decode)");
    return 0;
}

inline int luaHttpPostAsync(lua_State* L) {
    luaL_error(L, "HttpService:PostAsync disabilitato in sandbox");
    return 0;
}

inline int luaHttpRequestAsync(lua_State* L) {
    luaL_error(L, "HttpService:RequestAsync disabilitato in sandbox");
    return 0;
}

// ---------------------------------------------------------------------------
// ContextActionService
// ---------------------------------------------------------------------------

inline int luaContextBind(lua_State* L) {
    // BindAction(name, function, createTouchButton, ...keys) OR
    // ContextActionService:BindAction(name, fn, touch, ...)
    int base = 1;
    if (lua_istable(L, 1) && lua_isstring(L, 2)) base = 2;
    const char* name = luaL_checkstring(L, base);
    int fnArg = base + 1;
    if (!lua_isfunction(L, fnArg))
        return blockyLuaError(L, "BindAction(name, function, createTouchButton?, keys...)");
    bool touch = lua_toboolean(L, base + 2) != 0;
    // Replace existing
    auto& acts = contextActions();
    for (auto it = acts.begin(); it != acts.end(); ++it) {
        if (it->name == name) {
            if (it->callbackRef != -1) lua_unref(L, it->callbackRef);
            acts.erase(it);
            break;
        }
    }
    ContextAction a;
    a.name = name;
    a.createTouchButton = touch;
    lua_pushvalue(L, fnArg);
    a.callbackRef = lua_ref(L, -1);
    lua_pop(L, 1);
    for (int i = base + 3; i <= lua_gettop(L); ++i) {
        if (lua_isstring(L, i)) a.keys.push_back(lua_tostring(L, i));
        else if (lua_isnumber(L, i)) a.keys.push_back(std::to_string((int)lua_tointeger(L, i)));
    }
    contextBound()[name] = true;
    acts.push_back(std::move(a));
    return 0;
}

inline int luaContextBindAtPriority(lua_State* L) {
    int base = 1;
    if (lua_istable(L, 1) && lua_isstring(L, 2)) base = 2;
    const char* name = luaL_checkstring(L, base);
    int priority = (int)luaL_optinteger(L, base + 1, 2000);
    int fnArg = base + 2;
    if (!lua_isfunction(L, fnArg))
        return blockyLuaError(L, "BindActionAtPriority(name, priority, function, ...)");
    bool touch = lua_toboolean(L, base + 3) != 0;
    auto& acts = contextActions();
    for (auto it = acts.begin(); it != acts.end(); ++it) {
        if (it->name == name) {
            if (it->callbackRef != -1) lua_unref(L, it->callbackRef);
            acts.erase(it);
            break;
        }
    }
    ContextAction a;
    a.name = name;
    a.priority = priority;
    a.createTouchButton = touch;
    lua_pushvalue(L, fnArg);
    a.callbackRef = lua_ref(L, -1);
    lua_pop(L, 1);
    for (int i = base + 4; i <= lua_gettop(L); ++i) {
        if (lua_isstring(L, i)) a.keys.push_back(lua_tostring(L, i));
    }
    contextBound()[name] = true;
    acts.push_back(std::move(a));
    std::sort(acts.begin(), acts.end(),
              [](const ContextAction& x, const ContextAction& y) {
                  return x.priority > y.priority;
              });
    return 0;
}

inline int luaContextUnbind(lua_State* L) {
    int base = 1;
    if (lua_istable(L, 1)) base = 2;
    const char* name = luaL_optstring(L, base, "");
    auto& acts = contextActions();
    if (!name[0]) {
        for (auto& a : acts)
            if (a.callbackRef != -1) lua_unref(L, a.callbackRef);
        acts.clear();
        contextBound().clear();
        return 0;
    }
    for (auto it = acts.begin(); it != acts.end(); ++it) {
        if (it->name == name) {
            if (it->callbackRef != -1) lua_unref(L, it->callbackRef);
            acts.erase(it);
            break;
        }
    }
    contextBound().erase(name);
    return 0;
}

inline int luaContextUnbindAll(lua_State* L) {
    auto& acts = contextActions();
    for (auto& a : acts)
        if (a.callbackRef != -1) lua_unref(L, a.callbackRef);
    acts.clear();
    contextBound().clear();
    return 0;
}

inline int luaContextGetBoundActionInfo(lua_State* L) {
    int base = 1;
    if (lua_istable(L, 1)) base = 2;
    const char* name = luaL_checkstring(L, base);
    for (const auto& a : contextActions()) {
        if (a.name == name) {
            lua_newtable(L);
            lua_pushstring(L, a.name.c_str());
            lua_setfield(L, -2, "name");
            lua_pushinteger(L, a.priority);
            lua_setfield(L, -2, "priorityLevel");
            lua_pushboolean(L, a.createTouchButton ? 1 : 0);
            lua_setfield(L, -2, "createTouchButton");
            lua_newtable(L);
            for (int i = 0; i < (int)a.keys.size(); ++i) {
                lua_pushstring(L, a.keys[i].c_str());
                lua_rawseti(L, -2, i + 1);
            }
            lua_setfield(L, -2, "inputTypes");
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

inline int luaContextGetAllBoundActionInfo(lua_State* L) {
    lua_newtable(L);
    for (const auto& a : contextActions()) {
        lua_newtable(L);
        lua_pushinteger(L, a.priority);
        lua_setfield(L, -2, "priorityLevel");
        lua_pushboolean(L, a.createTouchButton ? 1 : 0);
        lua_setfield(L, -2, "createTouchButton");
        lua_setfield(L, -2, a.name.c_str());
    }
    return 1;
}

// ---------------------------------------------------------------------------
// TeleportService
// ---------------------------------------------------------------------------

inline int luaTeleport(lua_State* L) {
    int base = 1;
    if (lua_istable(L, 1) && !lua_isstring(L, 1)) base = 2;
    const char* place = luaL_optstring(L, base, "");
    std::fprintf(stderr, "[TeleportService] Teleport locale stub place=%s\n", place);
    lua_pushboolean(L, place[0] ? 1 : 0);
    return 1;
}

inline int luaTeleportAsync(lua_State* L) {
    int base = 1;
    if (lua_istable(L, 1)) base = 2;
    const char* place = luaL_optstring(L, base, "");
    std::fprintf(stderr, "[TeleportService] TeleportAsync stub place=%s\n", place);
    lua_pushboolean(L, place[0] ? 1 : 0);
    return 1;
}

inline int luaTeleportToPlaceInstance(lua_State* L) {
    int base = 1;
    if (lua_istable(L, 1)) base = 2;
    const char* place = luaL_optstring(L, base, "");
    const char* inst = luaL_optstring(L, base + 1, "");
    std::fprintf(stderr, "[TeleportService] ToPlaceInstance %s / %s\n", place, inst);
    lua_pushboolean(L, place[0] ? 1 : 0);
    return 1;
}

inline int luaGetLocalPlayerTeleportData(lua_State* L) {
    lua_pushnil(L);
    return 1;
}

inline int luaTeleportInitFailedConnect(lua_State* L) {
    int fidx = 1;
    if (lua_istable(L, 1) && lua_isfunction(L, 2)) fidx = 2;
    if (!lua_isfunction(L, fidx))
        return blockyLuaError(L, "TeleportInitFailed:Connect(function)");
    lua_pushvalue(L, fidx);
    teleportFailedRefs().push_back(lua_ref(L, -1));
    lua_pop(L, 1);
    return 0;
}

inline int luaTeleportGetFailedSignal(lua_State* L) {
    lua_newtable(L);
    pushC(L, luaTeleportInitFailedConnect, "Connect");
    lua_setfield(L, -2, "Connect");
    return 1;
}

// ---------------------------------------------------------------------------
// MarketplaceService
// ---------------------------------------------------------------------------

inline int luaPromptProductPurchase(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    // player, productId
    int productId = (int)luaL_optinteger(L, base + 1, 0);
    std::fprintf(stderr, "[Marketplace] PromptProductPurchase id=%d (no-op locale)\n", productId);
    return 0;
}

inline int luaPromptGamePassPurchase(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    int passId = (int)luaL_optinteger(L, base + 1, 0);
    std::fprintf(stderr, "[Marketplace] PromptGamePassPurchase id=%d (no-op)\n", passId);
    return 0;
}

inline int luaPromptPremiumPurchase(lua_State* L) {
    (void)L;
    std::fprintf(stderr, "[Marketplace] PromptPremiumPurchase (no-op)\n");
    return 0;
}

inline int luaUserOwnsGamePassAsync(lua_State* L) {
    lua_pushboolean(L, 0);
    return 1;
}

inline int luaGetProductInfo(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    int id = (int)luaL_optinteger(L, base, 0);
    lua_newtable(L);
    lua_pushinteger(L, id);
    lua_setfield(L, -2, "ProductId");
    lua_pushstring(L, "BlockyProduct");
    lua_setfield(L, -2, "Name");
    lua_pushinteger(L, 0);
    lua_setfield(L, -2, "PriceInRobux");
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "IsForSale");
    return 1;
}

inline int luaProcessReceiptNewIndex(lua_State* L) {
    // MarketplaceService.ProcessReceipt = function(receiptInfo)
    if (!lua_isfunction(L, 3)) return 0;
    lua_pushvalue(L, 3);
    processReceiptHooks()["default"] = lua_ref(L, -1);
    lua_pop(L, 1);
    return 0;
}

inline int luaMarketplaceIndex(lua_State* L) {
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;
    lua_pushvalue(L, 2);
    lua_rawget(L, 1);
    if (!lua_isnil(L, -1)) return 1;
    lua_pop(L, 1);
    if (std::strcmp(key, "ProcessReceipt") == 0) {
        auto it = processReceiptHooks().find("default");
        if (it != processReceiptHooks().end() && it->second != -1) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, it->second);
            return 1;
        }
        lua_pushnil(L);
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Attributes
// ---------------------------------------------------------------------------

inline int partIndexFromArg(lua_State* L, int& keyArg) {
    int partIdx = -1;
    keyArg = 1;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "__partIndex");
        if (lua_isnumber(L, -1)) partIdx = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
        keyArg = 2;
    } else if (lua_isnumber(L, 1)) {
        partIdx = (int)lua_tointeger(L, 1);
        keyArg = 2;
    }
    return partIdx;
}

inline int luaSetAttribute(lua_State* L) {
    int keyArg = 1;
    int partIdx = partIndexFromArg(L, keyArg);
    const char* key = luaL_checkstring(L, keyArg);
    std::string val;
    if (lua_isnumber(L, keyArg + 1)) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.14g", (double)lua_tonumber(L, keyArg + 1));
        val = buf;
    } else if (lua_isboolean(L, keyArg + 1)) {
        val = lua_toboolean(L, keyArg + 1) ? "true" : "false";
    } else {
        val = luaL_optstring(L, keyArg + 1, "");
    }
    if (partIdx < 0) return 0;
    attributes()[std::to_string(partIdx) + ":" + key] = val;
    return 0;
}

inline int luaGetAttribute(lua_State* L) {
    int keyArg = 1;
    int partIdx = partIndexFromArg(L, keyArg);
    const char* key = luaL_checkstring(L, keyArg);
    if (partIdx < 0) { lua_pushnil(L); return 1; }
    auto it = attributes().find(std::to_string(partIdx) + ":" + key);
    if (it == attributes().end()) { lua_pushnil(L); return 1; }
    char* end = nullptr;
    double n = std::strtod(it->second.c_str(), &end);
    if (end && *end == 0 && !it->second.empty())
        lua_pushnumber(L, n);
    else if (it->second == "true" || it->second == "false")
        lua_pushboolean(L, it->second == "true" ? 1 : 0);
    else
        lua_pushstring(L, it->second.c_str());
    return 1;
}

inline int luaGetAttributes(lua_State* L) {
    int keyArg = 1;
    int partIdx = partIndexFromArg(L, keyArg);
    lua_newtable(L);
    if (partIdx < 0) return 1;
    std::string prefix = std::to_string(partIdx) + ":";
    for (const auto& kv : attributes()) {
        if (kv.first.compare(0, prefix.size(), prefix) != 0) continue;
        std::string k = kv.first.substr(prefix.size());
        lua_pushstring(L, kv.second.c_str());
        lua_setfield(L, -2, k.c_str());
    }
    return 1;
}

inline int luaRemoveAttribute(lua_State* L) {
    int keyArg = 1;
    int partIdx = partIndexFromArg(L, keyArg);
    const char* key = luaL_checkstring(L, keyArg);
    if (partIdx < 0) return 0;
    attributes().erase(std::to_string(partIdx) + ":" + key);
    return 0;
}

// ---------------------------------------------------------------------------
// Color3 / UDim / UDim2 / Rect / NumberRange / NumberSequence
// ---------------------------------------------------------------------------

inline void pushColor3Table(lua_State* L, double r, double g, double b) {
    lua_newtable(L);
    lua_pushnumber(L, r); lua_setfield(L, -2, "R");
    lua_pushnumber(L, g); lua_setfield(L, -2, "G");
    lua_pushnumber(L, b); lua_setfield(L, -2, "B");
    lua_pushnumber(L, r); lua_setfield(L, -2, "r");
    lua_pushnumber(L, g); lua_setfield(L, -2, "g");
    lua_pushnumber(L, b); lua_setfield(L, -2, "b");
    pushC(L, [](lua_State* ls) -> int {
        lua_getfield(ls, 1, "R"); double r = lua_tonumber(ls, -1); lua_pop(ls, 1);
        lua_getfield(ls, 1, "G"); double g = lua_tonumber(ls, -1); lua_pop(ls, 1);
        lua_getfield(ls, 1, "B"); double b = lua_tonumber(ls, -1); lua_pop(ls, 1);
        lua_getfield(ls, 2, "R"); double r2 = lua_tonumber(ls, -1); lua_pop(ls, 1);
        lua_getfield(ls, 2, "G"); double g2 = lua_tonumber(ls, -1); lua_pop(ls, 1);
        lua_getfield(ls, 2, "B"); double b2 = lua_tonumber(ls, -1); lua_pop(ls, 1);
        double t = luaL_optnumber(ls, 3, 0.5);
        pushColor3Table(ls, r + (r2 - r) * t, g + (g2 - g) * t, b + (b2 - b) * t);
        return 1;
    }, "Lerp");
    lua_setfield(L, -2, "Lerp");
    pushC(L, [](lua_State* ls) -> int {
        lua_getfield(ls, 1, "R"); int r = (int)(lua_tonumber(ls, -1) * 255.0 + 0.5); lua_pop(ls, 1);
        lua_getfield(ls, 1, "G"); int g = (int)(lua_tonumber(ls, -1) * 255.0 + 0.5); lua_pop(ls, 1);
        lua_getfield(ls, 1, "B"); int b = (int)(lua_tonumber(ls, -1) * 255.0 + 0.5); lua_pop(ls, 1);
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02X%02X%02X",
                      std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
        lua_pushstring(ls, buf);
        return 1;
    }, "ToHex");
    lua_setfield(L, -2, "ToHex");
}

inline int luaColor3New(lua_State* L) {
    double r = luaL_optnumber(L, 1, 0);
    double g = luaL_optnumber(L, 2, 0);
    double b = luaL_optnumber(L, 3, 0);
    // Color3.new as method: Color3.new(r,g,b) or Color3:new — if table first, shift
    if (lua_istable(L, 1) && lua_gettop(L) >= 4) {
        r = luaL_optnumber(L, 2, 0);
        g = luaL_optnumber(L, 3, 0);
        b = luaL_optnumber(L, 4, 0);
    }
    pushColor3Table(L, r, g, b);
    return 1;
}

inline int luaColor3FromRGB(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    double r = luaL_optnumber(L, base, 0) / 255.0;
    double g = luaL_optnumber(L, base + 1, 0) / 255.0;
    double b = luaL_optnumber(L, base + 2, 0) / 255.0;
    pushColor3Table(L, r, g, b);
    return 1;
}

inline int luaColor3FromHSV(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    double h = luaL_optnumber(L, base, 0);
    double s = luaL_optnumber(L, base + 1, 0);
    double v = luaL_optnumber(L, base + 2, 0);
    double c = v * s;
    double x = c * (1.0 - std::fabs(std::fmod(h * 6.0, 2.0) - 1.0));
    double m = v - c;
    double r = 0, g = 0, b = 0;
    double hh = std::fmod(h, 1.0) * 6.0;
    if (hh < 1) { r = c; g = x; }
    else if (hh < 2) { r = x; g = c; }
    else if (hh < 3) { g = c; b = x; }
    else if (hh < 4) { g = x; b = c; }
    else if (hh < 5) { r = x; b = c; }
    else { r = c; b = x; }
    pushColor3Table(L, r + m, g + m, b + m);
    return 1;
}

inline int luaColor3FromHex(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    const char* hex = luaL_optstring(L, base, "000000");
    if (hex[0] == '#') ++hex;
    unsigned int v = 0;
    std::sscanf(hex, "%x", &v);
    if (std::strlen(hex) <= 3) {
        // short form
        int r = (v >> 8) & 0xF; r = r * 16 + r;
        int g = (v >> 4) & 0xF; g = g * 16 + g;
        int b = v & 0xF; b = b * 16 + b;
        pushColor3Table(L, r / 255.0, g / 255.0, b / 255.0);
    } else {
        pushColor3Table(L, ((v >> 16) & 255) / 255.0,
                        ((v >> 8) & 255) / 255.0, (v & 255) / 255.0);
    }
    return 1;
}

inline int luaColor3ToHSV(lua_State* L) {
    int base = lua_istable(L, 1) ? 1 : 1;
    double r = 0, g = 0, b = 0;
    if (lua_istable(L, base)) {
        lua_getfield(L, base, "R"); r = lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, base, "G"); g = lua_tonumber(L, -1); lua_pop(L, 1);
        lua_getfield(L, base, "B"); b = lua_tonumber(L, -1); lua_pop(L, 1);
    } else {
        r = luaL_optnumber(L, 1, 0);
        g = luaL_optnumber(L, 2, 0);
        b = luaL_optnumber(L, 3, 0);
    }
    double mx = std::max(r, std::max(g, b));
    double mn = std::min(r, std::min(g, b));
    double d = mx - mn;
    double h = 0;
    if (d > 1e-9) {
        if (mx == r) h = std::fmod(((g - b) / d), 6.0);
        else if (mx == g) h = (b - r) / d + 2.0;
        else h = (r - g) / d + 4.0;
        h /= 6.0;
        if (h < 0) h += 1.0;
    }
    double s = mx <= 1e-9 ? 0.0 : d / mx;
    lua_pushnumber(L, h);
    lua_pushnumber(L, s);
    lua_pushnumber(L, mx);
    return 3;
}

inline void pushUDim(lua_State* L, double scale, double offset) {
    lua_newtable(L);
    lua_pushnumber(L, scale); lua_setfield(L, -2, "Scale");
    lua_pushnumber(L, offset); lua_setfield(L, -2, "Offset");
}

inline int luaUDimNew(lua_State* L) {
    int base = lua_istable(L, 1) && lua_gettop(L) >= 3 ? 2 : 1;
    pushUDim(L, luaL_optnumber(L, base, 0), luaL_optnumber(L, base + 1, 0));
    return 1;
}

inline void pushUDim2(lua_State* L, double xs, double xo, double ys, double yo) {
    lua_newtable(L);
    pushUDim(L, xs, xo); lua_setfield(L, -2, "X");
    pushUDim(L, ys, yo); lua_setfield(L, -2, "Y");
    lua_pushnumber(L, xs); lua_setfield(L, -2, "WidthScale");
    lua_pushnumber(L, xo); lua_setfield(L, -2, "WidthOffset");
    lua_pushnumber(L, ys); lua_setfield(L, -2, "HeightScale");
    lua_pushnumber(L, yo); lua_setfield(L, -2, "HeightOffset");
    pushC(L, [](lua_State* ls) -> int {
        auto get = [&](int t, const char* a, const char* b) {
            lua_getfield(ls, t, a);
            if (lua_istable(ls, -1)) {
                lua_getfield(ls, -1, b);
                double v = lua_tonumber(ls, -1);
                lua_pop(ls, 2);
                return v;
            }
            lua_pop(ls, 1);
            return 0.0;
        };
        double t = luaL_optnumber(ls, 3, 0.5);
        double xs = get(1, "X", "Scale"), xo = get(1, "X", "Offset");
        double ys = get(1, "Y", "Scale"), yo = get(1, "Y", "Offset");
        double xs2 = get(2, "X", "Scale"), xo2 = get(2, "X", "Offset");
        double ys2 = get(2, "Y", "Scale"), yo2 = get(2, "Y", "Offset");
        pushUDim2(ls, xs + (xs2 - xs) * t, xo + (xo2 - xo) * t,
                  ys + (ys2 - ys) * t, yo + (yo2 - yo) * t);
        return 1;
    }, "Lerp");
    lua_setfield(L, -2, "Lerp");
}

inline int luaUDim2New(lua_State* L) {
    int base = 1;
    if (lua_istable(L, 1) && lua_gettop(L) >= 5) base = 2;
    pushUDim2(L,
              luaL_optnumber(L, base, 0), luaL_optnumber(L, base + 1, 0),
              luaL_optnumber(L, base + 2, 0), luaL_optnumber(L, base + 3, 0));
    return 1;
}

inline int luaUDim2FromScale(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    pushUDim2(L, luaL_optnumber(L, base, 0), 0, luaL_optnumber(L, base + 1, 0), 0);
    return 1;
}

inline int luaUDim2FromOffset(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    pushUDim2(L, 0, luaL_optnumber(L, base, 0), 0, luaL_optnumber(L, base + 1, 0));
    return 1;
}

inline int luaRectNew(lua_State* L) {
    int base = lua_istable(L, 1) && lua_gettop(L) >= 5 ? 2 : 1;
    double minX = luaL_optnumber(L, base, 0);
    double minY = luaL_optnumber(L, base + 1, 0);
    double maxX = luaL_optnumber(L, base + 2, 0);
    double maxY = luaL_optnumber(L, base + 3, 0);
    lua_newtable(L);
    lua_pushnumber(L, minX); lua_setfield(L, -2, "Min");
    // Also store as Vector2-like tables
    lua_newtable(L);
    lua_pushnumber(L, minX); lua_setfield(L, -2, "X");
    lua_pushnumber(L, minY); lua_setfield(L, -2, "Y");
    lua_setfield(L, -2, "Min");
    lua_newtable(L);
    lua_pushnumber(L, maxX); lua_setfield(L, -2, "X");
    lua_pushnumber(L, maxY); lua_setfield(L, -2, "Y");
    lua_setfield(L, -2, "Max");
    lua_pushnumber(L, maxX - minX); lua_setfield(L, -2, "Width");
    lua_pushnumber(L, maxY - minY); lua_setfield(L, -2, "Height");
    return 1;
}

inline int luaNumberRangeNew(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    double a = luaL_optnumber(L, base, 0);
    double b = luaL_optnumber(L, base + 1, a);
    if (b < a) std::swap(a, b);
    lua_newtable(L);
    lua_pushnumber(L, a); lua_setfield(L, -2, "Min");
    lua_pushnumber(L, b); lua_setfield(L, -2, "Max");
    return 1;
}

inline int luaNumberSequenceKeypointNew(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    lua_newtable(L);
    lua_pushnumber(L, luaL_optnumber(L, base, 0)); lua_setfield(L, -2, "Time");
    lua_pushnumber(L, luaL_optnumber(L, base + 1, 0)); lua_setfield(L, -2, "Value");
    lua_pushnumber(L, luaL_optnumber(L, base + 2, 0)); lua_setfield(L, -2, "Envelope");
    return 1;
}

inline int luaNumberSequenceNew(lua_State* L) {
    // NumberSequence.new(n) or NumberSequence.new({keypoints}) or (n0, n1)
    lua_newtable(L);
    lua_newtable(L);
    if (lua_isnumber(L, 1) && lua_isnumber(L, 2)) {
        lua_newtable(L);
        lua_pushnumber(L, 0); lua_setfield(L, -2, "Time");
        lua_pushnumber(L, lua_tonumber(L, 1)); lua_setfield(L, -2, "Value");
        lua_pushnumber(L, 0); lua_setfield(L, -2, "Envelope");
        lua_rawseti(L, -2, 1);
        lua_newtable(L);
        lua_pushnumber(L, 1); lua_setfield(L, -2, "Time");
        lua_pushnumber(L, lua_tonumber(L, 2)); lua_setfield(L, -2, "Value");
        lua_pushnumber(L, 0); lua_setfield(L, -2, "Envelope");
        lua_rawseti(L, -2, 2);
    } else if (lua_isnumber(L, 1)) {
        double v = lua_tonumber(L, 1);
        lua_newtable(L);
        lua_pushnumber(L, 0); lua_setfield(L, -2, "Time");
        lua_pushnumber(L, v); lua_setfield(L, -2, "Value");
        lua_pushnumber(L, 0); lua_setfield(L, -2, "Envelope");
        lua_rawseti(L, -2, 1);
        lua_newtable(L);
        lua_pushnumber(L, 1); lua_setfield(L, -2, "Time");
        lua_pushnumber(L, v); lua_setfield(L, -2, "Value");
        lua_pushnumber(L, 0); lua_setfield(L, -2, "Envelope");
        lua_rawseti(L, -2, 2);
    } else if (lua_istable(L, 1)) {
        int n = (int)lua_objlen(L, 1);
        for (int i = 1; i <= n; ++i) {
            lua_rawgeti(L, 1, i);
            lua_rawseti(L, -2, i);
        }
    }
    lua_setfield(L, -2, "Keypoints");
    return 1;
}

inline int luaBrickColorNew(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    const char* name = luaL_optstring(L, base, "Medium stone grey");
    lua_newtable(L);
    lua_pushstring(L, name); lua_setfield(L, -2, "Name");
    // Rough palette
    double r = 0.64, g = 0.64, b = 0.64;
    if (std::strstr(name, "Bright red") || std::strstr(name, "Really red")) { r = 1; g = 0; b = 0; }
    else if (std::strstr(name, "Bright blue")) { r = 0.05; g = 0.4; b = 1; }
    else if (std::strstr(name, "Bright green")) { r = 0.2; g = 0.8; b = 0.2; }
    else if (std::strstr(name, "Bright yellow")) { r = 1; g = 0.9; b = 0.1; }
    else if (std::strstr(name, "White") || std::strstr(name, "Institutional white")) { r = g = b = 0.95; }
    else if (std::strstr(name, "Black") || std::strstr(name, "Really black")) { r = g = b = 0.05; }
    pushColor3Table(L, r, g, b);
    lua_setfield(L, -2, "Color");
    return 1;
}

inline int luaBrickColorRandom(lua_State* L) {
    static const char* names[] = {
        "Bright red", "Bright blue", "Bright green", "Bright yellow",
        "White", "Black", "Medium stone grey", "Dark stone grey",
        "Light blue", "Pink", "Orange", "Cyan"
    };
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> d(0, (int)(sizeof(names) / sizeof(names[0])) - 1);
    lua_pushstring(L, names[d(rng)]);
    return luaBrickColorNew(L);
}

// ---------------------------------------------------------------------------
// Value objects (lightweight Instance helpers)
// ---------------------------------------------------------------------------

inline int luaValueIndex(lua_State* L) {
    const char* key = lua_tostring(L, 2);
    if (!key) return 0;
    if (std::strcmp(key, "Value") == 0) {
        lua_getfield(L, 1, "_value");
        return 1;
    }
    lua_pushvalue(L, 2);
    lua_rawget(L, 1);
    return 1;
}

inline int luaValueNewIndex(lua_State* L) {
    const char* key = lua_tostring(L, 2);
    if (key && std::strcmp(key, "Value") == 0) {
        lua_pushvalue(L, 3);
        lua_setfield(L, 1, "_value");
        return 0;
    }
    lua_rawset(L, 1);
    return 0;
}

inline void pushValueObject(lua_State* L, const char* className, const char* name) {
    lua_newtable(L);
    lua_pushstring(L, className); lua_setfield(L, -2, "ClassName");
    lua_pushstring(L, name); lua_setfield(L, -2, "Name");
    if (std::strcmp(className, "BoolValue") == 0) lua_pushboolean(L, 0);
    else if (std::strcmp(className, "StringValue") == 0) lua_pushstring(L, "");
    else if (std::strcmp(className, "ObjectValue") == 0) lua_pushnil(L);
    else lua_pushnumber(L, 0);
    lua_setfield(L, -2, "_value");
    lua_newtable(L);
    pushC(L, luaValueIndex, "__index");
    lua_setfield(L, -2, "__index");
    pushC(L, luaValueNewIndex, "__newindex");
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
}

inline int luaCreateIntValue(lua_State* L) {
    pushValueObject(L, "IntValue", luaL_optstring(L, 1, "IntValue"));
    return 1;
}
inline int luaCreateNumberValue(lua_State* L) {
    pushValueObject(L, "NumberValue", luaL_optstring(L, 1, "NumberValue"));
    return 1;
}
inline int luaCreateStringValue(lua_State* L) {
    pushValueObject(L, "StringValue", luaL_optstring(L, 1, "StringValue"));
    return 1;
}
inline int luaCreateBoolValue(lua_State* L) {
    pushValueObject(L, "BoolValue", luaL_optstring(L, 1, "BoolValue"));
    return 1;
}
inline int luaCreateObjectValue(lua_State* L) {
    pushValueObject(L, "ObjectValue", luaL_optstring(L, 1, "ObjectValue"));
    return 1;
}

// ---------------------------------------------------------------------------
// Utilities: tick, time, typeof
// ---------------------------------------------------------------------------

inline int luaTick(lua_State* L) {
    using clock = std::chrono::system_clock;
    double t = (double)std::chrono::duration<double>(clock::now().time_since_epoch()).count();
    lua_pushnumber(L, t);
    return 1;
}

inline int luaTime(lua_State* L) {
    double now = (double)std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    lua_pushnumber(L, now - sessionStart());
    return 1;
}

inline int luaTypeof(lua_State* L) {
    int t = lua_type(L, 1);
    if (t == LUA_TTABLE) {
        lua_getfield(L, 1, "ClassName");
        if (lua_isstring(L, -1)) {
            // Instance-like
            lua_pop(L, 1);
            lua_pushstring(L, "Instance");
            return 1;
        }
        lua_pop(L, 1);
        lua_getfield(L, 1, "X");
        bool hasX = !lua_isnil(L, -1); lua_pop(L, 1);
        lua_getfield(L, 1, "Y");
        bool hasY = !lua_isnil(L, -1); lua_pop(L, 1);
        lua_getfield(L, 1, "Z");
        bool hasZ = !lua_isnil(L, -1); lua_pop(L, 1);
        if (hasX && hasY && hasZ) { lua_pushstring(L, "Vector3"); return 1; }
        if (hasX && hasY && !hasZ) {
            lua_getfield(L, 1, "Scale");
            if (!lua_isnil(L, -1)) { lua_pop(L, 1); lua_pushstring(L, "UDim"); return 1; }
            lua_pop(L, 1);
            lua_pushstring(L, "Vector2");
            return 1;
        }
        lua_getfield(L, 1, "R");
        lua_getfield(L, 1, "G");
        lua_getfield(L, 1, "B");
        if (!lua_isnil(L, -3) && !lua_isnil(L, -2) && !lua_isnil(L, -1)) {
            lua_pop(L, 3);
            lua_pushstring(L, "Color3");
            return 1;
        }
        lua_pop(L, 3);
    }
    lua_pushstring(L, lua_typename(L, t));
    return 1;
}

inline int luaWarn(lua_State* L) {
    int n = lua_gettop(L);
    std::fprintf(stderr, "[warn]");
    for (int i = 1; i <= n; ++i) {
        const char* s = lua_tostring(L, i);
        std::fprintf(stderr, " %s", s ? s : lua_typename(L, lua_type(L, i)));
    }
    std::fprintf(stderr, "\n");
    return 0;
}

inline int luaDelay(lua_State* L) {
    // delay(sec, fn) — schedule via task.delay if available
    double sec = luaL_optnumber(L, 1, 0);
    if (!lua_isfunction(L, 2)) return 0;
    lua_getglobal(L, "task");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return 0; }
    lua_getfield(L, -1, "delay");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 2); return 0; }
    lua_pushnumber(L, sec);
    lua_pushvalue(L, 2);
    lua_call(L, 2, 0);
    lua_pop(L, 1);
    return 0;
}

inline int luaSpawn(lua_State* L) {
    if (!lua_isfunction(L, 1)) return 0;
    lua_getglobal(L, "task");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return 0; }
    lua_getfield(L, -1, "spawn");
    if (!lua_isfunction(L, -1)) { lua_pop(L, 2); return 0; }
    lua_pushvalue(L, 1);
    lua_call(L, 1, 0);
    lua_pop(L, 1);
    return 0;
}

inline int luaWait(lua_State* L) {
    lua_getglobal(L, "task");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "wait");
        if (lua_isfunction(L, -1)) {
            if (lua_isnumber(L, 1)) lua_pushvalue(L, 1);
            else lua_pushnumber(L, 0);
            lua_call(L, 1, 1);
            lua_remove(L, -2);
            return 1;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    lua_pushnumber(L, luaL_optnumber(L, 1, 0));
    return 1;
}

// ---------------------------------------------------------------------------
// Enum tables
// ---------------------------------------------------------------------------

inline void setEnumItem(lua_State* L, const char* name, int value) {
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
    // Also Name reverse: Enum.KeyCode[value] style via string map optional
}

inline void setEnumItemStr(lua_State* L, const char* name, const char* value) {
    lua_pushstring(L, value);
    lua_setfield(L, -2, name);
}

inline void registerEnum(lua_State* L) {
    lua_newtable(L); // Enum

    // KeyCode (GLFW-ish / Roblox-ish subset)
    lua_newtable(L);
    setEnumItem(L, "Unknown", 0);
    setEnumItem(L, "A", 65); setEnumItem(L, "B", 66); setEnumItem(L, "C", 67);
    setEnumItem(L, "D", 68); setEnumItem(L, "E", 69); setEnumItem(L, "F", 70);
    setEnumItem(L, "G", 71); setEnumItem(L, "H", 72); setEnumItem(L, "I", 73);
    setEnumItem(L, "J", 74); setEnumItem(L, "K", 75); setEnumItem(L, "L", 76);
    setEnumItem(L, "M", 77); setEnumItem(L, "N", 78); setEnumItem(L, "O", 79);
    setEnumItem(L, "P", 80); setEnumItem(L, "Q", 81); setEnumItem(L, "R", 82);
    setEnumItem(L, "S", 83); setEnumItem(L, "T", 84); setEnumItem(L, "U", 85);
    setEnumItem(L, "V", 86); setEnumItem(L, "W", 87); setEnumItem(L, "X", 88);
    setEnumItem(L, "Y", 89); setEnumItem(L, "Z", 90);
    setEnumItem(L, "Zero", 48); setEnumItem(L, "One", 49); setEnumItem(L, "Two", 50);
    setEnumItem(L, "Three", 51); setEnumItem(L, "Four", 52); setEnumItem(L, "Five", 53);
    setEnumItem(L, "Six", 54); setEnumItem(L, "Seven", 55); setEnumItem(L, "Eight", 56);
    setEnumItem(L, "Nine", 57);
    setEnumItem(L, "Space", 32); setEnumItem(L, "Return", 257); setEnumItem(L, "Escape", 256);
    setEnumItem(L, "LeftShift", 340); setEnumItem(L, "RightShift", 344);
    setEnumItem(L, "LeftControl", 341); setEnumItem(L, "RightControl", 345);
    setEnumItem(L, "LeftAlt", 342); setEnumItem(L, "Tab", 258);
    setEnumItem(L, "Backspace", 259); setEnumItem(L, "Delete", 261);
    setEnumItem(L, "Up", 265); setEnumItem(L, "Down", 264);
    setEnumItem(L, "Left", 263); setEnumItem(L, "Right", 262);
    setEnumItem(L, "F1", 290); setEnumItem(L, "F2", 291); setEnumItem(L, "F3", 292);
    setEnumItem(L, "F4", 293); setEnumItem(L, "F5", 294); setEnumItem(L, "F6", 295);
    setEnumItem(L, "F7", 296); setEnumItem(L, "F8", 297); setEnumItem(L, "F9", 298);
    setEnumItem(L, "F10", 299); setEnumItem(L, "F11", 300); setEnumItem(L, "F12", 301);
    setEnumItem(L, "MouseLeftButton", 0); setEnumItem(L, "MouseRightButton", 1);
    setEnumItem(L, "MouseMiddleButton", 2);
    lua_setfield(L, -2, "KeyCode");

    lua_newtable(L);
    setEnumItem(L, "MouseButton1", 0);
    setEnumItem(L, "MouseButton2", 1);
    setEnumItem(L, "MouseButton3", 2);
    setEnumItem(L, "Keyboard", 10);
    setEnumItem(L, "MouseMovement", 11);
    setEnumItem(L, "MouseWheel", 12);
    setEnumItem(L, "Touch", 13);
    setEnumItem(L, "Gamepad1", 20);
    setEnumItem(L, "Focus", 30);
    setEnumItem(L, "Accelerometer", 40);
    setEnumItem(L, "Gyro", 41);
    setEnumItem(L, "TextInput", 50);
    setEnumItem(L, "InputMethod", 60);
    setEnumItem(L, "None", 99);
    lua_setfield(L, -2, "UserInputType");

    lua_newtable(L);
    setEnumItemStr(L, "Plastic", "Plastic");
    setEnumItemStr(L, "Wood", "Wood");
    setEnumItemStr(L, "Slate", "Slate");
    setEnumItemStr(L, "Concrete", "Concrete");
    setEnumItemStr(L, "CorrodedMetal", "CorrodedMetal");
    setEnumItemStr(L, "DiamondPlate", "DiamondPlate");
    setEnumItemStr(L, "Foil", "Foil");
    setEnumItemStr(L, "Grass", "Grass");
    setEnumItemStr(L, "Ice", "Ice");
    setEnumItemStr(L, "Marble", "Marble");
    setEnumItemStr(L, "Granite", "Granite");
    setEnumItemStr(L, "Brick", "Brick");
    setEnumItemStr(L, "Pebble", "Pebble");
    setEnumItemStr(L, "Sand", "Sand");
    setEnumItemStr(L, "Fabric", "Fabric");
    setEnumItemStr(L, "SmoothPlastic", "SmoothPlastic");
    setEnumItemStr(L, "Metal", "Metal");
    setEnumItemStr(L, "WoodPlanks", "WoodPlanks");
    setEnumItemStr(L, "Cobblestone", "Cobblestone");
    setEnumItemStr(L, "Air", "Air");
    setEnumItemStr(L, "Water", "Water");
    setEnumItemStr(L, "Rock", "Rock");
    setEnumItemStr(L, "Glacier", "Glacier");
    setEnumItemStr(L, "Snow", "Snow");
    setEnumItemStr(L, "Sandstone", "Sandstone");
    setEnumItemStr(L, "Mud", "Mud");
    setEnumItemStr(L, "Basalt", "Basalt");
    setEnumItemStr(L, "Ground", "Ground");
    setEnumItemStr(L, "Asphalt", "Asphalt");
    setEnumItemStr(L, "LeafyGrass", "LeafyGrass");
    setEnumItemStr(L, "Salt", "Salt");
    setEnumItemStr(L, "Limestone", "Limestone");
    setEnumItemStr(L, "Pavement", "Pavement");
    setEnumItemStr(L, "ForceField", "ForceField");
    setEnumItemStr(L, "Neon", "Neon");
    setEnumItemStr(L, "Glass", "Glass");
    lua_setfield(L, -2, "Material");

    lua_newtable(L);
    setEnumItemStr(L, "Ball", "Ball");
    setEnumItemStr(L, "Block", "Block");
    setEnumItemStr(L, "Cylinder", "Cylinder");
    setEnumItemStr(L, "Wedge", "Wedge");
    setEnumItemStr(L, "CornerWedge", "CornerWedge");
    lua_setfield(L, -2, "PartType");

    lua_newtable(L);
    setEnumItem(L, "Fixed", 0);
    setEnumItem(L, "Attach", 1);
    setEnumItem(L, "Watch", 2);
    setEnumItem(L, "Track", 3);
    setEnumItem(L, "Follow", 4);
    setEnumItem(L, "Custom", 5);
    setEnumItem(L, "Scriptable", 6);
    lua_setfield(L, -2, "CameraType");

    lua_newtable(L);
    setEnumItemStr(L, "Success", "Success");
    setEnumItemStr(L, "ClosestNoPath", "ClosestNoPath");
    setEnumItemStr(L, "ClosestOutOfRange", "ClosestOutOfRange");
    setEnumItemStr(L, "FailStartNotEmpty", "FailStartNotEmpty");
    setEnumItemStr(L, "FailFinishNotEmpty", "FailFinishNotEmpty");
    setEnumItemStr(L, "NoPath", "NoPath");
    lua_setfield(L, -2, "PathStatus");

    lua_newtable(L);
    setEnumItemStr(L, "FallingDown", "FallingDown");
    setEnumItemStr(L, "Ragdoll", "Ragdoll");
    setEnumItemStr(L, "GettingUp", "GettingUp");
    setEnumItemStr(L, "Jumping", "Jumping");
    setEnumItemStr(L, "Swimming", "Swimming");
    setEnumItemStr(L, "Freefall", "Freefall");
    setEnumItemStr(L, "Flying", "Flying");
    setEnumItemStr(L, "Landed", "Landed");
    setEnumItemStr(L, "Running", "Running");
    setEnumItemStr(L, "RunningNoPhysics", "RunningNoPhysics");
    setEnumItemStr(L, "StrafingNoPhysics", "StrafingNoPhysics");
    setEnumItemStr(L, "Climbing", "Climbing");
    setEnumItemStr(L, "Seated", "Seated");
    setEnumItemStr(L, "PlatformStanding", "PlatformStanding");
    setEnumItemStr(L, "Dead", "Dead");
    setEnumItemStr(L, "Physics", "Physics");
    setEnumItemStr(L, "None", "None");
    lua_setfield(L, -2, "HumanoidStateType");

    lua_newtable(L);
    setEnumItemStr(L, "Front", "Front");
    setEnumItemStr(L, "Back", "Back");
    setEnumItemStr(L, "Left", "Left");
    setEnumItemStr(L, "Right", "Right");
    setEnumItemStr(L, "Top", "Top");
    setEnumItemStr(L, "Bottom", "Bottom");
    lua_setfield(L, -2, "NormalId");

    lua_newtable(L);
    setEnumItem(L, "World", 0);
    setEnumItem(L, "Camera", 1);
    setEnumItem(L, "Character", 2);
    lua_setfield(L, -2, "RenderPriority");

    lua_newtable(L);
    setEnumItemStr(L, "Begin", "Begin");
    setEnumItemStr(L, "Change", "Change");
    setEnumItemStr(L, "End", "End");
    setEnumItemStr(L, "Cancel", "Cancel");
    setEnumItemStr(L, "None", "None");
    lua_setfield(L, -2, "UserInputState");

    lua_newtable(L);
    setEnumItemStr(L, "Core", "Core");
    setEnumItemStr(L, "Enum", "Enum");
    setEnumItemStr(L, "User", "User");
    lua_setfield(L, -2, "ChatColor");

    lua_newtable(L);
    setEnumItem(L, "FontSize8", 8);
    setEnumItem(L, "FontSize9", 9);
    setEnumItem(L, "FontSize10", 10);
    setEnumItem(L, "FontSize11", 11);
    setEnumItem(L, "FontSize12", 12);
    setEnumItem(L, "FontSize14", 14);
    setEnumItem(L, "FontSize18", 18);
    setEnumItem(L, "FontSize24", 24);
    setEnumItem(L, "FontSize28", 28);
    setEnumItem(L, "FontSize36", 36);
    setEnumItem(L, "FontSize48", 48);
    setEnumItem(L, "FontSize60", 60);
    setEnumItem(L, "FontSize96", 96);
    lua_setfield(L, -2, "FontSize");

    lua_newtable(L);
    setEnumItemStr(L, "Legacy", "Legacy");
    setEnumItemStr(L, "Arial", "Arial");
    setEnumItemStr(L, "ArialBold", "ArialBold");
    setEnumItemStr(L, "SourceSans", "SourceSans");
    setEnumItemStr(L, "SourceSansBold", "SourceSansBold");
    setEnumItemStr(L, "SourceSansSemibold", "SourceSansSemibold");
    setEnumItemStr(L, "SourceSansLight", "SourceSansLight");
    setEnumItemStr(L, "SourceSansItalic", "SourceSansItalic");
    setEnumItemStr(L, "Bodoni", "Bodoni");
    setEnumItemStr(L, "Garamond", "Garamond");
    setEnumItemStr(L, "Cartoon", "Cartoon");
    setEnumItemStr(L, "Code", "Code");
    setEnumItemStr(L, "Highway", "Highway");
    setEnumItemStr(L, "SciFi", "SciFi");
    setEnumItemStr(L, "Arcade", "Arcade");
    setEnumItemStr(L, "Antique", "Antique");
    setEnumItemStr(L, "Gotham", "Gotham");
    setEnumItemStr(L, "GothamMedium", "GothamMedium");
    setEnumItemStr(L, "GothamBold", "GothamBold");
    setEnumItemStr(L, "GothamBlack", "GothamBlack");
    lua_setfield(L, -2, "Font");

    lua_newtable(L);
    setEnumItemStr(L, "Scale", "Scale");
    setEnumItemStr(L, "Fit", "Fit");
    setEnumItemStr(L, "Crop", "Crop");
    setEnumItemStr(L, "Stretch", "Stretch");
    lua_setfield(L, -2, "ScaleType");

    lua_newtable(L);
    setEnumItemStr(L, "List", "List");
    setEnumItemStr(L, "Grid", "Grid");
    lua_setfield(L, -2, "UILayoutType");

    lua_newtable(L);
    setEnumItemStr(L, "Horizontal", "Horizontal");
    setEnumItemStr(L, "Vertical", "Vertical");
    lua_setfield(L, -2, "FillDirection");

    lua_newtable(L);
    setEnumItemStr(L, "Center", "Center");
    setEnumItemStr(L, "Left", "Left");
    setEnumItemStr(L, "Right", "Right");
    setEnumItemStr(L, "Top", "Top");
    setEnumItemStr(L, "Bottom", "Bottom");
    lua_setfield(L, -2, "TextXAlignment");

    lua_newtable(L);
    setEnumItemStr(L, "Center", "Center");
    setEnumItemStr(L, "Top", "Top");
    setEnumItemStr(L, "Bottom", "Bottom");
    lua_setfield(L, -2, "TextYAlignment");

    lua_newtable(L);
    setEnumItemStr(L, "AutomaticSize", "XY");
    setEnumItemStr(L, "X", "X");
    setEnumItemStr(L, "Y", "Y");
    setEnumItemStr(L, "None", "None");
    setEnumItemStr(L, "XY", "XY");
    lua_setfield(L, -2, "AutomaticSize");

    lua_newtable(L);
    setEnumItemStr(L, "ScrollBarInset", "None");
    setEnumItemStr(L, "None", "None");
    setEnumItemStr(L, "ScrollBar", "ScrollBar");
    setEnumItemStr(L, "Always", "Always");
    lua_setfield(L, -2, "ScrollBarInset");

    lua_newtable(L);
    setEnumItemStr(L, "In", "In");
    setEnumItemStr(L, "Out", "Out");
    setEnumItemStr(L, "InOut", "InOut");
    lua_setfield(L, -2, "EasingDirection");

    lua_newtable(L);
    setEnumItemStr(L, "Linear", "Linear");
    setEnumItemStr(L, "Sine", "Sine");
    setEnumItemStr(L, "Back", "Back");
    setEnumItemStr(L, "Quad", "Quad");
    setEnumItemStr(L, "Quart", "Quart");
    setEnumItemStr(L, "Quint", "Quint");
    setEnumItemStr(L, "Bounce", "Bounce");
    setEnumItemStr(L, "Elastic", "Elastic");
    setEnumItemStr(L, "Exponential", "Exponential");
    setEnumItemStr(L, "Circular", "Circular");
    setEnumItemStr(L, "Cubic", "Cubic");
    lua_setfield(L, -2, "EasingStyle");

    lua_newtable(L);
    setEnumItem(L, "PlaybackState", 0);
    setEnumItemStr(L, "Begin", "Begin");
    setEnumItemStr(L, "Delayed", "Delayed");
    setEnumItemStr(L, "Playing", "Playing");
    setEnumItemStr(L, "Paused", "Paused");
    setEnumItemStr(L, "Completed", "Completed");
    setEnumItemStr(L, "Cancelled", "Cancelled");
    lua_setfield(L, -2, "PlaybackState");

    lua_newtable(L);
    setEnumItemStr(L, "Default", "Default");
    setEnumItemStr(L, "AlwaysOnTop", "AlwaysOnTop");
    setEnumItemStr(L, "Occluded", "Occluded");
    lua_setfield(L, -2, "BillboardGui");

    lua_newtable(L);
    setEnumItemStr(L, "Attachment0", "Attachment0");
    setEnumItemStr(L, "Attachment1", "Attachment1");
    lua_setfield(L, -2, "Beam");

    lua_newtable(L);
    setEnumItemStr(L, "Success", "Success");
    setEnumItemStr(L, "Failure", "Failure");
    setEnumItemStr(L, "PurchaseGranted", "PurchaseGranted");
    setEnumItemStr(L, "NotProcessedYet", "NotProcessedYet");
    lua_setfield(L, -2, "ProductPurchaseDecision");

    lua_newtable(L);
    setEnumItemStr(L, "DeveloperProduct", "DeveloperProduct");
    setEnumItemStr(L, "GamePass", "GamePass");
    setEnumItemStr(L, "Asset", "Asset");
    lua_setfield(L, -2, "InfoType");

    lua_newtable(L);
    setEnumItemStr(L, "Asset", "Asset");
    setEnumItemStr(L, "Product", "Product");
    setEnumItemStr(L, "GamePass", "GamePass");
    lua_setfield(L, -2, "ProductType");

    lua_newtable(L);
    setEnumItem(L, "CollisionFidelity", 0);
    setEnumItemStr(L, "Default", "Default");
    setEnumItemStr(L, "Hull", "Hull");
    setEnumItemStr(L, "Box", "Box");
    setEnumItemStr(L, "PreciseConvexDecomposition", "PreciseConvexDecomposition");
    lua_setfield(L, -2, "CollisionFidelity");

    lua_newtable(L);
    setEnumItemStr(L, "CameraSubject", "CameraSubject");
    setEnumItemStr(L, "Character", "Character");
    setEnumItemStr(L, "Vehicle", "Vehicle");
    lua_setfield(L, -2, "CameraSubject");

    lua_setglobal(L, "Enum");
}

// ---------------------------------------------------------------------------
// Random / math extras
// ---------------------------------------------------------------------------

inline int luaRandomNew(lua_State* L) {
    int seed = (int)luaL_optinteger(L, lua_istable(L, 1) ? 2 : 1, (lua_Integer)std::time(nullptr));
    lua_newtable(L);
    lua_pushinteger(L, seed);
    lua_setfield(L, -2, "_seed");
    pushC(L, [](lua_State* ls) -> int {
        lua_getfield(ls, 1, "_seed");
        unsigned s = (unsigned)lua_tointeger(ls, -1); lua_pop(ls, 1);
        s = s * 1103515245u + 12345u;
        lua_pushinteger(ls, (lua_Integer)s);
        lua_setfield(ls, 1, "_seed");
        int lo = (int)luaL_optinteger(ls, 2, 1);
        int hi = (int)luaL_optinteger(ls, 3, lo);
        if (hi < lo) std::swap(hi, lo);
        int span = hi - lo + 1;
        int v = lo + (int)((s >> 16) % (unsigned)std::max(span, 1));
        lua_pushinteger(ls, v);
        return 1;
    }, "NextInteger");
    lua_setfield(L, -2, "NextInteger");
    pushC(L, [](lua_State* ls) -> int {
        lua_getfield(ls, 1, "_seed");
        unsigned s = (unsigned)lua_tointeger(ls, -1); lua_pop(ls, 1);
        s = s * 1103515245u + 12345u;
        lua_pushinteger(ls, (lua_Integer)s);
        lua_setfield(ls, 1, "_seed");
        double lo = luaL_optnumber(ls, 2, 0);
        double hi = luaL_optnumber(ls, 3, 1);
        double t = ((s >> 8) & 0xffffff) / (double)0xffffff;
        lua_pushnumber(ls, lo + (hi - lo) * t);
        return 1;
    }, "NextNumber");
    lua_setfield(L, -2, "NextNumber");
    return 1;
}

inline int luaDateTimeNow(lua_State* L) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    lua_newtable(L);
    lua_pushnumber(L, (double)t); lua_setfield(L, -2, "UnixTimestamp");
    std::tm* tm = std::gmtime(&t);
    if (tm) {
        lua_pushinteger(L, tm->tm_year + 1900); lua_setfield(L, -2, "Year");
        lua_pushinteger(L, tm->tm_mon + 1); lua_setfield(L, -2, "Month");
        lua_pushinteger(L, tm->tm_mday); lua_setfield(L, -2, "Day");
        lua_pushinteger(L, tm->tm_hour); lua_setfield(L, -2, "Hour");
        lua_pushinteger(L, tm->tm_min); lua_setfield(L, -2, "Minute");
        lua_pushinteger(L, tm->tm_sec); lua_setfield(L, -2, "Second");
    }
    return 1;
}

inline int luaDateTimeFromUnixTimestamp(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    std::time_t t = (std::time_t)luaL_optnumber(L, base, 0);
    lua_newtable(L);
    lua_pushnumber(L, (double)t); lua_setfield(L, -2, "UnixTimestamp");
    std::tm* tm = std::gmtime(&t);
    if (tm) {
        lua_pushinteger(L, tm->tm_year + 1900); lua_setfield(L, -2, "Year");
        lua_pushinteger(L, tm->tm_mon + 1); lua_setfield(L, -2, "Month");
        lua_pushinteger(L, tm->tm_mday); lua_setfield(L, -2, "Day");
        lua_pushinteger(L, tm->tm_hour); lua_setfield(L, -2, "Hour");
        lua_pushinteger(L, tm->tm_min); lua_setfield(L, -2, "Minute");
        lua_pushinteger(L, tm->tm_sec); lua_setfield(L, -2, "Second");
    }
    return 1;
}

// ---------------------------------------------------------------------------
// OverlapParams / RaycastParams lightweight tables
// ---------------------------------------------------------------------------

inline int luaRaycastParamsNew(lua_State* L) {
    (void)L;
    lua_newtable(L);
    lua_pushstring(L, "RaycastParams"); lua_setfield(L, -2, "ClassName");
    lua_newtable(L); lua_setfield(L, -2, "FilterDescendantsInstances");
    lua_pushstring(L, "Exclude"); lua_setfield(L, -2, "FilterType");
    lua_pushboolean(L, 0); lua_setfield(L, -2, "IgnoreWater");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "RespectCanCollide");
    return 1;
}

inline int luaOverlapParamsNew(lua_State* L) {
    (void)L;
    lua_newtable(L);
    lua_pushstring(L, "OverlapParams"); lua_setfield(L, -2, "ClassName");
    lua_newtable(L); lua_setfield(L, -2, "FilterDescendantsInstances");
    lua_pushstring(L, "Exclude"); lua_setfield(L, -2, "FilterType");
    lua_pushinteger(L, 20); lua_setfield(L, -2, "MaxParts");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "RespectCanCollide");
    return 1;
}

inline int luaPathWaypointNew(lua_State* L) {
    int base = lua_istable(L, 1) ? 2 : 1;
    lua_newtable(L);
    lua_pushstring(L, "PathWaypoint"); lua_setfield(L, -2, "ClassName");
    if (lua_istable(L, base)) {
        lua_pushvalue(L, base);
        lua_setfield(L, -2, "Position");
    }
    lua_pushstring(L, luaL_optstring(L, base + 1, "Walk"));
    lua_setfield(L, -2, "Action");
    return 1;
}

inline int luaTweenInfoNew(lua_State* L) {
    int base = 1;
    if (lua_istable(L, 1) && lua_gettop(L) >= 2 && !lua_isnumber(L, 1)) base = 1;
    // TweenInfo.new(time, style, direction, repeat, reverse, delay)
    double time = luaL_optnumber(L, base, 1.0);
    const char* style = "Quad";
    const char* dir = "Out";
    if (lua_isstring(L, base + 1)) style = lua_tostring(L, base + 1);
    if (lua_isstring(L, base + 2)) dir = lua_tostring(L, base + 2);
    int rep = (int)luaL_optinteger(L, base + 3, 0);
    bool rev = lua_toboolean(L, base + 4) != 0;
    double delay = luaL_optnumber(L, base + 5, 0);
    lua_newtable(L);
    lua_pushnumber(L, time); lua_setfield(L, -2, "Time");
    lua_pushstring(L, style); lua_setfield(L, -2, "EasingStyle");
    lua_pushstring(L, dir); lua_setfield(L, -2, "EasingDirection");
    lua_pushinteger(L, rep); lua_setfield(L, -2, "RepeatCount");
    lua_pushboolean(L, rev ? 1 : 0); lua_setfield(L, -2, "Reverses");
    lua_pushnumber(L, delay); lua_setfield(L, -2, "DelayTime");
    return 1;
}

inline int luaCatalogSearchParamsNew(lua_State* L) {
    (void)L;
    lua_newtable(L);
    lua_pushstring(L, ""); lua_setfield(L, -2, "SearchKeyword");
    lua_pushinteger(L, 30); lua_setfield(L, -2, "Limit");
    lua_pushstring(L, "Relevance"); lua_setfield(L, -2, "SortType");
    return 1;
}

// ---------------------------------------------------------------------------
// registerApis
// ---------------------------------------------------------------------------

inline void registerApis(lua_State* L) {
    sessionStart(); // init clock

    // HttpService
    lua_newtable(L);
    pushC(L, luaJsonEncode, "JSONEncode");
    lua_setfield(L, -2, "JSONEncode");
    pushC(L, luaJsonDecode, "JSONDecode");
    lua_setfield(L, -2, "JSONDecode");
    pushC(L, luaUrlEncode, "UrlEncode");
    lua_setfield(L, -2, "UrlEncode");
    pushC(L, luaUrlDecode, "UrlDecode");
    lua_setfield(L, -2, "UrlDecode");
    pushC(L, luaGenerateGUID, "GenerateGUID");
    lua_setfield(L, -2, "GenerateGUID");
    pushC(L, luaHttpGetAsync, "GetAsync");
    lua_setfield(L, -2, "GetAsync");
    pushC(L, luaHttpPostAsync, "PostAsync");
    lua_setfield(L, -2, "PostAsync");
    pushC(L, luaHttpRequestAsync, "RequestAsync");
    lua_setfield(L, -2, "RequestAsync");
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "HttpEnabled");
    lua_setglobal(L, "HttpService");

    // ContextActionService
    lua_newtable(L);
    pushC(L, luaContextBind, "BindAction");
    lua_setfield(L, -2, "BindAction");
    pushC(L, luaContextBindAtPriority, "BindActionAtPriority");
    lua_setfield(L, -2, "BindActionAtPriority");
    pushC(L, luaContextUnbind, "UnbindAction");
    lua_setfield(L, -2, "UnbindAction");
    pushC(L, luaContextUnbindAll, "UnbindAllActions");
    lua_setfield(L, -2, "UnbindAllActions");
    pushC(L, luaContextGetBoundActionInfo, "GetBoundActionInfo");
    lua_setfield(L, -2, "GetBoundActionInfo");
    pushC(L, luaContextGetAllBoundActionInfo, "GetAllBoundActionInfo");
    lua_setfield(L, -2, "GetAllBoundActionInfo");
    lua_setglobal(L, "ContextActionService");

    // TeleportService
    lua_newtable(L);
    pushC(L, luaTeleport, "Teleport");
    lua_setfield(L, -2, "Teleport");
    pushC(L, luaTeleportAsync, "TeleportAsync");
    lua_setfield(L, -2, "TeleportAsync");
    pushC(L, luaTeleportToPlaceInstance, "TeleportToPlaceInstance");
    lua_setfield(L, -2, "TeleportToPlaceInstance");
    pushC(L, luaGetLocalPlayerTeleportData, "GetLocalPlayerTeleportData");
    lua_setfield(L, -2, "GetLocalPlayerTeleportData");
    pushC(L, luaTeleportGetFailedSignal, "GetPropertyChangedSignal");
    lua_setfield(L, -2, "GetPropertyChangedSignal");
    // TeleportInitFailed signal
    lua_newtable(L);
    pushC(L, luaTeleportInitFailedConnect, "Connect");
    lua_setfield(L, -2, "Connect");
    lua_setfield(L, -2, "TeleportInitFailed");
    lua_setglobal(L, "TeleportService");

    // MarketplaceService
    lua_newtable(L);
    pushC(L, luaPromptProductPurchase, "PromptProductPurchase");
    lua_setfield(L, -2, "PromptProductPurchase");
    pushC(L, luaPromptGamePassPurchase, "PromptGamePassPurchase");
    lua_setfield(L, -2, "PromptGamePassPurchase");
    pushC(L, luaPromptPremiumPurchase, "PromptPremiumPurchase");
    lua_setfield(L, -2, "PromptPremiumPurchase");
    pushC(L, luaUserOwnsGamePassAsync, "UserOwnsGamePassAsync");
    lua_setfield(L, -2, "UserOwnsGamePassAsync");
    pushC(L, luaGetProductInfo, "GetProductInfo");
    lua_setfield(L, -2, "GetProductInfo");
    lua_newtable(L);
    pushC(L, luaMarketplaceIndex, "__index");
    lua_setfield(L, -2, "__index");
    pushC(L, luaProcessReceiptNewIndex, "__newindex");
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    lua_setglobal(L, "MarketplaceService");

    // Attributes globals
    pushC(L, luaSetAttribute, "SetAttribute");
    lua_setglobal(L, "SetAttribute");
    pushC(L, luaGetAttribute, "GetAttribute");
    lua_setglobal(L, "GetAttribute");
    pushC(L, luaGetAttributes, "GetAttributes");
    lua_setglobal(L, "GetAttributes");
    pushC(L, luaRemoveAttribute, "RemoveAttribute");
    lua_setglobal(L, "RemoveAttribute");

    // Color3 (may already exist — enrich / replace methods)
    lua_newtable(L);
    pushC(L, luaColor3New, "new");
    lua_setfield(L, -2, "new");
    pushC(L, luaColor3FromRGB, "fromRGB");
    lua_setfield(L, -2, "fromRGB");
    pushC(L, luaColor3FromHSV, "fromHSV");
    lua_setfield(L, -2, "fromHSV");
    pushC(L, luaColor3FromHex, "fromHex");
    lua_setfield(L, -2, "fromHex");
    pushC(L, luaColor3ToHSV, "toHSV");
    lua_setfield(L, -2, "toHSV");
    lua_setglobal(L, "Color3");

    lua_newtable(L);
    pushC(L, luaUDimNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "UDim");

    lua_newtable(L);
    pushC(L, luaUDim2New, "new");
    lua_setfield(L, -2, "new");
    pushC(L, luaUDim2FromScale, "fromScale");
    lua_setfield(L, -2, "fromScale");
    pushC(L, luaUDim2FromOffset, "fromOffset");
    lua_setfield(L, -2, "fromOffset");
    lua_setglobal(L, "UDim2");

    lua_newtable(L);
    pushC(L, luaRectNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "Rect");

    lua_newtable(L);
    pushC(L, luaNumberRangeNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "NumberRange");

    lua_newtable(L);
    pushC(L, luaNumberSequenceNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "NumberSequence");

    lua_newtable(L);
    pushC(L, luaNumberSequenceKeypointNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "NumberSequenceKeypoint");

    lua_newtable(L);
    pushC(L, luaBrickColorNew, "new");
    lua_setfield(L, -2, "new");
    pushC(L, luaBrickColorRandom, "random");
    lua_setfield(L, -2, "random");
    lua_setglobal(L, "BrickColor");

    lua_newtable(L);
    pushC(L, luaRandomNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "Random");

    lua_newtable(L);
    pushC(L, luaDateTimeNow, "now");
    lua_setfield(L, -2, "now");
    pushC(L, luaDateTimeFromUnixTimestamp, "fromUnixTimestamp");
    lua_setfield(L, -2, "fromUnixTimestamp");
    lua_setglobal(L, "DateTime");

    lua_newtable(L);
    pushC(L, luaRaycastParamsNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "RaycastParams");

    lua_newtable(L);
    pushC(L, luaOverlapParamsNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "OverlapParams");

    lua_newtable(L);
    pushC(L, luaPathWaypointNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "PathWaypoint");

    lua_newtable(L);
    pushC(L, luaTweenInfoNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "TweenInfo");

    lua_newtable(L);
    pushC(L, luaCatalogSearchParamsNew, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "CatalogSearchParams");

    // Value factories
    pushC(L, luaCreateIntValue, "CreateIntValue");
    lua_setglobal(L, "CreateIntValue");
    pushC(L, luaCreateNumberValue, "CreateNumberValue");
    lua_setglobal(L, "CreateNumberValue");
    pushC(L, luaCreateStringValue, "CreateStringValue");
    lua_setglobal(L, "CreateStringValue");
    pushC(L, luaCreateBoolValue, "CreateBoolValue");
    lua_setglobal(L, "CreateBoolValue");
    pushC(L, luaCreateObjectValue, "CreateObjectValue");
    lua_setglobal(L, "CreateObjectValue");

    // Globals
    pushC(L, luaTick, "tick");
    lua_setglobal(L, "tick");
    pushC(L, luaTime, "time");
    lua_setglobal(L, "time");
    pushC(L, luaTypeof, "typeof");
    lua_setglobal(L, "typeof");
    pushC(L, luaWarn, "warn");
    lua_setglobal(L, "warn");
    pushC(L, luaDelay, "delay");
    lua_setglobal(L, "delay");
    pushC(L, luaSpawn, "spawn");
    lua_setglobal(L, "spawn");
    pushC(L, luaWait, "wait");
    lua_setglobal(L, "wait");

    registerEnum(L);

    // Attach to game
    lua_getglobal(L, "game");
    if (lua_istable(L, -1)) {
        lua_getglobal(L, "HttpService");
        lua_setfield(L, -2, "HttpService");
        lua_getglobal(L, "ContextActionService");
        lua_setfield(L, -2, "ContextActionService");
        lua_getglobal(L, "TeleportService");
        lua_setfield(L, -2, "TeleportService");
        lua_getglobal(L, "MarketplaceService");
        lua_setfield(L, -2, "MarketplaceService");
    }
    lua_pop(L, 1);
}

} // namespace studio_pro_apis
