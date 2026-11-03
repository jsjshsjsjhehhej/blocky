#pragma once

// Forno: fusione stile furnace alpha (un input, un combustibile, un output).

#include <algorithm>
#include <string>
#include <vector>

class Forno {
public:
    struct Recipe {
        std::string in, out, outName;
        int outN = 1;
        float time = 8.f;
    };
    struct Station {
        std::string name;
        std::string input;
        std::string fuel;
        std::string output;
        int outCount = 0;
        float left = 0;
        float fuelLeft = 0;
        bool lit = false;
    };

    static constexpr int kMax = 16;

    void reset() {
        stations_.clear();
        recipes_.clear();
        seedVanilla();
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        for (Station& s : stations_) {
            if (s.fuelLeft > 0.f) {
                s.fuelLeft -= dt;
                s.lit = true;
            } else {
                s.lit = false;
            }
            if (s.input.empty() || s.left <= 0.f) continue;
            const Recipe* r = findRecipe(s.input);
            if (!r) { s.left = 0; continue; }
            if (!s.lit) {
                if (s.fuel.empty()) continue;
                s.fuel.clear();
                s.fuelLeft = 8.f;
                s.lit = true;
            }
            s.left -= dt;
            if (s.left > 0.f) continue;
            if (!s.output.empty() && s.output != r->out) continue;
            s.output = r->out;
            s.outCount += r->outN;
            s.input.clear();
            s.left = 0;
        }
    }

    bool add(const std::string& name) {
        if (name.empty() || (int)stations_.size() >= kMax) return false;
        for (const Station& s : stations_)
            if (s.name == name) return false;
        Station s;
        s.name = name;
        stations_.push_back(s);
        return true;
    }

    bool put(const std::string& name, const std::string& item, bool asFuel) {
        Station* s = find(name);
        if (!s || item.empty()) return false;
        if (asFuel) {
            s->fuel = item;
            return true;
        }
        s->input = item;
        const Recipe* r = findRecipe(item);
        s->left = r ? r->time : 0.f;
        return r != nullptr;
    }

    std::string take(const std::string& name) {
        Station* s = find(name);
        if (!s || s->output.empty()) return {};
        std::string o = s->output;
        s->output.clear();
        s->outCount = 0;
        return o;
    }

    bool addRecipe(const std::string& in, const std::string& out, int n, float time) {
        if (in.empty() || out.empty() || findRecipe(in)) return false;
        Recipe r;
        r.in = in; r.out = out; r.outName = out; r.outN = n > 0 ? n : 1;
        r.time = time > 0.3f ? time : 8.f;
        recipes_.push_back(r);
        return true;
    }

    const Station* get(const std::string& name) const {
        for (const Station& s : stations_)
            if (s.name == name) return &s;
        return nullptr;
    }
    const std::vector<Station>& all() const { return stations_; }

private:
    void seedVanilla() {
        addRecipe("iron", "ironbar", 1, 8.f);
        addRecipe("gold", "goldbar", 1, 8.f);
        addRecipe("sand", "glass", 1, 6.f);
        addRecipe("cobble", "stone", 1, 8.f);
        addRecipe("wood", "coal", 1, 5.f);
        addRecipe("clay", "brick", 1, 8.f);
        addRecipe("pork", "cookedpork", 1, 6.f);
        addRecipe("beef", "cookedbeef", 1, 6.f);
        addRecipe("chicken", "cookedchicken", 1, 6.f);
        addRecipe("fish", "cookedfish", 1, 5.f);
    }
    Station* find(const std::string& name) {
        for (Station& s : stations_)
            if (s.name == name) return &s;
        return nullptr;
    }
    const Recipe* findRecipe(const std::string& in) const {
        for (const Recipe& r : recipes_)
            if (r.in == in) return &r;
        return nullptr;
    }

    std::vector<Station> stations_;
    std::vector<Recipe> recipes_;
};
