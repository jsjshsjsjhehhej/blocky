#pragma once

// Quanto spesso mandare lo stato di un altro giocatore. Chi e' addosso
// alla camera vuole ogni frame anche sotto lag; chi e' oltre la vista
// puo' aggiornarsi poche volte al secondo, e a livello alto i lontani
// poco importanti non si mandano proprio.

#include "Math.h"

namespace antilag {

struct PeerIn {
    int id = -1;
    float x = 0.f, y = 0.f, z = 0.f;
    float importance = 1.f; // 0..1, 1 = il player, basso = un oggetto
};

struct PeerOut {
    int id = -1;
    float interval = 0.f; // 0 = ogni frame
    bool drop = false;
    float distance = 0.f;
};

inline PeerOut planPeer(const PeerIn& peer, V3 self, int level, float baseInterval) {
    PeerOut o;
    o.id = peer.id;
    float dx = peer.x - self.x;
    float dy = peer.y - self.y;
    float dz = peer.z - self.z;
    o.distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    if (baseInterval < 0.f) baseInterval = 0.f;
    float imp = clampf(peer.importance, 0.f, 1.f);

    if (o.distance < 28.f || imp > 0.85f) {
        o.interval = level >= 5 ? std::max(baseInterval, 0.03f) : 0.f;
        o.drop = false;
        return o;
    }
    if (o.distance < 80.f) {
        float scale = 1.f + (float)level * 0.35f;
        o.interval = std::max(baseInterval, 0.04f) * scale * (1.15f - 0.5f * imp);
        o.drop = false;
        return o;
    }
    if (level >= 6 && imp < 0.25f && o.distance > 120.f) {
        o.drop = true;
        o.interval = 1.f;
        return o;
    }
    float far = 0.1f + (float)level * 0.04f;
    o.interval = std::max(baseInterval, far) * (1.4f - imp);
    if (o.interval > 0.5f) o.interval = 0.5f;
    o.drop = false;
    return o;
}

inline int planPeers(const PeerIn* peers, int count, V3 self, int level,
                     float baseInterval, PeerOut* out, int outCap) {
    if (!peers || !out || count <= 0 || outCap <= 0) return 0;
    int n = 0;
    for (int i = 0; i < count && n < outCap; ++i)
        out[n++] = planPeer(peers[i], self, level, baseInterval);
    return n;
}

}
