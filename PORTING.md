# Portare Blocky su una piattaforma nuova

Questo documento non è teoria: è il metodo che ha funzionato davvero per
Android, misurato sul codice com'è oggi.

## Il principio: non riscrivere il motore, ingannarlo

Il motore è nato per Windows + GLFW + OpenGL 3.3. La tentazione è riscriverlo
per ogni piattaforma. Su Android abbiamo fatto l'opposto: abbiamo messo davanti
al motore delle **finte intestazioni** (`android/app/src/main/cpp/glfw_shim/`)
che si chiamano `GLFW/glfw3.h` e `glad/gl.h` e stanno **prima** nel percorso di
ricerca. Il motore continua a chiamare `glfwGetKey` e `glfwSwapBuffers` come
sempre; dietro, quelle funzioni sono alimentate dai tocchi e da EGL.

Risultato: circa 15.000 righe di motore compilate **senza modifiche**.

## I quattro punti che vanno toccati davvero

### 1. Il ciclo di gioco

Su desktop il ciclo è nostro (`while (!glfwWindowShouldClose)`). Su Android e
iOS il sistema chiama noi, un frame alla volta. Per questo `Blocky_RunEngine`
è stato spezzato in tre funzioni, che ogni porting riusa:

    blockyAndroidInit()      // una volta
    blockyAndroidFrame()     // una per frame
    blockyAndroidShutdown()  // alla chiusura

Il nome dice "Android" per ragioni storiche, ma sono **generiche**: una porta
iOS o VR guida lo stesso ciclo.

### 2. I percorsi

Ogni funzione che costruisce un percorso deve passare da `plat::selfDir()` o
`plat::dataDir()` (`src/platform/Platform.h`), che hanno già il ramo per
Windows, macOS, Linux e Android.

**Questo è stato l'errore più costoso e più ripetuto.** Un ripiego del tipo
`return "blocky_profile.txt"` sembra innocuo, ma su Android la cartella di
lavoro è `/`, in sola lettura: il file non viene scritto e **non succede
niente di visibile**. Sono stati bug reali, uno per uno: account mai salvato,
rapporti di crash mai scritti, lingua dimenticata, e — il peggiore — il
controllo d'integrità che cercava i file nel posto sbagliato, concludeva che
il client fosse manomesso e **scollegava il giocatore un istante dopo la
connessione**.

Regola: *se una funzione ha un ripiego con un percorso relativo, è un bug che
aspetta.*

### 3. Le API di sistema

Dove la piattaforma ha un'API propria, si aggiunge un ramo — non si lascia
fallire in silenzio:

| Cosa | Windows | Android | macOS |
|---|---|---|---|
| Audio | `PlaySound` / MCI | OpenSL ES (`AndroidAudio.h`) | CoreAudio — **da fare** |
| Aprire un link | `ShellExecute` | Intent via JNI | `open` |
| Gamepad | XInput | — **da fare** | GameController — **da fare** |
| Scelta file | `GetOpenFileName` | — (si importa dal PC) | NSOpenPanel — **da fare** |

Dove un controllo **non esiste** sulla piattaforma, non va trattato come un
fallimento: la lettura della sezione `.text` dell'eseguibile è un concetto di
Windows, e trattarla come "manomissione" bocciava ogni client Android.
*L'assenza di un controllo non è una prova di colpa.*

### 4. Gli shader

`src/render/GlslPortable.h` riscrive al volo la riga `#version` e inserisce le
qualificazioni di precisione richieste da OpenGL ES. È il punto unico dove
adattare il linguaggio degli shader a un'altra variante.

## Apple: cosa costa davvero

Misurato sul codice attuale:

- **866** chiamate OpenGL dirette, in **19** file.
- **Nessuna** funzione oltre OpenGL 3.3 — niente compute shader, niente
  accesso diretto agli oggetti, niente estensioni moderne.

Da qui due conclusioni molto diverse fra loro:

**macOS con OpenGL è quasi gratis.** macOS supporta OpenGL fino alla 4.1 e noi
non superiamo la 3.3. GLFW gira nativamente, i percorsi hanno già il ramo
Apple, gli shader `#version 330 core` vanno bene. Resta l'audio (CoreAudio) e
i dialoghi di scelta file. Apple ha dichiarato OpenGL deprecato, ma funziona.

**Metal è un lavoro grosso, e va fatto in un ordine preciso.** Non è una
traduzione riga per riga: Metal ha un altro linguaggio di shader (MSL) e un
altro modello di pipeline. Le 866 chiamate devono prima passare tutte da
`src/gfx/GfxCore.h`, che oggi copre **solo le texture**. La strada è:

1. Allargare `GfxCore` a mesh/buffer, poi a shader e pipeline.
2. Spostare il codice esistente dietro quell'astrazione, verificando che
   OpenGL continui a funzionare identico a ogni passo.
3. Solo allora scrivere il secondo backend.

Saltare il passo 2 significa scrivere Metal alla cieca, senza modo di
distinguere un errore di traduzione da un errore di Metal.

**iOS somiglia ad Android**, non a macOS: OpenGL ES 3 c'è, e `GlslPortable.h`
risolve già il lato shader. Servirebbe la stessa ricetta — finte intestazioni,
i tre punti d'ingresso, un `libblocky.a` al posto del `.so`.

## Vedi anche

- `android/README.md` — la porta Android nel dettaglio, e il piano per la VR
  (`libblockyvr.so`, OpenXR con hand tracking).
