#pragma once

// AudioMixer: piu' suoni insieme, volume per suono e suono 3D che cala con
// la distanza. Generato da tools/genera.py il 2026-09-30.
//
// Prima il motore aveva UNA voce sola (PlaySound su Windows, un lettore
// OpenSL su Android): ogni suono fermava il precedente, quindi un passo
// tagliava uno sparo. Qui mixa miniaudio, la stessa libreria su ogni
// piattaforma (WASAPI su Windows, AAudio/OpenSL su Android).
//
// I file corti si decodificano una volta e restano in memoria (i passi non
// si rileggono dal disco a ogni passo); quelli lunghi (musica) si leggono a
// pezzi mentre suonano.

#include "miniaudio.h"

#include <glm/glm.hpp>

#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class AudioMixer {
public:
    struct Config {
        int maxVoices = 24;          // oltre, si ferma la voce piu' vecchia non in loop
        bool noDevice = false;       // test: nessuna scheda audio, si legge a mano
        unsigned sampleRate = 0;     // 0 = quello del dispositivo
    };

    // Distanze del suono 3D, in stud: piena potenza entro kNear, muto oltre kFar.
    static constexpr float kNear = 8.f;
    static constexpr float kFar = 180.f;
    static constexpr long kStreamAboveBytes = 2 * 1024 * 1024;

    // Da percorso scritto in uno script ("sounds/jump.wav") a file vero.
    // Lo imposta main.cpp; vuoto = il percorso si usa cosi' com'e'.
    std::function<std::string(const std::string&)> resolveScriptPath;

    AudioMixer() = default;
    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;
    ~AudioMixer() { shutdown(); }

    bool init() { return init(Config()); }

    bool init(const Config& cfg) {
        if (inited_) return true;
        cfg_ = cfg;
        ma_engine_config ec = ma_engine_config_init();
        if (cfg.noDevice) {
            ec.noDevice = MA_TRUE;
            ec.channels = 2;
            ec.sampleRate = cfg.sampleRate ? cfg.sampleRate : 48000;
        } else if (cfg.sampleRate) {
            ec.sampleRate = cfg.sampleRate;
        }
        const ma_result r = ma_engine_init(&ec, &engine_);
        if (r != MA_SUCCESS) {
            lastError_ = std::string("audio non disponibile: ") + ma_result_description(r);
            return false;
        }
        ma_engine_listener_set_world_up(&engine_, 0, 0.f, 1.f, 0.f);
        ma_engine_set_volume(&engine_, master_);
        inited_ = true;
        return true;
    }

    void shutdown() {
        if (!inited_) return;
        voices_.clear();
        cache_.clear();
        ma_engine_uninit(&engine_);
        inited_ = false;
    }

    bool ready() const { return inited_; }
    const std::string& lastError() const { return lastError_; }

    // Ritorna l'id della voce (>0), oppure 0 se non e' partita (vedi lastError).
    int play(const std::string& path, float volume = 1.f, bool loop = false) {
        return start(path, volume, loop, nullptr);
    }

    int playAt(const std::string& path, const glm::vec3& pos, float volume = 1.f, bool loop = false) {
        return start(path, volume, loop, &pos);
    }

    void stop(int id) {
        for (size_t i = 0; i < voices_.size(); ++i)
            if (voices_[i]->id == id) { voices_.erase(voices_.begin() + (long)i); return; }
    }

    void stopAll() { voices_.clear(); }

    bool isPlaying(int id) const {
        for (const auto& v : voices_)
            if (v->id == id) return !finished(*v);
        return false;
    }

    void setVolume(int id, float volume) {
        for (auto& v : voices_)
            if (v->id == id) ma_sound_set_volume(&v->sound, clampVol(volume));
    }

    void setMasterVolume(float volume) {
        master_ = clampVol(volume);
        if (inited_) ma_engine_set_volume(&engine_, master_);
    }
    float masterVolume() const { return master_; }

    // Dove sono le orecchie: la camera. Chiamato ogni frame in gioco.
    void setListener(const glm::vec3& pos, const glm::vec3& forward) {
        if (!inited_) return;
        ma_engine_listener_set_position(&engine_, 0, pos.x, pos.y, pos.z);
        if (glm::dot(forward, forward) > 1e-6f)
            ma_engine_listener_set_direction(&engine_, 0, forward.x, forward.y, forward.z);
    }

    int activeVoices() const {
        int n = 0;
        for (const auto& v : voices_) n += finished(*v) ? 0 : 1;
        return n;
    }

    ma_engine* engine() { return inited_ ? &engine_ : nullptr; }

    // ---- Interfaccia dei sistemi generati --------------------------------

    void reset() { stopAll(); }

    void update(float) { reap(); }

private:
    // ok = ma_sound inizializzato davvero: solo allora va liberato.
    struct Voice {
        int id = 0;
        bool loop = false;
        bool ok = false;
        ma_sound sound{};
        ~Voice() { if (ok) ma_sound_uninit(&sound); }
    };

    // Suono "modello" gia' decodificato: le voci ne sono copie leggere.
    struct Cached {
        bool ok = false;
        ma_sound proto{};
        ~Cached() { if (ok) ma_sound_uninit(&proto); }
    };

    static float clampVol(float v) { return v < 0.f ? 0.f : (v > 4.f ? 4.f : v); }

    static bool finished(const Voice& v) {
        return !v.loop && (ma_sound_at_end(&v.sound) || !ma_sound_is_playing(&v.sound));
    }

    void reap() {
        for (size_t i = 0; i < voices_.size();) {
            if (finished(*voices_[i])) voices_.erase(voices_.begin() + (long)i);
            else ++i;
        }
    }

    static long fileSize(const std::string& path) {
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return -1;
        std::fseek(f, 0, SEEK_END);
        const long n = std::ftell(f);
        std::fclose(f);
        return n;
    }

    int start(const std::string& path, float volume, bool loop, const glm::vec3* pos) {
        if (!inited_) {
            Config c = cfg_;
            if (!init(c)) return 0;
        }
        if (path.empty()) { lastError_ = "percorso del suono vuoto"; return 0; }

        reap();
        if ((int)voices_.size() >= cfg_.maxVoices) {
            // Voce piu' vecchia non in loop: e' quella che si nota meno.
            size_t victim = voices_.size();
            for (size_t i = 0; i < voices_.size(); ++i)
                if (!voices_[i]->loop) { victim = i; break; }
            if (victim == voices_.size()) {
                lastError_ = "troppi suoni in loop insieme";
                return 0;
            }
            voices_.erase(voices_.begin() + (long)victim);
        }

        auto v = std::make_unique<Voice>();
        const ma_uint32 spatial = pos ? 0 : MA_SOUND_FLAG_NO_SPATIALIZATION;
        ma_result r;
        const long size = fileSize(path);
        if (size < 0) {
            lastError_ = "file audio non trovato: " + path;
            return 0;
        }
        if (size > kStreamAboveBytes) {
            r = ma_sound_init_from_file(&engine_, path.c_str(), MA_SOUND_FLAG_STREAM | spatial,
                                        nullptr, nullptr, &v->sound);
        } else {
            Cached* c = cached(path);
            if (!c) return 0;
            r = ma_sound_init_copy(&engine_, &c->proto, spatial, nullptr, &v->sound);
        }
        if (r != MA_SUCCESS) {
            lastError_ = "impossibile aprire " + path + ": " + ma_result_description(r);
            return 0;
        }
        v->ok = true;

        v->loop = loop;
        v->id = nextId_++;
        ma_sound_set_looping(&v->sound, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&v->sound, clampVol(volume));
        if (pos) {
            ma_sound_set_position(&v->sound, pos->x, pos->y, pos->z);
            ma_sound_set_min_distance(&v->sound, kNear);
            ma_sound_set_max_distance(&v->sound, kFar);
            ma_sound_set_attenuation_model(&v->sound, ma_attenuation_model_linear);
        }
        r = ma_sound_start(&v->sound);
        if (r != MA_SUCCESS) {
            lastError_ = std::string("avvio fallito: ") + ma_result_description(r);
            return 0;
        }
        const int id = v->id;
        voices_.push_back(std::move(v));
        lastError_.clear();
        return id;
    }

    Cached* cached(const std::string& path) {
        auto it = cache_.find(path);
        if (it != cache_.end()) return it->second.get();
        if (cache_.size() >= 64) cache_.erase(cache_.begin());
        auto c = std::make_unique<Cached>();
        const ma_result r = ma_sound_init_from_file(&engine_, path.c_str(),
                                                    MA_SOUND_FLAG_DECODE, nullptr, nullptr, &c->proto);
        if (r != MA_SUCCESS) {
            lastError_ = "impossibile decodificare " + path + ": " + ma_result_description(r);
            return nullptr;
        }
        c->ok = true;
        Cached* raw = c.get();
        cache_[path] = std::move(c);
        return raw;
    }

    Config cfg_;
    ma_engine engine_{};
    bool inited_ = false;
    float master_ = 1.f;
    int nextId_ = 1;
    std::string lastError_;
    std::vector<std::unique_ptr<Voice>> voices_;
    std::map<std::string, std::unique_ptr<Cached>> cache_;
};
