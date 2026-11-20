# Prompt: renderer bello e rete liscia

Da incollare all'inizio di una sessione nuova. È il terzo prompt, accanto a
`docs/PROMPT_BLOCKY_LIVELLO_ROBLOX.md` (il piano generale) e `docs/PROMPT_API_LUAU.md`
(le API). Copre le due cose che separano un gioco "che gira" da un gioco "che sembra
fatto bene": **la grafica** e **il multigiocatore senza scatti**.
Una sessione = un passo (o due piccoli), finito, misurato e confrontato prima/dopo.

---

```
Lavoriamo su Blocky, il mio motore di gioco in stile Roblox (C++17/OpenGL + Luau,
Windows con MinGW + Android): C:\Users\User\Documents\RobloxEngine. Rispondimi SEMPRE
in italiano, con onesta': quello che non hai misurato o guardato, dillo.

=====================================================================
OBIETTIVO
=====================================================================
1. GRAFICA: i giochi di Blocky devono sembrare moderni (materiali veri, luce del cielo,
   ombre morbide, profondita', luci, acqua, atmosfera) SENZA cambiare i giochi gia' fatti
   e restando fluidi sul telefono (Android 7: "pochi oggetti ma fluidi").
2. RETE: il multigiocatore deve essere liscio come su Roblox: gli altri giocatori e gli NPC
   si muovono fluidi, il proprio personaggio risponde subito, i colpi di uno sparatutto
   contano dove li vedi, anche con 150 ms di ritardo e un po' di pacchetti persi;
   poi via internet (non solo in rete locale).

=====================================================================
COM'E' FATTO OGGI (leggi prima di toccare)
=====================================================================
GRAFICA
- OpenGL in avanti (forward), un solo passaggio. Gli shader stanno in
  src/render/GlShaders.h; src/render/GlslPortable.h (glslForTarget) li adatta a GLES per
  Android: OGNI shader nuovo deve passare di li' e funzionare anche su GLES.
- src/render: ShadowMap.h (UNA mappa d'ombra del sole, niente cascate), Bloom.h,
  SunSky.h (cielo procedurale), RenderPipeline.h, RenderCullLod.h (scarto e livelli di
  dettaglio), PartSpatialGrid.h, PartMeshes.h, Particles.h, LookPack.h, Texture2D.h.
  src/main_ui/WorldRenderPipeline.h e PartInstancedDraw.h (part a gruppi): il disegno del
  mondo. src/gfx: GfxCore.h, GlDevice.h.
- Materiali: src/engine/PartMaterial.h (partMaterialShading: specular, roughness...),
  texture in assets/textures/materials. Oggi: luce Blinn + riflesso "finto"
  (applyEnvReflection), non PBR. Niente normal map, niente SSAO, niente luce ambiente
  dal cielo (IBL).
- Livelli di qualita': src/platform/QualityProfile.h. Usali: Alto (PC), Medio, Basso
  (telefoni vecchi). Ogni effetto nuovo si spegne o si semplifica nei livelli bassi.
- Esistono anche vk_main.cpp e BlockyDx11.exe (prove Vulkan/DirectX): NON toccarli se
  non te lo chiedo. Si lavora sul percorso OpenGL/GLES.
RETE
- src/engine/LanNet.h: UDP (SOCK_DGRAM), pacchetti per tipo, delta e conferme.
  src/engine/WorldReplication.h: replica di part, GUI, NPC, Tool. NetGate.h, ServerToken.h,
  ServerBrowser.h. Server dedicato: src/blocky_servers (ServerWorld.h, blocky_server.cpp,
  blocky_hostd.cpp). Test: test_server_scripts e i test multiplayer in src/tools.
- Oggi NON c'e': interpolazione lato client (nel codice non ce n'e' traccia), previsione
  del proprio personaggio, passo fisso del server con istantanee numerate, compensazione
  del ritardo per i colpi, scelta di cosa mandare a chi (interesse), relay per internet.
- Memoria: il multigiocatore via dati mobili / altre regioni richiede un relay (CGNAT),
  non solo un ritocco della rete locale. L'account del server online (Railway) e' mio:
  non creare account e non usare credenziali.

=====================================================================
REGOLE
=====================================================================
1. Non rompere i giochi gia' fatti: stessi place, stessi script, devono continuare a
   girare; se l'aspetto cambia, deve migliorare (confronto prima/dopo).
2. Android prima di tutto: ogni effetto ha la sua versione per il livello Basso (o si
   spegne). Niente estensioni GL che GLES 3.0 non ha senza una via di riserva.
   Misura sul telefono solo quando te lo dico: adb -s NOTE58PRO000003267 install -r
   android/app/build/outputs/apk/debug/app-debug.apk (build: android/gradlew.bat
   assembleDebug -q), e non mentre uso il telefono; log con adb logcat (solo lettura).
3. Misura SEMPRE prima e dopo: BLOCKY_PROFILE=1 stampa i frame oltre 25 ms fase per fase;
   [shot] frame / pass nel log danno i tempi medi. Niente peggioramenti nascosti.
4. Guarda SEMPRE prima e dopo: BLOCKY_SHOT=<png> BLOCKY_SHOT_SEC=10
   BLOCKY_SHOT_PLACE=<place> [BLOCKY_SHOT_PLAY=1] build/bin/BlockyGameEngine.exe e leggi i
   png. MAI catture dello schermo del desktop.
5. Le API Roblox che toccano grafica e rete vanno rispettate (Lighting.Technology,
   ShadowSoftness, EnvironmentSpecularScale/DiffuseScale, ExposureCompensation,
   SurfaceAppearance (ColorMap, NormalMap, RoughnessMap, MetalnessMap), MaterialService e
   MaterialVariant, Atmosphere, i PostEffect, UnreliableRemoteEvent,
   Player:GetNetworkPing, workspace:GetServerTimeNow, StreamingEnabled): dove il motore
   aggiunge un potere, collegalo anche all'API (in src/engine/lua/api/*.luau, vedi
   src/engine/lua/README.md).
6. src/engine/LuaRuntime.h lo modifica anche Walter: rileggi prima di modificarlo, modifiche
   piccole. Niente divisioni o riscritture di file grandi senza il mio ok.
7. Ogni cosa nuova ha un test quando e' testabile (tests/compat per le API, src/tools/test_*
   per motore e rete) e tools/test_all.ps1 deve restare VERDE.
8. Niente commit (li faccio io con "salva"), niente download di librerie senza chiedermelo
   (prima di proporne una, dimmi nome, licenza, dimensione e perche'), niente cambi di
   permessi dei file, niente account o credenziali.

=====================================================================
SCENE DI PROVA (falle per prime, servono a confrontare)
=====================================================================
- Grafica: un place fisso, places/prova_render.blocky (generalo con uno script, come
  places/genera_laboratorio.mjs), con: una fila di part con tutti i materiali
  (Plastic, SmoothPlastic, Metal, Wood, Concrete, Brick, Glass, Neon, Marble, Granite,
  Slate, Sand, Grass, Ice, Fabric, DiamondPlate, CorrodedMetal, Asphalt), sfere lisce
  e ruvide, una stanza chiusa (per AO e luci), 30 luci (PointLight/SpotLight), acqua,
  una zona lontana (per nebbia e ombre lontane), un personaggio R6.
  Foto sempre dallo stesso punto (BLOCKY_SHOT_PLACE apre il place con la camera
  isometrica sulle part) e di giorno, al tramonto e di notte (Lighting.ClockTime).
- Rete: un simulatore di rete pessima per chi sviluppa, con variabili d'ambiente
  BLOCKY_NET_LAG=<ms> BLOCKY_NET_JITTER=<ms> BLOCKY_NET_LOSS=<percento> (in LanNet,
  solo per chi sviluppa, spento di partenza), e un test automatico con server e 2-4
  client finti (bot) che misura: errore di posizione dei giocatori remoti, scatti,
  ritardo di risposta del proprio personaggio, banda usata per giocatore.

=====================================================================
GRAFICA - I PASSI (in ordine di guadagno per fatica)
=====================================================================
G1 - Colore giusto: lavoro in spazio lineare (texture sRGB lette in lineare, uscita
     in sRGB), tonemapping ACES (o simile) e ExposureCompensation di Lighting.
     Fatta quando: la scena di prova non e' slavata ne' bruciata, e i colori dei place
     vecchi restano riconoscibili.
G2 - Materiali PBR: GGX per lo specular, metallic/roughness per materiale (partire da
     partMaterialShading), Fresnel di Schlick; Neon resta emissivo; Glass trasparente con
     riflesso. Su Basso: versione semplificata (niente specular multiplo).
     Fatta quando: metallo, legno, plastica, vetro si distinguono a colpo d'occhio nella
     foto; Basso sul telefono non perde frame.
G3 - Luce dal cielo (IBL): dal cielo procedurale (SunSky) una cubemap filtrata per i
     riflessi e una luce ambiente (irradiance, anche armoniche sferiche) aggiornate quando
     cambia l'ora; EnvironmentSpecularScale e EnvironmentDiffuseScale di Lighting.
     Fatta quando: il tramonto colora davvero gli oggetti e i riflessi seguono il cielo.
G4 - Ombre a cascate: 3 cascate sul PC (1-2 su Medio, 1 su Basso), PCF morbido,
     ShadowSoftness di Lighting, niente "acne" ne' bordi che tremano quando la camera si muove.
     Fatta quando: ombre nitide vicino e presenti anche lontano nella scena di prova.
G5 - Occlusione ambientale: SSAO (o GTAO) sul PC a meta' risoluzione con sfocatura;
     sui telefoni AO "cotta" nei vertici delle part (spigoli e contatti).
     Fatta quando: angoli e contatti hanno profondita'; costo misurato e scritto.
G6 - Normal map e texture dei materiali: normal map per i materiali di
     assets/textures/materials (generate o fatte a mano, originali), texture che non si
     stirano sulle part grandi (StudsPerTile), SurfaceAppearance con ColorMap/NormalMap/
     RoughnessMap/MetalnessMap per le MeshPart.
G7 - Tante luci: forward+ o clustered (luci divise in celle dello schermo), cosi' 30-100
     PointLight/SpotLight non fanno crollare i frame; ombre per le SpotLight piu' vicine
     (torce, fari) sul livello Alto; Light.Shadows rispettato.
G8 - Antialiasing e rifinitura: FXAA su Basso, TAA (con nitidezza) su Alto; bloom
     ritoccato (gia' c'e': Bloom.h), ColorCorrection/BloomEffect/SunRaysEffect/
     DepthOfFieldEffect/BlurEffect di Lighting fatti bene.
G9 - Atmosfera e acqua: Atmosphere di Roblox (Density, Haze, Glare, Color, Decay),
     nebbia di altezza, raggi del sole; acqua con riflessi (anche solo del cielo),
     rifrazione leggera, onde, schiuma ai bordi; Sky con le texture e le stelle.
G10 - Personaggi e mesh: skinning sulla GPU per i personaggi animati, LOD automatici per
     le mesh pesanti, mesh dettagliate senza crollo dei frame (instancing anche per le
     MeshPart uguali).
Per ogni passo: foto prima/dopo della scena di prova (giorno, tramonto, notte), tempi
prima/dopo su PC (BLOCKY_PROFILE) e, quando te lo dico, sul telefono, livello Basso
compreso. Scrivi i numeri nello "Stato" qui sotto.

Gia' noto: scatti quando tante part ancorate si muovono insieme (docs/limiti_trovati.md
n. 13): ~45 ms fuori dalle fasi misurate, segnaposto prof::mark gia' in main.cpp
(con BLOCKY_PROFILE=1 stampano i tratti del ciclo nei frame lenti). Va trovato e chiuso
prima o insieme a G1: un renderer bello che scatta non serve.

=====================================================================
RETE - I PASSI (in ordine di guadagno per fatica)
=====================================================================
R0 - Strumenti: il simulatore di rete pessima (BLOCKY_NET_LAG/JITTER/LOSS) e il test con
     server + bot che misura errore di posizione, scatti, ritardo e banda. Un pannello
     nello Studio/gioco con ping, pacchetti persi, banda (si apre con un tasto).
R1 - Interpolazione: gli altri giocatori, gli NPC e le part replicate si disegnano circa
     100 ms nel passato, in mezzo a due aggiornamenti ricevuti (posizione e rotazione,
     con estrapolazione breve se un pacchetto manca). Fatta quando: con 150 ms di lag e
     30 ms di jitter i giocatori remoti si muovono lisci nel test e nella foto in movimento.
R2 - Passo fisso del server: il server simula a un ritmo fisso (es. 30 Hz) e manda
     istantanee numerate; il client sa a che istante si riferisce ogni dato;
     workspace:GetServerTimeNow() vero (orologio condiviso).
R3 - Previsione del proprio personaggio: il client muove subito il suo personaggio, manda
     gli input numerati, il server conferma; se il server dice altro il client si
     corregge rigiocando gli input non ancora confermati (riconciliazione), senza strappi
     visibili. Fatta quando: con 150 ms il proprio personaggio risponde subito e le
     correzioni non si vedono.
R4 - Mandare solo quello che serve: delta per oggetto, numeri ridotti (posizioni in
     centimetri, rotazioni compresse), priorita' e interesse (vicino al giocatore piu'
     spesso, lontano di rado o mai: base per StreamingEnabled), limite di banda per
     giocatore. Fatta quando: la banda per giocatore del test scende e resta sotto un
     tetto scritto qui.
R5 - Due canali: affidabile e in ordine per RemoteEvent/RemoteFunction e gli eventi
     importanti; veloce e "a perdere" per posizioni e UnreliableRemoteEvent.
R6 - Compensazione del ritardo per gli spari: il server tiene la storia delle posizioni
     (es. 1 s) e, quando arriva un colpo, controlla il bersaglio dove il tiratore lo
     vedeva; con un'API (es. un modulo che uno sparatutto puo' usare dal server) e un test.
R7 - Internet: server relay per superare il CGNAT (giocatori con dati mobili e di altre
     regioni), con i token gia' esistenti (ServerToken.h) e il browser dei server
     (ServerBrowser.h). Il server online lo pubblico io sul mio account: tu prepari il
     codice e la guida, senza credenziali.
R8 - Sicurezza della rete: il server non si fida del client (velocita', teletrasporti,
     colpi impossibili: c'e' gia' un anti-cheat in parte), limiti ai RemoteEvent per
     secondo, pacchetti malformati scartati senza crash (fuzzer dei pacchetti).

=====================================================================
FINE SESSIONE
=====================================================================
- tools/test_all.ps1 VERDE; i giochi di prova (giochi/UltimoBinario, e
  C:\Users\User\Documents\dead-rails che NON si modifica) partono ancora senza errori nuovi.
- Aggiorna lo "Stato" in fondo a docs/PROMPT_RENDERER_RETE.md: passo fatto, foto
  prima/dopo (percorsi dei png), tempi PC (e telefono se misurato), numeri della rete.
- Dimmi cosa hai fatto, cosa non hai potuto misurare e cosa e' pronto per il mio "salva".
```

---

## Stato (aggiornarlo a fine sessione)

Aggiornato al 19 novembre 2026.

| | |
|---|---|
| Grafica | forward OpenGL/GLES, una sola mappa d'ombra, Blinn con riflesso finto, bloom, nebbia, tonemapping, cielo procedurale, instancing, LOD. Mancano PBR, IBL, cascate, SSAO, normal map, tante luci, TAA |
| Scatti (20 nov, misurati) | Lo scatto viene quando nasce un pezzo di mondo (Ultimo Binario con PROVA_AUTOMATICA e BLOCKY_PROFILE=1). Lo strumento ora misura il frame con l orologio (FrameProfiler.h: prima il dt era del frame precedente e tagliato a 50 ms, quindi i numeri non tornavano) e ha segnaposto in main.cpp, nei pass di WorldRenderPipeline.h e in LuaRuntime::fireHeartbeat. Risultato: (1) il task Luau che costruisce il pezzo riprende per 57-130 ms anche con Costruisci.luau a tempo (3 ms), quindi UNA singola operazione costa piu di 50 ms nel motore vero (nella prova senza grafica 2500 part si creano veloci): cercare quale (Instance.new con il renderer attivo? prompt? cassa? DescendantAdded?); (2) 14-27 frame fermi su [dopo bloom begin -> dopo cielo], cioe glClear e cielo: la CPU aspetta la GPU dopo che arrivano part nuove: servono i timer della GPU (glQueryCounter) e controllare buffer delle part, ombre e shader compilati al primo uso; (3) a volte l Heartbeat del treno 5-34 ms. Non sono la causa: griglia spaziale (O(n), meno di 1 ms), Destroy (200 part in 6 ms), autosalvataggio (spento nel Test), GC forzato (non c e). |
| Rete | UDP in rete locale, delta e conferme, replica, server dedicato. Mancano interpolazione, previsione, passo fisso, compensazione del ritardo, interesse, relay |
| Scene di prova | da fare: `places/prova_render.blocky`, simulatore di rete pessima, test con i bot |
| Passi fatti | nessuno |
| Prossimo | prima lo scatto del limite n. 13 e le scene di prova, poi G1-G2 (colore e PBR) e R0-R1 (strumenti e interpolazione) |
