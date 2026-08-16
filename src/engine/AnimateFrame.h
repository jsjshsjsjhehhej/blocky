#pragma once

/**
 * AnimateFrame — animazione professionale a frame.
 *
 * Ogni chiave = (frame, position [, rotation, scale]).
 * Pensato come timeline da Studio / Lua:
 *   AnimateFrame(0,  Vector3.new(0, 2, 0))
 *   AnimateFrame(15, Vector3.new(0, 4, 0))
 *   AnimateFrame(30, Vector3.new(0, 2, 0))
 *
 * Sample a fps fisso (default 30): frame → tempo = frame / fps
 */

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

struct AnimateFrame {
    int frame = 0;
    glm::vec3 position{0, 0, 0};
    glm::vec3 rotation{0, 0, 0}; // gradi
    glm::vec3 scale{1, 1, 1};
};

struct AnimateClip {
    std::string name = "Clip";
    int fps = 30;
    bool looped = true;
    std::vector<AnimateFrame> frames;

    void sortFrames() {
        std::sort(frames.begin(), frames.end(),
            [](const AnimateFrame& a, const AnimateFrame& b) { return a.frame < b.frame; });
    }

    int lastFrame() const {
        if (frames.empty()) return 0;
        return frames.back().frame;
    }

    float durationSeconds() const {
        int lf = lastFrame();
        float f = (float)std::max(1, fps);
        return (float)lf / f;
    }

    /** Imposta o aggiorna un frame (stesso numero = overwrite). */
    int setFrame(int frame, const glm::vec3& pos,
                 const glm::vec3& rot = {0, 0, 0},
                 const glm::vec3& scl = {1, 1, 1}) {
        frame = std::max(0, frame);
        for (int i = 0; i < (int)frames.size(); ++i) {
            if (frames[i].frame == frame) {
                frames[i].position = pos;
                frames[i].rotation = rot;
                frames[i].scale = scl;
                return i;
            }
        }
        frames.push_back({frame, pos, rot, scl});
        sortFrames();
        for (int i = 0; i < (int)frames.size(); ++i)
            if (frames[i].frame == frame) return i;
        return (int)frames.size() - 1;
    }

    /** Alias stile API: AnimateFrame(frame, position). */
    int animateFrame(int frame, const glm::vec3& position) {
        return setFrame(frame, position);
    }

    AnimateFrame sampleAtFrame(float frameF) const {
        if (frames.empty()) return {};
        if (frames.size() == 1) return frames[0];

        float maxF = (float)lastFrame();
        if (maxF < 1.f) maxF = 1.f;

        if (looped) {
            frameF = std::fmod(frameF, maxF + 1.f);
            if (frameF < 0.f) frameF += maxF + 1.f;
        } else {
            frameF = std::clamp(frameF, 0.f, maxF);
        }

        if (frameF <= (float)frames.front().frame) return frames.front();
        if (frameF >= (float)frames.back().frame) return frames.back();

        for (size_t i = 0; i + 1 < frames.size(); ++i) {
            const auto& a = frames[i];
            const auto& b = frames[i + 1];
            if (frameF > (float)b.frame) continue;
            float span = (float)(b.frame - a.frame);
            float t = span > 1e-5f ? (frameF - (float)a.frame) / span : 0.f;
            AnimateFrame out;
            out.frame = (int)std::lround(frameF);
            out.position = glm::mix(a.position, b.position, t);
            out.rotation = glm::mix(a.rotation, b.rotation, t);
            out.scale = glm::mix(a.scale, b.scale, t);
            return out;
        }
        return frames.back();
    }

    AnimateFrame sampleAtTime(float seconds) const {
        float f = (float)std::max(1, fps);
        return sampleAtFrame(seconds * f);
    }
};

/** Player runtime per un AnimateClip. */
struct AnimatePlayer {
    AnimateClip clip;
    float time = 0.f;
    bool playing = false;
    float speed = 1.f;

    void play() { playing = true; time = 0.f; }
    void stop() { playing = false; time = 0.f; }
    void pause() { playing = false; }

    AnimateFrame step(float dt) {
        if (playing) time += dt * speed;
        return clip.sampleAtTime(time);
    }

    float currentFrame() const {
        return time * (float)std::max(1, clip.fps);
    }
};
