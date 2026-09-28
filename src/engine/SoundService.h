#pragma once


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// I backend audio (PlaySound/MCI su Windows, OpenSL ES su Android) stanno
// tutti dietro plat::sys. Questo file non include piu' <windows.h>: non deve
// sapere su cosa gira, e' esattamente il punto.
#include "platform/SystemApi.h"
#include "platform/PlatformServices.h"

struct SoundInstance {
    std::string name = "Sound";
    std::string soundId;
    float volume = 1.f;
    bool looped = false;
    bool playing = false;
};

class SoundService {
public:
    std::vector<SoundInstance> sounds;
    std::string lastError;
    int nextIndex = 1;

    SoundInstance& createSound(const std::string& name = {}) {
        SoundInstance s;
        s.name = name.empty() ? ("Sound" + std::to_string(nextIndex++)) : name;
        sounds.push_back(std::move(s));
        return sounds.back();
    }

    SoundInstance* find(const std::string& name) {
        for (auto& s : sounds)
            if (s.name == name) return &s;
        return nullptr;
    }

    bool play(SoundInstance& s) {
        if (s.soundId.empty()) {
            lastError = "SoundId vuoto";
            return false;
        }
        stopAll();

        // La piattaforma sa dire se l'audio esiste: usiamo quella risposta
        // invece di scoprirlo da un #else dimenticato.
        if (!plat::has(plat::Service::Audio)) {
            lastError = "Audio non supportato su questa piattaforma";
            s.playing = false;
            return false;
        }
        if (!plat::sys::playSound(s.soundId, s.looped)) {
            lastError = "Riproduzione fallita: " + s.soundId;
            s.playing = false;
            return false;
        }
        s.playing = true;
        lastError.clear();
        return true;
    }

    bool playPath(const std::string& path, bool looped = false) {
        SoundInstance tmp;
        tmp.soundId = path;
        tmp.looped = looped;
        tmp.name = "Temp";
        return play(tmp);
    }

    void stopAll() {
        plat::sys::stopSound();
        for (auto& s : sounds) s.playing = false;
    }

    void stop(SoundInstance& s) {
        stopAll();
        s.playing = false;
    }
};
