// Prova isolata di TestSessionGuard: scatena crash nativi VERI (non
// simulati) uno per uno e verifica che ognuno venga intercettato e che il
// processo continui a girare normalmente dopo, invece di morire. Se questo
// programma stampa "ALL GUARD TESTS PASSED" ed esce con codice 0, il
// meccanismo di contenimento funziona davvero.
//
// Le due funzioni "crash" sotto sono deliberatamente __attribute__((noinline)):
// una lambda "nuda" che fa un solo store/una sola divisione e nient'altro
// viene trattata dal compilatore come funzione "leaf" (niente chiamate =
// niente info di unwind complete su x64/SEH), e in quel caso il recupero
// via longjmp si e' visto fallire nei test. Passando per una vera chiamata
// di funzione si riproduce fedelmente l'uso reale (avvolgere funzioni
// enormi come startTestScripts/fireHeartbeat, mai "leaf") invece di un
// caso sintetico che non si presenta mai nel motore vero.
//
// Lo stack overflow nativo NON e' testato qui: TestSessionGuard lo lascia
// deliberatamente passare al CrashHandler globale (vedi il commento in
// testa a platform/TestSessionGuard.h sul perche' un recupero anche per
// quel caso si e' rivelato inaffidabile).

#include "platform/TestSessionGuard.h"

#include <cstdio>
#include <string>

static volatile int g_zero = 0;

__attribute__((noinline)) static void crashNullDeref(int* p) {
    *p = 42;
}

__attribute__((noinline)) static int crashDivByZero(int v) {
    return 100 / v;
}

int main() {
    int ok = 0, total = 0;

    // 1) Accesso a puntatore nullo (EXCEPTION_ACCESS_VIOLATION)
    {
        ++total;
        std::string reason;
        int* p = nullptr;
        bool completed = TestSessionGuard::runGuarded([&]() {
            crashNullDeref(p);
        }, &reason);
        std::fprintf(stdout, "[1] null deref: completed=%d reason=%s\n",
                     (int)completed, reason.c_str());
        if (!completed && !reason.empty()) ++ok;
    }

    // 2) Divisione intera per zero (EXCEPTION_INT_DIVIDE_BY_ZERO)
    {
        ++total;
        std::string reason;
        bool completed = TestSessionGuard::runGuarded([&]() {
            int r = crashDivByZero(g_zero);
            std::fprintf(stdout, "non dovrebbe arrivare qui: %d\n", r);
        }, &reason);
        std::fprintf(stdout, "[2] div by zero: completed=%d reason=%s\n",
                     (int)completed, reason.c_str());
        if (!completed && !reason.empty()) ++ok;
    }

    // 3) Prova che il processo e' ancora perfettamente funzionante dopo
    //    entrambi i crash — alloca, calcola, stampa normalmente.
    {
        ++total;
        std::string reason;
        bool completed = TestSessionGuard::runGuarded([&]() {
            std::string s = "il processo funziona ancora";
            for (int i = 0; i < 1000; ++i) s += ".";
            std::fprintf(stdout, "[3] lavoro normale post-crash: len=%zu\n", s.size());
        }, &reason);
        if (completed) ++ok;
    }

    std::fprintf(stdout, "RISULTATO: %d/%d\n", ok, total);
    if (ok == total) {
        std::fprintf(stdout, "ALL GUARD TESTS PASSED\n");
        return 0;
    }
    std::fprintf(stderr, "GUARD TESTS FAILED\n");
    return 1;
}
