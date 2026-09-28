#pragma once


#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>

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
