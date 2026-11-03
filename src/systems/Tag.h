#pragma once

// Tag: etichette su id interi (indice part, modello, ...). Come CollectionService.

#include <algorithm>
#include <map>
#include <string>
#include <vector>

class Tag {
public:
    static constexpr int kMaxTags = 64;
    static constexpr int kMaxPerTag = 256;

    void reset() { tags_.clear(); }
    void update(float) {}

    bool add(const std::string& tag, int id) {
        if (tag.empty() || tag.size() > 32) return false;
        auto& v = tags_[tag];
        if ((int)v.size() >= kMaxPerTag) return false;
        for (int x : v)
            if (x == id) return true;
        v.push_back(id);
        return true;
    }

    bool remove(const std::string& tag, int id) {
        auto it = tags_.find(tag);
        if (it == tags_.end()) return false;
        auto& v = it->second;
        const auto n = v.size();
        v.erase(std::remove(v.begin(), v.end(), id), v.end());
        return v.size() != n;
    }

    bool has(const std::string& tag, int id) const {
        auto it = tags_.find(tag);
        if (it == tags_.end()) return false;
        for (int x : it->second)
            if (x == id) return true;
        return false;
    }

    std::vector<int> get(const std::string& tag) const {
        auto it = tags_.find(tag);
        return it == tags_.end() ? std::vector<int>{} : it->second;
    }

    int tagCount() const { return (int)tags_.size(); }

    std::vector<std::string> allTagNames() const {
        std::vector<std::string> out;
        out.reserve(tags_.size());
        for (const auto& kv : tags_)
            out.push_back(kv.first);
        return out;
    }

private:
    std::map<std::string, std::vector<int>> tags_;
};
