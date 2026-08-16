#pragma once

/**
 * Animazione modelli Workspace: keyframe TRS.
 * Supporta stile professionale AnimateFrame(frame, position):
 *   frame intero + posizione (rot/scale opzionali).
 * Tempo = frame / fps (default 30).
 */

#include "AnimateFrame.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

struct ModelAnimKey {
    float time = 0.f;
    int frame = 0; // AnimateFrame: indice frame (sincronizzato con time via fps)
    glm::vec3 position{0, 2, 0};
    glm::vec3 rotation{0}; // gradi Euler
    glm::vec3 scale{1, 1, 1};
};

struct ModelAnimClip {
    std::string name = "Anim";
    float duration = 2.f;
    int fps = 30; // AnimateFrame fps
    bool looped = true;
    std::vector<ModelAnimKey> keys;

    void sortKeys() {
        std::sort(keys.begin(), keys.end(),
            [](const ModelAnimKey& a, const ModelAnimKey& b) { return a.time < b.time; });
    }

    float frameToTime(int frame) const {
        return (float)std::max(0, frame) / (float)std::max(1, fps);
    }

    int timeToFrame(float t) const {
        return (int)std::lround(t * (float)std::max(1, fps));
    }

    int lastFrame() const {
        int lf = 0;
        for (const auto& k : keys) {
            int f = k.frame > 0 ? k.frame : timeToFrame(k.time);
            if (f > lf) lf = f;
        }
        return lf;
    }

    void syncKeyFrameFields(ModelAnimKey& k) const {
        k.frame = timeToFrame(k.time);
    }

    void ensureSeed(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl) {
        if (!keys.empty()) return;
        ModelAnimKey a{0.f, 0, pos, rot, scl};
        ModelAnimKey b{std::max(0.5f, duration), timeToFrame(std::max(0.5f, duration)), pos, rot, scl};
        keys.push_back(a);
        keys.push_back(b);
    }

    /**
     * true solo se i keyframe descrivono un movimento reale.
     * ensureSeed() crea due key IDENTICHE come punto di partenza: quella non è
     * un'animazione e non deve sovrascrivere ciò che sposti/scali in Studio.
     */
    bool hasMotion() const {
        if (keys.size() < 2) return false;
        const ModelAnimKey& k0 = keys.front();
        const float eps = 1e-4f;
        for (size_t i = 1; i < keys.size(); ++i) {
            const ModelAnimKey& k = keys[i];
            for (int c = 0; c < 3; ++c) {
                if (std::fabs(k.position[c] - k0.position[c]) > eps) return true;
                if (std::fabs(k.rotation[c] - k0.rotation[c]) > eps) return true;
                if (std::fabs(k.scale[c] - k0.scale[c]) > eps) return true;
            }
        }
        return false;
    }

    /** Sposta un clip ancora "fermo" sul nuovo transform (segue le modifiche in Studio). */
    void reseedIfStatic(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl) {
        if (hasMotion()) return;
        for (auto& k : keys) {
            k.position = pos;
            k.rotation = rot;
            k.scale = scl;
        }
    }

    /** API professionale: AnimateFrame(frame, position [, rot, scale]). */
    int animateFrame(int frame, const glm::vec3& pos,
                     const glm::vec3& rot = {0, 0, 0},
                     const glm::vec3& scl = {1, 1, 1}) {
        float t = frameToTime(frame);
        duration = std::max(duration, t);
        int idx = insertOrUpdate(t, pos, rot, scl);
        if (idx >= 0 && idx < (int)keys.size()) {
            keys[idx].frame = frame;
            keys[idx].time = t;
        }
        return idx;
    }

    AnimateClip toAnimateClip() const {
        AnimateClip c;
        c.name = name;
        c.fps = fps;
        c.looped = looped;
        for (const auto& k : keys)
            c.frames.push_back({k.frame > 0 ? k.frame : timeToFrame(k.time), k.position, k.rotation, k.scale});
        c.sortFrames();
        return c;
    }

    ModelAnimKey sample(float t) const {
        if (keys.empty()) return {};
        if (keys.size() == 1) return keys[0];
        float dur = std::max(0.001f, duration);
        if (looped) {
            t = std::fmod(t, dur);
            if (t < 0.f) t += dur;
        } else {
            t = std::clamp(t, 0.f, dur);
        }
        if (t <= keys.front().time) return keys.front();
        if (t >= keys.back().time) return keys.back();
        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            const auto& a = keys[i];
            const auto& b = keys[i + 1];
            if (t > b.time) continue;
            float span = b.time - a.time;
            float f = span > 1e-5f ? (t - a.time) / span : 0.f;
            ModelAnimKey out;
            out.time = t;
            out.position = glm::mix(a.position, b.position, f);
            out.rotation = glm::mix(a.rotation, b.rotation, f);
            out.scale = glm::mix(a.scale, b.scale, f);
            return out;
        }
        return keys.back();
    }

    int insertOrUpdate(float t, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl) {
        t = std::max(0.f, t);
        int fr = timeToFrame(t);
        for (int i = 0; i < (int)keys.size(); ++i) {
            if (std::fabs(keys[i].time - t) < 0.02f) {
                keys[i].position = pos;
                keys[i].rotation = rot;
                keys[i].scale = scl;
                keys[i].time = t;
                keys[i].frame = fr;
                sortKeys();
                for (int j = 0; j < (int)keys.size(); ++j)
                    if (std::fabs(keys[j].time - t) < 0.02f) return j;
                return i;
            }
        }
        keys.push_back({t, fr, pos, rot, scl});
        sortKeys();
        for (int i = 0; i < (int)keys.size(); ++i)
            if (std::fabs(keys[i].time - t) < 0.02f) return i;
        return (int)keys.size() - 1;
    }
};

struct ModelRigInfo {
    bool fileHasBones = false; // info dal primo load StaticMesh
    bool simpleRig = false;    // Root creato dall'utente
    std::string rootName = "Root";

    /** Per l'editor TRS basta il Rig Root (anche se il file ha ossa). */
    bool hasAnimRig() const { return simpleRig || fileHasBones; }
};
