# Blocky su Android — Fase 1

Progetto Gradle separato, non collegato al `CMakeLists.txt`/alle build
Windows del motore (zero rischio per quelle). Vedi il piano completo per il
contesto: `.claude/plans` di questa sessione, o chiedi a Claude di rifare il
punto.

## Cosa fa davvero

- Un `android_main()` (`app/src/main/cpp/main.cpp`) con `GameActivity` di
  Google (`androidx.games:games-activity`), lo stesso schema del template
  ufficiale "Native C++ (Game Activity)" di Android Studio.
- `Renderer.cpp` crea il contesto EGL/GLES 3 e disegna UN triangolo che
  cambia colore nel tempo, usando la **vertex shader vera** del motore
  (`kGlesPartVS` da `src/render/GlShaders.h`, inclusa via un percorso
  relativo — non ricopiata a mano).
- La fragment shader è nuova per questa fase: `kGlesStudAtmosFS` (la
  "gemella" di `kGlesPartVS` nel motore) si aspetta `vLocalN`/`vStudUV` che
  `kGlesPartVS` non emette — una cosa già così nel motore (usata anche da
  `src/garden/garden_main.cpp`), non introdotta qui. Non l'ho "sistemata"
  perché è fuori dallo scope di questa fase — segnalarlo è più onesto che
  nasconderlo dietro una shader che rischia di non linkare bene.

## Cosa NON fa (ancora)

Non è collegato al motore vero (`main.cpp`, `engine_lib`, Lua, il mondo di
gioco) — è solo la prova che NDK → `.so` → JNI/GameActivity → schermo
funziona, riusando shader vere invece di inventarne di nuove. Il porting
vero (loop di gioco, input touch, asset da `AAssetManager`) è la Fase 2,
scritta solo come traccia nel piano.

## Come si compila/gira

Dalla cartella `android/`:

```
./gradlew.bat assembleDebug
```

Per provarlo su un emulatore già configurato (`Medium_Phone`):

```
emulator -avd Medium_Phone
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.blocky.preview/.MainActivity
adb logcat -s BlockyPreview
```

## Librerie native collegate

`libblocky.so` è il motore completo. Oltre a EGL/GLESv3/android/log collega
**OpenSLES**, usata da `src/engine/AndroidAudio.h` per la riproduzione dei
suoni: su Windows il motore usa `PlaySound`/MCI, che fuori da lì non esistono,
e senza questa libreria il gioco girerebbe muto sul telefono.

OpenSL ES è stata scelta al posto di AAudio perché apre un file audio dandogli
il **percorso** e lo decodifica da sola (wav, mp3, ogg). È la stessa forma di
`PlaySound`, quindi `SoundService` non cambia nella sua logica. AAudio lavora
a campioni grezzi e avrebbe richiesto un decodificatore per ogni formato.

## Se Blocky va in VR — `libblockyvr.so` (piano, non ancora fatto)

La parte VR andrà in una libreria **separata**, non dentro `libblocky.so`:

- **Nome**: `libblockyvr.so`, con il suo `CMakeLists.txt` accanto a questo.
- **Cosa contiene**: la gestione della sessione OpenXR (creazione istanza,
  swapchain per i due occhi, ciclo dei frame) e l'**hand tracking**
  (`XR_EXT_hand_tracking`), che sostituisce i comandi a tocco.
- **Perché separata**: `libblocky.so` deve continuare a partire su un telefono
  normale, dove il runtime OpenXR non esiste. Tenendo la VR in una libreria a
  parte, il visore la carica e il telefono no — invece di avere un motore solo
  che fallisce il caricamento dove OpenXR manca.
- **Come si collegano**: le stesse funzioni che usa `android_entry.cpp` oggi —
  `blockyAndroidInit` / `blockyAndroidFrame` / `blockyAndroidShutdown`. La VR
  guida lo stesso ciclo, cambiando solo chi fornisce la matrice di vista e da
  dove arriva l'input.
- **Audio**: anche `libblockyvr.so` collegherà **OpenSLES**, per lo stesso
  motivo di `libblocky.so`.
