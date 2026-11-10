#pragma once

// Chi gira in questo frame. Il budget Lua vecchio, se sforato, staccava
// TUTTI i Heartbeat fino al riavvio dello script. Qui invece si tiene il
// costo medio di ciascuno e, quando il frame non ce la fa, si saltano
// prima i piu' cari e quelli che hanno gia' girato da poco. Il successivo
// li recupera: uno script lento resta vivo, non viene cancellato.

#include <algorithm>
#include <cstdint>

namespace antilag {

struct ScriptStat {
    int id = -1;
    float costEmaUs = 80.f;
    int priority = 10;
    float distance = 0.f;
    int lastRan = -100000;
    int skips = 0;
    int runs = 0;
    bool alive = false;
    bool seen = false;
};

class ScriptRoster {
public:
    static constexpr int kCap = 256;
    static constexpr int kCritical = 100;

    void reset() {
        for (auto& s : slots_) s = {};
        used_ = 0;
        frame_ = 0;
    }

    int frame() const { return frame_; }
    int aliveCount() const {
        int n = 0;
        for (int i = 0; i < used_; ++i)
            if (slots_[i].alive) ++n;
        return n;
    }

    void beginSweep() {
        for (int i = 0; i < used_; ++i)
            slots_[i].seen = false;
    }

    // Crea lo slot se manca. Non azzera il costo: la media serve proprio
    // fra un frame e l'altro.
    void consider(int id, int priority = 10, float distance = 0.f) {
        ScriptStat* s = find(id);
        if (!s) s = allocate(id);
        if (!s) return;
        s->alive = true;
        s->seen = true;
        s->priority = priority;
        s->distance = distance < 0.f ? 0.f : distance;
    }

    void endSweep() {
        for (int i = 0; i < used_; ++i) {
            if (slots_[i].alive && !slots_[i].seen)
                slots_[i].alive = false;
        }
    }

    void noteCost(int id, float us) {
        if (us < 0.f) us = 0.f;
        if (us > 100000.f) us = 100000.f;
        ScriptStat* s = find(id);
        if (!s) return;
        if (s->runs <= 0) s->costEmaUs = us;
        else s->costEmaUs = s->costEmaUs * 0.65f + us * 0.35f;
    }

    const ScriptStat* findStat(int id) const {
        for (int i = 0; i < used_; ++i)
            if (slots_[i].id == id && slots_[i].alive) return &slots_[i];
        return nullptr;
    }

    // Riempie outIds con chi deve girare. Il primo entra anche se da solo
    // supera il budget: un Heartbeat unico va eseguito, il tetto duro del
    // runtime lo ferma se e' un ciclo infinito. Dal secondo in poi si
    // rispetta il budget.
    int select(int stride, int budgetUs, int* outIds, int outCap) {
        ++frame_;
        if (stride < 1) stride = 1;
        if (budgetUs < 1) budgetUs = 1;
        if (!outIds || outCap <= 0) return 0;

        struct Cand {
            int slot;
            int overdue;
            int priority;
            float dist;
            float cost;
            bool critical;
        };
        Cand due[kCap];
        int nDue = 0;
        for (int i = 0; i < used_; ++i) {
            ScriptStat& s = slots_[i];
            if (!s.alive) continue;
            bool critical = s.priority >= kCritical;
            bool turn = stride <= 1 || ((frame_ + s.id) % stride) == 0;
            if (!critical && !turn) continue;
            Cand& c = due[nDue++];
            c.slot = i;
            c.overdue = frame_ - s.lastRan;
            c.priority = s.priority;
            c.dist = s.distance;
            c.cost = s.costEmaUs < 1.f ? 50.f : s.costEmaUs;
            c.critical = critical;
        }

        std::sort(due, due + nDue, [](const Cand& a, const Cand& b) {
            if (a.critical != b.critical) return a.critical && !b.critical;
            if (a.overdue != b.overdue) return a.overdue > b.overdue;
            if (a.priority != b.priority) return a.priority > b.priority;
            if (a.dist != b.dist) return a.dist < b.dist;
            return a.cost < b.cost;
        });

        int spent = 0;
        int n = 0;
        for (int i = 0; i < nDue; ++i) {
            Cand& c = due[i];
            int cost = (int)c.cost;
            if (cost < 1) cost = 1;
            bool fits = n == 0 || spent + cost <= budgetUs;
            if (!fits || n >= outCap) {
                slots_[c.slot].skips += 1;
                continue;
            }
            outIds[n++] = slots_[c.slot].id;
            slots_[c.slot].lastRan = frame_;
            slots_[c.slot].runs += 1;
            spent += cost;
        }
        return n;
    }

private:
    ScriptStat* find(int id) {
        for (int i = 0; i < used_; ++i)
            if (slots_[i].id == id) return &slots_[i];
        return nullptr;
    }

    ScriptStat* allocate(int id) {
        for (int i = 0; i < used_; ++i) {
            if (!slots_[i].alive) {
                slots_[i] = {};
                slots_[i].id = id;
                slots_[i].costEmaUs = 80.f;
                return &slots_[i];
            }
        }
        if (used_ >= kCap) return nullptr;
        ScriptStat& s = slots_[used_++];
        s = {};
        s.id = id;
        s.costEmaUs = 80.f;
        return &s;
    }

    ScriptStat slots_[kCap];
    int used_ = 0;
    int frame_ = 0;
};

}
