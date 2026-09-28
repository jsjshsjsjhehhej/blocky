
#include <windows.h>
#ifndef BLOCKY_DEV
#include "AutoUpdater.h"
#endif

typedef int (*BlockyRunFn)();

int main() {
#ifndef BLOCKY_DEV
    blk::AutoUpdater::tryAutoUpdate("Blocky Game Engine");
#endif
    HMODULE dll = LoadLibraryA("libblocky.dll");
    if (!dll) {
        MessageBoxA(nullptr,
            "libblocky.dll non trovata accanto a BlockyGameEngine.exe.\n"
            "Reinstalla o ricompila il progetto.",
            "Blocky Game Engine", MB_ICONERROR | MB_OK);
        return 1;
    }

    auto run = reinterpret_cast<BlockyRunFn>(GetProcAddress(dll, "Blocky_RunStudio"));
    if (!run) {
        MessageBoxA(nullptr,
            "libblocky.dll trovata ma non espone Blocky_RunStudio.\n"
            "La dll è di una versione incompatibile.",
            "Blocky Game Engine", MB_ICONERROR | MB_OK);
        FreeLibrary(dll);
        return 1;
    }

    const int code = run();
    FreeLibrary(dll);
    return code;
}
