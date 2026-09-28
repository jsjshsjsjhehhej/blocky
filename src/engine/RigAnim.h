#pragma once

/**
 * Animazioni del personaggio a keyframe, fatte dentro Blocky.
 *
 * Perche' non basta quello che c'era: R6WalkKey descrive un fotogramma con
 * quattro numeri — un angolo per braccio e uno per gamba. Va benissimo per una
 * camminata, ma non permette di girare la testa, piegare il torso, allungare un
 * braccio o spostare una parte. Qui ogni parte del corpo ha una traccia sua,
 * e ogni chiave porta una posa completa: posizione, rotazione sui tre assi e
 * scala.
 *
 * Tracce separate e non un unico elenco di fotogrammi interi: cosi' si puo'
 * mettere una chiave sul braccio a meta' clip senza dover ridichiarare dove
 * stanno gambe e testa in quell'istante — che e' il modo in cui si anima
 * davvero, ed e' anche come funziona l'editor di Roblox.
 *
 * Il formato su disco e' lo stesso JSON "indulgente" del resto del motore
 * (vedi jsonFind* in GameHub.h): scritto da noi, letto da noi.
 */

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <cstdio>
#include <filesystem>
#include <fstream>

#include "engine/GameHub.h"   // blockyDataDir, readTextFile, jsonFind*
#include "engine/Humanoid.h"

namespace blk {
namespace riganim {

/** Le parti che si possono animare, nell'ordine in cui compaiono nell'editor. */
inline const std::vector<std::string>& partNames() {
    static const std::vector<std::string> names = {
        "Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg"
    };
    return names;
}

struct Key {
    float t = 0.f;        // secondi dall'inizio della clip
    RigPose pose;
};

struct PartTrack {
    std::string part;     // uno dei nomi in partNames()
    std::vector<Key> keys;

    /** Le chiavi restano sempre in ordine di tempo: il campionamento conta su questo. */
    void sortKeys() {
        std::sort(keys.begin(), keys.end(),
                  [](const Key& a, const Key& b) { return a.t < b.t; });
    }

    /**
     * Posa a un dato istante.
     *
     * Prima della prima chiave e dopo l'ultima si tiene ferma quella piu'
     * vicina, invece di tornare a riposo: se metti una sola chiave col braccio
     * alzato, il braccio resta alzato per tutta la clip — che e' quello che ci
     * si aspetta vedendo una chiave sola sulla linea del tempo.
     */
    RigPose sample(float t) const {
        if (keys.empty()) return RigPose{};
        if (t <= keys.front().t) return keys.front().pose;
        if (t >= keys.back().t) return keys.back().pose;

        for (size_t i = 1; i < keys.size(); ++i) {
            if (t > keys[i].t) continue;
            const Key& a = keys[i - 1];
            const Key& b = keys[i];
            const float span = b.t - a.t;
            if (span <= 0.0001f) return b.pose;
            return rigPoseLerp(a.pose, b.pose, (t - a.t) / span);
        }
        return keys.back().pose;
    }
};

struct Clip {
    std::string id;
    std::string name = "Nuova animazione";
    float duration = 1.f;
    bool  looped = true;
    std::vector<PartTrack> tracks;

    PartTrack* find(const std::string& part) {
        for (auto& t : tracks)
            if (t.part == part) return &t;
        return nullptr;
    }
    const PartTrack* find(const std::string& part) const {
        for (const auto& t : tracks)
            if (t.part == part) return &t;
        return nullptr;
    }

    /** La traccia di una parte, creandola se non c'e'. */
    PartTrack& track(const std::string& part) {
        if (PartTrack* t = find(part)) return *t;
        PartTrack t;
        t.part = part;
        tracks.push_back(std::move(t));
        return tracks.back();
    }

    int keyCount() const {
        int n = 0;
        for (const auto& t : tracks) n += (int)t.keys.size();
        return n;
    }

    /**
     * Mette (o sostituisce) una chiave.
     *
     * Sostituire invece di accumulare e' voluto: rimettere una chiave allo
     * stesso istante significa "correggi questa posa", non "aggiungine una
     * seconda sovrapposta" che poi non si potrebbe piu' selezionare.
     */
    void setKey(const std::string& part, float t, const RigPose& pose) {
        PartTrack& tr = track(part);
        for (auto& k : tr.keys) {
            if (std::fabs(k.t - t) < 0.0005f) { k.pose = pose; return; }
        }
        Key k;
        k.t = t;
        k.pose = pose;
        tr.keys.push_back(k);
        tr.sortKeys();
    }

    void removeKey(const std::string& part, float t) {
        PartTrack* tr = find(part);
        if (!tr) return;
        for (size_t i = 0; i < tr->keys.size(); ++i) {
            if (std::fabs(tr->keys[i].t - t) < 0.0005f) {
                tr->keys.erase(tr->keys.begin() + (long)i);
                return;
            }
        }
    }
};

/**
 * Scrive la posa della clip sul personaggio.
 *
 * Tocca solo p.anim, mai rotX/rotZ: quelli restano di camminata, salto e
 * oggetto in mano, che continuano a funzionare come prima e si sommano a
 * questa posa invece di essere sostituiti.
 */
inline void applyToHumanoid(const Clip& clip, float t, Humanoid& h, float weight = 1.f) {
    weight = std::clamp(weight, 0.f, 1.f);
    for (auto& p : h.parts) {
        const PartTrack* tr = clip.find(p.name);
        if (!tr || tr->keys.empty()) continue;
        const RigPose sampled = tr->sample(t);
        p.anim = (weight >= 0.999f) ? sampled : rigPoseLerp(RigPose{}, sampled, weight);
    }
}

/** Riporta tutte le parti alla posa di riposo (fine anteprima, cambio clip). */
inline void clearPose(Humanoid& h) {
    for (auto& p : h.parts) p.anim = RigPose{};
}


// ---- Salvataggio su disco ---------------------------------------------

inline std::string clipsDir() { return blockyDataDir() + "/animations"; }

inline std::string clipPath(const std::string& id) {
    return clipsDir() + "/" + id + ".blockyanim";
}

inline std::string jsonNum(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", v);
    return buf;
}

inline std::string vecJson(const glm::vec3& v) {
    return "[" + jsonNum(v.x) + "," + jsonNum(v.y) + "," + jsonNum(v.z) + "]";
}

inline std::string toJson(const Clip& c) {
    std::string j;
    j += "{\n  \"format\": \"BlockyRigAnim\",\n  \"version\": 1,\n";
    j += "  \"id\": \"" + c.id + "\",\n";
    j += "  \"name\": \"" + c.name + "\",\n";
    j += "  \"duration\": " + jsonNum(c.duration) + ",\n";
    j += "  \"looped\": " + std::string(c.looped ? "true" : "false") + ",\n";
    j += "  \"tracks\": [\n";
    for (size_t i = 0; i < c.tracks.size(); ++i) {
        const PartTrack& tr = c.tracks[i];
        j += "    {\"part\": \"" + tr.part + "\", \"keys\": [\n";
        for (size_t k = 0; k < tr.keys.size(); ++k) {
            const Key& key = tr.keys[k];
            j += "      {\"t\": " + jsonNum(key.t)
               + ", \"pos\": " + vecJson(key.pose.pos)
               + ", \"rot\": " + vecJson(key.pose.rot)
               + ", \"scale\": " + vecJson(key.pose.scale) + "}";
            j += (k + 1 < tr.keys.size() ? ",\n" : "\n");
        }
        j += "    ]}";
        j += (i + 1 < c.tracks.size() ? ",\n" : "\n");
    }
    j += "  ]\n}\n";
    return j;
}

/** Legge "chiave": [x,y,z] a partire da una posizione nel testo. */
inline glm::vec3 readVec(const std::string& json, const char* key, size_t from, glm::vec3 fallback) {
    const std::string pat = std::string("\"") + key + "\"";
    size_t k = json.find(pat, from);
    if (k == std::string::npos) return fallback;
    k = json.find('[', k);
    if (k == std::string::npos) return fallback;
    glm::vec3 v = fallback;
    if (std::sscanf(json.c_str() + k, "[%f,%f,%f]", &v.x, &v.y, &v.z) != 3) return fallback;
    return v;
}

inline bool save(const Clip& c) {
    if (c.id.empty()) return false;
    plat::makeDirs(clipsDir());
    std::ofstream f(clipPath(c.id), std::ios::binary);
    if (!f) return false;
    const std::string j = toJson(c);
    f.write(j.data(), (std::streamsize)j.size());
    return true;
}

inline bool load(const std::string& id, Clip& out) {
    const std::string json = readTextFile(clipPath(id));
    if (json.empty()) return false;

    out = Clip{};
    out.id = id;
    out.name = jsonFindStr(json, "name");
    if (out.name.empty()) out.name = id;
    float dur = 1.f;
    jsonFindFloat(json, "duration", 0, dur);
    out.duration = std::max(0.05f, dur);
    out.looped = json.find("\"looped\": true") != std::string::npos;

    // Si scorre il testo cercando i blocchi "part", e dentro ognuno le chiavi.
    // Parser volutamente ingenuo: questi file li scriviamo noi, non arrivano
    // da fuori — la stessa convenzione usata in tutto il resto del motore.
    size_t pos = 0;
    while ((pos = json.find("\"part\"", pos)) != std::string::npos) {
        const std::string part = jsonFindStr(json, "part", pos);
        size_t blockEnd = json.find("\"part\"", pos + 6);
        if (blockEnd == std::string::npos) blockEnd = json.size();

        PartTrack tr;
        tr.part = part;
        size_t kp = pos;
        while ((kp = json.find("\"t\":", kp)) != std::string::npos && kp < blockEnd) {
            Key key;
            jsonFindFloat(json, "t", kp, key.t);
            key.pose.pos   = readVec(json, "pos",   kp, glm::vec3(0.f));
            key.pose.rot   = readVec(json, "rot",   kp, glm::vec3(0.f));
            key.pose.scale = readVec(json, "scale", kp, glm::vec3(1.f));
            tr.keys.push_back(key);
            kp += 4;
        }
        tr.sortKeys();
        if (!tr.keys.empty()) out.tracks.push_back(std::move(tr));
        pos = blockEnd;
    }
    return true;
}

/** Elenco delle animazioni salvate, per il menu a tendina dell'editor. */
inline std::vector<std::string> listSavedIds() {
    std::vector<std::string> ids;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(clipsDir(), ec)) {
        if (ec) break;
        if (!e.is_regular_file(ec)) continue;
        if (e.path().extension() != ".blockyanim") continue;
        ids.push_back(e.path().stem().string());
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace riganim
} // namespace blk
