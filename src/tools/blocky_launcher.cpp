/**
 * Blocky.exe — lanciatore minimo dell'hub/player.
 *
 * Non linka GLFW, ImGui, assimp o glad: carica libblocky.dll a runtime e
 * chiama Blocky_Run(). Tutto il motore vive nella dll; questo eseguibile è
 * solo il punto d'ingresso che Windows Explorer sa aprire.
 *
 * Windows cerca la dll prima nella cartella dell'eseguibile: basta che
 * libblocky.dll stia accanto a questo .exe, come fanno già gli asset.
 */

#include <windows.h>
#include <cstdio>

typedef int (*BlockyRunFn)();

int main() {
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
