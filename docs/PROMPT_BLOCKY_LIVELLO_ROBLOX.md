# Prompt: portare Blocky al livello di Roblox Studio

Da incollare all'inizio di una sessione nuova. Il lavoro è troppo per una sessione sola:
**una fase per sessione** (o una parte di fase), nell'ordine. Alla fine di ogni sessione
aggiorna la sezione "Stato" in fondo a questo file.

---

```
Lavoriamo su Blocky, il mio motore di gioco in stile Roblox (C++17/OpenGL + Luau,
Windows con MinGW + Android): C:\Users\User\Documents\RobloxEngine.
Rispondimi SEMPRE in italiano, onesto: "presente" non e' "funzionante", e quello
che non hai provato dillo.

OBIETTIVO FINALE
Blocky al livello (o quasi) di Roblox Studio: un gioco Roblox scritto in Luau, preso
com'e' (progetto Rojo), deve girare in Blocky senza modifiche; lo Studio deve
permettere di costruirlo, provarlo e pubblicarlo come Roblox Studio. Deve girare
fluido anche su Android 7 ("pochi oggetti ma fluidi").

LEGGI PRIMA (in quest'ordine)
- docs/PROMPT_BLOCKY_LIVELLO_ROBLOX.md: questo piano e la sezione "Stato" in fondo
- docs/limiti_trovati.md: le differenze da Roblox gia' trovate, ognuna con lo script che la dimostra
- docs/api_coverage.md: copertura dell'API Roblox, "funzionante" e "presente"
- src/engine/lua/README.md: come si aggiunge un'API in Luau (senza ricompilare)

COM'E' FATTO BLOCKY (cosa toccare)
- src/engine/LuaRuntime.h (~420 KB): il ponte Luau <-> motore. LO MODIFICA ANCHE
  WALTER: rileggilo subito prima di ogni modifica, modifiche piccole e precise,
  niente riscritture o divisioni senza il mio ok esplicito.
- src/engine/LuaRuntimeExpand.inl, StudioProApis.h (tipi: UDim, NumberSequence...),
  PartConstraints*.h, PartProps.def (proprieta' delle part in una riga ciascuna).
- src/engine/lua/prelude.luau: la base delle API in Luau.
  src/engine/lua/api/*.luau: un file per API, Internal.modulo("nome", function(I) ... end)
  con I.realFuncs[classe][metodo], I.realProps[classe][prop], I.Signal.
  src/engine/lua/core/*.luau: le CoreScripts (interfaccia di base, es. BarraVita.luau).
  Da build/bin il motore li RILEGGE A OGNI PLAY: non serve ricompilare.
- src/main_ui/*.h: lo Studio (ImGui). src/main.cpp: il ciclo principale.
- Rojo: src/engine/RojoProject.h, main_ui/StudioRojo.h.
- Dati generati dal dump dell'API Roblox 0.747: tools/roblox_api (gen_api, gen_enums,
  gen_probe, gen_blocky_defs), src/engine/RobloxApiData.h, RobloxEnumsData.h.

COME SI LAVORA (ogni compito, sempre cosi')
1. MISURA prima: uno script di prova (cartella di prova + build/bin/test_compat.exe <cartella>)
   o un gioco vero che mostri il problema.
2. TEST prima del codice: tests/compat/NN_nome.lua (COMPAT_OK = true in fondo).
3. IMPLEMENTA come Roblox, in modo generico (MAI codice fatto apposta per un gioco):
   prima in Luau (src/engine/lua/api), in C++ solo per dare al Luau un dato o un
   potere che non ha.
4. VERIFICA: cmake --build build -j 8 -- -k 0, poi TUTTI i test
   (build/bin/test_*.exe tranne test_hang_repro e test_pool) e test_compat al 100%.
5. GUARDA il risultato: BLOCKY_SHOT=<png> BLOCKY_SHOT_SEC=25 BLOCKY_SHOT_PLACE=baseplate
   BLOCKY_SHOT_PLAY=1 [BLOCKY_ROJO=<default.project.json>] [BLOCKY_PROFILE=1]
   build/bin/BlockyGameEngine.exe > log.txt 2>&1, poi leggi il png e il log.
   MAI catture dello schermo del desktop.
6. REGISTRA: aggiorna docs/limiti_trovati.md (sposta nei "corretti" con il numero del test),
   rifai la copertura (node tools/roblox_api/gen_probe.mjs + build/bin/test_api_coverage.exe),
   aggiorna lo "Stato" di questo file.
7. RACCONTA con onesta': cosa funziona, cosa e' solo presente, cosa non hai provato.

GIOCHI DI PROVA (misurano tutto il resto)
- giochi/UltimoBinario (Rojo, ~3500 righe, nostro): deve girare senza errori.
  Config.PROVA_AUTOMATICA = true lo fa partire da solo (rimettilo a false dopo).
- C:\Users\User\Documents\dead-rails (Rojo, ~5300 righe, MIO, NON SI MODIFICA):
  scritto per Roblox vero. Quando gira in Blocky senza errori e si gioca, una grossa
  fetta dell'obiettivo e' raggiunta.
Ogni sessione: rilanciali e annota il primo errore che resta.

REGOLE
- Niente commit: li faccio io quando dico "salva". Niente git push.
- Non copiare asset, nomi o giochi Roblox (o di altri): giochi originali; i dati di
  Roblox (dump API, .r6a) solo come misura.
- Niente cambi di permessi dei file (ACL): li faccio io. Alcune cartelle le ho protette.
- Niente download senza chiedere. Niente password, token o credenziali.
- Non uccidere processi Blocky che potrebbero essere miei (BlockyGameEngine, BlockyServer).
- Prima di cancellare file: prove che non servono e conferma mia.
- Le decisioni grosse (cambiare un comportamento che i place gia' fatti usano) le chiedi.

FASI (nell'ordine; una per sessione)

FASE 0 - Base sicura contro i bug
- tools/test_all (compila + tutti i test + test_compat) che dice VERDE o ROSSO, e un
  hook git pre-commit che lo lancia.
- Sistemare test_physics (oggi sempre rosso: 2 errori) perche' un rosso significhi
  sempre "hai rotto qualcosa".
- Test "giochi veri": apre ogni progetto Rojo di prova con la prova automatica, gioca
  30 s, FALLISCE se c'e' un errore Lua.
- Fuzzer delle API: per ogni classe e membro del dump, chiamate con argomenti a caso
  (nil, numeri, testi, tabelle, Instance) dentro pcall: il motore non deve mai andare
  in crash ne' bloccarsi.
- Build di test con le asserzioni di Luau accese (LUAU_ASSERT / api check).
Fatta quando: tools/test_all verde, fuzzer senza crash, i due giochi senza errori nuovi.

FASE 1 - Sviluppo veloce
- (Con il mio OK) dividere LuaRuntime.h in .cpp per area (part, GUI, NPC, suono,
  rete...): una modifica deve ricompilare in secondi, non in 10 minuti.
- Registro delle proprieta' in una riga (come PartProps.def) per TUTTE le classi:
  genera getter, setter, pannello Proprieta' dello Studio, salvataggio, copertura.
- Registro delle classi (BLOCKY_CLASS("Seat", crea...)) al posto della catena di
  strcmp in Instance.new.
- tools/nuova_api.mjs <Classe>: dal dump crea lo scheletro del modulo Luau e del test.
Fatta quando: aggiungere una proprieta' = una riga; build incrementale < 30 s.

FASE 2 - Script veloci
- Vector3 sul tipo "vector" nativo di Luau (zero allocazioni, come Roblox).
- Proprieta' trovate con una tabella (hash), non con catene di strcmp.
- Compilazione nativa Luau.CodeGen per gli script con --!native (x64 e arm64),
  provata sul telefono.
- Limite di istruzioni (limiti n. 2): non fermare uno script lento la prima volta;
  avviso in Output e stop solo dopo secondi (Roblox: ~10 s), come Roblox.
- Banco di prova fisso (calcoli, tabelle, Vector3, part.Position, GUI) con i tempi
  scritti qui sotto, da rifare a ogni modifica.
Fatta quando: part.Position e Vector3 almeno 5 volte piu' veloci di oggi
(4,6 ms per 1500 / 5000 operazioni).

FASE 3 - Il modello delle Instance come Roblox
- Un solo albero: oggetti del motore e Instance Luau con le stesse regole per
  Parent/Children/Destroy/Clone/GetDescendants/FindFirstAncestor*/ChildAdded/
  DescendantAdded/AncestryChanged/Changed/GetPropertyChangedSignal.
- Clone() di qualsiasi cosa (Model con part, script, GUI, valori).
- Tutti i punti di docs/limiti_trovati.md: Name delle GUI e clic per elemento (n. 16),
  Enum come EnumItem veri (n. 4), cilindri con l'asse X con migrazione dei place (n. 1),
  materiali che ripiegano (n. 3), Tool (n. 8), Seat (n. 9-10), leaderstats (n. 7),
  Instance.new("Part") non ancorata come Roblox (n. 15: impostazione per place,
  chiedimi), ...
Fatta quando: test_api_coverage "Instance" e "BasePart" funzionanti > 90%.

FASE 4 - Fisica dagli script
- Vincoli Roblox con Instance.new + Attachment collegati ai vincoli che il motore ha
  gia' (PartConstraints: LinearVelocity, AlignPosition, AlignOrientation,
  AngularVelocity, VectorForce, Weld, Hinge, Rope, Spring...).
- AssemblyLinearVelocity/AngularVelocity, ApplyImpulse, GetMass, CustomPhysicalProperties.
- PhysicsService (collision group), workspace:GetPartBoundsInBox/InRadius,
  GetPartsInPart, Raycast/Blockcast/Spherecast con RaycastParams completi, OverlapParams.
- Touched/TouchEnded affidabili anche per le part libere.
- Il personaggio portato dalle piattaforme che si muovono (limite n. 11).
Fatta quando: un'auto o un treno fisico scritti per Roblox (LinearVelocity + weld)
si muovono in Blocky.

FASE 5 - Personaggi e NPC
- NPC = Model + Humanoid + HumanoidRootPart come su Roblox (non solo Actors:Spawn),
  con collisioni contro i muri, MoveTo e PathfindingService veri (Path, waypoint).
- Animator/AnimationTrack dal percorso standard; animazioni R6 di default (pipeline
  Blender -> .blkanim gia' fatta, vedi memoria).
- Player:LoadCharacter, CharacterAutoLoads, RespawnTime, CharacterAdded/Removing,
  Humanoid.Died, morte e respawn come Roblox.
- VehicleSeat con Throttle/Steer, Seat.Occupant, Humanoid.Sit/SeatPart (fatti in parte).
Fatta quando: gli zombie di dead-rails inseguono, colpiscono e muoiono come su Roblox.

FASE 6 - Effetti e grafica
- ParticleEmitter completo (Size/Transparency/Color con sequenze, Texture, Emit,
  Acceleration, Drag, SpreadAngle...), Beam, Trail, Fire, Smoke, Sparkles, Explosion
  (oggetto), Highlight, SelectionBox.
- PointLight/SpotLight/SurfaceLight, Lighting completo (ClockTime, Ambient, Fog...),
  Sky, Atmosphere, Bloom/ColorCorrection/SunRays/DepthOfField.
- BillboardGui e SurfaceGui con le GUI dentro.
Fatta quando: gli effetti di dead-rails (fumo del treno, fuoco, luci) si vedono.

FASE 7 - GUI completa e CoreScripts
- ImageLabel/ImageButton con immagini vere, TextBox (focus, testo, FocusLost),
  ScrollingFrame che scorre, UIListLayout/UIGridLayout/UIPadding/UIAspectRatio/
  UIScale/UISizeConstraint/UIGradient, Font e FontFace, TextService, RichText.
- CoreScripts in src/engine/lua/core: barra della vita (fatta), zaino 1-9, lista dei
  giocatori con leaderstats, chat, prompt "E Apri", menu. Spegnibili con
  StarterGui:SetCoreGuiEnabled. Leggere: aggiornare solo quando qualcosa cambia.
- ContextActionService con i pulsanti per il telefono.
Fatta quando: l'HUD di dead-rails e di Ultimo Binario si vede uguale a Roblox.

FASE 8 - Suono
- Suono 3D che cala con la distanza (RollOff*), PlaybackSpeed, TimePosition, Playing,
  Pause/Resume, Looped, SoundGroup, effetti (riverbero...), SoundService.

FASE 9 - Dati e rete
- DataStore completo: GetAsync/SetAsync/UpdateAsync/IncrementAsync/RemoveAsync,
  OrderedDataStore (classifiche), limiti come Roblox; MessagingService.
- Modello di rete come Roblox: server che comanda, replica delle Instance e delle
  proprieta' ai client, RemoteEvent/RemoteFunction, UnreliableRemoteEvent, client che
  non puo' barare. Replica dei Player al client (vedi memoria project_server_scripts).
- Multigiocatore via internet con relay (vedi memoria project_internet_multiplayer_plan).
- TeleportService tra place.

FASE 10 - Lo Studio come Roblox Studio
- Explorer e Proprieta' per TUTTE le classi (dal registro della fase 1), con
  inserimento oggetti, trascinamento nell'albero, ricerca.
- Strumenti: Select/Move/Scale/Rotate con snap, multi-selezione, gruppi (Ctrl+G),
  allineamento, Undo/Redo per OGNI operazione.
- Editor degli script: autocompletamento e controllo dei tipi con Luau.Analysis
  (c'e' gia' in build/_deps/luau-src/Analysis), vai alla definizione, errori sottolineati,
  debugger (punti di interruzione gia' fatti), Output, barra dei comandi.
- Test con piu' giocatori dallo Studio (server + 2 client).
- Editor del terreno, Toolbox, gestione degli asset, Animation Editor (c'e'),
  pubblicazione dei giochi, plugin dello Studio in Luau.
Fatta quando: si costruisce, si prova e si pubblica un gioco intero senza uscire dallo Studio.

FASE 11 - Piattaforme e qualita'
- Android: APK (android/gradlew.bat assembleDebug -q; adb -s NOTE58PRO000003267
  install -r ... solo quando lo chiedo, e non mentre uso il telefono), 60 fps sui
  giochi di prova sul telefono, memoria sotto controllo.
- BLOCKY_PROFILE: niente frame oltre 25 ms nei giochi di prova (limite n. 13: scatti
  con tante part che si muovono, indagine a meta': ~45 ms fuori dalle fasi misurate,
  segnaposto prof::mark gia' messi in main.cpp).
- Segnalazione dei crash, temi dello Studio, lingue.

COME FINISCE OGNI SESSIONE
- Tutti i test verdi (o i rossi spiegati), i due giochi rilanciati, copertura rifatta.
- Aggiorna la sezione "Stato" qui sotto: fase, cosa fatto, primo errore che resta
  nei giochi, numeri della copertura e del banco di prova.
- Dimmi cosa e' pronto per il mio "salva".
```

---

## Stato (aggiornarlo a fine sessione)

Aggiornato al 19 novembre 2026 (sera). Fase 0 iniziata, non finita.

| | |
|---|---|
| Copertura API Roblox | funzionante 8,1% · presente 78,8% (docs/api_coverage.md). Non rifatta in questa sessione |
| Test di compatibilità | 80 / 80 verdi dentro test_all (manca ancora il file 45, il conto stampato è 80) |
| Test rossi noti | nessuno dei 53 lanciati da tools/test_all. test_hang_repro e test_pool restano esclusi |
| Banco di prova Luau | calcoli 2,7 ms · tabelle 2,0 ms · Vector3 4,6 ms · part.Position 4,6 ms (non rifatto) |
| Ultimo Binario | 30 s con PROVA_AUTOMATICA, poi rimessa a false. Il treno parte. Primo errore: `PlayerAdded` / `CharacterAdded` in Main, `task.wait()` rifiutato fuori da `task.spawn`. Cabina, sedile e trascinamento non provati a mano |
| dead-rails | 30 s, progetto collegato. Il png salvato mostra lo Studio sul baseplate vuoto, con l'avviso Rojo che il servizio Workspace viene saltato. Nel log, a ripetizione: `SettingsMenu:288` `string.sub` su nil. `Train:160` / `NumberSequenceKeypoint` non sono comparsi in questa corsa |
| Fatto | tools/test_all.ps1 stampa VERDE (53 test, circa 2 minuti). Hook git pre-commit che lo lancia. test_physics verde: le due tolleranze strette non erano un mondo rotto. Crash vero, ora chiuso: una part ancorata spostata (ruote dell'auto, part mosse da uno script) faceva crashare Jolt. Per questo test_veicoli e test_server_scripts uscivano con un breakpoint |
| Prossima fase | 0, il resto: il test dei giochi veri dentro test_all, il fuzzer delle API, la build con le asserzioni di Luau accese |
