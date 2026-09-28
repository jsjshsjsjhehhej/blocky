#pragma once


#ifdef _WIN32
  #ifdef BLOCKY_API_BUILDING_DLL
    #define BLOCKY_API extern "C" __declspec(dllexport)
  #else
    #define BLOCKY_API extern "C" __declspec(dllimport)
  #endif
#else
  #define BLOCKY_API extern "C"
#endif

BLOCKY_API int Blocky_Init();

BLOCKY_API int Blocky_Run();

BLOCKY_API int Blocky_RunStudio();

BLOCKY_API int Blocky_RunEditor();

BLOCKY_API void Blocky_LoadGame(const char* path);

BLOCKY_API void Blocky_Shutdown();
