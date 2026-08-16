#pragma once

/**
 * AntiCheat + AntiTamper Blocky (client).
 * - Clamp stats / speed / teleport
 * - Integrità config (seal) e shadow Humanoid
 * - Anti-debug / timing anomaly (speedhack clock)
 * - Fly / fall-speed heuristics
 * Nota: lato client non è inviolabile; in multiplayer le stesse regole
 * devono vivere sul server (authority).
 */

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
    float maxHorizSpeed = 45.f;      // u/s
    float maxTeleportStuds = 80.f;   // per secondo (orizzontale) — largo per lag/collisioni
    float maxFallSpeed = 120.f;
    float maxAirTime = 8.f;          // secondi in aria senza cadere → fly (obby ok)
    float maxDtSpike = 0.12f;        // dt sospetto (clock speedhack)
    float maxStepUp = 6.f;           // salita legittima su un blocco (studs/frame)
    int   strikesBeforeKick = 6;
    int   tamperBeforeHardMute = 3;
    bool  enabled = true;
    bool  antiTamper = true;         // seal + debug + shadow
    bool  antiDebug = true;
    bool  killHostileProcs = true;   // chiude solo tool cheat noti (non app a caso)
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

    // Anti-tamper
    uint32_t configSeal = 0;
    bool sealed = false;
    float shadowWalk = 16.f;
    float shadowJump = 50.f;
    float airTime = 0.f;
    float lastFrameTime = 0.f;
    bool integrityOk = true;
    bool debuggerSeen = false;
    uint32_t heartbeat = 0;

    // Inject / tool esterni → warning player (consumato da UI)
    std::string playerWarning;
    float lastProcScan = 0.f;
    int hostileKills = 0;
    int injectIncidents = 0; // quante volte (debounce) → ban via Economy
};

/** Username per ban permanente (settato da main quando loggato). */
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

/** Strike inject → al 2° ban permanente (stesso sistema economia). */
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
    // Mix campi critici — se qualcuno patcha i limiti in memoria, il seal non torna.
    float pack[] = {
        c.maxWalkSpeed, c.maxJumpPower, c.maxHorizSpeed, c.maxTeleportStuds,
        c.maxFallSpeed, c.maxAirTime, c.maxDtSpike,
        (float)c.strikesBeforeKick, (float)c.tamperBeforeHardMute,
        c.enabled ? 1.f : 0.f, c.antiTamper ? 1.f : 0.f, c.antiDebug ? 1.f : 0.f,
        0xB10C4u / 1000.f // magic costante
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
        // Ripristina limiti sicuri di default
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

/** Tool noti che si attaccano ai processi (memory edit / debug). */
inline bool acIsHostileCheatTool(const char* exeLower) {
    if (!exeLower || !exeLower[0]) return false;
    // Offuscate: i nomi non compaiono in chiaro nel binario, altrimenti
    // basterebbe cercarli nell'exe e rinominare il proprio tool.
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

/** Kill SOLO processi cheat noti. Mai Explorer/antivirus/nostro exe. */
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

            // Non tocchiamo tool di sistema / IDE (falsi positivi)
            if (std::strstr(lower, "devenv") || std::strstr(lower, "cursor")
                || std::strstr(lower, "code.exe") || std::strstr(lower, "msvsmon")
                || std::strstr(lower, "windbg")) // WinDbg spesso legit per dev
                continue;

            if (!acIsHostileCheatTool(lower)) continue;

            // processhacker: solo warning, non kill (tool admin legittimo)
            if (std::strstr(lower, OBF("processhacker").c_str())) {
                outKilledName = name; // riusato come "visto"
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

/** Se qualcosa è “dentro” Blocky (debugger) o tool cheat aperti → kill + warning (+ ban al 2°). */
inline void acScanInjectAndWarn(float nowTime) {
    auto& ac = antiCheat();
    if (!ac.cfg.enabled || !ac.cfg.antiTamper) return;
    if (economy().banned) {
        ac.playerWarning = "Account BANNATO per cheat — non puoi giocare.";
        return;
    }
    if (nowTime - ac.lastProcScan < 2.5f) return;
    ac.lastProcScan = nowTime;

    // 1) Debugger / processo attaccato a noi
    const bool dbg = ac.cfg.antiDebug && acDetectDebugger();
    if (dbg) {
        if (!ac.debuggerSeen) {
            // Rising edge: ogni nuovo aggancio = 1 strike (2° = ban)
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
                // Kill durante debugger: stesso incidente (debounce Economy evita doppio strike)
                if (ac.playerWarning.find("BANNATO") == std::string::npos
                    && ac.playerWarning.find("AVVISO") == std::string::npos) {
                    ac.playerWarning =
                        "Tool cheat chiuso: " + killed + ". Non riprovare — rischio ban.";
                }
            }
        }
        return;
    }
    // Debugger tolto → il prossimo aggancio conta di nuovo
    ac.debuggerSeen = false;

    // 2) Tool cheat in background → kill + strike (2° volta = ban)
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
            // Process Hacker ecc.: solo warning, NON strike/ban
            char low[260];
            std::snprintf(low, sizeof(low), "%s", killed.c_str());
            acToLowerInPlace(low);
            if (std::strstr(low, OBF("processhacker").c_str())) {
                // Il messaggio all'utente resta in chiaro: e' giusto che si legga.
                ac.playerWarning =
                    "ATTENZIONE: Process Hacker in esecuzione. "
                    "Usarlo su Blocky puo' portare al ban.";
            }
        }
    }
}

/** Chiama una volta a boot / fine loading. */
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
    // Non spegne antiTamper / seal
}

/** Marca il prossimo spostamento come legittimo (respawn / script). */
inline void acAllowTeleport() {
    antiCheat().hasLast = false;
}

/**
 * Dopo void-respawn / spawn legittimo: sblocca il movimento.
 * Prima il soft-kick restava attivo e il player non poteva più muoversi.
 */
inline void acOnLegitimateRespawn() {
    auto& ac = antiCheat();
    ac.hasLast = false;
    ac.softKick = false;
    ac.muteMoveUntil = 0.f;
    // Non tocca hardMute da debugger reale; solo mute movimento “falso”
    if (!ac.debuggerSeen)
        ac.hardMuteUntil = 0.f;
    ac.airTime = 0.f;
    // Abbassa strike movimento così non ri-kicka subito
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

    // Valori oltre i limiti = cheat o memory edit
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

/** Tick integrità ogni frame (debug, clock, heartbeat, inject scan). */
inline void acTickIntegrity(float dt, float nowTime) {
    auto& ac = antiCheat();
    if (!ac.cfg.enabled || !ac.cfg.antiTamper) {
        ac.lastFrameTime = nowTime;
        return;
    }

    ++ac.heartbeat;
    acVerifyConfigSeal();
    acScanInjectAndWarn(nowTime);

    // Speedhack orologio: dt troppo grande o salti temporali
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

/** Consuma warning da mostrare in chat/UI (una volta). */
inline bool acTakePlayerWarning(std::string& out) {
    auto& ac = antiCheat();
    if (ac.playerWarning.empty()) return false;
    out = std::move(ac.playerWarning);
    ac.playerWarning.clear();
    return true;
}

/** Chiama dopo il movimento del frame. Corregge teleport / speedhack / fly. */
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
        // Solo ferma input — NON snappare la posizione (causava "teleport" sui blocchi)
        moveVel = {0, 0, 0};
        if (ac.hasLast) ac.lastPos = pos;
        ac.hasLast = true;
        return;
    }

    // NaN / Inf → snap sicuro
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
        // Soglia larga: lag + collisioni + step su blocchi non sono cheat
        float maxDist = ac.cfg.maxTeleportStuds * std::max(dt, 1.f / 20.f) + 8.f;
        if (dist > maxDist) {
            // Solo correzioni enormi (vero teleport hack)
            pos.x = ac.lastPos.x;
            pos.z = ac.lastPos.z;
            moveVel.x = moveVel.z = 0.f;
            acFlag("teleport blocked", 1);
        }
        // Fly: solo salite impossibili (non step-up su un Part)
        // Step-up legittimo fino a maxStepUp; jump ha velocityY > 0
        if (!grounded && d.y > ac.cfg.maxStepUp && velocityY <= 0.1f) {
            pos.y = ac.lastPos.y;
            acFlag("fly ascent blocked", 1);
        }
    }

    if (!grounded) {
        // Caduta / jump legittimi: non contare airtime anti-fly
        if (velocityY < -2.f || velocityY > 2.f)
            ac.airTime = 0.f;
        else
            ac.airTime += dt;
        // Solo se fluttua a mid-air senza cadere (noclip) — mai durante void fall
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

/** Overload retrocompatibile. */
inline void acValidateMovement(glm::vec3& pos, glm::vec3& moveVel, float dt, float nowTime) {
    acValidateMovement(pos, moveVel, dt, nowTime, true, 0.f);
}
