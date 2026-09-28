#pragma once


#include "GameHub.h"
#include "../platform/ObfStr.h"
#include "../platform/Platform.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif


inline uint64_t intgHashBytes(const void* data, size_t n, uint64_t h = 1469598103934665603ull) {
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

inline uint64_t intgHashFile(const std::string& path, uint64_t seed = 1469598103934665603ull) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return 0;
    uint64_t h = seed;
    unsigned char buf[64 * 1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        h = intgHashBytes(buf, n, h);
    fclose(f);
    return h;
}

inline std::string intgHex(uint64_t h) {
    char b[24];
    std::snprintf(b, sizeof(b), "%016llX", (unsigned long long)h);
    return b;
}


struct IntegrityReport {
    bool ok = true;
    std::string fingerprint;
    std::vector<std::string> failures;
    uint64_t exeHash = 0;
    uint64_t contentHash = 0;
    uint64_t textBaseline = 0;
    int suspiciousModules = 0;

    void fail(const std::string& why) {
        ok = false;
        if (failures.size() < 16) failures.push_back(why);
    }
};

inline IntegrityReport& integrity() {
    static IntegrityReport r;
    return r;
}


inline std::string intgSelfPath() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return n ? std::string(buf, n) : std::string();
#else
    return {};
#endif
}

/**
 * Cartella da cui il client controlla la propria integrita'.
 *
 * Fuori da Windows qui non c'era niente: intgSelfPath() ritornava una stringa
 * vuota e questa funzione ripiegava su ".", la cartella di lavoro. Su Android
 * quella e' "/", dove nessuno dei file del gioco esiste — quindi il controllo
 * concludeva che mancassero tutti, cioe' che il client fosse manomesso, e il
 * gate scollegava il giocatore un istante dopo che si era connesso. Il sintomo
 * era "il telefono non si collega al server": in realta' si collegava, e
 * veniva cacciato subito.
 *
 * plat::selfDir() risponde su ogni piattaforma e su Android punta alla copia
 * degli asset estratta dall'APK, che e' esattamente cio' che va verificato.
 */
inline std::string intgSelfDir() {
    return plat::selfDir();
}


inline uint64_t intgHashOwnTextSection() {
#ifdef _WIN32
    HMODULE mod = GetModuleHandleA(nullptr);
    if (!mod) return 0;
    const unsigned char* base = (const unsigned char*)mod;

    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (std::memcmp(sec->Name, ".text", 5) != 0) continue;
        const unsigned char* p = base + sec->VirtualAddress;
        size_t len = sec->Misc.VirtualSize;
        if (!len || len > 64u * 1024u * 1024u) return 0;

        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return 0;
        if (mbi.State != MEM_COMMIT) return 0;
        if (mbi.Protect & PAGE_GUARD) return 0;
        if ((mbi.Protect & 0xFF) == PAGE_NOACCESS) return 0;

        size_t avail = (size_t)mbi.RegionSize
                     - (size_t)((const unsigned char*)p - (const unsigned char*)mbi.BaseAddress);
        if (avail < len) len = avail;
        if (!len) return 0;
        return intgHashBytes(p, len);
    }
#endif
    return 0;
}


inline void intgToLower(std::string& s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
}

inline int intgScanModules(std::vector<std::string>& outNames) {
#ifdef _WIN32
    const std::string kBad[] = {
        OBF("injector"), OBF("cheatengine"), OBF("speedhack"),
        OBF("dbghelp_hook"), OBF("extremeinjector"), OBF("xenos"),
        OBF("hyperion"), OBF("dll_inject"), OBF("detours"),
        OBF("easyhook"), OBF("megadumper"), OBF("scylla"),
        OBF("hookdll"), OBF("processhacker"),
    };
    HMODULE mods[512];
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed))
        return 0;
    int count = (int)(needed / sizeof(HMODULE));
    if (count > 512) count = 512;
    int hits = 0;
    for (int i = 0; i < count; ++i) {
        char path[MAX_PATH] = {};
        DWORD n = GetModuleFileNameExA(GetCurrentProcess(), mods[i], path, MAX_PATH);
        if (n == 0) {
            ++hits;
            if (outNames.size() < 8) outNames.push_back("<modulo senza path su disco>");
            continue;
        }
        std::string low(path, n);
        intgToLower(low);
        for (const std::string& bad : kBad) {
            if (low.find(bad) != std::string::npos) {
                ++hits;
                if (outNames.size() < 8) outNames.push_back(low);
                break;
            }
        }
    }
    return hits;
#else
    (void)outNames;
    return 0;
#endif
}


inline std::vector<std::string> intgCriticalFiles() {
    std::string d = intgSelfDir();
    return {
        d + "/assets/places/manifest.json",
        d + "/assets/character.fbx",
        d + "/assets/roblox-noob.glb",
        d + "/assets/scripts/animate_example.lua",
    };
}

inline void intgBoot() {
    IntegrityReport& r = integrity();
    r = IntegrityReport{};

    std::string self = intgSelfPath();
    r.exeHash = intgHashFile(self);
#ifdef _WIN32
    // Fuori da Windows questi due controlli NON ESISTONO, e la differenza
    // conta: intgSelfPath() e la lettura della sezione .text sono meccanismi
    // dell eseguibile PE. Trattare "non verificabile" come "manomesso"
    // significava bocciare ogni client Android — ed e esattamente quello che
    // succedeva: il telefono si collegava al server e veniva scollegato
    // subito dopo, senza spiegazione.
    //
    // L assenza di un controllo non e una prova di colpa. I controlli che
    // funzionano ovunque — le impronte dei file di gioco qui sotto — restano
    // attivi e continuano a valere.
    if (r.exeHash == 0) r.fail("Eseguibile non leggibile per la verifica.");
#endif

#ifdef __ANDROID__
    // Il controllo giusto per Android non e' l'eseguibile PE (qui non
    // esiste: e' una .so dentro un APK): e' CHI ha firmato l'APK installato.
    // Un APK modificato va rifirmato con un'ALTRA chiave — chi lo modifica
    // non ha la nostra chiave privata — quindi la firma cambia e questo
    // confronto lo scopre.
    //
    // kExpectedApkSha256 e' la chiave di SVILUPPO (debug.keystore), la
    // stessa usata per firmare gli APK di test — vedi GOOGLE_SETUP.md per
    // come e' stata ricavata (keytool -list -v). Il giorno della
    // pubblicazione va sostituita con la SHA-256 della chiave di RELEASE,
    // altrimenti ogni build firmata per il rilascio risultera' "manomessa"
    // — e' il prezzo di un controllo che funziona: va aggiornato quando la
    // firma vera cambia, esattamente come le impronte dei file qui sotto.
    // Un ARRAY, non un puntatore: e' quello che rende il controllo sotto
    // possibile. sizeof su un const char* darebbe la dimensione di un
    // puntatore (4 o 8), non la lunghezza della stringa — un errore che
    // avrebbe reso il controllo inutile senza sembrarlo.
    static constexpr char kExpectedApkSha256[] =
        "CD61F17B1E58DA3A42690C941C7166FB85171D5D1AB6A2FDE6B0E9F7A924A445";
    // Una SHA-256 e' sempre 32 byte, cioe' 64 caratteri esadecimali. Una
    // trascrizione a mano ne perde facilmente uno senza che si veda a
    // occhio (successo qui: la prima versione era di 63 caratteri, non 64
    // — scoperto solo confrontando col telefono vero, non guardandola).
    // Con questo controllo l'errore diventa un fallimento di build, non un
    // anti-tamper che boccia ogni utente legittimo finche' qualcuno non se
    // ne accorge da un dispositivo reale.
    static_assert(sizeof(kExpectedApkSha256) - 1 == 64,
                  "kExpectedApkSha256 deve essere lunga esattamente 64 caratteri esadecimali");
    const std::string& apkSig = plat::androidSigningCertSha256Ref();
    if (apkSig.empty()) {
        r.fail("Firma dell'APK non leggibile.");
    } else if (apkSig != kExpectedApkSha256) {
        r.fail("Firma dell'APK non corrisponde: APK modificato o firmato con un'altra chiave.");
    }
#endif

    uint64_t ch = 1469598103934665603ull;
    int found = 0;
    for (const std::string& f : intgCriticalFiles()) {
        uint64_t h = intgHashFile(f);
        if (h) { ++found; ch = intgHashBytes(&h, sizeof(h), ch); }
    }
    r.contentHash = found ? ch : 0;

    r.textBaseline = intgHashOwnTextSection();
#ifdef _WIN32
    if (r.textBaseline == 0) r.fail("Sezione .text non verificabile.");
#endif

    std::vector<std::string> bad;
    r.suspiciousModules = intgScanModules(bad);
    for (const std::string& b : bad) r.fail("Modulo sospetto: " + b);

    uint64_t fp = intgHashBytes(&r.exeHash, sizeof(r.exeHash));
    fp = intgHashBytes(&r.contentHash, sizeof(r.contentHash), fp);
    r.fingerprint = intgHex(fp);

#ifdef BLOCKY_INTEGRITY_VERBOSE
    std::printf("[dbg] %s %s %s m=%d %s\n",
                intgHex(r.exeHash).c_str(), intgHex(r.contentHash).c_str(),
                intgHex(r.textBaseline).c_str(), r.suspiciousModules,
                r.ok ? "ok" : "ko");
#endif
}

inline bool intgTick(float dt) {
    IntegrityReport& r = integrity();
    static float acc = 0.f;
    acc += dt;
    if (acc < 5.f) return r.ok;
    acc = 0.f;

    if (r.textBaseline) {
        uint64_t now = intgHashOwnTextSection();
        if (now != 0 && now != r.textBaseline)
            r.fail("Codice modificato in memoria durante la partita.");
    }
    std::vector<std::string> bad;
    int hits = intgScanModules(bad);
    if (hits > r.suspiciousModules) {
        r.suspiciousModules = hits;
        for (const std::string& b : bad) r.fail("Modulo iniettato: " + b);
    }
    return r.ok;
}

inline std::string intgFailureText() {
    const IntegrityReport& r = integrity();
    if (r.ok) return {};
    std::string s;
    for (size_t i = 0; i < r.failures.size(); ++i) {
        if (i) s += "\n";
        s += "- " + r.failures[i];
    }
    return s;
}
