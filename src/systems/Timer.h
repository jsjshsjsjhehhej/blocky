#pragma once
#include <algorithm>
#include <string>
#include <vector>
class Timer {
public:
    struct Slot { std::string name; float left = 0; bool loop = false; float period = 0; bool fired = false; };
    static constexpr int kMax = 32;
    void reset() { slots_.clear(); fired_.clear(); }
    void update(float dt) {
        if (dt < 0.f) dt = 0.f;
        fired_.clear();
        for (Slot& s : slots_) {
            s.fired = false;
            if (s.left <= 0.f && !s.loop) continue;
            s.left -= dt;
            if (s.left <= 0.f) {
                s.fired = true;
                fired_.push_back(s.name);
                if (s.loop) s.left += s.period > 0.01f ? s.period : 1.f;
                else s.left = 0.f;
            }
        }
    }
    bool start(const std::string& name, float seconds, bool loop) {
        if (name.empty() || seconds <= 0.f) return false;
        for (Slot& s : slots_) {
            if (s.name == name) { s.left = seconds; s.loop = loop; s.period = seconds; return true; }
        }
        if ((int)slots_.size() >= kMax) return false;
        Slot s; s.name = name; s.left = seconds; s.loop = loop; s.period = seconds;
        slots_.push_back(s);
        return true;
    }
    bool done(const std::string& name) const {
        for (const auto& n : fired_) if (n == name) return true;
        return false;
    }
    float left(const std::string& name) const {
        for (const Slot& s : slots_) if (s.name == name) return s.left;
        return 0.f;
    }
    const std::vector<std::string>& fired() const { return fired_; }
private:
    std::vector<Slot> slots_;
    std::vector<std::string> fired_;
};
