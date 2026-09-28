#pragma once

// Contenimento crash per la sessione di Test/Play in Studio.
//
// Prima di questo file: qualunque eccezione nativa durante l'esecuzione di
// script (un bug in una delle ~150 funzioni C esposte a Lua, un puntatore
// nullo, ecc.) arrivava al filtro globale in CrashHandler.h e terminava
// TUTTO il processo. Con questo file, il codice che gira durante Test/Play
// puo' essere eseguito "armato": se qualcosa crasha, si intercetta,
// si registra il motivo, e si torna al chiamante con un errore invece di
// morire — l'utente perde la sessione di Test ma non il resto di Studio
// (place aperto, lavoro non salvato altrove, ecc.).
//
// Perche' un vectored exception handler + setjmp/longjmp e non
// __try/__except: __try/__except e' un'estensione SEH di MSVC che i
// toolchain MinGW supportano in modo incompleto/dipendente dalla build;
// AddVectoredExceptionHandler e' un'API Win32 normale, funziona identica
// su qualunque compilatore. Verificato con crash VERI (non simulati):
// null-pointer deref e divisione intera per zero vengono intercettati e
// il processo continua a girare normalmente dopo (src/tools/test_crash_guard.cpp).
//
// ATTENZIONE alle funzioni "leaf": una lambda/funzione che non chiama
// nient'altro (es. un singolo store, `*p = 1;`, e basta) puo' essere
// compilata su x64 senza le info di unwind SEH complete — in quel caso il
// longjmp di recupero fallisce in modo inaffidabile (visto letteralmente
// in test: stessa build, stesso crash, funziona o no a seconda che ci sia
// una chiamata a funzione reale prima del crash). Non e' un problema per
// l'uso previsto (avvolgere operazioni vere come startTestScripts() o il
// tick per-frame di scripting, che chiamano decine di altre funzioni), ma
// va tenuto a mente se in futuro si prova a proteggere qualcosa di molto
// piccolo e isolato.
//
// NON intercetta EXCEPTION_STACK_OVERFLOW deliberatamente: Windows concede
// una singola pagina di stack "di grazia" per far girare l'exception
// handler quando lo stack e' esaurito, e anche riarmando una guard page a
// mano (quello che fa _resetstkoflw() di MSVC, che MinGW non fornisce) il
// recupero si e' rivelato inaffidabile nei test — rientra in loop di
// ri-crash invece di riprendere pulito. Un recupero "quasi funzionante"
// che lascia stack/heap in uno stato sottilmente incoerente e' peggio di
// nessun recupero, quindi qui uno stack overflow vero continua a finire
// nel CrashHandler globale come prima (e dopo il fix dell'interrupt di
// Luau che lo causava stasera, dovrebbe restare un caso raro: la
// ricorsione infinita ORDINARIA di uno script Lua e' gia' fermata in modo
// pulito dal limite di profondita' interno di Luau stesso, come un normale
// errore Lua dentro pcall — non arriva mai a toccare lo stack nativo).

#include "CrashHandler.h"

#ifdef _WIN32
#include <windows.h>

#include <csetjmp>
#include <cstdio>
#include <string>

namespace TestSessionGuard {

inline thread_local jmp_buf g_recoveryPoint;
inline thread_local volatile bool g_armed = false;
inline thread_local char g_lastReason[256] = {};
inline thread_local DWORD g_lastCode = 0;

inline LONG WINAPI vehHandler(EXCEPTION_POINTERS* ep) {
    if (!g_armed || !ep || !ep->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP)
        return EXCEPTION_CONTINUE_SEARCH;
    // Vedi commento in testa al file: uno stack overflow vero non si
    // intercetta qui, va al CrashHandler globale come prima.
    if (code == EXCEPTION_STACK_OVERFLOW)
        return EXCEPTION_CONTINUE_SEARCH;

    g_lastCode = code;
    std::snprintf(g_lastReason, sizeof(g_lastReason), "%s (0x%08lX)",
                  CrashHandler::exceptionCodeName(code), (unsigned long)code);

    g_armed = false; // one-shot: non intercettare di nuovo finche' non si riarma
    longjmp(g_recoveryPoint, 1);
}

inline void install() {
    static void* handle = AddVectoredExceptionHandler(1, vehHandler);
    (void)handle;
}

// Esegue fn() con il contenimento armato. Ritorna true se fn() e' tornata
// normalmente; false se e' stata intercettata una crash (outReason, se
// passato, riceve il motivo leggibile). ATTENZIONE: come ogni recupero
// basato su longjmp, gli oggetti C++ con distruttore non banale costruiti
// dentro fn() prima del crash NON vengono distrutti — va usato solo
// intorno a codice che non tiene lock/risorse critiche a lungo termine
// (esattamente il caso dello scripting di Test/Play).
template <typename Fn>
inline bool runGuarded(Fn&& fn, std::string* outReason = nullptr) {
    install();
    g_lastReason[0] = '\0';
    g_lastCode = 0;
    if (setjmp(g_recoveryPoint) == 0) {
        g_armed = true;
        fn();
        g_armed = false;
        return true;
    }
    // Il caso STACK_OVERFLOW salta qui senza aver formattato g_lastReason
    // (vedi commento in vehHandler sul perche') — lo si fa ora, con lo
    // stack di nuovo normale.
    if (g_lastReason[0] == '\0' && g_lastCode != 0) {
        std::snprintf(g_lastReason, sizeof(g_lastReason), "%s (0x%08lX)",
                      CrashHandler::exceptionCodeName(g_lastCode), (unsigned long)g_lastCode);
    }
    if (outReason) *outReason = g_lastReason;
    return false;
}

} // namespace TestSessionGuard

#else

#include <string>

namespace TestSessionGuard {
template <typename Fn>
inline bool runGuarded(Fn&& fn, std::string* = nullptr) {
    fn();
    return true;
}
inline void install() {}
} // namespace TestSessionGuard

#endif
