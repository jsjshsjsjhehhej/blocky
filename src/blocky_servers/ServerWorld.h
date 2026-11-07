#pragma once

/**
 * Il mondo del server dedicato: il place del gioco, gli Script server, i
 * giocatori collegati e la fisica.
 *
 * Senza, BlockyServer era solo un ripetitore: ogni client caricava il place
 * per conto suo e gli Script (server) non giravano da nessuna parte — il
 * client li salta (scriptShouldRunHere), il server non aveva Luau. Qui il
 * server fa la parte dell'host di una partita normale:
 *   - carica il place (--place file.json / .blocky);
 *   - esegue gli Script server (part e ServerScriptService), senza finestra;
 *   - un Player Luau per ogni client: PlayerAdded / PlayerRemoving /
 *     GetPlayers, Character con HumanoidRootPart alla posizione che il
 *     client manda, Touched dei giocatori sulle part, OnServerEvent(player);
 *   - la fisica (Jolt, come nel gioco) sulle part libere: il server ne e'
 *     l'autorita', i client le ricevono e le fanno scivolare (netDriven);
 *   - replica le part ai client con lo stesso WorldDelta dell'host.
 *
 * Gli ID di rete delle part del place sono assegnati nello stesso ordine
 * anche dai client (assignPlaceNetIds), cosi' gli aggiornamenti del server
 * finiscono sulle part che il client ha gia', invece di aggiungerne copie.
 *
 * Limiti: i giocatori non hanno un corpo fisico sul server (non spingono le
 * casse del server); quello che gli script cambiano su un Player (Health,
 * leaderstats, teletrasporti) resta sul server e non arriva ancora al client.
 */

#include "engine/BlockyPhysics.h"
#include "engine/DataModel.h"
#include "engine/GameHub.h"
#include "engine/LuaRuntime.h"
#include "engine/PartConstraintsLua.h"
#include "engine/WorldReplication.h"

#include <functional>
#include <map>
#include <string>

namespace blkserver {

/** ID di rete 1..N alle part del place, nell'ordine del file. Uguale su server e client. */
inline void assignPlaceNetIds(DataModel& world) {
    uint32_t next = 1;
    for (auto& p : world.parts) p.netId = 0;
    WorldReplication::ensureNetIds(world, next);
}

class ServerWorld {
public:
    using Log = std::function<void(const std::string&)>;

    DataModel world;
    LuaRuntime lua;
    bphys::World phys;
    bool physics = true;   // false = come prima: le part libere le simulano i client
    int scriptsRun = 0;
    int scriptErrors = 0;

    // Servizi che gli script si aspettano; senza grafica, come in test_compat.
    SoundService sounds;
    LightingService lighting;
    Animator animator;
    NavGrid nav;
    Tag tags;
    Classifica classifica;
    Prossimita proximity;

    bool load(const std::string& placePath, LanNet& net, const Log& log) {
        log_ = log;
        if (!loadPlaceFromJson(placePath, world, &lighting)) {
            log("  Place: impossibile leggere " + placePath);
            return false;
        }
        assignPlaceNetIds(world);
        net.nextPartNetId = (uint32_t)world.parts.size() + 1;

        lua.soundService = &sounds;
        lua.lighting = &lighting;
        lua.animator = &animator;
        lua.net = &net;
        lua.nav = &nav;
        lua.tags = &tags;
        lua.classifica = &classifica;
        lua.proximity = &proximity;
        lua.runIsServer = true;
        lua.runIsClient = false;
        lua.extensions.push_back({"Constraints", &partconstraintslua::registerLua, &world});
        if (physics) bindPhysics();
        if (!lua.init(&world)) {
            log("  Luau: avvio fallito: " + lua.lastError);
            return false;
        }
        // Prima degli script: Players deve gia' essere quello del server.
        lua.enableServerPlayers();

        // RemoteEvent / RemoteFunction del place: registrati come fa il gioco.
        for (const auto& s : world.instances) {
            if (s.className == "RemoteEvent" || s.className == "RemoteFunction") {
                scriptControls().remoteServer[s.name];
                scriptControls().remoteClient[s.name];
            }
        }

        // Solo gli Script server: i LocalScript e quelli "vecchi" (Legacy)
        // girano gia' su ogni client — eseguirli anche qui li raddoppierebbe.
        for (int i = 0; i < (int)world.parts.size(); ++i) {
            const auto& p = world.parts[(size_t)i];
            if (p.script.empty() || p.scriptType != ScriptType::Server) continue;
            runOne(lua.runPartScript(p.script, i), "Part '" + p.name + "'");
        }
        for (int i = 0; i < (int)world.instances.size(); ++i) {
            const auto& s = world.instances[(size_t)i];
            if (s.source.empty() || s.className != "Script" || s.scriptType != ScriptType::Server) continue;
            runOne(lua.runSharedScript(s.source, i), "Script '" + s.name + "'");
        }
        log("  Place: " + (world.placeName.empty() ? placePath : world.placeName) + " — "
            + std::to_string(world.parts.size()) + " part, "
            + std::to_string(scriptsRun) + " script server avviati"
            + (scriptErrors ? ", " + std::to_string(scriptErrors) + " con errori" : std::string()));
        log(std::string("  Fisica sul server: ") + (physics ? "SI (autorita' sulle part libere)" : "no"));
        return true;
    }

    /** Un passo di gioco del server: giocatori, eventi, script, fisica, replica. */
    void tick(LanNet& net, float dt) {
        trackPlayers(net);
        lua.drainRemoteEvents();
        lua.fireHeartbeat(dt);
        if (physics) stepPhysics(dt);
        net.tickWorldReplication(world, dt);
    }

    int playerCount() const { return (int)known_.size(); }

private:
    struct Known { std::string name; };
    std::map<int, Known> known_;   // id peer -> giocatore gia' annunciato agli script
    std::vector<glm::vec3> focus_;
    Log log_;

    void runOne(bool ok, const std::string& who) {
        if (ok) {
            ++scriptsRun;
            return;
        }
        ++scriptErrors;
        if (log_) log_("  Errore in " + who + ": " + lua.lastError);
    }

    // Chi entra, chi esce, dove sono: dai peer di LanNet.
    void trackPlayers(LanNet& net) {
        std::map<int, bool> seen;
        focus_.clear();
        for (const auto& peer : net.peers) {
            if (!peer.used || peer.id == 0) continue;
            const int id = peer.id;
            seen[id] = true;
            auto it = known_.find(id);
            if (it == known_.end()) {
                known_[id] = Known{peer.name};
                if (log_) log_(std::string("[Script] PlayerAdded: ") + peer.name + " (#" + std::to_string(id) + ")");
                lua.serverPlayerJoined(id, peer.name);
            }
            if (peer.hasValidPos) {
                lua.serverPlayerMoved(id, peer.lastValidPos);
                focus_.push_back(peer.lastValidPos);
            }
        }
        for (auto it = known_.begin(); it != known_.end();) {
            if (seen.count(it->first)) { ++it; continue; }
            if (log_) log_("[Script] PlayerRemoving: " + it->second.name);
            lua.serverPlayerLeft(it->first);
            it = known_.erase(it);
        }
    }

    void bindPhysics() {
        QualityProfile q;
        q.mobile = false;
        q.maxVisibleParts = 100000;   // budget "pc": tanti corpi svegli
        q.maxDrawDistance = 600.f;
        phys.configure(q);
        using W = bphys::World;
        lua.phys.world = &phys;
        lua.phys.applyImpulse = [](void* w, int part, float x, float y, float z) {
            return w && static_cast<W*>(w)->applyImpulse(part, {x, y, z});
        };
        lua.phys.setLinVel = [](void* w, int part, float x, float y, float z) {
            return w && static_cast<W*>(w)->setLinearVelocity(part, {x, y, z});
        };
        lua.phys.getLinVel = [](void* w, int part, float* x, float* y, float* z) {
            if (!w || !x || !y || !z) return false;
            glm::vec3 v;
            if (!static_cast<W*>(w)->getLinearVelocity(part, v)) return false;
            *x = v.x; *y = v.y; *z = v.z;
            return true;
        };
        lua.phys.setAngVel = [](void* w, int part, float x, float y, float z) {
            return w && static_cast<W*>(w)->setAngularVelocity(part, {x, y, z});
        };
        lua.phys.getMass = [](void* w, int part) {
            return w ? static_cast<W*>(w)->getMass(part) : 0.f;
        };
        lua.phys.setGroupsCollidable = [](void* w, int a, int b, bool on) {
            if (w) static_cast<W*>(w)->setGroupsCollidable(a, b, on);
        };
        lua.phys.groupsCollidable = [](void* w, int a, int b) {
            return !w || static_cast<W*>(w)->groupsCollidable(a, b);
        };
        lua.phys.radialImpulse = [](void* w, float cx, float cy, float cz, float radius, float force) {
            if (w) static_cast<W*>(w)->applyRadialImpulse({cx, cy, cz}, radius, force);
        };
    }

    void stepPhysics(float dt) {
        // La fisica resta sveglia attorno a ogni giocatore (e allo spawn, se
        // non c'e' nessuno: le part che cadono all'avvio si assestano).
        const glm::vec3 main = focus_.empty() ? world.spawn.position : focus_.front();
        phys.setExtraFocus(focus_);
        phys.setGravity(lua.readWorkspaceGravity());
        phys.sync(world.parts, main, dt);
        phys.syncJoints(world.welds);
        phys.step(dt);
        phys.writeBack(world.parts, 1.f);
    }
};

} // namespace blkserver
