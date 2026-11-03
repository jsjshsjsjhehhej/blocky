#pragma once

// Missioni: catene di obiettivi. Uno script fa Start/Advance/Complete.

#include <string>
#include <vector>

class Missioni {
public:
    struct Step {
        std::string id;
        std::string text;
        bool done = false;
    };
    struct Quest {
        std::string id;
        std::string title;
        std::vector<Step> steps;
        bool active = false;
        bool completed = false;
        int reward = 0;
    };

    static constexpr int kMaxQuests = 24;

    void reset() { quests_.clear(); }
    void update(float) {}

    bool start(const std::string& id, const std::string& title, int reward) {
        if (id.empty() || (int)quests_.size() >= kMaxQuests) return false;
        for (const Quest& q : quests_)
            if (q.id == id) return false;
        Quest q;
        q.id = id;
        q.title = title.empty() ? id : title;
        q.active = true;
        q.reward = reward;
        quests_.push_back(q);
        return true;
    }

    bool addStep(const std::string& questId, const std::string& stepId, const std::string& text) {
        Quest* q = find(questId);
        if (!q || q->completed || stepId.empty()) return false;
        for (const Step& s : q->steps)
            if (s.id == stepId) return false;
        Step s;
        s.id = stepId;
        s.text = text.empty() ? stepId : text;
        q->steps.push_back(s);
        return true;
    }

    bool advance(const std::string& questId, const std::string& stepId) {
        Quest* q = find(questId);
        if (!q || !q->active) return false;
        for (Step& s : q->steps) {
            if (s.id == stepId && !s.done) {
                s.done = true;
                bool all = true;
                for (const Step& x : q->steps)
                    if (!x.done) all = false;
                if (all) {
                    q->completed = true;
                    q->active = false;
                }
                return true;
            }
        }
        return false;
    }

    bool complete(const std::string& questId) {
        Quest* q = find(questId);
        if (!q || q->completed) return false;
        for (Step& s : q->steps) s.done = true;
        q->completed = true;
        q->active = false;
        return true;
    }

    const Quest* get(const std::string& id) const {
        for (const Quest& q : quests_)
            if (q.id == id) return &q;
        return nullptr;
    }

    const std::vector<Quest>& all() const { return quests_; }

    int activeCount() const {
        int n = 0;
        for (const Quest& q : quests_)
            if (q.active) ++n;
        return n;
    }

private:
    Quest* find(const std::string& id) {
        for (Quest& q : quests_)
            if (q.id == id) return &q;
        return nullptr;
    }

    std::vector<Quest> quests_;
};
