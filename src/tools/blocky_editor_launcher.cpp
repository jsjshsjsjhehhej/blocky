/**
 * BlockyStudio.exe — lanciatore minimo dell'editor place.
 *
 * Stessa dll degli altri due, export Blocky_RunEditor(). Prima era un
 * eseguibile a sé con studio_main.cpp compilato per conto suo (il vantaggio
 * era iterare sull'editor senza ricompilare tutto main.cpp); ora
 * studio_main.cpp vive dentro libblocky.dll insieme al resto, quindi quel
 * vantaggio di velocità di build sparisce — resta solo la separazione dei
 * tre punti d'ingresso, ognuno con il proprio piccolo .exe.
 */

#include <windows.h>

typedef int (*BlockyRunFn)();

int main() {
    HMODULE dll = LoadLibraryA("libblocky.dll");
    if (!dll) {
        MessageBoxA(nullptr,
            "libblocky.dll non trovata accanto a BlockyStudio.exe.\n"
            "Reinstalla o ricompila il progetto.",
            "Blocky Studio", MB_ICONERROR | MB_OK);
        return 1;
    }

    auto run = reinterpret_cast<BlockyRunFn>(GetProcAddress(dll, "Blocky_RunEditor"));
    if (!run) {
        MessageBoxA(nullptr,
            "libblocky.dll trovata ma non espone Blocky_RunEditor.\n"
            "La dll è di una versione incompatibile.",
            "Blocky Studio", MB_ICONERROR | MB_OK);
        FreeLibrary(dll);
        return 1;
    }

    const int code = run();
    FreeLibrary(dll);
    return code;
}
