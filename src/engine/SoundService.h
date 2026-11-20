#pragma once


#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "DataModel.h"

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
    int voice = 0;   // voce del mixer, se c'e'
};

class SoundService {
public:
    std::vector<SoundInstance> sounds;
    std::string lastError;
    int nextIndex = 1;

    // Se impostati (main.cpp li collega all'AudioMixer) i suoni si SOMMANO.
    // Senza, resta il vecchio backend a voce singola di plat::sys.
    std::function<int(const std::string& path, float volume, bool looped)> mixPlay;
    std::function<int(const std::string& path, glm::vec3 pos, float volume, bool looped)> mixPlayAt;
    std::function<void(int voice, glm::vec3 pos)> mixSetPosition;
    std::function<void(int voice, float pitch)> mixSetPitch;
    std::function<void(int voice)> mixStop;
    std::function<void()> mixStopAll;
    std::function<std::string()> mixError;

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
        if (mixPlay) {
            if (s.voice && mixStop) mixStop(s.voice);   // stesso Sound ripartito: niente doppione
            s.voice = mixPlay(s.soundId, s.volume, s.looped);
            s.playing = s.voice > 0;
            lastError = s.playing ? std::string() : (mixError ? mixError() : "Riproduzione fallita");
            return s.playing;
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

    // Voci attaccate a una Part: ogni frame si spostano con lei.
    struct TrackedVoice { int voice = 0; int part = -1; };
    std::vector<TrackedVoice> tracked;

    int playHeard(const std::string& path, float volume, bool looped,
                  const glm::vec3* at, int part) {
        int voice = 0;
        if (at && mixPlayAt)
            voice = mixPlayAt(path, *at, volume, looped);
        else if (mixPlay)
            voice = mixPlay(path, volume, looped);
        else {
            SoundInstance tmp;
            tmp.soundId = path;
            tmp.volume = volume;
            tmp.looped = looped;
            tmp.name = "Temp";
            if (!play(tmp)) return 0;
            voice = tmp.voice;
        }
        if (voice > 0 && part >= 0)
            tracked.push_back({voice, part});
        return voice;
    }

    void forgetVoice(int voice) {
        if (voice <= 0) return;
        if (mixStop) mixStop(voice);
        for (size_t i = tracked.size(); i-- > 0; )
            if (tracked[i].voice == voice)
                tracked.erase(tracked.begin() + (long)i);
    }

    void updateTracked(const DataModel& world, const glm::vec3& ears) {
        if (!mixSetPosition) return;
        for (size_t i = tracked.size(); i-- > 0; ) {
            TrackedVoice& t = tracked[i];
            if (t.part < 0 || t.part >= (int)world.parts.size()) {
                if (mixStop) mixStop(t.voice);
                tracked.erase(tracked.begin() + (long)i);
                continue;
            }
            const WorkspacePart& p = world.parts[(size_t)t.part];
            const glm::vec3 pos = world.partHeldInHand(p) ? ears : p.position;
            mixSetPosition(t.voice, pos);
        }
    }

    bool playPath(const std::string& path, bool looped = false) {
        SoundInstance tmp;
        tmp.soundId = path;
        tmp.looped = looped;
        tmp.name = "Temp";
        return play(tmp);
    }

    void stopAll() {
        if (mixStopAll) mixStopAll();
        else plat::sys::stopSound();
        for (auto& s : sounds) { s.playing = false; s.voice = 0; }
        tracked.clear();
    }

    void stop(SoundInstance& s) {
        if (mixStop) {
            if (s.voice) mixStop(s.voice);
        } else {
            stopAll();
        }
        s.playing = false;
        s.voice = 0;
    }
};
