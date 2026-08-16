#pragma once

/**
 * Pacchetti animazione R6, stile Roblox.
 *
 * L'unita' che il giocatore equipaggia non e' una clip singola ma un set di
 * ruoli (Idle/Walk/Run/Jump/Fall/Climb/Sit). Un pacchetto riempie i ruoli che
 * vuole e lascia vuoti gli altri: quelli ricadono sui default della libreria.
 *
 * Tre invarianti, e sono il motivo per cui questo file esiste separato dal
 * wizard accessori:
 *
 *   1. Pubblicare NON cambia mai i default globali. Nessuna chiamata a
 *      setDefaultWalk/setDefaultJump da qui. Importare una pistola non puo'
 *      piu' diventare la tua camminata.
 *   2. Un'animazione da tool vale solo mentre il tool e' in mano
 *      (setToolOverride / clearToolOverride) e non viene mai salvata.
 *   3. Si memorizzano solo AnimationId, mai path. Gli id passano da
 *      sanitizeGameId (solo [a-z0-9_]), quindi da qui non si costruiscono
 *      path e non si esce dalla cartella dati.
 *
 * Uso tipico:
 *      AnimPackDraft d;
 *      d.displayName = "Fancy Moves";
 *      d.clipId[(int)AnimRole::Walk] = "FancyWalk";
 *      std::string err;
 *      if (!animPacks().publish(d, err)) showError(err);
 *
 *      animPacks().equip("fancy_moves");
 *      const R6WalkClip* c = animPacks().clipFor(AnimRole::Walk);
 */

#include "../engine/AnimLibrary.h"
#include "../engine/Animator.h"   // looksLikeToolAnim: euristica sul nome, non solo sul kind

#include <algorithm>
#include <map>
#include <string>
#include <vector>

enum class AnimRole : int {
    Idle = 0,
    Walk,
    Run,
    Jump,
    Fall,
    Climb,
    Sit,
    Count
};

/** Chiave stabile su disco: non tradurla mai. */
inline const char* animRoleKey(AnimRole r) {
    switch (r) {
    case AnimRole::Idle: return "idle";
    case AnimRole::Walk: return "walk";
    case AnimRole::Run: return "run";
    case AnimRole::Jump: return "jump";
    case AnimRole::Fall: return "fall";
    case AnimRole::Climb: return "climb";
    case AnimRole::Sit: return "sit";
    default: return "?";
    }
}

/** Etichetta da mostrare nella UI. */
inline const char* animRoleLabel(AnimRole r) {
    switch (r) {
    case AnimRole::Idle: return "Fermo";
    case AnimRole::Walk: return "Cammina";
    case AnimRole::Run: return "Corri";
    case AnimRole::Jump: return "Salta";
    case AnimRole::Fall: return "Cade";
    case AnimRole::Climb: return "Scala";
    case AnimRole::Sit: return "Seduto";
    default: return "?";
    }
}

/** Ruoli di locomozione: qui una clip di tipo Tool non entra mai. */
inline bool animRoleIsLocomotion(AnimRole r) {
    return r == AnimRole::Walk || r == AnimRole::Run || r == AnimRole::Jump
        || r == AnimRole::Fall || r == AnimRole::Climb;
}

/** Tetti volutamente bassi: il file indice resta leggibile e limitato. */
static const size_t kAnimPackMaxPacks = 256;
static const size_t kAnimPackMaxNameLen = 64;

struct AnimPack {
    std::string id;                              // slug, da sanitizeGameId
    std::string displayName;
    std::string author;
    std::string clipId[(int)AnimRole::Count];    // vuoto = eredita dai default
    bool builtin = false;                        // non cancellabile/sovrascrivibile

    int filledRoles() const {
        int n = 0;
        for (int i = 0; i < (int)AnimRole::Count; ++i)
            if (!clipId[i].empty()) ++n;
        return n;
    }
    bool valid() const { return !id.empty() && filledRoles() > 0; }
};

/** Quello che la UI riempie prima di pubblicare. */
struct AnimPackDraft {
    std::string displayName;
    std::string author;
    std::string clipId[(int)AnimRole::Count];
};

inline std::string animTrim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

struct AnimPackLibrary {
    std::map<std::string, AnimPack> byId;
    std::string equippedId;

    // Override temporaneo del tool: vive in RAM, non finisce mai su disco.
    std::string toolClipId;
    AnimRole toolRole = AnimRole::Idle;

    // ---- lettura ------------------------------------------------------

    const AnimPack* get(const std::string& id) const {
        auto it = byId.find(id);
        return it == byId.end() ? nullptr : &it->second;
    }

    const AnimPack* equipped() const {
        return equippedId.empty() ? nullptr : get(equippedId);
    }

    std::vector<const AnimPack*> list() const {
        std::vector<const AnimPack*> out;
        for (const auto& kv : byId) out.push_back(&kv.second);
        std::sort(out.begin(), out.end(), [](const AnimPack* a, const AnimPack* b) {
            if (a->builtin != b->builtin) return a->builtin > b->builtin; // builtin in cima
            return a->displayName < b->displayName;
        });
        return out;
    }

    /**
     * La regola di conflitto, in un posto solo.
     * tool in mano -> pacchetto equipaggiato -> default libreria -> pack R6.
     */
    const R6WalkClip* clipFor(AnimRole role) const {
        if (!toolClipId.empty() && role == toolRole) {
            if (const R6WalkClip* c = animLibrary().resolve(toolClipId)) return c;
        }
        if (const AnimPack* p = equipped()) {
            const std::string& id = p->clipId[(int)role];
            if (!id.empty()) {
                if (const R6WalkClip* c = animLibrary().resolve(id)) return c;
            }
        }
        const AnimLibrary& lib = animLibrary();
        if (role == AnimRole::Walk || role == AnimRole::Run) {
            if (const R6WalkClip* c = lib.resolve(lib.defaultWalkId)) return c;
            return lib.resolve("classic_walk");
        }
        if (role == AnimRole::Jump || role == AnimRole::Fall) {
            if (const R6WalkClip* c = lib.resolve(lib.defaultJumpId)) return c;
            return lib.resolve("classic_jump");
        }
        return nullptr;
    }

    // ---- tool (transitorio) -------------------------------------------

    void setToolOverride(const std::string& clipId, AnimRole role = AnimRole::Idle) {
        toolClipId = animLibrary().resolve(clipId) ? clipId : std::string();
        toolRole = role;
    }
    void clearToolOverride() { toolClipId.clear(); }

    // ---- scrittura ----------------------------------------------------

    /**
     * Valida e registra un pacchetto. Non tocca mai i default globali.
     * Ritorna false con `err` leggibile: la UI lo mostra e basta.
     */
    bool publish(const AnimPackDraft& draft, std::string& err, std::string* outId = nullptr) {
        std::string name = animTrim(draft.displayName);
        if (name.empty()) { err = "Dai un nome al pacchetto."; return false; }
        if (name.size() > kAnimPackMaxNameLen) name.resize(kAnimPackMaxNameLen);

        std::string id = sanitizeGameId(name);
        if (id.empty() || id == "game") {
            err = "Il nome deve contenere almeno una lettera o un numero.";
            return false;
        }

        const AnimPack* existing = get(id);
        if (existing && existing->builtin) {
            err = "\"" + name + "\" e' un pacchetto di sistema: scegli un altro nome.";
            return false;
        }
        if (!existing && byId.size() >= kAnimPackMaxPacks) {
            err = "Hai raggiunto il massimo di pacchetti. Cancellane uno.";
            return false;
        }

        AnimPack p;
        p.id = id;
        p.displayName = name;
        p.author = animTrim(draft.author).substr(0, kAnimPackMaxNameLen);

        for (int i = 0; i < (int)AnimRole::Count; ++i) {
            std::string clip = animTrim(draft.clipId[i]);
            if (clip.empty()) continue;
            const R6WalkClip* c = animLibrary().resolve(clip);
            if (!c) {
                err = std::string("Animazione \"") + clip + "\" non trovata per il ruolo "
                    + animRoleLabel((AnimRole)i) + ".";
                return false;
            }
            // Invariante 1: una clip da tool non puo' diventare locomozione.
            // Non basta il kind: una clip importata da Blender arriva quasi sempre
            // marcata "walk" anche se e' una posa da pistola. looksLikeToolAnim
            // guarda anche id/displayName/clipName.
            if (animRoleIsLocomotion((AnimRole)i) && Animator::looksLikeToolAnim(*c)) {
                err = std::string("\"") + c->displayName + "\" sembra un'animazione da tool: "
                    + "non puo' fare da " + animRoleLabel((AnimRole)i) + ".";
                return false;
            }
            p.clipId[i] = c->id;   // sempre l'id canonico, mai quello digitato
        }

        if (p.filledRoles() == 0) {
            err = "Assegna almeno un'animazione a un ruolo.";
            return false;
        }

        byId[id] = p;
        save();
        if (outId) *outId = id;
        return true;
    }

    bool equip(const std::string& id) {
        const AnimPack* p = get(id);
        if (!p) return false;
        equippedId = p->id;
        save();
        return true;
    }

    void unequip() {
        equippedId.clear();
        save();
    }

    bool remove(const std::string& id) {
        auto it = byId.find(id);
        if (it == byId.end() || it->second.builtin) return false;
        byId.erase(it);
        if (equippedId == id) equippedId.clear();
        save();
        return true;
    }

    // ---- persistenza ---------------------------------------------------

    static std::string indexPath() { return blockyDataDir() + "/anim_packs.json"; }

    void save() const {
        std::ostringstream j;
        j << "{\n  \"equippedPackId\": \"" << r6Escape(equippedId) << "\",\n"
          << "  \"packs\": [\n";
        bool first = true;
        for (const auto& kv : byId) {
            const AnimPack& p = kv.second;
            if (p.builtin) continue;            // i builtin si riseminano da soli
            if (!first) j << ",\n";
            first = false;
            j << "    {\"packId\":\"" << r6Escape(p.id)
              << "\",\"packName\":\"" << r6Escape(p.displayName)
              << "\",\"author\":\"" << r6Escape(p.author) << "\"";
            for (int i = 0; i < (int)AnimRole::Count; ++i)
                j << ",\"role_" << animRoleKey((AnimRole)i) << "\":\""
                  << r6Escape(p.clipId[i]) << "\"";
            j << "}";
        }
        j << "\n  ]\n}\n";
        std::ofstream f(indexPath());
        if (f) f << j.str();
    }

    bool load() {
        byId.clear();
        equippedId.clear();
        std::string json = readTextFile(indexPath());
        if (!json.empty()) {
            equippedId = sanitizeGameId(jsonFindStr(json, "equippedPackId"));
            if (equippedId == "game") equippedId.clear();
            size_t pos = 0;
            while ((pos = json.find("\"packId\"", pos)) != std::string::npos) {
                AnimPack p;
                p.id = sanitizeGameId(jsonFindStr(json, "packId", pos));
                p.displayName = r6Unescape(jsonFindStr(json, "packName", pos));
                p.author = r6Unescape(jsonFindStr(json, "author", pos));
                if (p.displayName.size() > kAnimPackMaxNameLen)
                    p.displayName.resize(kAnimPackMaxNameLen);
                for (int i = 0; i < (int)AnimRole::Count; ++i) {
                    std::string key = std::string("role_") + animRoleKey((AnimRole)i);
                    std::string clip = jsonFindStr(json, key, pos);
                    // Rileggere valida di nuovo: un file editato a mano non passa.
                    if (!clip.empty() && animLibrary().resolve(clip))
                        p.clipId[i] = sanitizeGameId(clip);
                }
                if (p.valid() && byId.size() < kAnimPackMaxPacks)
                    byId[p.id] = p;
                pos += 8;
            }
        }
        seedBuiltin();
        if (!equippedId.empty() && !get(equippedId)) equippedId.clear();
        return !byId.empty();
    }

    /** Pacchetto "Classic" sempre presente, non cancellabile. */
    void seedBuiltin() {
        AnimPack p;
        p.id = "classic";
        p.displayName = "Classic R6";
        p.author = "Blocky";
        p.builtin = true;
        if (animLibrary().resolve("classic_walk")) p.clipId[(int)AnimRole::Walk] = "classic_walk";
        if (animLibrary().resolve("classic_jump")) p.clipId[(int)AnimRole::Jump] = "classic_jump";
        if (p.filledRoles() > 0) byId[p.id] = p;
    }
};

inline AnimPackLibrary& animPacks() {
    static AnimPackLibrary packs;
    return packs;
}
