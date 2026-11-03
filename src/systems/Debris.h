#pragma once

// Debris: dopo N secondi i nomi in lista sono "scaduti". Lo script distrugge
// la part quando TakeExpired() restituisce il nome.

#include <algorithm>
#include <string>
#include <vector>

class Debris {
public:
    struct Item {
        std::string name;
        float left = 0.f;
    };

    static constexpr int kMax = 64;

    void reset() {
        items_.clear();
        expired_.clear();
    }

    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        expired_.clear();
        for (Item& it : items_) it.left -= dt;
        std::vector<Item> keep;
        keep.reserve(items_.size());
        for (Item& it : items_) {
            if (it.left <= 0.f) expired_.push_back(it.name);
            else keep.push_back(it);
        }
        items_.swap(keep);
    }

    bool add(const std::string& name, float seconds) {
        if (name.empty() || name.size() > 40 || seconds < 0.f) return false;
        if ((int)items_.size() >= kMax) return false;
        Item it;
        it.name = name;
        it.left = seconds;
        items_.push_back(it);
        return true;
    }

    const std::vector<std::string>& expired() const { return expired_; }
    int pending() const { return (int)items_.size(); }

    std::string takeExpired() {
        if (expired_.empty()) return {};
        std::string n = expired_.front();
        expired_.erase(expired_.begin());
        return n;
    }

private:
    std::vector<Item> items_;
    std::vector<std::string> expired_;
};
