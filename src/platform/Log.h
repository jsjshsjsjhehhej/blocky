#pragma once


#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace blog {

enum Level { Err = 0, Warn = 1, Info = 2, Debug = 3 };

inline Level& levelRef()  { static Level l = Info;    return l; }
inline FILE*& fileRef()   { static FILE* f = nullptr; return f; }

inline void setLevel(Level l) { levelRef() = l; }

inline void toFile(const std::string& path) {
    if (fileRef()) { std::fclose(fileRef()); fileRef() = nullptr; }
    fileRef() = std::fopen(path.c_str(), "a");
}

inline const char* levelTag(Level l) {
    switch (l) {
    case Err:   return "ERR ";
    case Warn:  return "WARN";
    case Info:  return "INFO";
    default:    return "DBG ";
    }
}

// Le ultime righe in memoria, per la Developer Console (F9): sul telefono
// non c'e' un file da aprire. Un solo thread scrive il log (il loop del
// motore), quindi niente lock.
struct Entry {
    Level level;
    double time;        // secondi dall'avvio
    std::string tag;
    std::string text;
};
constexpr size_t kRecent = 400;
inline std::vector<Entry>& recent() { static std::vector<Entry> v; return v; }
inline size_t& recentHead() { static size_t h = 0; return h; }   // prossima posizione da sovrascrivere

// In ordine dalla piu' vecchia alla piu' nuova.
inline std::vector<Entry> recentOrdered() {
    const auto& v = recent();
    std::vector<Entry> out;
    out.reserve(v.size());
    if (v.size() < kRecent) return v;
    for (size_t k = 0; k < v.size(); ++k) out.push_back(v[(recentHead() + k) % v.size()]);
    return out;
}

inline void write(Level lv, const char* tag, const char* fmt, ...) {
    if (lv > levelRef()) return;

    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    char line[1152];
    std::snprintf(line, sizeof(line), "[%s][%s] %s\n", levelTag(lv), tag, msg);

    std::fputs(line, stderr);
    if (fileRef()) { std::fputs(line, fileRef()); std::fflush(fileRef()); }

    {
        static const auto t0 = std::chrono::steady_clock::now();
        Entry e{lv, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), tag, msg};
        auto& v = recent();
        if (v.size() < kRecent) v.push_back(std::move(e));
        else { v[recentHead()] = std::move(e); recentHead() = (recentHead() + 1) % kRecent; }
    }

#ifdef __ANDROID__
    // Su Android stdout e stderr non vanno da nessuna parte: l'unico canale
    // e' logcat. Senza questa riga TUTTO il logging del motore era invisibile
    // proprio sulla piattaforma dove serviva di piu' — ed e' il motivo per
    // cui ogni bug della porta e' stato diagnosticato aggiungendo a mano una
    // __android_log_print alla volta. Qui vale per ogni LOG_* del motore.
    __android_log_print(
        lv == Err ? ANDROID_LOG_ERROR : (lv == Warn ? ANDROID_LOG_WARN : ANDROID_LOG_INFO),
        "Blocky", "[%s] %s", tag, msg);
#endif
}

}

#define LOG_ERR(tag, ...)  ::blog::write(::blog::Err,   tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) ::blog::write(::blog::Warn,  tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) ::blog::write(::blog::Info,  tag, __VA_ARGS__)
#define LOG_DBG(tag, ...)  ::blog::write(::blog::Debug, tag, __VA_ARGS__)

#ifdef BLOCKY_DEV
  #define LOG_SEC(tag, ...) ::blog::write(::blog::Info, tag, __VA_ARGS__)
#else
  #define LOG_SEC(tag, ...) ((void)0)
#endif
