/**
 * BlockyGameEngine.exe — lanciatore minimo dello Studio classico.
 *
 * Stessa dll di Blocky.exe (libblocky.dll), export diverso: Blocky_RunStudio()
 * invece di Blocky_Run(). Il ramo Studio-vs-player era prima deciso a
 * compilazione (due copie di main.cpp); ora è la stessa dll, e la scelta la
 * fa questo eseguibile chiamando l'export giusto.
 */

#include <windows.h>

typedef int (*BlockyRunFn)();

int main() {
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
