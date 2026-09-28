#pragma once


#include <algorithm>
#include <string>
#include <vector>

struct AnimTrack {
    std::string clipName;
    bool playing = false;
    bool looped = true;
    float speed = 1.f;
    float weight = 1.f;
    int priority = 0;
    float time = 0.f;

    bool valid() const { return !clipName.empty(); }
};

struct ModelAnimator {
    std::vector<AnimTrack> tracks;

    int load(const std::string& clipName) {
        for (size_t i = 0; i < tracks.size(); ++i)
            if (tracks[i].clipName == clipName) return (int)i;

        AnimTrack t;
        t.clipName = clipName;
        tracks.push_back(std::move(t));
        return (int)tracks.size() - 1;
    }

    AnimTrack* get(int id) {
        if (id < 0 || id >= (int)tracks.size()) return nullptr;
        return &tracks[(size_t)id];
    }

    const AnimTrack* get(int id) const {
        if (id < 0 || id >= (int)tracks.size()) return nullptr;
        return &tracks[(size_t)id];
    }

    void play(int id) {
        if (AnimTrack* t = get(id)) t->playing = true;
    }

    void stop(int id) {
        if (AnimTrack* t = get(id)) { t->playing = false; t->time = 0.f; }
    }

    void stopAll() {
        for (AnimTrack& t : tracks) { t.playing = false; t.time = 0.f; }
    }

    int dominant() const {
        int best = -1;
        float bestWeight = 0.f;
        int bestPriority = -1;

        for (size_t i = 0; i < tracks.size(); ++i) {
            const AnimTrack& t = tracks[i];
            if (!t.playing || !t.valid() || t.weight <= 0.f) continue;
            if (t.weight > bestWeight ||
                (t.weight == bestWeight && t.priority > bestPriority)) {
                bestWeight = t.weight;
                bestPriority = t.priority;
                best = (int)i;
            }
        }
        return best;
    }

    void advance(float dt) {
        for (AnimTrack& t : tracks)
            if (t.playing) t.time += dt * std::max(0.f, t.speed);
    }

    bool anyPlaying() const {
        for (const AnimTrack& t : tracks)
            if (t.playing) return true;
        return false;
    }
};
