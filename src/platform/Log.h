#pragma once

/**
 * Log di Blocky — facile in sviluppo, sicuro in distribuzione.
 *
 * Il conflitto che risolve: i log dell'anti-cheat ("KICK ... script estraneo
 * CAFEBABE", "Reject impronta ...") rimettevano IN CHIARO nel binario proprio
 * cio' che ObfStr aveva nascosto, e stampavano a runtime dicendo al cheater
 * cosa era stato rilevato. Toglierli del tutto pero' rende Blocky impossibile
 * da debuggare.
 *
 * Due canali:
 *
 *   LOG_INFO / LOG_WARN / LOG_ERR / LOG_DBG   log normali dell'app.
 *       Sempre presenti. Livello a runtime (blog::setLevel), opzionale su file.
 *       Usali per tutto cio' che non svela le difese.
 *
 *   LOG_SEC                                   canale sicurezza.
 *       In sviluppo (BLOCKY_DEV): stampa come gli altri — debug comodo.
 *       In distribuzione: compilato a NULLA. La format string non entra
 *       nemmeno nel binario, quindi non c'e' niente da leggere e niente da
 *       stampare. E' cosi' che il log non danneggia l'anti-cheat.
 *
 * Uso:
 *     LOG_INFO("net", "join #%d %s", id, name);
 *     LOG_SEC("net", "kick #%d impronta %s", id, fp);   // sparisce in release
 */

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <string>

namespace blog {

enum Level { Err = 0, Warn = 1, Info = 2, Debug = 3 };

inline Level& levelRef()  { static Level l = Info;    return l; }
inline FILE*& fileRef()   { static FILE* f = nullptr; return f; }

inline void setLevel(Level l) { levelRef() = l; }

/** Copia anche su file. Comodo per debuggare un crash a posteriori. */
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
}

} // namespace blog

#define LOG_ERR(tag, ...)  ::blog::write(::blog::Err,   tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) ::blog::write(::blog::Warn,  tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) ::blog::write(::blog::Info,  tag, __VA_ARGS__)
#define LOG_DBG(tag, ...)  ::blog::write(::blog::Debug, tag, __VA_ARGS__)

// Canale sicurezza: presente solo nelle build di sviluppo. In distribuzione
// diventa ((void)0), quindi format string e argomenti non finiscono nel
// binario e non c'e' output a runtime che riveli cosa e' stato rilevato.
#ifdef BLOCKY_DEV
  #define LOG_SEC(tag, ...) ::blog::write(::blog::Info, tag, __VA_ARGS__)
#else
  #define LOG_SEC(tag, ...) ((void)0)
#endif
