#pragma once

// Prossimita: prompt 3D. Scan dalla posizione del player, HUD mostra il testo,
// TakeActivated() quando lo script (o il tasto) conferma.

#include <cmath>
#include <string>
#include <vector>

class Prossimita {
public:
    struct Prompt {
        std::string name;
        std::string text;
        std::string objectText;
        float x = 0, y = 0, z = 0;
        float radius = 6.f;
        bool enabled = true;
        int partIndex = -1; // se >=0, posizione sync da Part
    };

    static constexpr int kMax = 32;

    void reset() {
        prompts_.clear();
        nearest_.clear();
        activated_.clear();
        hold_ = false;
    }

    void update(float) {}

    bool add(const std::string& name, const std::string& text,
             float x, float y, float z, float radius, int partIndex = -1) {
        if (name.empty() || name.size() > 24 || (int)prompts_.size() >= kMax)
            return false;
        for (Prompt& p : prompts_) {
            if (p.name == name) {
                p.text = text.empty() ? "Usa" : text.substr(0, 32);
                p.x = x; p.y = y; p.z = z;
                p.radius = radius > 0.5f ? radius : 0.5f;
                p.partIndex = partIndex;
                p.enabled = true;
                return true;
            }
        }
        Prompt p;
        p.name = name;
        p.text = text.empty() ? "Usa" : text.substr(0, 32);
        p.x = x; p.y = y; p.z = z;
        p.radius = radius > 0.5f ? radius : 0.5f;
        p.partIndex = partIndex;
        prompts_.push_back(p);
        return true;
    }

    Prompt* find(const std::string& name) {
        for (Prompt& p : prompts_)
            if (p.name == name) return &p;
        return nullptr;
    }

    bool remove(const std::string& name) {
        for (auto it = prompts_.begin(); it != prompts_.end(); ++it) {
            if (it->name == name) {
                prompts_.erase(it);
                if (nearest_ == name) nearest_.clear();
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
        float best = 1e9f;
        for (const Prompt& p : prompts_) {
            if (!p.enabled) continue;
            const float dx = px - p.x, dy = py - p.y, dz = pz - p.z;
            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (d <= p.radius && d < best) {
                best = d;
                nearest_ = p.name;
            }
        }
    }

    const std::vector<Prompt>& all() const { return prompts_; }

    /** La part `removed` e' stata tolta: i prompt dopo di lei scalano, i suoi si staccano. */
    void onPartErased(int removed) {
        for (auto& p : prompts_) {
            if (p.partIndex == removed) p.partIndex = -1;
            else if (p.partIndex > removed) --p.partIndex;
        }
    }

    void requestActivate() { hold_ = true; }

    std::string takeActivated() {
        if (!hold_ || nearest_.empty()) {
            hold_ = false;
            return {};
        }
        hold_ = false;
        const std::string n = nearest_;
        return n;
    }

    const std::string& nearest() const { return nearest_; }

    const char* nearestText() const {
        for (const Prompt& p : prompts_)
            if (p.name == nearest_) return p.text.c_str();
        return "";
    }

    int count() const { return (int)prompts_.size(); }

private:
    std::vector<Prompt> prompts_;
    std::string nearest_;
    std::string activated_;
    bool hold_ = false;
};
