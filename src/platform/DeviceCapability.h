#pragma once

// Stima se il dispositivo e' "debole" per far girare un gioco in modo fluido.
// Usato per mostrare un avviso quando si entra in un gioco (soprattutto su
// telefoni Android vecchi/economici). Solo euristica: non blocca niente.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__ANDROID__)
#include <unistd.h>
#include <fstream>
#include <sys/system_properties.h>
#else
#include <unistd.h>
#include <fstream>
#endif

namespace plat {
namespace devcap {

struct Info {
    bool queried = false;
    bool weak = false;
    int androidSdk = 0;          // 0 = non Android / sconosciuto
    int androidMajor = 0;        // versione "umana" (8 = Oreo), 0 = sconosciuta
    int cpuCores = 0;
    std::uint64_t ramBytes = 0;
    char reason[256]{};
};

// Soglie: Android < 9 (API 28), RAM totale < 4 GB, al massimo 4 core.
// (minSdk dell'APK e' 26 = Android 8: su Android 7 Blocky non si installa
// proprio, quindi l'avviso copre i telefoni Android 8 ancora supportati.)
inline constexpr int kMinAndroidSdk = 28;
inline constexpr std::uint64_t kMinRamBytes = 4ull * 1024ull * 1024ull * 1024ull;
inline constexpr int kMinCores = 4;

inline int androidSdkToMajor(int sdk) {
    if (sdk <= 0) return 0;
    if (sdk <= 25) return 7;   // 24-25 = Android 7
    if (sdk <= 27) return 8;
    if (sdk == 28) return 9;
    if (sdk == 29) return 10;
    if (sdk == 30) return 11;
    if (sdk <= 32) return 12;
    if (sdk == 33) return 13;
    if (sdk == 34) return 14;
    return 15 + (sdk - 35);
}

inline std::uint64_t queryTotalRam() {
#if defined(_WIN32)
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) return (std::uint64_t)ms.ullTotalPhys;
    return 0;
#else
    std::ifstream f("/proc/meminfo");
    std::string line;
    while (f && std::getline(f, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            unsigned long long kb = 0;
            std::sscanf(line.c_str(), "MemTotal: %llu kB", &kb);
            return (std::uint64_t)kb * 1024ull;
        }
    }
    long pages = sysconf(_SC_PHYS_PAGES);
    long psz = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && psz > 0) return (std::uint64_t)pages * (std::uint64_t)psz;
    return 0;
#endif
}

inline int queryCpuCores() {
#if defined(_WIN32)
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 0;
#endif
}

inline int queryAndroidSdk() {
#if defined(__ANDROID__)
    char buf[PROP_VALUE_MAX]{};
    if (__system_property_get("ro.build.version.sdk", buf) > 0)
        return std::atoi(buf);
#endif
    return 0;
}

inline Info& info() {
    static Info i;
    if (i.queried) return i;
    i.queried = true;
    i.ramBytes = queryTotalRam();
    i.cpuCores = queryCpuCores();
    i.androidSdk = queryAndroidSdk();
    i.androidMajor = androidSdkToMajor(i.androidSdk);

    std::string why;
    if (i.androidSdk > 0 && i.androidSdk < kMinAndroidSdk) {
        why += "Android " + std::to_string(i.androidMajor) + " (consigliato 9 o superiore)";
    }
    if (i.ramBytes > 0 && i.ramBytes < kMinRamBytes) {
        if (!why.empty()) why += " · ";
        char b[48];
        std::snprintf(b, sizeof(b), "RAM %.1f GB (consigliati 4 GB)",
                      (double)i.ramBytes / (1024.0 * 1024.0 * 1024.0));
        why += b;
    }
    if (i.cpuCores > 0 && i.cpuCores <= kMinCores) {
        if (!why.empty()) why += " · ";
        why += "CPU " + std::to_string(i.cpuCores) + " core";
    }
    i.weak = !why.empty();
    std::strncpy(i.reason, why.c_str(), sizeof(i.reason) - 1);
    return i;
}

inline bool isWeakDevice() { return info().weak; }

} // namespace devcap
} // namespace plat
