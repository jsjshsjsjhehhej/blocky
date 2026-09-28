
#define BLOCKY_API_BUILDING_DLL
#include "BlockyApi.h"

extern int Blocky_RunEngine();
extern void blockySetEngineMode(bool isGameEngine);
extern void blockyStageLoadPath(const char* path);

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
