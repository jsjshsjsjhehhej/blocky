#pragma once


#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "platform/Platform.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <direct.h>
#else
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <csignal>
#include <exception>
#include <cstdlib>

namespace CrashHandler {

inline constexpr int kMaxBreadcrumbs = 24;
inline constexpr int kBreadcrumbLen = 160;

struct State {
    char appName[64] = "Blocky";
    char version[32] = "1.0";
    char context[256] = {};
    char breadcrumbs[kMaxBreadcrumbs][kBreadcrumbLen] = {};
    int breadcrumbCount = 0;
    int breadcrumbHead = 0;
    std::atomic<bool> handling{false};
    char lastReportPath[512] = {};
};

inline State& state() {
    static State s;
    return s;
}

/**
 * Dove finiscono i rapporti di crash.
 *
 * Fuori da Windows qui c'era il percorso RELATIVO "blocky_data/crashes", che
 * funziona solo se la cartella di lavoro e' scrivibile. Su Android e' "/", in
 * sola lettura: il rapporto non veniva scritto e un crash non lasciava
 * assolutamente nulla da leggere. plat::dataDir() risponde su ogni
 * piattaforma, e su Windows restituisce lo stesso %APPDATA%\Blocky di prima,
 * quindi i rapporti gia' salvati restano dove sono.
 */
inline std::string crashDir() {
    const std::string dir = plat::dataDir() + "/crashes";
    plat::makeDirs(dir);
    return dir;
}

inline void setContext(const char* ctx) {
    if (!ctx) return;
    std::snprintf(state().context, sizeof(state().context), "%s", ctx);
}

inline void setAppInfo(const char* name, const char* version = nullptr) {
    if (name && name[0])
        std::snprintf(state().appName, sizeof(state().appName), "%s", name);
    if (version && version[0])
        std::snprintf(state().version, sizeof(state().version), "%s", version);
}

inline void note(const char* msg) {
    if (!msg || !msg[0]) return;
    auto& s = state();
    int i = s.breadcrumbHead % kMaxBreadcrumbs;
    std::snprintf(s.breadcrumbs[i], kBreadcrumbLen, "%s", msg);
    s.breadcrumbHead = (s.breadcrumbHead + 1) % kMaxBreadcrumbs;
    if (s.breadcrumbCount < kMaxBreadcrumbs) ++s.breadcrumbCount;
}

#ifdef _WIN32
inline const char* exceptionCodeName(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:         return "ACCESS_VIOLATION (lettura/scrittura memoria non valida)";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:               return "BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:    return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DENORMAL_OPERAND:     return "FLOAT_DENORMAL";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "FLOAT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:       return "FLOAT_INEXACT";
    case EXCEPTION_FLT_INVALID_OPERATION:    return "FLOAT_INVALID_OP";
    case EXCEPTION_FLT_OVERFLOW:             return "FLOAT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:          return "FLOAT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:            return "FLOAT_UNDERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:      return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:            return "IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "INTEGER_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:             return "INTEGER_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:      return "INVALID_DISPOSITION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_PRIV_INSTRUCTION:         return "PRIVILEGED_INSTRUCTION";
    case EXCEPTION_SINGLE_STEP:              return "SINGLE_STEP";
    case EXCEPTION_STACK_OVERFLOW:           return "STACK_OVERFLOW";
    default:                                 return "EXCEPTION_UNKNOWN";
    }
}

inline void describeAccessViolation(EXCEPTION_RECORD* er, char* out, size_t outN) {
    if (!er || er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || er->NumberParameters < 2) {
        out[0] = '\0';
        return;
    }
    const char* op = (er->ExceptionInformation[0] == 0) ? "lettura"
                   : (er->ExceptionInformation[0] == 1) ? "scrittura"
                   : (er->ExceptionInformation[0] == 8) ? "DEP/esecuzione"
                   : "accesso";
    std::snprintf(out, outN, "Tentativo di %s all'indirizzo 0x%p",
                  op, (void*)er->ExceptionInformation[1]);
}
#endif

inline const char* signalName(int sig) {
    switch (sig) {
    case SIGSEGV: return "SIGSEGV (accesso memoria non valido)";
    case SIGABRT: return "SIGABRT (abort / assert fallita)";
    case SIGFPE:  return "SIGFPE (errore aritmetico / div zero)";
    case SIGILL:  return "SIGILL (istruzione illegale)";
#ifdef SIGBUS
    case SIGBUS:  return "SIGBUS (bus error)";
#endif
    default:      return "SIGNAL";
    }
}

inline void writeStack(FILE* f) {
#ifdef _WIN32
    HANDLE proc = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    if (!SymInitialize(proc, nullptr, TRUE)) {
        std::fprintf(f, "(stack: SymInitialize fallita)\n");
        return;
    }
    void* frames[48] = {};
    USHORT n = CaptureStackBackTrace(0, 48, frames, nullptr);
    HMODULE mainMod = GetModuleHandleA(nullptr);
    std::fprintf(f, "ModuleBase: 0x%p\n", (void*)mainMod);
    std::fprintf(f, "Stack (%u frame):\n", (unsigned)n);
    char symBuf[sizeof(SYMBOL_INFO) + 256];
    for (USHORT i = 0; i < n; ++i) {
        auto* sym = (SYMBOL_INFO*)symBuf;
        std::memset(symBuf, 0, sizeof(symBuf));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 addr = (DWORD64)(uintptr_t)frames[i];
        DWORD64 disp = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisp = 0;
        if (SymFromAddr(proc, addr, &disp, sym)) {
            if (SymGetLineFromAddr64(proc, addr, &lineDisp, &line)) {
                std::fprintf(f, "  #%02u  %s + 0x%llx  (%s:%lu)\n",
                             (unsigned)i, sym->Name, (unsigned long long)disp,
                             line.FileName ? line.FileName : "?", (unsigned long)line.LineNumber);
            } else {
                std::fprintf(f, "  #%02u  %s + 0x%llx  [0x%p]\n",
                             (unsigned)i, sym->Name, (unsigned long long)disp, frames[i]);
            }
        } else {
            uintptr_t rva = (uintptr_t)frames[i] - (uintptr_t)mainMod;
            std::fprintf(f, "  #%02u  0x%p  (rva +0x%llx)\n",
                         (unsigned)i, frames[i], (unsigned long long)rva);
        }
    }
    SymCleanup(proc);
#else
    std::fprintf(f, "(stack non disponibile su questa piattaforma)\n");
#endif
}

inline void writeBreadcrumbs(FILE* f) {
    auto& s = state();
    std::fprintf(f, "Breadcrumbs (ultime azioni):\n");
    if (s.breadcrumbCount <= 0) {
        std::fprintf(f, "  (nessuna)\n");
        return;
    }
    int start = (s.breadcrumbHead - s.breadcrumbCount + kMaxBreadcrumbs * 2) % kMaxBreadcrumbs;
    for (int i = 0; i < s.breadcrumbCount; ++i) {
        int idx = (start + i) % kMaxBreadcrumbs;
        std::fprintf(f, "  - %s\n", s.breadcrumbs[idx]);
    }
}

inline bool writeReport(const char* kind, const char* reason, const char* detail,
                        char* outPath, size_t outPathN) {
    auto& s = state();
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);

    std::string dir = crashDir();
    std::snprintf(outPath, outPathN, "%s%scrash_%s.txt",
                  dir.c_str(),
#ifdef _WIN32
                  "\\"
#else
                  "/"
#endif
                  , stamp);

    FILE* f = std::fopen(outPath, "wb");
    if (!f) return false;

    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm);

    std::fprintf(f, "=== Blocky Crash Report ===\n");
    std::fprintf(f, "App:      %s\n", s.appName);
    std::fprintf(f, "Version:  %s\n", s.version);
    std::fprintf(f, "Time:     %s\n", timeBuf);
    std::fprintf(f, "Kind:     %s\n", kind ? kind : "?");
    std::fprintf(f, "Reason:   %s\n", reason ? reason : "?");
    if (detail && detail[0])
        std::fprintf(f, "Detail:   %s\n", detail);
    if (s.context[0])
        std::fprintf(f, "Context:  %s\n", s.context);
#ifdef _WIN32
    std::fprintf(f, "PID:      %lu\n", (unsigned long)GetCurrentProcessId());
#endif
    std::fprintf(f, "\n");
    // Flush dopo ogni sezione: se il crash e' cosi' violento da far fallire
    // di nuovo qualcosa qui dentro (es. lo stack walk su uno stack gia'
    // corrotto), quanto scritto finora resta comunque su disco invece di
    // perdersi nel buffer di stdio non ancora svuotato da fclose.
    std::fflush(f);
    writeBreadcrumbs(f);
    std::fprintf(f, "\n");
    std::fflush(f);
    writeStack(f);
    std::fflush(f);
    std::fprintf(f, "\n=== Fine report ===\n");
    std::fprintf(f, "Invia questo file per aiutare a correggere il crash.\n");
    std::fclose(f);

    std::snprintf(s.lastReportPath, sizeof(s.lastReportPath), "%s", outPath);
    return true;
}

inline void showReportUi(const char* reason, const char* detail, const char* path) {
#ifdef _WIN32
    char msg[1024];
    std::snprintf(msg, sizeof(msg),
        "%s e' crashato.\n\n"
        "Motivo:\n%s\n%s%s\n\n"
        "Report salvato in:\n%s\n\n"
        "OK = apri cartella crash\nAnnulla = chiudi",
        state().appName,
        reason ? reason : "sconosciuto",
        (detail && detail[0]) ? detail : "",
        (detail && detail[0]) ? "\n" : "",
        path && path[0] ? path : "(non salvato)");

    int r = MessageBoxA(nullptr, msg, "Blocky — Crash Report",
                        MB_OKCANCEL | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
    if (r == IDOK && path && path[0]) {
        std::string args = "/select,\"" + std::string(path) + "\"";
        ShellExecuteA(nullptr, "open", "explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }
#else
    std::fprintf(stderr, "\n*** CRASH: %s\n%s\nReport: %s\n",
                 reason ? reason : "?", detail ? detail : "", path ? path : "?");
#endif
}

inline void handleCrash(const char* kind, const char* reason, const char* detail) {
    auto& s = state();
    bool expected = false;
    if (!s.handling.compare_exchange_strong(expected, true))
        return;

    char path[512] = {};
    writeReport(kind, reason, detail, path, sizeof(path));

    std::fprintf(stderr, "\n[CRASH] %s: %s\n  %s\n  report: %s\n",
                 kind ? kind : "?", reason ? reason : "?",
                 detail ? detail : "", path);

    // Anche le ultime azioni sul flusso degli errori, non solo nel file.
    // Su Android stderr finisce in logcat, ed e l unico posto dove un crash
    // si puo leggere davvero: il file da solo non lo apre nessuno, e senza le
    // briciole si sa CHE e successo ma non DOVE.
    writeBreadcrumbs(stderr);

    showReportUi(reason, detail, path);

#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    std::_Exit(1);
}

#ifdef _WIN32
inline LONG WINAPI unhandledFilter(EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ExceptionRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP)
        return EXCEPTION_CONTINUE_SEARCH;

    char detail[256] = {};
    describeAccessViolation(ep->ExceptionRecord, detail, sizeof(detail));
    if (!detail[0]) {
        std::snprintf(detail, sizeof(detail),
                      "ExceptionAddress=0x%p  Code=0x%08lX",
                      ep->ExceptionRecord->ExceptionAddress, (unsigned long)code);
    }

    char reason[256];
    std::snprintf(reason, sizeof(reason), "%s (0x%08lX)",
                  exceptionCodeName(code), (unsigned long)code);
    handleCrash("Win32 Exception", reason, detail);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

inline void onCrashSignal(int sig) {
    std::signal(sig, SIG_DFL);
    char reason[128];
    std::snprintf(reason, sizeof(reason), "%s (%d)", signalName(sig), sig);
    handleCrash("Signal", reason, state().context);
}

inline void onTerminate() {
    const char* what = "std::terminate (eccezione C++ non gestita)";
    try {
        auto e = std::current_exception();
        if (e) {
            try { std::rethrow_exception(e); }
            catch (const std::exception& ex) {
                char buf[256];
                std::snprintf(buf, sizeof(buf), "std::exception: %s", ex.what());
                handleCrash("C++ terminate", buf, state().context);
                return;
            } catch (...) {
                handleCrash("C++ terminate", "eccezione sconosciuta", state().context);
                return;
            }
        }
    } catch (...) {}
    handleCrash("C++ terminate", what, state().context);
}

inline void onPureCall() {
    handleCrash("Runtime", "Pure virtual function call", state().context);
}

inline void onInvalidParam(const wchar_t*, const wchar_t*, const wchar_t*,
                           unsigned int, uintptr_t) {
    handleCrash("Runtime", "Invalid parameter (CRT)", state().context);
}

inline void install(const char* appName, const char* version = "dev") {
    setAppInfo(appName, version);
    note("CrashHandler installato");

#ifdef _WIN32
    SetUnhandledExceptionFilter(unhandledFilter);
    SetErrorMode(SEM_FAILCRITICALERRORS);
#if defined(_MSC_VER)
    _set_purecall_handler(onPureCall);
    _set_invalid_parameter_handler(onInvalidParam);
#endif
#endif

    std::set_terminate(onTerminate);
    std::signal(SIGSEGV, onCrashSignal);
    std::signal(SIGABRT, onCrashSignal);
    std::signal(SIGFPE, onCrashSignal);
    std::signal(SIGILL, onCrashSignal);
#ifdef SIGBUS
    std::signal(SIGBUS, onCrashSignal);
#endif
}

}
