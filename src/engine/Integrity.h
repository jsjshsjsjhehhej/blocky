#pragma once

/**
 * Blocky — integrity check profondo (client).
 *
 * Tre livelli, dal piu' debole al piu' utile:
 *
 *   1. FILE     hash dell'eseguibile e dei file critici su disco.
 *               Prende chi patcha il .exe o sostituisce shader/asset.
 *   2. MEMORIA  hash della sezione .text presa all'avvio e ricontrollata
 *               a intervalli. Prende chi installa hook DOPO il boot —
 *               che e' come lavorano gli executor. Confrontare la .text
 *               in RAM con quella su disco NON funziona (le rilocazioni
 *               ASLR la cambiano legittimamente), quindi il paragone e'
 *               sempre contro la baseline presa da noi all'avvio.
 *   3. MODULI   DLL caricate nel processo: nomi sospetti e moduli senza
 *               path su disco (tipico dell'injection manuale).
 *
 * Il risultato e' anche un'impronta (fingerprint) da mandare al server
 * nell'handshake.
 *
 * ONESTA' — questo NON rende il client inviolabile e non puo' farlo:
 * gira sulla macchina dell'attaccante, e chi patcha abbastanza a fondo
 * puo' far restituire "tutto ok" a queste funzioni. Serve ad alzare il
 * costo e a dare al SERVER un dato su cui decidere. La sicurezza vera
 * resta l'autorita' server: vedi la nota in LuaScriptSecurity.h.
 */

#include "GameHub.h"
#include "../platform/ObfStr.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>   // CreateToolhelp32Snapshot: enumerazione processi
#endif

// ---------------------------------------------------------------- hash

/** FNV-1a 64: non crittografico, ma qui serve rilevare modifiche, non firmare. */
inline uint64_t intgHashBytes(const void* data, size_t n, uint64_t h = 1469598103934665603ull) {
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

/** Hash di un file a blocchi. Ritorna 0 se non leggibile. */
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

// ---------------------------------------------------------------- stato

struct IntegrityReport {
    bool ok = true;
    std::string fingerprint;              // da mandare al server
    std::vector<std::string> failures;    // motivi leggibili
    uint64_t exeHash = 0;
    uint64_t contentHash = 0;
    uint64_t textBaseline = 0;            // .text all'avvio
    int suspiciousModules = 0;
    int injectorApps = 0;                 // app esterne che POSSONO iniettare
    std::vector<std::string> injectorNames;

    void fail(const std::string& why) {
        ok = false;
        if (failures.size() < 16) failures.push_back(why);
    }
};

inline IntegrityReport& integrity() {
    static IntegrityReport r;
    return r;
}

// ---------------------------------------------------------------- self

inline std::string intgSelfPath() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return n ? std::string(buf, n) : std::string();
#else
    return {};
#endif
}

inline std::string intgSelfDir() {
    std::string p = intgSelfPath();
    size_t s = p.find_last_of("\\/");
    return s == std::string::npos ? std::string(".") : p.substr(0, s);
}

// ------------------------------------------------- livello 2: memoria

/**
 * Hash della sezione .text mappata in memoria.
 * Si naviga il PE header del proprio modulo: DOS -> NT -> tabella sezioni.
 */
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

        // Le pagine vanno verificate prima di leggerle: se qualcuno ha cambiato
        // i permessi della .text leggerla a occhi chiusi farebbe crashare il
        // gioco. Niente __try/__except: e' SEH di MSVC, qui si compila con
        // MinGW. VirtualQuery fa lo stesso controllo ed e' portabile.
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) return 0;
        if (mbi.State != MEM_COMMIT) return 0;
        if (mbi.Protect & PAGE_GUARD) return 0;
        if ((mbi.Protect & 0xFF) == PAGE_NOACCESS) return 0;

        size_t avail = (size_t)mbi.RegionSize
                     - (size_t)((const unsigned char*)p - (const unsigned char*)mbi.BaseAddress);
        if (avail < len) len = avail;      // non uscire dalla regione verificata
        if (!len) return 0;
        return intgHashBytes(p, len);
    }
#endif
    return 0;
}

// -------------------------------------------------- livello 3: moduli

inline void intgToLower(std::string& s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
}

/**
 * DLL caricate: cerca nomi noti di tool di injection e moduli senza path
 * (una DLL mappata a mano spesso non ha un nome su disco).
 */
inline int intgScanModules(std::vector<std::string>& outNames) {
#ifdef _WIN32
    // Offuscate: nel binario non compaiono in chiaro, si ricostruiscono qui.
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
            // modulo senza path: mappato a mano
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

// ------------------------------------ livello 4: app che POSSONO iniettare

/**
 * Processi esterni con la capacita' di iniettare codice in altri processi.
 *
 * Non si va per nome: la lista dei nomi la aggiri rinominando l'exe. Si
 * guarda il PRIVILEGIO. Per scrivere nella memoria di un altro processo
 * serve SeDebugPrivilege attivo nel token, e quello e' verificabile:
 * OpenProcess -> OpenProcessToken -> GetTokenInformation.
 *
 * Falsi positivi: alcuni strumenti legittimi lo tengono attivo (antivirus,
 * debugger, Process Explorer). Per questo la funzione RIPORTA e non uccide:
 * la decisione sta a chi chiama, e in Blocky il risultato blocca la sessione
 * invece di terminare processi altrui a indovinare.
 */
struct InjectorInfo {
    unsigned long pid = 0;
    std::string name;
};

inline bool intgProcessHasDebugPrivilege(unsigned long pid) {
#ifdef _WIN32
    HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!proc) {
        proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!proc) return false;   // non apribile: non possiamo dire nulla
    }
    HANDLE tok = nullptr;
    if (!OpenProcessToken(proc, TOKEN_QUERY, &tok)) {
        CloseHandle(proc);
        return false;
    }

    DWORD need = 0;
    GetTokenInformation(tok, TokenPrivileges, nullptr, 0, &need);
    if (need == 0 || need > 64 * 1024) {
        CloseHandle(tok); CloseHandle(proc);
        return false;
    }
    std::vector<unsigned char> buf(need);
    bool found = false;
    if (GetTokenInformation(tok, TokenPrivileges, buf.data(), need, &need)) {
        LUID want{};
        if (LookupPrivilegeValueA(nullptr, "SeDebugPrivilege", &want)) {
            const TOKEN_PRIVILEGES* tp = (const TOKEN_PRIVILEGES*)buf.data();
            for (DWORD i = 0; i < tp->PrivilegeCount; ++i) {
                const LUID_AND_ATTRIBUTES& la = tp->Privileges[i];
                if (la.Luid.LowPart == want.LowPart && la.Luid.HighPart == want.HighPart) {
                    // presente non basta: deve essere ABILITATO
                    if (la.Attributes & SE_PRIVILEGE_ENABLED) found = true;
                    break;
                }
            }
        }
    }
    CloseHandle(tok);
    CloseHandle(proc);
    return found;
#else
    (void)pid;
    return false;
#endif
}

/** Processi di sistema/sicurezza che lo tengono attivo per mestiere. */
inline bool intgIsExpectedPrivilegedProcess(const std::string& lowerName) {
    static const char* kOk[] = {
        "svchost.exe", "lsass.exe", "services.exe", "csrss.exe", "wininit.exe",
        "smss.exe", "winlogon.exe", "msmpeng.exe", "nissrv.exe", "securityhealthservice.exe",
        "mssense.exe", "sensece.exe", "wmiprvse.exe", "taskmgr.exe", "explorer.exe",
        "searchindexer.exe", "dwm.exe", "audiodg.exe", "fontdrvhost.exe",
    };
    for (const char* k : kOk)
        if (lowerName == k) return true;
    return false;
}

inline int intgScanInjectorCapableApps(std::vector<InjectorInfo>& out) {
#ifdef _WIN32
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    DWORD self = GetCurrentProcessId();
    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);
    int hits = 0;
    if (Process32First(snap, &pe)) {
        do {
            if (pe.th32ProcessID == self || pe.th32ProcessID <= 4) continue;
            char name[MAX_PATH] = {};
#ifdef UNICODE
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, name, MAX_PATH, nullptr, nullptr);
#else
            std::snprintf(name, sizeof(name), "%s", pe.szExeFile);
#endif
            std::string low = name;
            intgToLower(low);
            if (intgIsExpectedPrivilegedProcess(low)) continue;
            if (!intgProcessHasDebugPrivilege(pe.th32ProcessID)) continue;
            ++hits;
            if (out.size() < 12) {
                InjectorInfo inf;
                inf.pid = pe.th32ProcessID;
                inf.name = name;
                out.push_back(inf);
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return hits;
#else
    (void)out;
    return 0;
#endif
}

// ---------------------------------------------------------------- boot

/**
 * File il cui contenuto deve corrispondere: se li tocchi, il gioco lo sa.
 *
 * Solo file che esistono davvero accanto all'eseguibile in una build reale.
 * Gli shader NON sono qui: sono compilati dentro il binario, quindi li copre
 * gia' l'hash della .text. Un path inesistente darebbe hash 0 e renderebbe
 * il controllo silenziosamente inutile.
 */
inline std::vector<std::string> intgCriticalFiles() {
    std::string d = intgSelfDir();
    return {
        d + "/assets/places/manifest.json",
        d + "/assets/character.fbx",
        d + "/assets/roblox-noob.glb",
        d + "/assets/scripts/animate_example.lua",
    };
}

/**
 * Controllo completo all'avvio. Da chiamare una volta sola, presto,
 * prima che il gioco carichi roba di terze parti.
 */
inline void intgBoot() {
    IntegrityReport& r = integrity();
    r = IntegrityReport{};

    // 1. eseguibile
    std::string self = intgSelfPath();
    r.exeHash = intgHashFile(self);
    if (r.exeHash == 0) r.fail("Eseguibile non leggibile per la verifica.");

    // 2. file critici (assenti = non fatale, alterati = si vede nell'hash)
    uint64_t ch = 1469598103934665603ull;
    int found = 0;
    for (const std::string& f : intgCriticalFiles()) {
        uint64_t h = intgHashFile(f);
        if (h) { ++found; ch = intgHashBytes(&h, sizeof(h), ch); }
    }
    r.contentHash = found ? ch : 0;

    // 3. baseline .text: da qui in poi ogni differenza e' un hook a runtime
    r.textBaseline = intgHashOwnTextSection();
    if (r.textBaseline == 0) r.fail("Sezione .text non verificabile.");

    // 4. moduli
    std::vector<std::string> bad;
    r.suspiciousModules = intgScanModules(bad);
    for (const std::string& b : bad) r.fail("Modulo sospetto: " + b);

    // 5. app esterne che possono iniettare (SeDebugPrivilege attivo)
    std::vector<InjectorInfo> inj;
    r.injectorApps = intgScanInjectorCapableApps(inj);
    for (const InjectorInfo& i : inj) {
        r.injectorNames.push_back(i.name);
        r.fail("App con privilegi di injection attiva: " + i.name);
    }

    // impronta per il server: identita' della build + contenuti
    uint64_t fp = intgHashBytes(&r.exeHash, sizeof(r.exeHash));
    fp = intgHashBytes(&r.contentHash, sizeof(r.contentHash), fp);
    r.fingerprint = intgHex(fp);

    // Niente log qui: una riga "[Integrity] ... FAIL" e' esattamente la stringa
    // che si cerca per trovare il controllo e toglierlo. In debug locale usa
    // BLOCKY_INTEGRITY_VERBOSE, che nella build di release non esiste.
#ifdef BLOCKY_INTEGRITY_VERBOSE
    std::printf("[dbg] %s %s %s m=%d %s\n",
                intgHex(r.exeHash).c_str(), intgHex(r.contentHash).c_str(),
                intgHex(r.textBaseline).c_str(), r.suspiciousModules,
                r.ok ? "ok" : "ko");
#endif
}

/**
 * Ricontrollo periodico. Chiamalo dal game loop con il dt.
 * Ritorna false la prima volta che rileva una manomissione.
 */
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

    // App con privilegi di injection APERTE durante la partita: prima non
    // c'erano, quindi sono state avviate adesso — il momento piu' sospetto.
    std::vector<InjectorInfo> inj;
    int now = intgScanInjectorCapableApps(inj);
    if (now > r.injectorApps) {
        r.injectorApps = now;
        for (const InjectorInfo& i : inj)
            r.fail("App di injection avviata in partita: " + i.name);
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
