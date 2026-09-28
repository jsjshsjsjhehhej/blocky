#pragma once


#include "Humanoid.h"
#include "../platform/ObfStr.h"
#include "../platform/Log.h"
#include "Economy.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <glm/glm.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#endif

struct AntiCheatConfig {
    float maxWalkSpeed = 28.f;
    float maxJumpPower = 70.f;
    float maxHorizSpeed = 45.f;
    float maxTeleportStuds = 80.f;
    float maxFallSpeed = 120.f;
    float maxAirTime = 8.f;
    float maxDtSpike = 0.12f;
    float maxStepUp = 6.f;
    int   strikesBeforeKick = 6;
    int   tamperBeforeHardMute = 3;
    bool  enabled = true;
    bool  antiTamper = true;
    bool  antiDebug = true;
    bool  killHostileProcs = true;
};

struct AntiCheatState {
    AntiCheatConfig cfg;
    glm::vec3 lastPos{0};
    bool hasLast = false;
    int strikes = 0;
    int flagsThisFrame = 0;
    int tamperHits = 0;
    std::string lastReason;
    bool softKick = false;
    float muteMoveUntil = 0.f;
    float hardMuteUntil = 0.f;

    uint32_t configSeal = 0;
    bool sealed = false;
    float shadowWalk = 16.f;
    float shadowJump = 50.f;
    float airTime = 0.f;
    float lastFrameTime = 0.f;
    bool integrityOk = true;
    bool debuggerSeen = false;
    uint32_t heartbeat = 0;

    std::string playerWarning;
    float lastProcScan = 0.f;
    int hostileKills = 0;
    int injectIncidents = 0;
};

inline const char*& acLinkedUser() {
    static const char* u = "";
    return u;
}
inline void acSetLinkedUser(const char* user) {
    acLinkedUser() = (user && user[0]) ? user : "";
}

inline AntiCheatState& antiCheat() {
    static AntiCheatState s;
    return s;
}

inline void acReportInjectStrike(float nowTime, const char* detail) {
    auto& ac = antiCheat();
    const char* user = acLinkedUser();
    if (!user || !user[0]) user = "Player";
    bool banned = ecoReportConfirmedCheat(user, EcoCheatKind::HostileInject, nowTime, detail);
    ++ac.injectIncidents;
    if (banned) {
        ac.playerWarning =
            "BANNATO PERMANENTE: tool cheat / inject usato piu' di una volta.";
    } else {
        ac.playerWarning =
            std::string("AVVISO ") + std::to_string(economy().cheatStrikes) + "/2: "
            + (detail ? detail : "tool cheat")
            + " — un altro tentativo = BAN permanente.";
    }
}

inline uint32_t acHashBytes(const void* data, size_t n, uint32_t seed = 2166136261u) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint32_t h = seed;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

inline uint32_t acComputeConfigSeal(const AntiCheatConfig& c) {
    float pack[] = {
        c.maxWalkSpeed, c.maxJumpPower, c.maxHorizSpeed, c.maxTeleportStuds,
        c.maxFallSpeed, c.maxAirTime, c.maxDtSpike,
        (float)c.strikesBeforeKick, (float)c.tamperBeforeHardMute,
        c.enabled ? 1.f : 0.f, c.antiTamper ? 1.f : 0.f, c.antiDebug ? 1.f : 0.f,
        0xB10C4u / 1000.f
    };
    return acHashBytes(pack, sizeof(pack), 0xA11C4EAFu);
}

inline void acSealConfig() {
    auto& ac = antiCheat();
    ac.configSeal = acComputeConfigSeal(ac.cfg);
    ac.sealed = true;
    ac.integrityOk = true;
}

inline bool acVerifyConfigSeal() {
    auto& ac = antiCheat();
    if (!ac.sealed || !ac.cfg.antiTamper) return true;
    uint32_t now = acComputeConfigSeal(ac.cfg);
    if (now != ac.configSeal) {
        ac.integrityOk = false;
        ac.lastReason = "config tamper (seal)";
        ++ac.tamperHits;
        ac.cfg = AntiCheatConfig{};
        ac.cfg.enabled = true;
        ac.cfg.antiTamper = true;
        ac.cfg.antiDebug = true;
        acSealConfig();
        return false;
    }
    return true;
}

inline void acFlag(const char* reason, int strikeAdd = 1) {
    auto& ac = antiCheat();
    ac.lastReason = reason;
    ++ac.flagsThisFrame;
    ac.strikes += strikeAdd;
}

inline void acTamperFlag(const char* reason) {
    auto& ac = antiCheat();
    ac.lastReason = reason;
    ++ac.flagsThisFrame;
    ++ac.tamperHits;
    ac.integrityOk = false;
    ac.strikes += 2;
}

inline bool acDetectDebugger() {
#ifdef _WIN32
    if (IsDebuggerPresent()) return true;
    BOOL remote = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote) && remote)
        return true;
#endif
    return false;
}

inline void acToLowerInPlace(char* s) {
    if (!s) return;
    for (; *s; ++s) {
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
    }
}

inline bool acIsHostileCheatTool(const char* exeLower) {
    if (!exeLower || !exeLower[0]) return false;
    static const std::string kBad[] = {
        OBF("cheatengine"), OBF("cheat engine"),
        OBF("x64dbg"), OBF("x32dbg"), OBF("ollydbg"),
        OBF("extremeinjector"), OBF("extreme injector"),
        OBF("dllinjector"), OBF("dll injector"),
        OBF("processhacker"),
        OBF("megadumper"), OBF("scylla"),
        OBF("artmoney"), OBF("tsearch"),
    };
    for (const std::string& b : kBad) {
        if (std::strstr(exeLower, b.c_str())) return true;
    }
    return false;
}

inline int acKillHostileProcesses(std::string& outKilledName) {
    outKilledName.clear();
    int killed = 0;
#ifdef _WIN32
    DWORD self = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);
    if (Process32First(snap, &pe)) {
        do {
            if (pe.th32ProcessID == self || pe.th32ProcessID == 0) continue;
            char name[MAX_PATH] = {};
#ifdef UNICODE
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, name, MAX_PATH, nullptr, nullptr);
#else
            std::snprintf(name, sizeof(name), "%s", pe.szExeFile);
#endif
            char lower[MAX_PATH];
            std::snprintf(lower, sizeof(lower), "%s", name);
            acToLowerInPlace(lower);

            if (std::strstr(lower, "devenv") || std::strstr(lower, "cursor")
                || std::strstr(lower, "code.exe") || std::strstr(lower, "msvsmon")
                || std::strstr(lower, "windbg"))
                continue;

            if (!acIsHostileCheatTool(lower)) continue;

            if (std::strstr(lower, OBF("processhacker").c_str())) {
                outKilledName = name;
                continue;
            }

            HANDLE hp = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION,
                                    FALSE, pe.th32ProcessID);
            if (!hp) continue;
            if (TerminateProcess(hp, 1)) {
                ++killed;
                outKilledName = name;
                LOG_SEC("ac", "Killed hostile: %s (pid %lu)",
                        name, (unsigned long)pe.th32ProcessID);
            }
            CloseHandle(hp);
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
#else
    (void)outKilledName;
#endif
    return killed;
}

inline void acScanInjectAndWarn(float nowTime) {
    auto& ac = antiCheat();
    if (!ac.cfg.enabled || !ac.cfg.antiTamper) return;
    if (economy().banned) {
        ac.playerWarning = "Account BANNATO per cheat — non puoi giocare.";
        return;
    }
    if (nowTime - ac.lastProcScan < 2.5f) return;
    ac.lastProcScan = nowTime;

    const bool dbg = ac.cfg.antiDebug && acDetectDebugger();
    if (dbg) {
        if (!ac.debuggerSeen) {
            ac.debuggerSeen = true;
            acTamperFlag("app esterna agganciata (debugger)");
            acReportInjectStrike(nowTime, "debugger/inject agganciato a Blocky");
        }
        ac.hardMuteUntil = std::max(ac.hardMuteUntil, nowTime + 6.f);
        ac.softKick = true;
        ac.muteMoveUntil = ac.hardMuteUntil;

        if (ac.cfg.killHostileProcs) {
            std::string killed;
            int n = acKillHostileProcesses(killed);
            if (n > 0) {
                ac.hostileKills += n;
                if (ac.playerWarning.find("BANNATO") == std::string::npos
                    && ac.playerWarning.find("AVVISO") == std::string::npos) {
                    ac.playerWarning =
                        "Tool cheat chiuso: " + killed + ". Non riprovare — rischio ban.";
                }
            }
        }
        return;
    }
    ac.debuggerSeen = false;

    if (ac.cfg.killHostileProcs) {
        std::string killed;
        int n = acKillHostileProcesses(killed);
        if (n > 0) {
            ac.hostileKills += n;
            acTamperFlag("hostile process killed");
            ac.softKick = true;
            ac.muteMoveUntil = nowTime + 3.f;
            std::string detail = "tool cheat chiuso: " + killed;
            acReportInjectStrike(nowTime, detail.c_str());
        } else if (!killed.empty()) {
            char low[260];
            std::snprintf(low, sizeof(low), "%s", killed.c_str());
            acToLowerInPlace(low);
            if (std::strstr(low, OBF("processhacker").c_str())) {
                ac.playerWarning =
                    "ATTENZIONE: Process Hacker in esecuzione. "
                    "Usarlo su Blocky puo' portare al ban.";
            }
        }
    }
}

inline void acBoot() {
    auto& ac = antiCheat();
    ac.cfg = AntiCheatConfig{};
    acSealConfig();
    ac.shadowWalk = 16.f;
    ac.shadowJump = 50.f;
    ac.airTime = 0.f;
    ac.tamperHits = 0;
    ac.integrityOk = true;
    ac.debuggerSeen = false;
    ac.heartbeat = 0xC0FFEEu;
    ac.hasLast = false;
    ac.strikes = 0;
    ac.softKick = false;
    ac.muteMoveUntil = 0.f;
    ac.hardMuteUntil = 0.f;
    ac.lastReason.clear();
    ac.playerWarning.clear();
    ac.lastProcScan = 0.f;
    ac.hostileKills = 0;
    ac.injectIncidents = 0;
}

inline void acReset() {
    auto& ac = antiCheat();
    ac.hasLast = false;
    ac.strikes = 0;
    ac.softKick = false;
    ac.muteMoveUntil = 0.f;
    ac.hardMuteUntil = 0.f;
    ac.airTime = 0.f;
    ac.lastReason.clear();
}

inline void acAllowTeleport() {
    antiCheat().hasLast = false;
}

inline void acOnLegitimateRespawn() {
    auto& ac = antiCheat();
    ac.hasLast = false;
    ac.softKick = false;
    ac.muteMoveUntil = 0.f;
    if (!ac.debuggerSeen)
        ac.hardMuteUntil = 0.f;
    ac.airTime = 0.f;
    ac.strikes = std::min(ac.strikes, 2);
    ac.flagsThisFrame = 0;
    if (ac.lastReason.find("teleport") != std::string::npos
        || ac.lastReason.find("fly") != std::string::npos
        || ac.lastReason.find("soft-kick") != std::string::npos)
        ac.lastReason.clear();
}

inline void acSyncShadowFromHumanoid(const Humanoid& h) {
    auto& ac = antiCheat();
    ac.shadowWalk = std::min(h.walkSpeed, ac.cfg.maxWalkSpeed);
    ac.shadowJump = std::min(h.jumpPower, ac.cfg.maxJumpPower);
}

inline void acClampHumanoid(Humanoid& h) {
    auto& ac = antiCheat();
    ac.flagsThisFrame = 0;
    if (!ac.cfg.enabled) return;

    if (ac.cfg.antiTamper)
        acVerifyConfigSeal();

    if (h.walkSpeed > ac.cfg.maxWalkSpeed) {
        if (h.walkSpeed > ac.cfg.maxWalkSpeed * 1.35f)
            acTamperFlag("walkSpeed memory edit");
        else
            acFlag("walkSpeed clamp");
        h.walkSpeed = ac.cfg.maxWalkSpeed;
    }
    if (h.jumpPower > ac.cfg.maxJumpPower) {
        if (h.jumpPower > ac.cfg.maxJumpPower * 1.35f)
            acTamperFlag("jumpPower memory edit");
        else
            acFlag("jumpPower clamp");
        h.jumpPower = ac.cfg.maxJumpPower;
    }
    if (h.health > h.maxHealth) h.health = h.maxHealth;
    if (h.health < 0.f) h.health = 0.f;
    if (h.velocityY < -ac.cfg.maxFallSpeed) {
        h.velocityY = -ac.cfg.maxFallSpeed;
        acFlag("fallSpeed clamp");
    }
    if (h.velocityY > ac.cfg.maxJumpPower * 0.9f) {
        h.velocityY = ac.cfg.maxJumpPower * 0.9f;
        acFlag("jumpVel clamp");
    }

    ac.shadowWalk = h.walkSpeed;
    ac.shadowJump = h.jumpPower;
}

inline void acTickIntegrity(float dt, float nowTime) {
    auto& ac = antiCheat();
    if (!ac.cfg.enabled || !ac.cfg.antiTamper) {
        ac.lastFrameTime = nowTime;
        return;
    }

    ++ac.heartbeat;
    acVerifyConfigSeal();
    acScanInjectAndWarn(nowTime);

    if (ac.lastFrameTime > 0.f) {
        float wall = nowTime - ac.lastFrameTime;
        if (wall > ac.cfg.maxDtSpike && dt < wall * 0.35f) {
            acFlag("timing anomaly", 1);
        }
        if (dt > ac.cfg.maxDtSpike * 2.f) {
            acFlag("dt spike", 1);
        }
    }
    ac.lastFrameTime = nowTime;

    if (ac.tamperHits >= ac.cfg.tamperBeforeHardMute) {
        ac.hardMuteUntil = std::max(ac.hardMuteUntil, nowTime + 8.f);
        ac.softKick = true;
        ac.muteMoveUntil = ac.hardMuteUntil;
        ac.lastReason = "anti-tamper hard mute";
        ac.tamperHits = ac.cfg.tamperBeforeHardMute / 2;
    }
}

inline bool acTakePlayerWarning(std::string& out) {
    auto& ac = antiCheat();
    if (ac.playerWarning.empty()) return false;
    out = std::move(ac.playerWarning);
    ac.playerWarning.clear();
    return true;
}

inline void acValidateMovement(glm::vec3& pos, glm::vec3& moveVel, float dt, float nowTime,
                               bool grounded = true, float velocityY = 0.f) {
    auto& ac = antiCheat();
    ac.flagsThisFrame = 0;
    if (!ac.cfg.enabled) {
        ac.lastPos = pos;
        ac.hasLast = true;
        return;
    }

    const bool muted = (ac.softKick && nowTime < ac.muteMoveUntil)
                    || (nowTime < ac.hardMuteUntil);
    if (muted) {
        moveVel = {0, 0, 0};
        if (ac.hasLast) ac.lastPos = pos;
        ac.hasLast = true;
        return;
    }

    if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) {
        if (ac.hasLast) pos = ac.lastPos;
        else pos = {0, 0, 0};
        moveVel = {0, 0, 0};
        acTamperFlag("invalid position");
        return;
    }

    float hSpd = std::sqrt(moveVel.x * moveVel.x + moveVel.z * moveVel.z);
    if (hSpd > ac.cfg.maxHorizSpeed) {
        float s = ac.cfg.maxHorizSpeed / hSpd;
        moveVel.x *= s;
        moveVel.z *= s;
        acFlag("speedhack horiz", 1);
    }

    if (ac.hasLast && dt > 1e-4f) {
        glm::vec3 d = pos - ac.lastPos;
        float dist = glm::length(glm::vec2(d.x, d.z));
        float maxDist = ac.cfg.maxTeleportStuds * std::max(dt, 1.f / 20.f) + 8.f;
        if (dist > maxDist) {
            pos.x = ac.lastPos.x;
            pos.z = ac.lastPos.z;
            moveVel.x = moveVel.z = 0.f;
            acFlag("teleport blocked", 1);
        }
        if (!grounded && d.y > ac.cfg.maxStepUp && velocityY <= 0.1f) {
            pos.y = ac.lastPos.y;
            acFlag("fly ascent blocked", 1);
        }
    }

    if (!grounded) {
        if (velocityY < -2.f || velocityY > 2.f)
            ac.airTime = 0.f;
        else
            ac.airTime += dt;
        if (ac.airTime > ac.cfg.maxAirTime && std::fabs(velocityY) < 1.f) {
            acFlag("fly / airtime", 1);
            if (ac.hasLast) pos.y = std::min(pos.y, ac.lastPos.y);
        }
    } else {
        ac.airTime = 0.f;
    }

    if (ac.flagsThisFrame > 0) {
        LOG_SEC("ac", "%s (strikes=%d tamper=%d)",
                ac.lastReason.c_str(), ac.strikes, ac.tamperHits);
    }
    if (ac.strikes >= ac.cfg.strikesBeforeKick) {
        ac.softKick = true;
        ac.muteMoveUntil = nowTime + 4.f;
        ac.strikes = ac.cfg.strikesBeforeKick / 2;
        ac.lastReason = "soft-kick 4s";
        LOG_SEC("ac", "soft-kick movimento 4s");
    }

    ac.lastPos = pos;
    ac.hasLast = true;
}

inline void acValidateMovement(glm::vec3& pos, glm::vec3& moveVel, float dt, float nowTime) {
    acValidateMovement(pos, moveVel, dt, nowTime, true, 0.f);
}
