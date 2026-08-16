#pragma once

/**
 * SoundService — audio stile Roblox (semplificato).
 * Windows: PlaySound (WAV) + MCI (mp3/ogg dove supportato).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

struct SoundInstance {
    std::string name = "Sound";
    std::string soundId; // percorso file .wav / .mp3
    float volume = 1.f;  // 0..1 (best-effort)
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
        stopAll(); // un canale alla volta (semplice)
#ifdef _WIN32
        std::string path = s.soundId;
        for (char& c : path)
            if (c == '/') c = '\\';

        // WAV → PlaySound
        std::string lower = path;
        for (char& c : lower)
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        bool isWav = lower.size() >= 4 && lower.substr(lower.size() - 4) == ".wav";

        if (isWav) {
            DWORD flags = SND_FILENAME | SND_ASYNC | SND_NODEFAULT;
            if (s.looped) flags |= SND_LOOP;
            if (!PlaySoundA(path.c_str(), nullptr, flags)) {
                lastError = "PlaySound fallito: " + path;
                s.playing = false;
                return false;
            }
        } else {
            // MCI per mp3 ecc.
            mciSendStringA("close blocky_snd", nullptr, 0, nullptr);
            char cmd[1024];
            std::snprintf(cmd, sizeof(cmd), "open \"%s\" type mpegvideo alias blocky_snd", path.c_str());
            if (mciSendStringA(cmd, nullptr, 0, nullptr) != 0) {
                // fallback: prova come waveaudio
                std::snprintf(cmd, sizeof(cmd), "open \"%s\" alias blocky_snd", path.c_str());
                if (mciSendStringA(cmd, nullptr, 0, nullptr) != 0) {
                    lastError = "MCI open fallito: " + path;
                    s.playing = false;
                    return false;
                }
            }
            mciSendStringA(s.looped ? "play blocky_snd repeat" : "play blocky_snd", nullptr, 0, nullptr);
        }
        s.playing = true;
        lastError.clear();
        return true;
#else
        lastError = "Audio non supportato su questa piattaforma";
        (void)s;
        return false;
#endif
    }

    bool playPath(const std::string& path, bool looped = false) {
        SoundInstance tmp;
        tmp.soundId = path;
        tmp.looped = looped;
        tmp.name = "Temp";
        return play(tmp);
    }

    void stopAll() {
#ifdef _WIN32
        // SND_ASYNC|SND_PURGE: non blocca il thread (PlaySound(null) sincrono freezava la chiusura)
        PlaySoundA(nullptr, nullptr, SND_ASYNC | SND_PURGE);
        mciSendStringA("stop blocky_snd", nullptr, 0, nullptr);
        mciSendStringA("close blocky_snd", nullptr, 0, nullptr);
#endif
        for (auto& s : sounds) s.playing = false;
    }

    void stop(SoundInstance& s) {
        stopAll();
        s.playing = false;
    }
};
