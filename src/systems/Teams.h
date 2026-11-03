#pragma once

#include <algorithm>
#include <string>
#include <vector>

class Teams {
public:
    struct Team {
        std::string name;
        float r = 0.2f, g = 0.6f, b = 1.f;
        int members = 0;
    };

    static constexpr int kMaxTeams = 8;

    void reset() {
        teams_.clear();
        mine_.clear();
    }
    void update(float) {}

    bool create(const std::string& name, float r, float g, float b) {
        if (name.empty() || name.size() > 20 || (int)teams_.size() >= kMaxTeams)
            return false;
        for (const Team& t : teams_)
            if (t.name == name) return false;
        Team t;
        t.name = name;
        t.r = std::clamp(r, 0.f, 1.f);
        t.g = std::clamp(g, 0.f, 1.f);
        t.b = std::clamp(b, 0.f, 1.f);
        teams_.push_back(t);
        return true;
    }

    bool join(const std::string& name) {
        Team* t = find(name);
        if (!t) return false;
        if (!mine_.empty()) {
            if (Team* old = find(mine_)) {
                old->members = std::max(0, old->members - 1);
            }
        }
        mine_ = name;
        t->members += 1;
        return true;
    }

    void leave() {
        if (Team* t = find(mine_))
            t->members = std::max(0, t->members - 1);
        mine_.clear();
    }

    const std::string& mine() const { return mine_; }
    const std::vector<Team>& all() const { return teams_; }

private:
    Team* find(const std::string& name) {
        for (Team& t : teams_)
            if (t.name == name) return &t;
        return nullptr;
    }

    std::vector<Team> teams_;
    std::string mine_;
};
