// Test di AudioMixer (src/systems/AudioMixer.h) senza scheda audio: il mixer
// gira in modalita' "noDevice" e il segnale mixato si legge a mano, quindi si
// misura davvero che due suoni si sommano invece di fermarsi a vicenda.

#include "systems/AudioMixer.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;

static void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_fail;
}

// WAV mono 16 bit: una sinusoide, cosi' si sa esattamente quanto "pesa".
static std::string writeTone(const char* name, float hz, float seconds, float amp) {
    const int rate = 48000;
    const int n = (int)(rate * seconds);
    std::vector<int16_t> pcm((size_t)n);
    for (int i = 0; i < n; ++i)
        pcm[(size_t)i] = (int16_t)(std::sin(6.2831853f * hz * (float)i / rate) * amp * 32767.f);
    const std::string path = std::string("test_audio_") + name + ".wav";
    FILE* f = std::fopen(path.c_str(), "wb");
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    const uint32_t bytes = (uint32_t)n * 2;
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    std::fwrite(pcm.data(), 2, (size_t)n, f);
    std::fclose(f);
    return path;
}

// Legge "seconds" di audio mixato e ne ritorna il valore RMS.
static float render(AudioMixer& m, float seconds) {
    const ma_uint64 frames = (ma_uint64)(48000 * seconds);
    std::vector<float> buf((size_t)frames * 2);
    ma_uint64 read = 0;
    ma_engine_read_pcm_frames(m.engine(), buf.data(), frames, &read);
    double sum = 0;
    for (ma_uint64 i = 0; i < read * 2; ++i) sum += (double)buf[(size_t)i] * buf[(size_t)i];
    return read ? (float)std::sqrt(sum / (double)(read * 2)) : 0.f;
}

int main() {
    std::printf("=== AudioMixer ===\n");
    const std::string a = writeTone("a", 440.f, 0.5f, 0.3f);
    const std::string b = writeTone("b", 660.f, 0.5f, 0.3f);

    AudioMixer::Config cfg;
    cfg.noDevice = true;
    cfg.maxVoices = 4;
    AudioMixer m;
    check(m.init(cfg), "mixer avviato senza scheda audio");

    const int ida = m.play(a);
    const float one = render(m, 0.1f);
    const int idb = m.play(b);
    const float two = render(m, 0.1f);
    std::printf("  RMS con un suono %.4f, con due %.4f\n", one, two);
    check(ida && idb && m.isPlaying(ida) && m.isPlaying(idb), "il secondo suono NON ferma il primo");
    check(two > one * 1.3f, "i due suoni si sommano davvero nel segnale");

    m.stop(ida);
    check(!m.isPlaying(ida) && m.isPlaying(idb), "Stop ferma solo quel suono");
    render(m, 0.6f);
    m.update(0.f);
    check(m.activeVoices() == 0, "suoni finiti liberati da soli");

    for (int i = 0; i < 6; ++i) m.play(a);
    check(m.activeVoices() == 4, "mai piu' voci del massimo (le piu' vecchie lasciano posto)");
    m.stopAll();

    const int loop = m.play(a, 1.f, true);
    render(m, 1.2f);
    m.update(0.f);
    check(m.isPlaying(loop), "in loop continua oltre la durata del file");
    m.stopAll();

    // 3D: stessa fonte, vicina e lontana dall'ascoltatore.
    m.setListener({0, 0, 0}, {0, 0, -1});
    m.playAt(a, {0, 0, -4});
    const float nearRms = render(m, 0.1f);
    m.stopAll();
    m.playAt(a, {0, 0, -120});
    const float farRms = render(m, 0.1f);
    m.stopAll();
    m.playAt(a, {0, 0, -400});
    const float outRms = render(m, 0.1f);
    m.stopAll();
    std::printf("  3D: vicino %.4f, a 120 stud %.4f, a 400 stud %.4f\n", nearRms, farRms, outRms);
    check(nearRms > farRms * 2.f && outRms < 0.001f, "piu' forte da vicino, muto oltre la distanza massima");

    m.setMasterVolume(0.f);
    m.play(a);
    check(render(m, 0.1f) < 1e-5f, "volume generale a zero = silenzio");
    m.setMasterVolume(1.f);
    m.stopAll();

    check(m.play("non_esiste.wav") == 0 && !m.lastError().empty(), "file mancante: errore chiaro, niente crash");

    m.shutdown();
    std::remove(a.c_str());
    std::remove(b.c_str());
    std::printf("\n%s (%d falliti)\n", g_fail ? "FALLITO" : "TUTTO OK", g_fail);
    return g_fail ? 1 : 0;
}
