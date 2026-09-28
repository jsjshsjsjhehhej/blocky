
#include <windows.h>
#include <cstdio>
#ifndef BLOCKY_DEV
#include "AutoUpdater.h"
#endif

typedef int (*BlockyRunFn)();

int main() {
#ifndef BLOCKY_DEV
    blk::AutoUpdater::tryAutoUpdate("Blocky");
#endif
    HMODULE dll = LoadLibraryA("libblocky.dll");
    if (!dll) {
        MessageBoxA(nullptr,
            "libblocky.dll non trovata accanto a Blocky.exe.\n"
            "Reinstalla o ricompila il progetto.",
            "Blocky", MB_ICONERROR | MB_OK);
        return 1;
    }

    auto run = reinterpret_cast<BlockyRunFn>(GetProcAddress(dll, "Blocky_Run"));
    if (!run) {
        MessageBoxA(nullptr,
            "libblocky.dll trovata ma non espone Blocky_Run.\n"
            "La dll è di una versione incompatibile.",
            "Blocky", MB_ICONERROR | MB_OK);
        FreeLibrary(dll);
        return 1;
    }

    const int code = run();
    FreeLibrary(dll);
    return code;
}
