/**
 * Implementazione dell'API esportata — vedi BlockyApi.h per il contratto.
 *
 * Corpo minuscolo apposta: ogni funzione qui dentro chiama qualcosa che vive
 * altrove (main.cpp, studio_main.cpp) e che oggi resta `static`/a
 * collegamento interno. Il confine della dll passa solo da qui: nessun altro
 * simbolo del motore è esportato, come richiesto dalla specifica
 * anti-reverse ("funzioni esportate minime, nessuna classe esportata").
 */

#define BLOCKY_API_BUILDING_DLL
#include "BlockyApi.h"

// Definite in main.cpp — non dichiarate in un header condiviso perché non
// devono essere chiamabili da nessun altro punto del motore: solo da qui.
extern int Blocky_RunEngine();
extern void blockySetEngineMode(bool isGameEngine);
extern void blockyStageLoadPath(const char* path);

// Definita in studio_main.cpp.
extern int Blocky_RunEditorImpl();

int Blocky_Init() {
    return 0;
}

int Blocky_Run() {
    blockySetEngineMode(false);
    return Blocky_RunEngine();
}

int Blocky_RunStudio() {
    blockySetEngineMode(true);
    return Blocky_RunEngine();
}

int Blocky_RunEditor() {
    return Blocky_RunEditorImpl();
}

void Blocky_LoadGame(const char* path) {
    blockyStageLoadPath(path);
}

void Blocky_Shutdown() {
}
