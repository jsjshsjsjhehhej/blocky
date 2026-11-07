#pragma once

// Replica Workspace stile Roblox (slice 1):
//   Host = autorita' sulle Part → clienti applicano Upsert/Destroy.
// Pacchetti piccoli (UDP-friendly), un part per datagramma.

#include "DataModel.h"
#include "PartMaterial.h"
#include "PartShape.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace WorldReplication {

enum class Op : uint8_t {
    Upsert = 1,
    Destroy = 2,
};

#pragma pack(push, 1)
struct PartPacket {
    char     magic[4]; // 'B','L','K','1' — stesso header via NetHeader fuori
    uint8_t  type;     // NetMsg::WorldDelta
    uint8_t  peerId;
    uint16_t gameIndex;
    uint8_t  op;
    uint32_t netId;
    float    px, py, pz;
    float    sx, sy, sz;
    float    rx, ry, rz;
    float    cr, cg, cb;
    uint8_t  shape;
    uint8_t  material;
    uint8_t  anchored;
    uint8_t  canCollide;
    char     name[24];
};
#pragma pack(pop)

static_assert(sizeof(PartPacket) <= 128, "PartPacket deve restare UDP-safe");

inline void fillPartPacket(PartPacket& out, uint8_t type, uint8_t peerId, uint16_t gi,
                           Op op, const WorkspacePart& p) {
    out.magic[0] = 'B'; out.magic[1] = 'L'; out.magic[2] = 'K'; out.magic[3] = '1';
    out.type = type;
    out.peerId = peerId;
    out.gameIndex = gi;
    out.op = (uint8_t)op;
    out.netId = p.netId;
    out.px = p.position.x; out.py = p.position.y; out.pz = p.position.z;
    out.sx = p.size.x; out.sy = p.size.y; out.sz = p.size.z;
    out.rx = p.rotation.x; out.ry = p.rotation.y; out.rz = p.rotation.z;
    out.cr = p.color.x; out.cg = p.color.y; out.cb = p.color.z;
    out.shape = (uint8_t)p.shape;
    out.material = (uint8_t)p.material;
    out.anchored = p.anchored ? 1 : 0;
    out.canCollide = p.canCollide ? 1 : 0;
    std::snprintf(out.name, sizeof(out.name), "%.23s", p.name.c_str());
}

inline void ensureNetIds(DataModel& world, uint32_t& nextId) {
    if (nextId < 1) nextId = 1;
    for (auto& p : world.parts) {
        if (p.netId == 0)
            p.netId = nextId++;
        else if (p.netId >= nextId)
            nextId = p.netId + 1;
    }
}

inline WorkspacePart* findByNetId(DataModel& world, uint32_t id) {
    if (id == 0) return nullptr;
    for (auto& p : world.parts)
        if (p.netId == id) return &p;
    return nullptr;
}

inline void applyUpsert(DataModel& world, const PartPacket& pkt) {
    if (pkt.netId == 0) return;
    WorkspacePart* existing = findByNetId(world, pkt.netId);
    WorkspacePart scratch;
    WorkspacePart& p = existing ? *existing : scratch;
    p.netId = pkt.netId;
    p.name = pkt.name[0] ? std::string(pkt.name) : ("Part" + std::to_string(pkt.netId));
    const glm::vec3 pos{pkt.px, pkt.py, pkt.pz};
    const glm::vec3 rot{pkt.rx, pkt.ry, pkt.rz};
    // Part libera: la simula chi la manda (server / host). Qui la si fa
    // scivolare verso l'ultima posizione ricevuta invece di saltare a 12 Hz;
    // ancorata, nuova o lontanissima: subito al suo posto.
    p.netDriven = pkt.anchored == 0;
    p.netTargetPos = pos;
    p.netTargetRot = rot;
    const bool snap = !existing || !p.netDriven || glm::length(p.position - pos) > 25.f;
    if (snap) {
        p.position = pos;
        p.rotation = rot;
    }
    p.size = {std::max(0.05f, pkt.sx), std::max(0.05f, pkt.sy), std::max(0.05f, pkt.sz)};
    p.color = {pkt.cr, pkt.cg, pkt.cb};
    if (pkt.shape < (uint8_t)PartShape::Count)
        p.shape = (PartShape)pkt.shape;
    if (pkt.material < (uint8_t)PartMaterial::Count)
        p.setMaterial((PartMaterial)pkt.material);
    p.anchored = pkt.anchored != 0;
    p.canCollide = pkt.canCollide != 0;
    if (!existing)
        world.parts.push_back(std::move(scratch));
}

/** Chi toglie davvero la part (il runtime Lua aggiorna anche gli indici degli script). */
using EraseFn = std::function<void(int index)>;

inline void applyDestroy(DataModel& world, uint32_t netId, const EraseFn& erase = nullptr) {
    if (netId == 0) return;
    for (int i = (int)world.parts.size() - 1; i >= 0; --i) {
        if (world.parts[(size_t)i].netId != netId) continue;
        if (erase) erase(i);
        else world.parts.erase(world.parts.begin() + i);
    }
}

inline bool applyPacket(DataModel& world, const PartPacket& pkt, const EraseFn& erase = nullptr) {
    if (pkt.op == (uint8_t)Op::Destroy) {
        applyDestroy(world, pkt.netId, erase);
        return true;
    }
    if (pkt.op == (uint8_t)Op::Upsert) {
        applyUpsert(world, pkt);
        return true;
    }
    return false;
}

/**
 * Client, ogni frame: le part mosse dal server scivolano verso l'ultima
 * posizione ricevuta (circa 90% in 1/6 s), cosi' 12 aggiornamenti al
 * secondo sembrano un movimento continuo.
 */
inline void smoothNetDriven(DataModel& world, float dt) {
    const float k = 1.f - std::exp(-dt * 14.f);
    for (auto& p : world.parts) {
        if (!p.netDriven) continue;
        p.position += (p.netTargetPos - p.position) * k;
        for (int a = 0; a < 3; ++a) {
            float d = std::fmod(p.netTargetRot[a] - p.rotation[a] + 540.f, 360.f) - 180.f;   // giro piu' corto
            p.rotation[a] += d * k;
        }
    }
}

/** Hash grezzo per dirty-check (posizione/size/rot/colore/flag). */
inline uint64_t partStateHash(const WorkspacePart& p) {
    auto mix = [](uint64_t h, uint32_t v) {
        h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    };
    auto fbits = [](float f) {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        return u;
    };
    uint64_t h = p.netId;
    h = mix(h, fbits(p.position.x));
    h = mix(h, fbits(p.position.y));
    h = mix(h, fbits(p.position.z));
    h = mix(h, fbits(p.size.x));
    h = mix(h, fbits(p.size.y));
    h = mix(h, fbits(p.size.z));
    h = mix(h, fbits(p.rotation.x));
    h = mix(h, fbits(p.rotation.y));
    h = mix(h, fbits(p.rotation.z));
    h = mix(h, fbits(p.color.x));
    h = mix(h, (uint32_t)p.shape | ((uint32_t)p.material << 8)
                   | (p.anchored ? 1u << 16 : 0) | (p.canCollide ? 1u << 17 : 0));
    return h;
}

struct SentCache {
    std::unordered_map<uint32_t, uint64_t> hashByNetId;
    void clear() { hashByNetId.clear(); }
};

} // namespace WorldReplication
