#pragma once

/**
 * Riproduzione dei suoni su Android.
 *
 * Il motore chiedeva l'audio a PlaySound e MCI, che sono API di Windows: fuori
 * da li' SoundService rispondeva "Audio non supportato su questa piattaforma" e
 * il gioco girava completamente muto sul telefono.
 *
 * Qui c'e' l'equivalente Android, con OpenSL ES. La scelta e' voluta: OpenSL ES
 * sa aprire un file audio dandogli il PERCORSO e decodificarlo da solo — wav,
 * mp3, ogg — ed e' esattamente cio' che serviva, perche' e' la stessa forma di
 * PlaySound e quindi SoundService non cambia di una riga nella sua logica.
 * AAudio, piu' moderno, lavora invece a campioni grezzi: avrebbe voluto dire
 * scrivere un decodificatore per ogni formato, per un gioco che deve solo
 * "suonare questo file".
 *
 * Un solo suono per volta, come su Windows: SoundService chiama stopAll()
 * prima di ogni play, quindi un lettore unico riprodotto ogni volta rispecchia
 * esattamente il comportamento che il motore si aspetta.
 */

#include <string>

#ifdef __ANDROID__
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>
#endif

namespace blk {
namespace androidaudio {

#ifdef __ANDROID__

struct State {
    SLObjectItf engineObj = nullptr;
    SLEngineItf engine = nullptr;
    SLObjectItf outputMix = nullptr;

    SLObjectItf playerObj = nullptr;
    SLPlayItf   play = nullptr;
    SLSeekItf   seek = nullptr;

    bool ready = false;
};

inline State& state() { static State s; return s; }

/** Motore e uscita audio si creano una volta sola e restano vivi. */
inline bool ensureEngine() {
    State& s = state();
    if (s.ready) return true;

    if (slCreateEngine(&s.engineObj, 0, nullptr, 0, nullptr, nullptr) != SL_RESULT_SUCCESS)
        return false;
    if ((*s.engineObj)->Realize(s.engineObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
        return false;
    if ((*s.engineObj)->GetInterface(s.engineObj, SL_IID_ENGINE, &s.engine) != SL_RESULT_SUCCESS)
        return false;
    if ((*s.engine)->CreateOutputMix(s.engine, &s.outputMix, 0, nullptr, nullptr)
            != SL_RESULT_SUCCESS)
        return false;
    if ((*s.outputMix)->Realize(s.outputMix, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS)
        return false;

    __android_log_print(ANDROID_LOG_INFO, "Blocky", "audio: OpenSL ES pronto");
    s.ready = true;
    return true;
}

inline void destroyPlayer() {
    State& s = state();
    if (!s.playerObj) return;
    if (s.play) (*s.play)->SetPlayState(s.play, SL_PLAYSTATE_STOPPED);
    (*s.playerObj)->Destroy(s.playerObj);
    s.playerObj = nullptr;
    s.play = nullptr;
    s.seek = nullptr;
}

inline void stop() { destroyPlayer(); }

/**
 * Riproduce un file audio dal percorso.
 *
 * Il lettore viene ricreato a ogni suono invece di essere riusato: OpenSL ES
 * lega la sorgente al lettore quando lo si crea, quindi cambiare file
 * significa comunque rifare l'oggetto. Ricrearlo sempre rende il
 * comportamento uno solo, senza un caso "stesso file di prima" da mantenere a
 * parte.
 */
inline bool playFile(const std::string& path, bool looped) {
    if (path.empty() || !ensureEngine()) return false;
    State& s = state();
    destroyPlayer();

    // OpenSL ES vuole un URI, non un percorso nudo.
    const std::string uri = "file://" + path;
    SLDataLocator_URI loc{SL_DATALOCATOR_URI, (SLchar*)uri.c_str()};
    SLDataFormat_MIME fmt{SL_DATAFORMAT_MIME, nullptr, SL_CONTAINERTYPE_UNSPECIFIED};
    SLDataSource src{&loc, &fmt};

    SLDataLocator_OutputMix outLoc{SL_DATALOCATOR_OUTPUTMIX, s.outputMix};
    SLDataSink sink{&outLoc, nullptr};

    // SEEK serve solo per il ciclo continuo; se manca, il suono parte lo stesso
    // e semplicemente non si ripete.
    const SLInterfaceID ids[1] = {SL_IID_SEEK};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    if ((*s.engine)->CreateAudioPlayer(s.engine, &s.playerObj, &src, &sink, 1, ids, req)
            != SL_RESULT_SUCCESS)
        return false;
    if ((*s.playerObj)->Realize(s.playerObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) {
        destroyPlayer();
        return false;
    }
    if ((*s.playerObj)->GetInterface(s.playerObj, SL_IID_PLAY, &s.play) != SL_RESULT_SUCCESS) {
        destroyPlayer();
        return false;
    }

    if (looped &&
        (*s.playerObj)->GetInterface(s.playerObj, SL_IID_SEEK, &s.seek) == SL_RESULT_SUCCESS) {
        (*s.seek)->SetLoop(s.seek, SL_BOOLEAN_TRUE, 0, SL_TIME_UNKNOWN);
    }

    if ((*s.play)->SetPlayState(s.play, SL_PLAYSTATE_PLAYING) != SL_RESULT_SUCCESS) {
        __android_log_print(ANDROID_LOG_ERROR, "Blocky", "audio: avvio fallito %s", path.c_str());
        destroyPlayer();
        return false;
    }
    return true;
}

#else   // non Android: nessun audio da questa parte

inline bool playFile(const std::string&, bool) { return false; }
inline void stop() {}

#endif

} // namespace androidaudio
} // namespace blk
