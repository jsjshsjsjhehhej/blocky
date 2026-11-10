#pragma once

// Assegna una banda di dettaglio a ogni part finche' il punteggio del
// frame non e' esaurito. Full costa 4, Flat 2, Impostor 1. Si spende
// prima sulle part grandi a schermo. Una luce non riceve mai Skip, anche
// se il punteggio e' gia' a zero: al massimo resta Flat e si va in debito
// di qualche punto.

#include "LodLadder.h"

#include <algorithm>
#include <vector>

namespace antilag {

struct DetailIn {
    int index = -1;
    float screenPx = 0.f;
    bool important = false;
    bool transparent = false;
};

struct DetailOut {
    int index = -1;
    Lod lod = Lod::Skip;
    int cost = 0;
};

inline int detailPointsForLevel(int level, int basePoints) {
    if (basePoints < 64) basePoints = 64;
    static const float kScale[8] = {
        1.f, 0.85f, 0.70f, 0.55f, 0.40f, 0.28f, 0.18f, 0.12f
    };
    if (level < 0) level = 0;
    if (level > 7) level = 7;
    int pts = (int)(basePoints * kScale[level]);
    if (pts < 48) pts = 48;
    return pts;
}

inline int assignDetail(const DetailIn* items, int count, int pointBudget,
                        bool studs, DetailOut* out, int outCap) {
    if (!items || !out || count <= 0 || outCap <= 0) return 0;
    int n = count < outCap ? count : outCap;
    if (pointBudget < 16) pointBudget = 16;

    std::vector<int> order((size_t)n);
    for (int i = 0; i < n; ++i) order[(size_t)i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        if (items[a].important != items[b].important)
            return items[a].important && !items[b].important;
        return items[a].screenPx > items[b].screenPx;
    });

    int spent = 0;
    int written = 0;
    for (int slot = 0; slot < n; ++slot) {
        const DetailIn& it = items[order[(size_t)slot]];
        int left = pointBudget - spent;
        Lod lod = Lod::Skip;
        if (left >= lodCost(Lod::Full) && it.screenPx >= 18.f)
            lod = lodFor(it.screenPx, studs, it.important);
        else if (left >= lodCost(Lod::Flat) && it.screenPx >= 3.f)
            lod = Lod::Flat;
        else if (left >= lodCost(Lod::Impostor) && it.screenPx >= 1.2f && !it.important)
            lod = Lod::Impostor;
        else if (it.important)
            lod = Lod::Flat;
        else if (it.screenPx >= 8.f && left >= lodCost(Lod::Flat))
            lod = Lod::Flat;
        else
            lod = Lod::Skip;

        if (lod == Lod::Full && !studs) lod = Lod::Flat;
        if (lod == Lod::Full && it.screenPx < 18.f) lod = Lod::Flat;
        int cost = lodCost(lod);
        if (lod != Lod::Skip && !it.important && spent + cost > pointBudget && spent > 0)
            lod = Lod::Skip, cost = 0;
        DetailOut& o = out[written++];
        o.index = it.index >= 0 ? it.index : order[(size_t)slot];
        o.lod = lod;
        o.cost = cost;
        spent += cost;
    }
    return written;
}

inline int spentPoints(const DetailOut* outs, int n) {
    int s = 0;
    if (!outs) return 0;
    for (int i = 0; i < n; ++i) s += outs[i].cost;
    return s;
}

inline int countLod(const DetailOut* outs, int n, Lod lod) {
    int c = 0;
    if (!outs) return 0;
    for (int i = 0; i < n; ++i)
        if (outs[i].lod == lod) ++c;
    return c;
}

}
