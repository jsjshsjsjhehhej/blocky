#pragma once

/**
 * Indice di un gioco pubblicato.
 *
 * È il contratto fra chi pubblica e chi gioca: il client scarica questo file,
 * capisce cosa gli serve e lo prende. Non ricalcola collider, non compila
 * script, non converte formati — tutto è già deciso qui.
 *
 * Due strade di costruzione, perché i dati stanno in due posti diversi:
 *   - dal mondo vivo (pubblicazione dallo Studio): entità, luci, collider
 *   - da una cartella (strumento a riga di comando): inventario e hash
 * Nessuna delle due sa fare il lavoro dell'altra, e fingere il contrario
 * produrrebbe un indice a metà senza dirlo.
 *
 * Il collider di una Part non ha bisogno della mesh: posizione, dimensione e
 * rotazione bastano. Per i modelli importati serve il volume della mesh, che
 * lo conosce solo chi l'ha caricata: lo si passa da fuori invece di tirare
 * assimp dentro questo header.
 */

#include <cmath>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "engine/DataModel.h"
#include "engine/LightingService.h"

namespace GameIndex {

/** Categoria d'uso, dedotta dal formato reale del file. */
enum class AssetKind { Mesh, Texture, Sound, Script, Data, Unknown };

inline const char* assetKindName(AssetKind k) {
    switch (k) {
    case AssetKind::Mesh:    return "mesh";
    case AssetKind::Texture: return "texture";
    case AssetKind::Sound:   return "sound";
    case AssetKind::Script:  return "script";
    case AssetKind::Data:    return "data";
    default:                 return "unknown";
    }
}

inline AssetKind kindFromFormat(const std::string& format) {
    if (format == "glb" || format == "fbx" || format == "fbx-ascii") return AssetKind::Mesh;
    if (format == "png" || format == "jpg") return AssetKind::Texture;
    if (format == "wav" || format == "ogg") return AssetKind::Sound;
    if (format == "lua-bytecode") return AssetKind::Script;
    if (format == "testo") return AssetKind::Data;
    return AssetKind::Unknown;
}

struct AssetEntry {
    std::string id;        // percorso relativo dentro il pacchetto
    AssetKind kind = AssetKind::Unknown;
    std::string format;
    std::uint64_t bytes = 0;
    std::string checksum;
};

/**
 * Volume di collisione precalcolato, in coordinate mondo.
 * Un box, non la geometria a triangoli: su una scena di scatole dà lo stesso
 * risultato a una frazione del costo, ed è la forma che il client sa già
 * risolvere senza codice nuovo.
 */
struct ColliderEntry {
    std::string owner;     // nome della Part o del Model
    bool fromModel = false;
    glm::vec3 min{0.f};
    glm::vec3 max{0.f};
};

struct ScriptEntry {
    std::string owner;     // Part a cui è agganciato
    std::uint64_t bytes = 0;
    std::string checksum;
    bool compiled = false; // true quando il pacchetto porta il bytecode
};

struct LightingBlock {
    float clockTime = 14.f;
    float brightness = 2.f;
    float ambient = 0.42f;
    glm::vec3 outdoorAmbient{0.38f, 0.42f, 0.52f};
    glm::vec3 fogColor{0.55f, 0.65f, 0.78f};
    float fogStart = 200.f;
    float fogEnd = 800.f;
    bool globalShadows = false;
};

struct EntityCounts {
    int parts = 0;
    int models = 0;
    int labels = 0;
    int friends = 0;
    int backpack = 0;
    bool baseplate = true;
};

struct Index {
    std::string gameId;
    std::string title;
    int formatVersion = 1;
    /** Cambia a ogni ripubblicazione: il client la confronta per sapere se aggiornare. */
    std::string contentHash;

    LightingBlock lighting;
    EntityCounts entities;
    std::vector<AssetEntry> assets;
    std::vector<ColliderEntry> colliders;
    std::vector<ScriptEntry> scripts;

    std::uint64_t totalBytes() const {
        std::uint64_t n = 0;
        for (const AssetEntry& a : assets) n += a.bytes;
        return n;
    }
};

// ------------------------------------------------------------------ hash

/** FNV-1a 64: stessa famiglia usata da AssetAudit, così i valori si confrontano. */
inline std::uint64_t hashString(const std::string& s, std::uint64_t seed = 1469598103934665603ull) {
    std::uint64_t h = seed;
    for (unsigned char c : s) {
        h ^= (std::uint64_t)c;
        h *= 1099511628211ull;
    }
    return h;
}

inline std::string toHex(std::uint64_t v) {
    std::ostringstream o;
    o << std::hex << v;
    return o.str();
}

// ------------------------------------------------------- costruzione

/** Restituisce false se il modello non ha un volume noto (mesh non caricata). */
using ModelBoundsFn = std::function<bool(int modelIndex, glm::vec3& outMin, glm::vec3& outMax)>;

inline void appendPartColliders(const DataModel& world, std::vector<ColliderEntry>& out) {
    for (const WorkspacePart& p : world.parts) {
        if (!p.canCollide) continue;

        // AABB della Part ruotata: si trasformano gli otto vertici e si
        // riprende min/max, così una rotazione non lascia spigoli fuori.
        const glm::vec3 half = p.size * 0.5f;
        const glm::mat4 rot = partRotationMatrix(p.rotation);

        glm::vec3 lo(1e30f), hi(-1e30f);
        for (int c = 0; c < 8; ++c) {
            const glm::vec3 corner((c & 1) ? half.x : -half.x,
                                   (c & 2) ? half.y : -half.y,
                                   (c & 4) ? half.z : -half.z);
            const glm::vec3 w = p.position + glm::vec3(rot * glm::vec4(corner, 0.f));
            lo = glm::min(lo, w);
            hi = glm::max(hi, w);
        }

        ColliderEntry e;
        e.owner = p.name;
        e.fromModel = false;
        e.min = lo;
        e.max = hi;
        out.push_back(std::move(e));
    }
}

inline void appendModelColliders(const DataModel& world, const ModelBoundsFn& bounds,
                                 std::vector<ColliderEntry>& out) {
    if (!bounds) return;
    for (int i = 0; i < (int)world.models.size(); ++i) {
        if (!world.models[(size_t)i].canCollide) continue;
        glm::vec3 lo, hi;
        if (!bounds(i, lo, hi)) continue;   // mesh non caricata: si salta invece di inventare

        ColliderEntry e;
        e.owner = world.models[(size_t)i].name;
        e.fromModel = true;
        e.min = lo;
        e.max = hi;
        out.push_back(std::move(e));
    }
}

inline void appendScripts(const DataModel& world, std::vector<ScriptEntry>& out) {
    for (const WorkspacePart& p : world.parts) {
        if (p.script.empty()) continue;
        ScriptEntry s;
        s.owner = p.name;
        s.bytes = (std::uint64_t)p.script.size();
        s.checksum = toHex(hashString(p.script));
        s.compiled = false;   // diventa true quando il publisher sigilla il bytecode
        out.push_back(std::move(s));
    }
    // I modelli importati possono avere script agganciati (Animator, IA di
    // un mostro): stesso trattamento delle Part, non un caso a parte.
    for (const WorkspaceModel& m : world.models) {
        if (m.script.empty()) continue;
        ScriptEntry s;
        s.owner = m.name;
        s.bytes = (std::uint64_t)m.script.size();
        s.checksum = toHex(hashString(m.script));
        s.compiled = false;
        out.push_back(std::move(s));
    }
}

inline LightingBlock lightingFrom(const LightingService& l) {
    LightingBlock b;
    b.clockTime = l.clockTime;
    b.brightness = l.brightness;
    b.ambient = l.ambient;
    b.outdoorAmbient = l.outdoorAmbient;
    b.fogColor = l.fogColor;
    b.fogStart = l.fogStart;
    b.fogEnd = l.fogEnd;
    b.globalShadows = l.globalShadows;
    return b;
}

/** Indice completo a partire dal mondo in memoria (pubblicazione dallo Studio). */
inline Index buildFromWorld(const DataModel& world, const LightingService& lighting,
                            const std::string& gameId, const std::string& title,
                            const ModelBoundsFn& modelBounds = {}) {
    Index idx;
    idx.gameId = gameId;
    idx.title = title.empty() ? world.placeName : title;
    idx.lighting = lightingFrom(lighting);

    idx.entities.parts = (int)world.parts.size();
    idx.entities.models = (int)world.models.size();
    idx.entities.labels = (int)world.labels.size();
    idx.entities.friends = (int)world.friends.size();
    idx.entities.backpack = (int)world.backpack.size();
    idx.entities.baseplate = world.hasBaseplate;

    appendPartColliders(world, idx.colliders);
    appendModelColliders(world, modelBounds, idx.colliders);
    appendScripts(world, idx.scripts);
    return idx;
}

/**
 * Hash unico di tutto il contenuto.
 * Un solo confronto dice al client se la sua copia è vecchia, invece di
 * scorrere l'elenco degli asset uno per uno.
 */
inline void finalizeHash(Index& idx) {
    std::uint64_t h = hashString(idx.gameId);
    h = hashString(idx.title, h);
    for (const AssetEntry& a : idx.assets) {
        h = hashString(a.id, h);
        h = hashString(a.checksum, h);
    }
    for (const ScriptEntry& s : idx.scripts) h = hashString(s.checksum, h);
    for (const ColliderEntry& c : idx.colliders) h = hashString(c.owner, h);
    idx.contentHash = toHex(h);
}

// ------------------------------------------------------------------ JSON

inline std::string escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

inline void writeVec3(std::ostringstream& j, const glm::vec3& v) {
    j << "[" << v.x << "," << v.y << "," << v.z << "]";
}

inline std::string toJson(const Index& idx) {
    std::ostringstream j;
    j.setf(std::ios::fixed);
    j.precision(3);

    j << "{\n";
    j << "  \"format\": \"BlockyGameIndex\",\n";
    j << "  \"version\": " << idx.formatVersion << ",\n";
    j << "  \"gameId\": \"" << escape(idx.gameId) << "\",\n";
    j << "  \"title\": \"" << escape(idx.title) << "\",\n";
    j << "  \"contentHash\": \"" << idx.contentHash << "\",\n";
    j << "  \"totalBytes\": " << idx.totalBytes() << ",\n";

    const LightingBlock& L = idx.lighting;
    j << "  \"lighting\": {\"clockTime\":" << L.clockTime
      << ",\"brightness\":" << L.brightness
      << ",\"ambient\":" << L.ambient
      << ",\"outdoorAmbient\":"; writeVec3(j, L.outdoorAmbient);
    j << ",\"globalShadows\":" << (L.globalShadows ? "true" : "false") << "},\n";

    j << "  \"fog\": {\"color\":"; writeVec3(j, L.fogColor);
    j << ",\"start\":" << L.fogStart << ",\"end\":" << L.fogEnd << "},\n";

    const EntityCounts& E = idx.entities;
    j << "  \"entities\": {\"parts\":" << E.parts
      << ",\"models\":" << E.models
      << ",\"labels\":" << E.labels
      << ",\"friends\":" << E.friends
      << ",\"backpack\":" << E.backpack
      << ",\"baseplate\":" << (E.baseplate ? "true" : "false") << "},\n";

    j << "  \"assets\": [\n";
    for (size_t i = 0; i < idx.assets.size(); ++i) {
        const AssetEntry& a = idx.assets[i];
        j << "    {\"id\":\"" << escape(a.id) << "\""
          << ",\"kind\":\"" << assetKindName(a.kind) << "\""
          << ",\"format\":\"" << a.format << "\""
          << ",\"bytes\":" << a.bytes
          << ",\"checksum\":\"" << a.checksum << "\"}"
          << (i + 1 < idx.assets.size() ? "," : "") << "\n";
    }
    j << "  ],\n";

    j << "  \"colliders\": [\n";
    for (size_t i = 0; i < idx.colliders.size(); ++i) {
        const ColliderEntry& c = idx.colliders[i];
        j << "    {\"owner\":\"" << escape(c.owner) << "\""
          << ",\"source\":\"" << (c.fromModel ? "model" : "part") << "\""
          << ",\"min\":"; writeVec3(j, c.min);
        j << ",\"max\":"; writeVec3(j, c.max);
        j << "}" << (i + 1 < idx.colliders.size() ? "," : "") << "\n";
    }
    j << "  ],\n";

    j << "  \"scripts\": [\n";
    for (size_t i = 0; i < idx.scripts.size(); ++i) {
        const ScriptEntry& s = idx.scripts[i];
        j << "    {\"owner\":\"" << escape(s.owner) << "\""
          << ",\"bytes\":" << s.bytes
          << ",\"checksum\":\"" << s.checksum << "\""
          << ",\"compiled\":" << (s.compiled ? "true" : "false") << "}"
          << (i + 1 < idx.scripts.size() ? "," : "") << "\n";
    }
    j << "  ]\n}\n";
    return j.str();
}

} // namespace GameIndex
