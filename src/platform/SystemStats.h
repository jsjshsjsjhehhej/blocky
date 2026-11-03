#pragma once

/**
 * CPU / RAM / GPU (OpenGL) — stile statistiche prestazioni Roblox.
 * Solo chiamate OS qui (SystemApi boundary).
 */

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__ANDROID__)
#include <unistd.h>
#include <cstdlib>
#else
#include <unistd.h>
#include <sys/resource.h>
#include <fstream>
#include <sstream>
#endif

#include <glad/gl.h>

namespace plat {
namespace sysstats {

struct Snapshot {
    float cpuProcessPct = 0.f;
    std::uint64_t memWorkingSetBytes = 0;
    std::uint64_t memPrivateBytes = 0;
    char gpuVendor[128]{};
    char gpuRenderer[192]{};
    char gpuVersion[128]{};
    bool gpuQueried = false;
    bool vramKnown = false;
    std::uint64_t vramTotalBytes = 0;
    std::uint64_t vramFreeBytes = 0;
};

inline Snapshot& snap() {
    static Snapshot s;
    return s;
}

inline float& accum() {
    static float t = 0.f;
    return t;
}

inline void formatMb(char* buf, size_t n, std::uint64_t bytes) {
    const double mb = (double)bytes / (1024.0 * 1024.0);
    std::snprintf(buf, n, "%.1f MB", mb);
}

#if defined(_WIN32)
struct CpuSample {
    ULARGE_INTEGER lastSys = {};
    ULARGE_INTEGER lastUser = {};
    bool have = false;
};
inline CpuSample& cpuSample() {
    static CpuSample c;
    return c;
}

inline void sampleCpuProcess() {
    FILETIME ftCreate{}, ftExit{}, ftKernel{}, ftUser{};
    if (!GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser))
        return;
    ULARGE_INTEGER sys{}, user{};
    sys.LowPart = ftKernel.dwLowDateTime;
    sys.HighPart = ftKernel.dwHighDateTime;
    user.LowPart = ftUser.dwLowDateTime;
    user.HighPart = ftUser.dwHighDateTime;
    CpuSample& c = cpuSample();
    if (c.have) {
        const double sysDelta = (double)(sys.QuadPart - c.lastSys.QuadPart);
        const double userDelta = (double)(user.QuadPart - c.lastUser.QuadPart);
        const double total = sysDelta + userDelta;
        if (total > 0.0) {
            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            const int ncpu = std::max(1, (int)si.dwNumberOfProcessors);
            snap().cpuProcessPct = (float)std::clamp((total / 10000.0) / (double)ncpu, 0.0, 100.0);
        }
    }
    c.lastSys = sys;
    c.lastUser = user;
    c.have = true;
}

inline void sampleMemory() {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        return;
    snap().memWorkingSetBytes = pmc.WorkingSetSize;
    snap().memPrivateBytes = pmc.PrivateUsage;
}

#elif defined(__ANDROID__)
inline void sampleCpuProcess() {
    static long long lastTotal = 0;
    static double lastWall = 0.0;
    std::ifstream st("/proc/self/stat");
    if (!st) return;
    std::string ignore;
    long long utime = 0, stime = 0;
    for (int i = 0; i < 13; ++i) st >> ignore;
    st >> utime >> stime;
    const long long total = utime + stime;
    const double wall = (double)clock() / (double)CLOCKS_PER_SEC;
    if (lastWall > 0.0 && wall > lastWall) {
        const double delta = (double)(total - lastTotal);
        const long hz = sysconf(_SC_CLK_TCK);
        if (hz > 0) {
            const double pct = (delta / (double)hz) / (wall - lastWall) * 100.0;
            snap().cpuProcessPct = (float)std::clamp(pct, 0.0, 100.0);
        }
    }
    lastTotal = total;
    lastWall = wall;
}

inline void sampleMemory() {
    std::ifstream f("/proc/self/status");
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::uint64_t kb = 0;
            std::sscanf(line.c_str(), "VmRSS: %llu kB", (unsigned long long*)&kb);
            snap().memWorkingSetBytes = kb * 1024ull;
        } else if (line.rfind("VmData:", 0) == 0) {
            std::uint64_t kb = 0;
            std::sscanf(line.c_str(), "VmData: %llu kB", (unsigned long long*)&kb);
            snap().memPrivateBytes = kb * 1024ull;
        }
    }
}
#else
inline void sampleCpuProcess() {
    static struct rusage last{};
    static bool have = false;
    struct rusage now{};
    if (getrusage(RUSAGE_SELF, &now) != 0) return;
    if (have) {
        const double user = (double)(now.ru_utime.tv_sec - last.ru_utime.tv_sec)
                          + (now.ru_utime.tv_usec - last.ru_utime.tv_usec) * 1e-6;
        const double sys = (double)(now.ru_stime.tv_sec - last.ru_stime.tv_sec)
                         + (now.ru_stime.tv_usec - last.ru_stime.tv_usec) * 1e-6;
        snap().cpuProcessPct = (float)std::clamp((user + sys) * 100.0, 0.0, 100.0);
    }
    last = now;
    have = true;
}

inline void sampleMemory() {
    struct rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0)
        snap().memWorkingSetBytes = (std::uint64_t)ru.ru_maxrss * 1024ull;
}
#endif

inline void queryGpuOnce() {
    Snapshot& s = snap();
    if (s.gpuQueried) return;
    s.gpuQueried = true;
    const char* v = (const char*)glGetString(GL_VENDOR);
    const char* r = (const char*)glGetString(GL_RENDERER);
    const char* ver = (const char*)glGetString(GL_VERSION);
    if (v) std::strncpy(s.gpuVendor, v, sizeof(s.gpuVendor) - 1);
    if (r) std::strncpy(s.gpuRenderer, r, sizeof(s.gpuRenderer) - 1);
    if (ver) std::strncpy(s.gpuVersion, ver, sizeof(s.gpuVersion) - 1);

#if !defined(__ANDROID__)
    // Estensioni vendor desktop OpenGL (non disponibili su OpenGL ES).
    if (GLAD_GL_NVX_gpu_memory_info) {
        GLint totalKb = 0, availKb = 0;
        glGetIntegerv(0x9048, &totalKb);
        glGetIntegerv(0x9049, &availKb);
        if (totalKb > 0) {
            s.vramKnown = true;
            s.vramTotalBytes = (std::uint64_t)totalKb * 1024ull;
            s.vramFreeBytes = (std::uint64_t)std::max(0, availKb) * 1024ull;
        }
    } else if (GLAD_GL_ATI_meminfo) {
        GLint info[4] = {0, 0, 0, 0};
        glGetIntegerv(0x87FB, info);
        if (info[0] > 0) {
            s.vramKnown = true;
            s.vramFreeBytes = (std::uint64_t)info[0] * 1024ull;
        }
    }
#endif
}

/** Aggiorna campioni (throttle ~4 Hz). Chiamare con contesto GL attivo. */
inline void update(float dt) {
    accum() += dt;
    if (accum() < 0.25f) return;
    accum() = 0.f;
    sampleCpuProcess();
    sampleMemory();
    queryGpuOnce();
}

} // namespace sysstats
} // namespace plat
