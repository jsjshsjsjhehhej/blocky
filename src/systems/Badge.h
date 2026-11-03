#pragma once
#include <string>
#include <vector>
class Badge {
public:
    static constexpr int kMax = 64;
    void reset() { got_.clear(); }
    void update(float) {}
    bool award(const std::string& id) {
        if (id.empty() || id.size() > 32 || (int)got_.size() >= kMax) return false;
        for (const auto& s : got_)
            if (s == id) return false;
        got_.push_back(id);
        last_ = id;
        return true;
    }
    bool has(const std::string& id) const {
        for (const auto& s : got_)
            if (s == id) return true;
        return false;
    }
    const std::string& last() const { return last_; }
    int count() const { return (int)got_.size(); }
private:
    std::vector<std::string> got_;
    std::string last_;
};
