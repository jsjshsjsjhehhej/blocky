#pragma once

/**
 * API pubblica di libblocky.dll — l'unico punto di contatto fra un
 * eseguibile e l'engine.
 *
 * Tre eseguibili la caricano oggi: Blocky.exe (hub/player), BlockyGameEngine.exe
 * (Studio classico dentro main.cpp) e BlockyStudio.exe (l'editor place
 * separato). Ognuno è un guscio minimo — LoadLibrary, GetProcAddress, una
 * chiamata — senza GLFW, ImGui o assimp linkati direttamente: quella roba
 * vive tutta dentro la dll.
 *
 * Funzioni esportate al minimo indispensabile, nessuna classe attraversa il
 * confine della dll: un layout di vtable o di struct che cambia fra una
 * build e l'altra della dll e una vecchia dell'exe si romperebbe in modo
 * silenzioso. Solo funzioni C, solo tipi a dimensione fissa.
 *
 * Blocky_Init() e Blocky_Shutdown() sono nella firma per rispettare il
 * contratto previsto, ma oggi sono no-op dichiarati: l'inizializzazione e la
 * pulizia vivono ancora dentro le singole Blocky_Run*() e tornano solo
 * quando la finestra si chiude. Scorporarle davvero (init separato da un
 * loop che il chiamante pompa lui) è un secondo passo, non fatto qui.
 */

#ifdef _WIN32
  #ifdef BLOCKY_API_BUILDING_DLL
    #define BLOCKY_API extern "C" __declspec(dllexport)
  #else
    #define BLOCKY_API extern "C" __declspec(dllimport)
  #endif
#else
  #define BLOCKY_API extern "C"
#endif

/** Riservato: oggi ogni Blocky_Run*() fa il proprio setup. Ritorna sempre 0. */
BLOCKY_API int Blocky_Init();

/** Hub/player — stesso comportamento storico di Blocky.exe. Blocca finché
 *  la finestra non si chiude, poi ritorna il codice di uscita. */
BLOCKY_API int Blocky_Run();

/** Studio classico dentro main.cpp — stesso comportamento storico di
 *  BlockyGameEngine.exe. Non è nella lista originale della specifica: un
 *  solo Blocky_Run() non basta a coprire sia il player sia l'editor,
 *  perché oggi sono due rami a runtime dello stesso main(), non due
 *  fasi di un unico ciclo. */
BLOCKY_API int Blocky_RunStudio();

/** Editor place separato — stesso comportamento storico di BlockyStudio.exe.
 *  Stessa ragione di Blocky_RunStudio(): un terzo binario, un terzo export. */
BLOCKY_API int Blocky_RunEditor();

/**
 * Mette in coda un place da aprire all'avvio, prima di chiamare una delle
 * Run. Vince su quanto dedotto dagli argv del processo (l'associazione
 * file .blocky su Windows): un host che incorpora la dll potrebbe non avere
 * controllo sulla riga di comando.
 */
BLOCKY_API void Blocky_LoadGame(const char* path);

/** Riservato: oggi ogni Blocky_Run*() fa la propria pulizia prima di
 *  ritornare. No-op. */
BLOCKY_API void Blocky_Shutdown();
