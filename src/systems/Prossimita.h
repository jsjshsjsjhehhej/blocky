#pragma once

// Prossimita: prompt sull'oggetto, come in Roblox ("E  Open").
// Scan dalla posizione del player. Il tasto del prompt (o Y sul pad) lo attiva.
// HoldDuration > 0: tieni premuto finche' l'anello si chiude.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

inline char proxNormKey(char c) {
    if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
    if (c < 'A' || c > 'Z') return 'E';
    return c;
}

class Prossimita {
public:
    struct Prompt {
        std::string name;
        std::string text;
        std::string objectText;
        float x = 0, y = 0, z = 0;
        float radius = 6.f;
        bool enabled = true;
        int partIndex = -1;
        char key = 'E';
        float holdDuration = 0.f;
        float hold = 0.f;
        bool fed = false;
    };

    struct Shown {
        int index = -1;
        float dist = 0.f;
    };

    // Una mappa grande (citta', casse, scavi) ne ha centinaia. 4096 non pesa:
    // ogni frame si misura solo la distanza, e se ne disegnano al massimo 4.
    static constexpr int kMax = 4096;

    void reset() {
        prompts_.clear();
        shown_.clear();
        pending_.clear();
        nearest_.clear();
        hold_ = false;
    }

    void update(float) {}

    bool add(const std::string& name, const std::string& text,
             float x, float y, float z, float radius, int partIndex = -1) {
        if (name.empty() || name.size() > 24 || (int)prompts_.size() >= kMax)
            return false;
        for (Prompt& p : prompts_) {
            if (p.name == name) {
                p.text = text.empty() ? "Open" : text.substr(0, 32);
                p.x = x; p.y = y; p.z = z;
                p.radius = radius > 0.5f ? radius : 0.5f;
                p.partIndex = partIndex;
                p.enabled = true;
                return true;
            }
        }
        Prompt p;
        p.name = name;
        p.text = text.empty() ? "Open" : text.substr(0, 32);
        p.x = x; p.y = y; p.z = z;
        p.radius = radius > 0.5f ? radius : 0.5f;
        p.partIndex = partIndex;
        prompts_.push_back(p);
        return true;
    }

    void style(const std::string& name, const std::string& objectText, char key, float hold) {
        Prompt* p = find(name);
        if (!p) return;
        p->objectText = objectText.substr(0, 32);
        p->key = proxNormKey(key);
        p->holdDuration = std::clamp(hold, 0.f, 8.f);
    }

    Prompt* find(const std::string& name) {
        for (Prompt& p : prompts_)
            if (p.name == name) return &p;
        return nullptr;
    }

    const Prompt* promptPtr(int index) const {
        if (index < 0 || index >= (int)prompts_.size()) return nullptr;
        return &prompts_[index];
    }

    bool remove(const std::string& name) {
        for (auto it = prompts_.begin(); it != prompts_.end(); ++it) {
            if (it->name == name) {
                prompts_.erase(it);
                if (nearest_ == name) nearest_.clear();
                shown_.clear();
                pending_.erase(std::remove(pending_.begin(), pending_.end(), name), pending_.end());
                return true;
            }
        }
        return false;
    }

    void setEnabled(const std::string& name, bool on) {
        for (Prompt& p : prompts_)
            if (p.name == name) p.enabled = on;
    }

    template <typename PartFn>
    void syncParts(PartFn&& getPartPos) {
        for (Prompt& p : prompts_) {
            if (p.partIndex < 0) continue;
            float x = 0, y = 0, z = 0;
            if (getPartPos(p.partIndex, x, y, z)) {
                p.x = x; p.y = y; p.z = z;
            }
        }
    }

    void scan(float px, float py, float pz) {
        nearest_.clear();
        shown_.clear();
        struct Cand { int i; float d; };
        std::vector<Cand> cands;
        cands.reserve(prompts_.size());
        for (int i = 0; i < (int)prompts_.size(); ++i) {
            const Prompt& p = prompts_[i];
            if (!p.enabled) continue;
            const float dx = px - p.x, dy = py - p.y, dz = pz - p.z;
            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (d <= p.radius) cands.push_back({i, d});
        }
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.d < b.d; });
        if ((int)cands.size() > 4) cands.resize(4);
        std::vector<char> keep(prompts_.size(), 0);
        for (const Cand& c : cands) {
            shown_.push_back({c.i, c.d});
            keep[c.i] = 1;
        }
        for (int i = 0; i < (int)prompts_.size(); ++i)
            if (!keep[i]) prompts_[i].hold = 0.f;
        if (!shown_.empty()) nearest_ = prompts_[shown_[0].index].name;
    }

    const std::vector<Prompt>& all() const { return prompts_; }
    const std::vector<Shown>& shown() const { return shown_; }

    bool usesKey(char key) const {
        key = proxNormKey(key);
        for (const Shown& s : shown_) {
            const Prompt& p = prompts_[s.index];
            if (p.enabled && p.key == key) return true;
        }
        return false;
    }

    /**
     * La part removed e' stata tolta: i prompt dentro di lei se ne vanno con lei
     * (come su Roblox, Destroy della part distrugge i figli), gli indici dopo
     * scalano. Restituisce i nomi dei prompt tolti (per scollegare i Triggered).
     */
    std::vector<std::string> onPartErased(int removed) {
        std::vector<std::string> gone;
        for (auto it = prompts_.begin(); it != prompts_.end();) {
            if (it->partIndex == removed) {
                gone.push_back(it->name);
                if (nearest_ == it->name) nearest_.clear();
                it = prompts_.erase(it);
                continue;
            }
            if (it->partIndex > removed) --it->partIndex;
            ++it;
        }
        if (!gone.empty()) {
            shown_.clear();
            for (const auto& g : gone)
                pending_.erase(std::remove(pending_.begin(), pending_.end(), g), pending_.end());
        }
        return gone;
    }

    void beginInput() {
        for (Shown& s : shown_) prompts_[s.index].fed = false;
    }

    void feedKey(char key, bool down, bool pressed, float dt) {
        key = proxNormKey(key);
        Prompt* best = nullptr;
        float bestD = 1e9f;
        for (const Shown& s : shown_) {
            Prompt& p = prompts_[s.index];
            if (!p.enabled || p.key != key) continue;
            if (s.dist < bestD) { bestD = s.dist; best = &p; }
        }
        if (best) apply(*best, down, pressed, dt);
    }

    // Y del pad: il prompt piu' vicino, qualunque tasto mostri.
    void feedInteract(bool down, bool pressed, float dt) {
        if (shown_.empty()) return;
        apply(prompts_[shown_[0].index], down, pressed, dt);
    }

    void releaseHolds() {
        for (Prompt& p : prompts_) p.hold = 0.f;
    }

    void requestActivate() { hold_ = true; }

    std::string takeActivated() {
        if (!hold_ || nearest_.empty()) {
            hold_ = false;
            return {};
        }
        hold_ = false;
        return nearest_;
    }

    std::string takeQueued() {
        if (pending_.empty()) return {};
        std::string n = pending_.front();
        pending_.erase(pending_.begin());
        return n;
    }

    const std::string& nearest() const { return nearest_; }

    const char* nearestText() const {
        for (const Prompt& p : prompts_)
            if (p.name == nearest_) return p.text.c_str();
        return "";
    }

    int count() const { return (int)prompts_.size(); }

    std::string& clickHint() { return clickHint_; }
    const std::string& clickHint() const { return clickHint_; }

private:
    void queue(const std::string& n) {
        for (const std::string& q : pending_)
            if (q == n) return;
        if ((int)pending_.size() < 8) pending_.push_back(n);
    }

    void apply(Prompt& p, bool down, bool pressed, float dt) {
        if (down || pressed) {
            if (p.fed) return;
            p.fed = true;
            if (p.holdDuration <= 0.01f) {
                if (pressed) queue(p.name);
            } else {
                p.hold += dt;
                if (p.hold >= p.holdDuration) {
                    p.hold = 0.f;
                    queue(p.name);
                }
            }
        } else if (!p.fed) {
            p.hold = std::max(0.f, p.hold - dt * 2.5f);
        }
    }

    std::vector<Prompt> prompts_;
    std::vector<Shown> shown_;
    std::vector<std::string> pending_;
    std::string nearest_;
    std::string clickHint_;
    bool hold_ = false;
};
