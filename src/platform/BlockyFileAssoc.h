#pragma once


#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace BlockyFileAssoc {

#ifdef _WIN32
inline std::string exePath() {
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return buf;
}

inline std::string exeDir() {
    std::string p = exePath();
    auto s = p.find_last_of("\\/");
    return s == std::string::npos ? "." : p.substr(0, s);
}

inline bool writeRegSz(HKEY root, const char* sub, const char* name, const char* value) {
    HKEY k = nullptr;
    if (RegCreateKeyExA(root, sub, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS)
        return false;
    LONG r = RegSetValueExA(k, name, 0, REG_SZ,
                            (const BYTE*)value, (DWORD)(std::strlen(value) + 1));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}

inline void registerPlaceAssociation() {
    std::string exe = exePath();
    for (char& c : exe)
        if (c == '/') c = '\\';

    std::string icon = "\"" + exe + "\",0";
    std::string cmd = "\"" + exe + "\" \"%1\"";

    writeRegSz(HKEY_CURRENT_USER, "Software\\Classes\\.blocky", nullptr, "Blocky.Place");
    writeRegSz(HKEY_CURRENT_USER, "Software\\Classes\\.blocky", "Content Type", "application/x-blocky-place");
    writeRegSz(HKEY_CURRENT_USER, "Software\\Classes\\.blocky\\OpenWithProgids", "Blocky.Place", "");

    writeRegSz(HKEY_CURRENT_USER, "Software\\Classes\\Blocky.Place", nullptr, "Blocky Place");
    writeRegSz(HKEY_CURRENT_USER, "Software\\Classes\\Blocky.Place\\DefaultIcon", nullptr, icon.c_str());
    writeRegSz(HKEY_CURRENT_USER, "Software\\Classes\\Blocky.Place\\shell", nullptr, "open");
    writeRegSz(HKEY_CURRENT_USER, "Software\\Classes\\Blocky.Place\\shell\\open", nullptr, "Apri con Blocky");
    writeRegSz(HKEY_CURRENT_USER, "Software\\Classes\\Blocky.Place\\shell\\open\\command",
               nullptr, cmd.c_str());

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

inline std::string placePathFromArgs() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2) {
        if (argv) LocalFree(argv);
        return {};
    }
    std::string path;
    for (int i = 1; i < argc; ++i) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (n <= 1) continue;
        std::string a(n - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, a.data(), n, nullptr, nullptr);
        if (a.empty() || a[0] == '-') continue;
        std::string low = a;
        for (char& c : low) c = (char)std::tolower((unsigned char)c);
        if (low.size() >= 7 && low.compare(low.size() - 7, 7, ".blocky") == 0) {
            path = a;
            break;
        }
        if (low.size() >= 5 && low.compare(low.size() - 5, 5, ".json") == 0) {
            path = a;
            break;
        }
    }
    LocalFree(argv);
    return path;
}
#else
inline void registerPlaceAssociation() {}
inline std::string placePathFromArgs() { return {}; }
#endif

}
