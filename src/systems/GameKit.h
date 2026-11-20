#pragma once

/**
 * Blocky Game Kit — feature pack per generi comuni (obby, arena, tycoon pads,
 * round-based). Stato runtime: checkpoint, kill/speed/coin/teleport pads,
 * porte, wave, match timer, announcer. Usato da Studio UI + Luau + HUD.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gamekit {

enum class PadKind : int {
    Checkpoint = 0,
    Kill = 1,
    Damage = 2,
    Speed = 3,
    Jump = 4,
    Coin = 5,
    Teleport = 6,
    Heal = 7,
    Win = 8,
};

enum class MatchPhase : int {
    Idle = 0,
    Countdown = 1,
    Playing = 2,
    Ended = 3,
};

struct Pad {
    std::string name;
    PadKind kind = PadKind::Checkpoint;
    float x = 0, y = 0, z = 0;
    float hx = 2.f, hy = 1.f, hz = 2.f;
    float strength = 1.f;   // damage / speed mult / jump boost / coins
    float cooldown = 0.6f;
    float cdLeft = 0.f;
    int order = 0;           // checkpoint order / wave id
    float destX = 0, destY = 0, destZ = 0; // teleport
    bool enabled = true;
    bool inside = false;
};

struct Door {
    std::string name;
    float x = 0, y = 0, z = 0;
    float hx = 1.5f, hy = 3.f, hz = 0.4f;
    bool open = false;
    bool requiresKey = false;
    std::string keyId;
    float openY = 4.f;
    float anim = 0.f; // 0 closed .. 1 open
};

struct WaveDef {
    std::string name;
    int count = 5;
    float interval = 2.f;
    float spawnX = 0, spawnY = 2, spawnZ = 0;
    float hp = 50.f;
};

struct Announcement {
    std::string text;
    float ttl = 2.5f;
    int style = 0; // 0 info, 1 warn, 2 success
};

struct SessionStats {
    int coins = 0;
    int kills = 0;
    int deaths = 0;
    int checkpoints = 0;
    float surviveTime = 0.f;
    float bestTime = 0.f;
    int wins = 0;
};

class GameKit {
public:
    static constexpr int kMaxPads = 128;
    static constexpr int kMaxDoors = 32;
    static constexpr int kMaxWaves = 24;
    static constexpr int kMaxAnnounce = 8;

    SessionStats stats;
    MatchPhase phase = MatchPhase::Idle;
    float matchDuration = 120.f;
    float matchTimeLeft = 0.f;
    float countdownLeft = 0.f;
    int winCoins = 50;
    int winCheckpointOrder = -1; // -1 = use Win pads
    bool autoRespawnOnKill = true;
    float killY = -50.f;
    float playerSpeedMul = 1.f;
    float playerJumpMul = 1.f;
    float boostTtl = 0.f;
    int lastCheckpoint = -1;
    float cpX = 0, cpY = 0, cpZ = 0;
    bool hasCheckpoint = false;
    bool hasKey = false;
    std::string heldKey;
    int waveIndex = 0;
    float waveTimer = 0.f;
    int waveSpawned = 0;
    bool wavesEnabled = false;
    std::vector<Pad> pads;
    std::vector<Door> doors;
    std::vector<WaveDef> waves;
    std::vector<Announcement> announce;
    std::vector<std::string> logLines;

    // Events last frame (for Lua / HUD)
    std::vector<std::string> triggeredPads;
    bool wonThisFrame = false;
    bool diedThisFrame = false;

    void reset() {
        pads.clear();
        doors.clear();
        waves.clear();
        announce.clear();
        logLines.clear();
        triggeredPads.clear();
        stats = {};
        phase = MatchPhase::Idle;
        matchTimeLeft = 0.f;
        countdownLeft = 0.f;
        playerSpeedMul = 1.f;
        playerJumpMul = 1.f;
        boostTtl = 0.f;
        lastCheckpoint = -1;
        hasCheckpoint = false;
        hasKey = false;
        heldKey.clear();
        waveIndex = 0;
        waveTimer = 0.f;
        waveSpawned = 0;
        wavesEnabled = false;
        wonThisFrame = false;
        diedThisFrame = false;
    }

    void log(const char* msg) {
        if (!msg) return;
        logLines.push_back(msg);
        if ((int)logLines.size() > 40)
            logLines.erase(logLines.begin(), logLines.begin() + 10);
    }

    void pushAnnounce(const std::string& text, float ttl = 2.5f, int style = 0) {
        if ((int)announce.size() >= kMaxAnnounce)
            announce.erase(announce.begin());
        Announcement a;
        a.text = text;
        a.ttl = ttl;
        a.style = style;
        announce.push_back(std::move(a));
    }

    bool addPad(const std::string& name, PadKind kind,
                float x, float y, float z,
                float hx = 2.f, float hy = 1.f, float hz = 2.f,
                float strength = 1.f, int order = 0) {
        if (name.empty() || (int)pads.size() >= kMaxPads) return false;
        for (const auto& p : pads)
            if (p.name == name) return false;
        Pad p;
        p.name = name;
        p.kind = kind;
        p.x = x; p.y = y; p.z = z;
        p.hx = std::max(0.2f, hx);
        p.hy = std::max(0.2f, hy);
        p.hz = std::max(0.2f, hz);
        p.strength = strength;
        p.order = order;
        pads.push_back(p);
        char buf[96];
        std::snprintf(buf, sizeof(buf), "Pad '%s' aggiunto", name.c_str());
        log(buf);
        return true;
    }

    bool removePad(const std::string& name) {
        for (auto it = pads.begin(); it != pads.end(); ++it) {
            if (it->name == name) {
                pads.erase(it);
                return true;
            }
        }
        return false;
    }

    Pad* findPad(const std::string& name) {
        for (auto& p : pads)
            if (p.name == name) return &p;
        return nullptr;
    }

    bool setTeleportDest(const std::string& name, float x, float y, float z) {
        Pad* p = findPad(name);
        if (!p) return false;
        p->destX = x; p->destY = y; p->destZ = z;
        return true;
    }

    bool addDoor(const std::string& name, float x, float y, float z,
                 bool needsKey = false, const char* keyId = "") {
        if (name.empty() || (int)doors.size() >= kMaxDoors) return false;
        for (const auto& d : doors)
            if (d.name == name) return false;
        Door d;
        d.name = name;
        d.x = x; d.y = y; d.z = z;
        d.requiresKey = needsKey;
        d.keyId = keyId ? keyId : "";
        doors.push_back(d);
        return true;
    }

    bool removeDoor(const std::string& name) {
        for (auto it = doors.begin(); it != doors.end(); ++it) {
            if (it->name == name) {
                doors.erase(it);
                return true;
            }
        }
        return false;
    }

    Door* findDoor(const std::string& name) {
        for (auto& d : doors)
            if (d.name == name) return &d;
        return nullptr;
    }

    bool openDoor(const std::string& name, bool force = false) {
        Door* d = findDoor(name);
        if (!d) return false;
        if (!force && d->requiresKey) {
            if (!hasKey || (!d->keyId.empty() && heldKey != d->keyId))
                return false;
        }
        d->open = true;
        pushAnnounce(std::string("Porta: ") + name, 2.f, 2);
        return true;
    }

    bool closeDoor(const std::string& name) {
        Door* d = findDoor(name);
        if (!d) return false;
        d->open = false;
        return true;
    }

    bool addWave(const std::string& name, int count, float interval,
                 float x, float y, float z, float hp = 50.f) {
        if ((int)waves.size() >= kMaxWaves) return false;
        WaveDef w;
        w.name = name.empty() ? ("Wave" + std::to_string(waves.size() + 1)) : name;
        w.count = std::clamp(count, 1, 40);
        w.interval = std::max(0.2f, interval);
        w.spawnX = x; w.spawnY = y; w.spawnZ = z;
        w.hp = std::max(1.f, hp);
        waves.push_back(w);
        return true;
    }

    void startMatch(float durationSec = 120.f, float countdown = 3.f) {
        matchDuration = std::max(10.f, durationSec);
        matchTimeLeft = matchDuration;
        countdownLeft = std::max(0.f, countdown);
        phase = countdownLeft > 0.01f ? MatchPhase::Countdown : MatchPhase::Playing;
        stats.surviveTime = 0.f;
        wonThisFrame = false;
        diedThisFrame = false;
        waveIndex = 0;
        waveSpawned = 0;
        waveTimer = 0.f;
        pushAnnounce(countdownLeft > 0.01f ? "Prontezza…" : "Via!", 2.f, 1);
        log("Match avviato");
    }

    void endMatch(bool win) {
        phase = MatchPhase::Ended;
        if (win) {
            stats.wins++;
            stats.coins += winCoins;
            wonThisFrame = true;
            pushAnnounce("Hai vinto", 3.f, 2);
            if (stats.surviveTime > 0.f
                && (stats.bestTime <= 0.f || stats.surviveTime < stats.bestTime))
                stats.bestTime = stats.surviveTime;
        } else {
            pushAnnounce("Fine partita", 2.5f, 1);
        }
        log(win ? "Match: win" : "Match: end");
    }

    void giveKey(const std::string& id) {
        hasKey = true;
        heldKey = id.empty() ? "key" : id;
        pushAnnounce(std::string("Chiave: ") + heldKey, 2.f, 2);
    }

    void clearKey() {
        hasKey = false;
        heldKey.clear();
    }

    void setCheckpoint(float x, float y, float z, int order) {
        hasCheckpoint = true;
        cpX = x; cpY = y; cpZ = z;
        lastCheckpoint = order;
        stats.checkpoints++;
    }

    void respawnAtCheckpoint(float& px, float& py, float& pz) {
        if (hasCheckpoint) {
            px = cpX; py = cpY + 1.f; pz = cpZ;
        }
        diedThisFrame = true;
        stats.deaths++;
        pushAnnounce("Respawn", 1.5f, 1);
    }

    static const char* padKindName(PadKind k) {
        switch (k) {
        case PadKind::Checkpoint: return "Checkpoint";
        case PadKind::Kill: return "Kill";
        case PadKind::Damage: return "Damage";
        case PadKind::Speed: return "Speed";
        case PadKind::Jump: return "Jump";
        case PadKind::Coin: return "Coin";
        case PadKind::Teleport: return "Teleport";
        case PadKind::Heal: return "Heal";
        case PadKind::Win: return "Win";
        }
        return "Pad";
    }

    static PadKind padKindFromName(const char* s) {
        if (!s) return PadKind::Checkpoint;
        if (!std::strcmp(s, "Kill") || !std::strcmp(s, "kill")) return PadKind::Kill;
        if (!std::strcmp(s, "Damage") || !std::strcmp(s, "damage")) return PadKind::Damage;
        if (!std::strcmp(s, "Speed") || !std::strcmp(s, "speed")) return PadKind::Speed;
        if (!std::strcmp(s, "Jump") || !std::strcmp(s, "jump")) return PadKind::Jump;
        if (!std::strcmp(s, "Coin") || !std::strcmp(s, "coin")) return PadKind::Coin;
        if (!std::strcmp(s, "Teleport") || !std::strcmp(s, "teleport")) return PadKind::Teleport;
        if (!std::strcmp(s, "Heal") || !std::strcmp(s, "heal")) return PadKind::Heal;
        if (!std::strcmp(s, "Win") || !std::strcmp(s, "win")) return PadKind::Win;
        return PadKind::Checkpoint;
    }

    // Player pose + optional HP hooks (nullptr = skip heal/damage numbers)
    struct TickIn {
        float px = 0, py = 0, pz = 0;
        float* health = nullptr;
        float maxHealth = 100.f;
        bool grounded = true;
    };
    struct TickOut {
        bool teleport = false;
        float tx = 0, ty = 0, tz = 0;
        bool kill = false;
        float damage = 0.f;
        float heal = 0.f;
        float speedMul = 1.f;
        float jumpMul = 1.f;
        bool win = false;
    };

    void update(float dt, TickIn& in, TickOut& out) {
        triggeredPads.clear();
        wonThisFrame = false;
        diedThisFrame = false;
        out = {};
        out.speedMul = playerSpeedMul;
        out.jumpMul = playerJumpMul;

        // Announcements
        for (size_t i = 0; i < announce.size();) {
            announce[i].ttl -= dt;
            if (announce[i].ttl <= 0.f) announce.erase(announce.begin() + (int)i);
            else ++i;
        }

        // Boost decay
        if (boostTtl > 0.f) {
            boostTtl -= dt;
            if (boostTtl <= 0.f) {
                boostTtl = 0.f;
                playerSpeedMul = 1.f;
                playerJumpMul = 1.f;
            }
        }
        out.speedMul = playerSpeedMul;
        out.jumpMul = playerJumpMul;

        // Match phases
        if (phase == MatchPhase::Countdown) {
            countdownLeft -= dt;
            if (countdownLeft <= 0.f) {
                phase = MatchPhase::Playing;
                pushAnnounce("VIA!", 1.5f, 2);
            }
        } else if (phase == MatchPhase::Playing) {
            stats.surviveTime += dt;
            if (matchDuration > 0.f) {
                matchTimeLeft -= dt;
                if (matchTimeLeft <= 0.f) {
                    matchTimeLeft = 0.f;
                    endMatch(false);
                    if (!announce.empty()) announce.back().text = "Tempo scaduto";
                }
            }
        }

        // Void kill
        if (in.py < killY) {
            out.kill = true;
            if (autoRespawnOnKill)
                respawnAtCheckpoint(in.px, in.py, in.pz);
        }

        // Doors anim
        for (auto& d : doors) {
            float target = d.open ? 1.f : 0.f;
            if (d.anim < target) d.anim = std::min(target, d.anim + dt * 2.f);
            else if (d.anim > target) d.anim = std::max(target, d.anim - dt * 2.f);
        }

        // Pads
        for (auto& p : pads) {
            if (!p.enabled) continue;
            if (p.cdLeft > 0.f) p.cdLeft -= dt;
            const bool now = std::fabs(in.px - p.x) <= p.hx
                          && std::fabs(in.py - p.y) <= p.hy
                          && std::fabs(in.pz - p.z) <= p.hz;
            const bool entered = now && !p.inside;
            p.inside = now;
            if (!entered || p.cdLeft > 0.f) continue;
            p.cdLeft = p.cooldown;
            triggeredPads.push_back(p.name);
            applyPad(p, in, out);
        }

        // Door touch → try open
        for (auto& d : doors) {
            if (d.open) continue;
            // Non "near": windows.h lo definisce come macro vuota.
            const bool touching = std::fabs(in.px - d.x) <= d.hx + 1.f
                               && std::fabs(in.py - d.y) <= d.hy
                               && std::fabs(in.pz - d.z) <= d.hz + 1.f;
            if (touching) openDoor(d.name, false);
        }

        // Waves (emit events via log; actual mob spawn left to Lua/UI hook)
        if (wavesEnabled && phase == MatchPhase::Playing && !waves.empty()) {
            if (waveIndex < (int)waves.size()) {
                const WaveDef& w = waves[waveIndex];
                waveTimer += dt;
                if (waveSpawned < w.count && waveTimer >= w.interval) {
                    waveTimer = 0.f;
                    waveSpawned++;
                    char buf[128];
                    std::snprintf(buf, sizeof(buf), "WAVE_SPAWN|%s|%d|%.1f|%.1f|%.1f|%.0f",
                                  w.name.c_str(), waveSpawned, w.spawnX, w.spawnY, w.spawnZ, w.hp);
                    log(buf);
                }
                if (waveSpawned >= w.count) {
                    waveIndex++;
                    waveSpawned = 0;
                    waveTimer = 1.5f;
                    if (waveIndex < (int)waves.size())
                        pushAnnounce(std::string("Wave ") + waves[waveIndex].name, 2.f, 1);
                    else
                        pushAnnounce("Waves complete", 2.f, 2);
                }
            }
        }

        if (out.win && phase == MatchPhase::Playing)
            endMatch(true);
    }

    void applyPad(Pad& p, TickIn& in, TickOut& out) {
        switch (p.kind) {
        case PadKind::Checkpoint:
            setCheckpoint(p.x, p.y, p.z, p.order);
            pushAnnounce(std::string("Checkpoint ") + std::to_string(p.order), 1.8f, 2);
            if (winCheckpointOrder >= 0 && p.order >= winCheckpointOrder)
                out.win = true;
            break;
        case PadKind::Kill:
            out.kill = true;
            if (autoRespawnOnKill)
                respawnAtCheckpoint(in.px, in.py, in.pz);
            break;
        case PadKind::Damage:
            out.damage += std::max(1.f, p.strength);
            if (in.health) {
                *in.health = std::max(0.f, *in.health - out.damage);
                if (*in.health <= 0.f) {
                    out.kill = true;
                    if (autoRespawnOnKill)
                        respawnAtCheckpoint(in.px, in.py, in.pz);
                }
            }
            break;
        case PadKind::Speed:
            playerSpeedMul = std::clamp(p.strength, 1.f, 3.f);
            boostTtl = std::max(boostTtl, 4.f);
            pushAnnounce("Speed boost!", 1.5f, 0);
            break;
        case PadKind::Jump:
            playerJumpMul = std::clamp(p.strength, 1.f, 3.f);
            boostTtl = std::max(boostTtl, 4.f);
            pushAnnounce("Jump boost!", 1.5f, 0);
            break;
        case PadKind::Coin: {
            int n = std::max(1, (int)p.strength);
            stats.coins += n;
            pushAnnounce(std::string("+") + std::to_string(n) + " coins", 1.2f, 2);
            break;
        }
        case PadKind::Teleport:
            out.teleport = true;
            out.tx = p.destX; out.ty = p.destY; out.tz = p.destZ;
            in.px = p.destX; in.py = p.destY; in.pz = p.destZ;
            break;
        case PadKind::Heal:
            out.heal += std::max(1.f, p.strength);
            if (in.health)
                *in.health = std::min(in.maxHealth, *in.health + out.heal);
            pushAnnounce("Heal", 1.2f, 2);
            break;
        case PadKind::Win:
            out.win = true;
            break;
        }
    }

    // Quick presets for Studio one-click kits
    void buildObbyKit(float originX, float originZ) {
        reset();
        addPad("Start", PadKind::Checkpoint, originX, 1, originZ, 3, 1, 3, 1, 0);
        addPad("CP1", PadKind::Checkpoint, originX + 20, 1, originZ, 2, 1, 2, 1, 1);
        addPad("CP2", PadKind::Checkpoint, originX + 40, 1, originZ, 2, 1, 2, 1, 2);
        addPad("Lava1", PadKind::Kill, originX + 30, 0.5f, originZ, 4, 0.6f, 4, 1, 0);
        addPad("Speed1", PadKind::Speed, originX + 10, 1, originZ, 2, 1, 2, 1.6f, 0);
        addPad("Finish", PadKind::Win, originX + 55, 1, originZ, 3, 1, 3, 1, 0);
        winCheckpointOrder = -1;
        pushAnnounce("Obby Kit pronto", 2.f, 0);
        log("Preset Obby");
    }

    void buildArenaKit(float originX, float originZ) {
        reset();
        addPad("ArenaHeal", PadKind::Heal, originX, 1, originZ, 2, 1, 2, 25.f, 0);
        addPad("ArenaSpeed", PadKind::Speed, originX + 8, 1, originZ, 2, 1, 2, 1.4f, 0);
        addPad("ArenaDmg", PadKind::Damage, originX - 8, 1, originZ, 3, 1, 3, 15.f, 0);
        addWave("W1", 4, 1.5f, originX + 12, 2, originZ, 40.f);
        addWave("W2", 6, 1.2f, originX - 12, 2, originZ, 55.f);
        wavesEnabled = true;
        startMatch(180.f, 3.f);
        pushAnnounce("Arena Kit pronto", 2.f, 0);
        log("Preset Arena");
    }

    void buildTycoonPads(float originX, float originZ) {
        reset();
        addPad("Collector", PadKind::Coin, originX, 1, originZ, 3, 1, 3, 5.f, 0);
        addPad("ShopTP", PadKind::Teleport, originX + 15, 1, originZ, 2, 1, 2, 1, 0);
        setTeleportDest("ShopTP", originX + 15, 1, originZ + 20);
        addDoor("ShopDoor", originX + 15, 2, originZ + 10, false, "");
        openDoor("ShopDoor", true);
        pushAnnounce("Tycoon pads pronti", 2.f, 0);
        log("Preset Tycoon");
    }

    int padCount() const { return (int)pads.size(); }
    int doorCount() const { return (int)doors.size(); }
    int waveCount() const { return (int)waves.size(); }
};

} // namespace gamekit
