# Prompt: tantissime API Luau per fare ogni tipo di gioco

Da incollare all'inizio di una sessione nuova. Si affianca al piano generale
(`docs/PROMPT_BLOCKY_LIVELLO_ROBLOX.md`): quello copre tutto il motore e lo Studio,
questo **solo le API Luau** che servono agli script dei giochi.

Va insieme alla **lista di controllo** `docs/API_CHECKLIST.md`: 2080 proprietà, metodi
ed eventi di 257 classi Roblox, generata dal dump vero con
`node tools/roblox_api/gen_checklist.mjs`. Il prompt dice **come** lavorare, la lista dice
**cosa** manca, una casella per volta.

---

```
Lavoriamo su Blocky, il mio motore di gioco in stile Roblox (C++17/OpenGL + Luau, Windows
con MinGW + Android): C:\Users\User\Documents\RobloxEngine. Rispondimi SEMPRE in italiano,
con onesta': "presente" (esiste e non va in errore) NON e' "funzionante" (fa davvero
quello che fa su Roblox). Quello che non hai provato, dillo.

=====================================================================
OBIETTIVO
=====================================================================
In Blocky si deve poter scrivere OGNI tipo di gioco con le API di Roblox, come su Roblox.
Uno script Roblox preso com'e' deve girare e fare la stessa cosa. Generi da coprire:
obby, tycoon, simulator (raccolta/vendita), sparatutto (prima e terza persona), corse e
veicoli, tower defense, RPG e avventura, horror, survival e crafting, picchiaduro, sport
(calcio, basket), sandbox da costruire, roleplay di citta', puzzle e escape room, platform
2D/2.5D, mondo aperto, giochi di viaggio (come i miei Ultimo Binario e dead-rails), party
game a minigiochi, idle/clicker, gestionali, pesca, giochi di carte e da tavolo, rhythm
game, giochi con i pet, giochi di guida di treni/aerei/barche.

=====================================================================
PRIMA DI TUTTO, LEGGI (e non rifare quello che c'e')
=====================================================================
1. src/engine/lua/README.md: come si scrive un modulo API:
     local Internal = ...
     Internal.modulo("nome", function(I)
         I.realFuncs.Classe = I.realFuncs.Classe or {}
         I.realFuncs.Classe.Metodo = function(self, ...) ... end     -- metodi
         I.realProps.Classe = I.realProps.Classe or {}
         I.realProps.Classe.Prop = function(obj) return valore end   -- proprieta' in lettura
     end)
   I.Signal.new() per gli eventi, I.isLuaInstance(v) / I.store[v] per le Instance Luau.
   I metodi di una classe valgono anche per le figlie (BasePart vale per Part, Seat...).
   __blkCoreGui("Heartbeat", f) = un Heartbeat che lo Studio non azzera (per le CoreScripts).
2. I moduli che ci sono gia': src/engine/lua/api/*.luau (attributi, discendenti, enumitem,
   fisica, geometria, gioco, personaggio, proprieta, rete, servizi, e quelli "di Blocky"
   sotto game:GetService("Dev"): devkit, stato, tabelle, spazio, rumore, lavagna, percorso)
   e src/engine/lua/core/*.luau (BarraVita). Estendili invece di duplicarli.
3. docs/API_CHECKLIST.md: la lista di controllo. docs/limiti_trovati.md: le differenze da
   Roblox gia' trovate. docs/api_coverage.md: la copertura misurata.
4. Il dump dell'API Roblox 0.747 (tools/roblox_api_dump/Full-API-Dump.json) e' la verita'
   su nomi, argomenti, tipi e valori di partenza. Gli Enum veri stanno in
   src/engine/RobloxEnumsData.h (generato da tools/roblox_api/gen_enums.mjs).

=====================================================================
REGOLE
=====================================================================
1. Prima le API VERE di Roblox, con nomi, argomenti, valori di partenza e comportamento
   di Roblox (controlla il dump). Un gioco Roblox non conoscera' mai le nostre aggiunte.
2. Le aggiunte di Blocky (game:GetService("Dev")...) solo se nessuna API Roblox fa la
   stessa cosa, sempre segnate "di Blocky" nel commento in testa al file.
3. Prima in Luau (src/engine/lua/api/<area>.luau, un file per area: non un file per
   metodo e non un file gigante). Il motore li rilegge a ogni Play da build/bin: NON serve
   ricompilare. C++ solo per dare al Luau un dato o un potere del motore che non ha
   (disegno, fisica, suono, input, file), e il piu' piccolo possibile.
4. src/engine/LuaRuntime.h lo modifica anche Walter: rileggi la parte che tocchi SUBITO
   prima di modificarla, modifiche piccole e precise, niente riscritture, niente
   divisioni del file senza il mio ok esplicito. Stessa cautela per LuaRuntimeExpand.inl.
5. Ogni API ha un test in tests/compat/NN_nome.lua (prendi il numero libero dopo
   l'ultimo: ls tests/compat; COMPAT_OK = true in fondo). Il test controlla il
   COMPORTAMENTO (il valore cambia, l'evento scatta, l'oggetto si muove), non solo che
   il nome esista. Un test puo' coprire piu' membri della stessa classe.
6. Spunta [x] in docs/API_CHECKLIST.md solo i membri che funzionano DAVVERO e hanno un
   test. Se un membro non si puo' fare come Roblox (es. servizi online), scrivi accanto
   "(in Blocky: ...)" e spiega cosa fa invece, ma NON spuntarlo come Roblox.
7. Mai codice fatto apposta per un gioco: sempre generico, come Roblox.
8. Errori come Roblox: dove Roblox da' errore, errore con un messaggio chiaro (e viceversa:
   niente errori dove Roblox va avanti). Un oggetto distrutto si comporta come su Roblox.
9. Leggero: deve restare fluido su Android 7. Niente lavoro a ogni frame se non serve,
   niente tabelle nuove nei cicli caldi, niente ricerche in tutto il workspace a ogni frame.
10. Niente commit (li faccio io con "salva"), niente git push, niente download senza
    chiedere, niente cambi di permessi dei file (ACL: li gestisco io, alcune cartelle sono
    protette), niente catture del desktop (solo BLOCKY_SHOT), niente asset o giochi
    copiati da Roblox o da altri: giochi originali.
11. Lavora in autonomia: finito un membro passa al successivo della stessa classe, finita
    una classe alla successiva del pacchetto. Fermati solo per decisioni grosse
    (cambiare un comportamento che i place gia' fatti usano: chiedimelo) o se un test
    resta rosso e non capisci perche'.

=====================================================================
COME SI LAVORA (ogni membro)
=====================================================================
1. Guarda nel dump come e' fatto (argomenti, tipo, valore di partenza, Yields, ReadOnly).
2. Prova cosa fa oggi Blocky: una cartella di prova (nello scratchpad) con a.lua, poi
   build/bin/test_compat.exe <cartella>. Stampa con print, metti COMPAT_OK = true in fondo.
   ATTENZIONE: uno script che lavora piu' di 50 ms di fila senza task.wait() viene fermato
   (limiti_trovati n. 2): spezza le prove pesanti con task.wait().
3. Scrivi il test in tests/compat, poi il modulo, poi rilancia build/bin/test_compat.exe
   (senza argomenti fa tutti i test di compatibilita'). Niente build se hai toccato solo Luau.
4. Se hai toccato il C++: cmake --build build -j 8 -- -k 0 (5-10 minuti), poi
   powershell -NoProfile -ExecutionPolicy Bypass -File tools\test_all.ps1: deve dire VERDE.
5. Per tutto quello che si VEDE (GUI, effetti, luci, personaggi, camera) guarda davvero:
   BLOCKY_SHOT=<png> BLOCKY_SHOT_SEC=15 BLOCKY_SHOT_PLACE=baseplate BLOCKY_SHOT_PLAY=1
   [BLOCKY_ROJO=<default.project.json>] build/bin/BlockyGameEngine.exe > log.txt 2>&1
   poi leggi il png e cerca "error|attempt|non e' ancora" nel log.
6. Spunta la casella, aggiorna docs/limiti_trovati.md se hai chiuso un limite, passa oltre.
7. Ogni tanto (a fine pacchetto) rifai la copertura: node tools/roblox_api/gen_probe.mjs,
   poi build/bin/test_api_coverage.exe (scrive docs/api_coverage.md).

=====================================================================
P0 - PRIMA DI TUTTO: GLI EVENTI CHE NON POSSONO ASPETTARE
=====================================================================
Su Roblox OGNI funzione collegata a un evento gira in un thread suo e puo' aspettare
(task.wait, WaitForChild, :Wait(), chiamate Async). In Blocky oggi task.wait() dentro
PlayerAdded / CharacterAdded viene rifiutato ("fuori da task.spawn"). Rompe un sacco di
giochi (anche Ultimo Binario). Va risolto per TUTTI gli eventi: quelli in C++
(Touched, Heartbeat, MouseButton1Click, Triggered, PlayerAdded, CharacterAdded, Died,
OnServerEvent...) e quelli Luau (I.Signal). Test: in ogni tipo di evento un task.wait(0.1)
seguito da una scrittura che il test controlla dopo.

=====================================================================
P1 - IL LINGUAGGIO E I TIPI DI DATI (li usa ogni gioco)
=====================================================================
I tipi di dati non sono nel dump delle classi: questa e' la lista da coprire tutta, con
operatori, tostring, typeof e uguaglianza come Roblox.
- Libreria standard Luau come su Roblox: task (spawn, defer, delay, wait, cancel,
  synchronize e desynchronize senza effetto), coroutine (create, resume, wrap, yield,
  status, close, isyieldable, running), table (clone, freeze, isfrozen, move, find, create,
  insert, remove, sort, concat, pack, unpack, clear, maxn), string (format con %q %d %s %x
  %f %g, split, rep, sub, find, match, gmatch, gsub, byte, char, lower, upper, reverse,
  len, pack, unpack, packsize), math (noise, clamp, sign, round, floor, ceil, abs, min,
  max, random, randomseed, huge, pi, sqrt, pow, exp, log, log10, fmod, modf, sin, cos,
  tan, asin, acos, atan, atan2, sinh, cosh, tanh, map, lerp), buffer (tutte le
  read/write), utf8, bit32, os (time con tabella, clock, date con i formati, difftime),
  vector (create, magnitude, normalize, cross, dot, ...), print/warn/error/assert/pcall/
  xpcall/select/tonumber/tostring/type/typeof/ipairs/pairs/next/rawequal/rawlen/
  getmetatable/setmetatable (sicuri), _G e shared, tick(), time(), elapsedTime(), wait(),
  delay(), spawn() (i vecchi), version(), gcinfo().
- Vector3: new, zero, one, xAxis, yAxis, zAxis, FromNormalId, FromAxis, X, Y, Z,
  Magnitude, Unit, Abs, Ceil, Floor, Sign, Cross, Dot, Angle, Lerp, Max, Min, FuzzyEq,
  + - * / // con numeri e vettori, -v, ==, tostring "x, y, z".
- Vector2: new, zero, one, xAxis, yAxis, X, Y, Magnitude, Unit, Abs, Ceil, Floor, Sign,
  Cross, Dot, Angle, Lerp, Max, Min, FuzzyEq, operatori.
- CFrame: new (3, 7 e 12 numeri, da Vector3, lookAt), lookAt, lookAlong, fromEulerAngles,
  fromEulerAnglesXYZ, fromEulerAnglesYXZ, Angles, fromOrientation, fromAxisAngle,
  fromMatrix, identity, Position, Rotation, X, Y, Z, LookVector, RightVector, UpVector,
  XVector, YVector, ZVector, Inverse, Lerp, Orthonormalize, ToWorldSpace,
  ToObjectSpace, PointToWorldSpace, PointToObjectSpace, VectorToWorldSpace,
  VectorToObjectSpace, GetComponents, components, ToEulerAnglesXYZ, ToEulerAnglesYXZ,
  ToOrientation, ToAxisAngle, FuzzyEq, AngleBetween, CFrame*CFrame, CFrame*Vector3,
  CFrame+-Vector3, tostring.
- Color3: new, fromRGB, fromHSV, fromHex, R, G, B, Lerp, ToHSV, ToHex, ==.
- BrickColor: new (nome, numero, r g b, Color3), palette, random, White/Black/Red/Blue/
  Green/Yellow/Gray/DarkGray, Name, Number, Color, r, g, b; tutti i nomi veri.
- UDim (new, Scale, Offset, +, -), UDim2 (new, fromScale, fromOffset, X, Y, Width,
  Height, Lerp, +, -), Rect (new, Min, Max, Width, Height), Region3 (new, CFrame, Size,
  ExpandToGrid), Region3int16, Vector3int16, Vector2int16, Ray (new, Origin, Direction,
  Unit, ClosestPoint, Distance), NumberRange (new, Min, Max), NumberSequence (new con 1 o
  2 numeri o lista, Keypoints), NumberSequenceKeypoint (new con 2 o 3 argomenti),
  ColorSequence (new con 1 o 2 colori o lista), ColorSequenceKeypoint, TweenInfo (new
  con tutti gli argomenti, EasingStyle, EasingDirection, Time, DelayTime, RepeatCount,
  Reverses), PhysicalProperties (new con 1, 3 o 5 argomenti, Density, Friction,
  Elasticity, FrictionWeight, ElasticityWeight), Font (new, fromEnum, fromName,
  fromId, Family, Weight, Style, Bold), Axes, Faces, DateTime (now, fromUnixTimestamp,
  fromUnixTimestampMillis, fromUniversalTime, fromLocalTime, fromIsoDate,
  UnixTimestamp, UnixTimestampMillis, ToUniversalTime, ToLocalTime, ToIsoDate,
  FormatUniversalTime, FormatLocalTime), Random (new, NextNumber, NextInteger,
  NextUnitVector, Shuffle, Clone), RaycastParams (FilterType, FilterDescendantsInstances,
  IgnoreWater, CollisionGroup, RespectCanCollide, BruteForceAllSlow, AddToFilter),
  RaycastResult (Instance, Position, Normal, Material, Distance), OverlapParams
  (FilterType, FilterDescendantsInstances, MaxParts, CollisionGroup, RespectCanCollide,
  AddToFilter), CatalogSearchParams (anche finto), Path2DControlPoint, Content.
- Enum: tutti gli Enum veri come EnumItem (Name, Value, EnumType, IsA), Enum.X:GetEnumItems,
  FromName, FromValue; == e confronto con il nome dove Roblox lo accetta.
- RBXScriptSignal / RBXScriptConnection completi (Connect, Once, Wait, ConnectParallel
  come Connect, Disconnect, Connected) per TUTTI gli eventi.

=====================================================================
P2 .. P10 - LE CLASSI (lista completa in docs/API_CHECKLIST.md)
=====================================================================
Per ogni pacchetto: apri la sezione nella lista di controllo e lavora classe per classe.
Qui sotto cosa conta di piu' in ognuno (da fare per primo) e cosa deve succedere.

P2 - L'albero delle Instance (86 membri)
- UN SOLO ALBERO: oggetti del motore e Instance Luau con le stesse regole. Parent,
  GetChildren, GetDescendants, FindFirstChild (anche ricorsivo), FindFirstChildOfClass,
  FindFirstChildWhichIsA, FindFirstAncestor/OfClass/WhichIsA, FindFirstDescendant,
  WaitForChild (con timeout e avviso dopo 5 s come Roblox), IsA con l'ereditarieta' del
  dump, IsDescendantOf, IsAncestorOf, GetFullName, Destroy (figli compresi, Destroying,
  Parent bloccato dopo), ClearAllChildren, Clone (profondo, con Archivable, script,
  valori, attributi, tag), Name e Archivable.
- Eventi: ChildAdded/Removed, DescendantAdded/Removing, AncestryChanged, Changed (sui
  Value con il valore nuovo), GetPropertyChangedSignal, AttributeChanged su TUTTO.
- Tutti i *Value con .Value e .Changed; Configuration; BindableEvent/Function;
  CollectionService completo (AddTag, RemoveTag, HasTag, GetTags, GetTagged,
  GetAllTags, GetInstanceAddedSignal, GetInstanceRemovedSignal);
  ModuleScript con require (Instance e percorso "./x" come Roblox), require che
  restituisce sempre la stessa tabella, errori di require circolare come Roblox.
- Script e LocalScript con Enabled/Disabled, RunContext; script.Parent; le regole di dove
  girano (ServerScriptService, StarterPlayerScripts, StarterCharacterScripts,
  StarterGui, ReplicatedFirst, workspace).

P3 - Mondo e part (237 membri)
- BasePart completa: Size, Position, CFrame, Orientation, Rotation, Color, BrickColor,
  Material, MaterialVariant, Transparency, Reflectance, CanCollide, CanTouch, CanQuery,
  Anchored, Massless, Locked, CastShadow, Shape (asse X dei cilindri come Roblox, con la
  migrazione dei place: limiti n. 1, chiedimi), le facce (TopSurface...), Touched e
  TouchEnded affidabili, GetTouchingParts, GetConnectedParts, GetRootPart,
  CanCollideWith, SubtractAsync/UnionAsync/IntersectAsync (c'e' la CSG: modulo geometria).
- Model: PivotTo, GetPivot, GetBoundingBox, GetExtentsSize, ScaleTo, GetScale,
  PrimaryPart, WorldPivot, MoveTo, TranslateBy, LevelOfDetail (ignorato).
- Workspace/WorldRoot: Raycast, Blockcast, Spherecast, Shapecast, GetPartBoundsInBox,
  GetPartBoundsInRadius, GetPartsInPart, ArePartsTouchingOthers, Gravity, CurrentCamera,
  FallenPartsDestroyHeight (c'e'), GetServerTimeNow, DistributedGameTime,
  StreamingEnabled (accettato).
- Terrain: FillBlock, FillBall, FillCylinder, FillWedge, FillRegion, ReadVoxels,
  WriteVoxels, ReplaceMaterial, Clear, CellCenterToWorld, WorldToCell, WaterColor,
  WaterWaveSize; l'acqua fa nuotare (il materiale Water c'e' gia' per le part).
- Decal e Texture (Texture, Face, Color3, Transparency, StudsPerTile, OffsetStuds),
  SpecialMesh (MeshType Brick/Sphere/Cylinder/Wedge/FileMesh, Scale, Offset), MeshPart
  (MeshId con i modelli che Blocky sa leggere), SurfaceAppearance (colore).
- Instance.new("Part") ancorata o no di partenza: limiti n. 15, CHIEDIMI.

P4 - Fisica e movimento (223 membri)
- AssemblyLinearVelocity, AssemblyAngularVelocity, ApplyImpulse, ApplyImpulseAtPosition,
  ApplyAngularImpulse, AssemblyMass, AssemblyCenterOfMass, GetMass,
  CustomPhysicalProperties, GetJoints, GetNoCollisionConstraints.
- Attachment completo (Position, CFrame, WorldPosition, WorldCFrame, Axis,
  SecondaryAxis, Visible) e TUTTI i vincoli con Instance.new collegati a quelli che il
  motore ha gia' (src/engine/PartConstraints*.h: oggi si usano solo da
  Constraints.LinearVelocity(...) di Blocky): WeldConstraint, Weld e Motor6D (C0, C1,
  Part0, Part1, Transform, MaxVelocity, DesiredAngle), Hinge (motore e servo),
  Prismatic, Cylindrical, BallSocket, Universal, Plane, Rope, Rod, Spring,
  NoCollisionConstraint, LinearVelocity, AngularVelocity, AlignPosition,
  AlignOrientation, VectorForce, Torque, LineForce, e i vecchi BodyVelocity,
  BodyPosition, BodyGyro, BodyForce, BodyAngularVelocity, BodyThrust (usatissimi).
- PhysicsService: RegisterCollisionGroup, CollisionGroupSetCollidable,
  CollisionGroupsAreCollidable, GetRegisteredCollisionGroups (modulo fisica: estendi).
- Seat/VehicleSeat: Occupant, Disabled, Sit; VehicleSeat Throttle, Steer, ThrottleFloat,
  SteerFloat, MaxSpeed, Torque, TurnSpeed, HeadsUpDisplay. Chi e' seduto si muove con
  il sedile (gia' fatto nel motore).
- Prova finale: un'auto (4 ruote con HingeConstraint motore) e un treno (LinearVelocity
  + WeldConstraint) scritti per Roblox si muovono in Blocky.

P5 - Personaggi, NPC e animazioni (336 membri)
- Humanoid completo (vedi lista): stati (GetState, ChangeState, SetStateEnabled,
  GetStateEnabled, StateChanged, tutti gli Enum.HumanoidStateType), Health/MaxHealth e la
  rigenerazione di default (script Health), WalkSpeed, JumpPower/JumpHeight/
  UseJumpPower, HipHeight, AutoRotate, AutoJumpEnabled, PlatformStand, Sit, SeatPart,
  Jump, MoveDirection, Move, MoveTo/MoveToFinished (8 s di timeout come Roblox),
  WalkToPoint, WalkToPart, FloorMaterial, RootPart, RigType, DisplayName,
  DisplayDistanceType, HealthDisplayType, NameDisplayDistance, TakeDamage,
  EquipTool/UnequipTools, GetAccessories, AddAccessory, RemoveAccessories,
  ApplyDescription/GetAppliedDescription (HumanoidDescription), eventi Died, Running,
  Jumping, FreeFalling, Climbing, Swimming, Seated, Touched, HealthChanged.
- NPC come su Roblox: Model + Humanoid + HumanoidRootPart (+ arti R6 e Motor6D) messo nel
  workspace cammina, cade, salta, prende danno, muore (con BreakJointsOnDeath), collide
  con i muri. Rig creato a mano o clonato da ReplicatedStorage.
- Animator (LoadAnimation, GetPlayingAnimationTracks, AnimationPlayed),
  AnimationController, Animation (AnimationId con i file .blkanim di Blocky),
  AnimationTrack completo (Play con fade/peso/velocita', Stop, AdjustSpeed,
  AdjustWeight, TimePosition, Length, Looped, Priority, IsPlaying, Speed, WeightCurrent,
  WeightTarget, Stopped, Ended, DidLoop, KeyframeReached, GetMarkerReachedSignal,
  GetTimeOfKeyframe), animazioni R6 di default (corsa, salto, caduta, nuoto, seduto,
  strumento in mano), KeyframeSequence.
- Player completo: Character, CharacterAdded/Removing/AppearanceLoaded, LoadCharacter,
  RespawnLocation, Team, TeamColor, Neutral, UserId, Name, DisplayName, AccountAge,
  MembershipType, Kick, GetMouse, CameraMode, CameraMinZoomDistance/Max, DevTouch*,
  HealthDisplayDistance, GetAttribute..., leaderstats come Roblox (limiti n. 7).
- Players completo: LocalPlayer, CharacterAutoLoads, RespawnTime, MaxPlayers,
  GetPlayers, GetPlayerFromCharacter, GetPlayerByUserId, PlayerAdded, PlayerRemoving,
  GetUserIdFromNameAsync/GetNameFromUserIdAsync (in locale), CreateHumanoidModelFromDescription.
- Team e Teams (AutoAssignable, TeamColor, GetPlayers, PlayerAdded/Removed),
  SpawnLocation per squadra.
- PathfindingService (CreatePath con AgentRadius/Height/CanJump/CanClimb/WaypointSpacing/
  Costs, ComputeAsync, GetWaypoints con Action Walk/Jump, Status, Blocked,
  CheckOcclusionAsync), PathfindingModifier, PathfindingLink.
- Clothing: Shirt, Pants, ShirtGraphic, BodyColors (fatti in parte), Accessory e Hat con
  Handle e Attachment.

P6 - Strumenti e combattimento (123 membri)
- Tool completo (Handle, Grip, GripPos/Forward/Right/Up, RequiresHandle, CanBeDropped,
  ToolTip, TextureId, Enabled, ManualActivationOnly, Activated, Deactivated, Equipped
  con il Mouse, Unequipped, Activate, Deactivate), Backpack, StarterPack, StarterGear,
  il tasto Backspace per buttare, la barra 1-9 (CoreScript).
- Mouse/PlayerMouse: Hit, Target, Origin, UnitRay, X, Y, ViewSizeX/Y, Icon,
  TargetFilter, Button1Down/Up, Button2Down/Up, Move, WheelForward/Backward, Idle.
- Explosion completa (Position, BlastRadius, BlastPressure, DestroyJointRadiusPercent,
  ExplosionType, Visible, Hit con la distanza), ForceField (Visible), Debris (AddItem).
- ProximityPrompt completo (ActionText, ObjectText, KeyboardKeyCode, GamepadKeyCode,
  HoldDuration, MaxActivationDistance, RequiresLineOfSight, Exclusivity, Style,
  ClickablePrompt, UIOffset, Enabled, Triggered, TriggerEnded, PromptShown,
  PromptHidden, PromptButtonHoldBegan/Ended, InputHoldBegin/End),
  ProximityPromptService (Enabled, MaxPromptsVisible, PromptTriggered...).
- ClickDetector (MaxActivationDistance, CursorIcon, MouseClick, RightMouseClick,
  MouseHoverEnter, MouseHoverLeave), DragDetector (DragStyle, ResponseStyle,
  DragStart, DragContinue, DragEnd, MaxDragTranslation...).

P7 - GUI (389 membri)
- GuiObject completo: Size, Position, AnchorPoint, Rotation, ZIndex, LayoutOrder,
  Visible, ClipsDescendants, AutomaticSize, BackgroundColor3, BackgroundTransparency,
  BorderColor3, BorderSizePixel, BorderMode, SizeConstraint, Selectable, Active,
  Interactable, AbsolutePosition, AbsoluteSize, AbsoluteRotation, eventi MouseEnter,
  MouseLeave, MouseMoved, MouseWheelForward/Backward, InputBegan, InputChanged,
  InputEnded, TouchTap..., SelectionGained/Lost; TweenSize/TweenPosition/
  TweenSizeAndPosition.
- GuiButton: Activated, MouseButton1/2 Click/Down/Up, AutoButtonColor, Modal, Selected,
  Style. CLIC PER ELEMENTO, non per nome (limiti n. 16) e Name che arriva al motore.
- Testo: Text, TextColor3, TextSize, TextScaled, TextWrapped, TextXAlignment,
  TextYAlignment, TextTransparency, TextStrokeColor3, TextStrokeTransparency, Font,
  FontFace, LineHeight, RichText (b, i, u, font color, size, stroke, br),
  MaxVisibleGraphemes, TextBounds, TextFits, ContentText.
- TextBox: CaptureFocus, ReleaseFocus, IsFocused, Focused, FocusLost(enterPressed),
  ClearTextOnFocus, PlaceholderText, PlaceholderColor3, MultiLine, TextEditable,
  CursorPosition, SelectionStart, ReturnPressedFromOnScreenKeyboard; tastiera del
  telefono su Android.
- Immagini: ImageLabel/ImageButton con Image (file locali e asset di Blocky), ImageColor3,
  ImageTransparency, ImageRectOffset, ImageRectSize, ScaleType (Stretch, Slice, Tile,
  Fit, Crop), SliceCenter, SliceScale, TileSize, ResampleMode, HoverImage, PressedImage.
- ScrollingFrame che scorre davvero (rotella, trascinamento, tocco): CanvasSize,
  CanvasPosition, AutomaticCanvasSize, ScrollingDirection, ScrollBarThickness,
  ScrollBarImageColor3, ScrollingEnabled, ElasticBehavior, AbsoluteCanvasSize,
  AbsoluteWindowSize.
- ViewportFrame con WorldModel e Camera (personaggi e oggetti 3D nella GUI: anteprime
  di negozio), CanvasGroup (GroupTransparency, GroupColor3).
- Contenitori: ScreenGui (Enabled, DisplayOrder, IgnoreGuiInset, ResetOnSpawn,
  ZIndexBehavior, ScreenInsets, SafeAreaCompatibility), BillboardGui (Adornee,
  StudsOffset, StudsOffsetWorldSpace, ExtentsOffset, Size in scala e in stud,
  AlwaysOnTop, MaxDistance, LightInfluence, ClipsDescendants), SurfaceGui (Face,
  Adornee, CanvasSize, PixelsPerStud, SizingMode, AlwaysOnTop, LightInfluence).
- UI*: UIListLayout (FillDirection, HorizontalAlignment, VerticalAlignment, SortOrder,
  Padding, Wraps, HorizontalFlex, VerticalFlex, ItemLineAlignment), UIGridLayout
  (CellSize, CellPadding, FillDirectionMaxCells, StartCorner), UIPageLayout (pagine,
  JumpTo, Next, Previous, Animated, Circular, PageEnter/Leave), UITableLayout,
  UIPadding, UICorner, UIStroke (fatti), UIGradient (Color, Transparency, Rotation,
  Offset, Enabled), UIScale, UIAspectRatioConstraint, UISizeConstraint,
  UITextSizeConstraint, UIFlexItem, UIDragDetector.
- PlayerGui (GetTopbarTransparency, ScreenOrientation, SelectionImageObject),
  StarterGui (SetCore: SendNotification, ResetButtonCallback, ChatMakeSystemMessage...;
  GetCore; SetCoreGuiEnabled/GetCoreGuiEnabled per Health, Backpack, PlayerList, Chat,
  EmotesMenu, All), GuiService (MenuIsOpen, GetGuiInset, SelectedObject, MenuOpened/
  Closed), TextService (GetTextSize, GetTextBoundsAsync, FilterStringAsync in locale).
- TweenService sulle GUI.

P8 - Input, camera, tempo (151 membri)
- UserInputService completo: InputBegan, InputChanged, InputEnded (InputObject con
  KeyCode, UserInputType, UserInputState, Position, Delta, IsModifierKeyDown),
  IsKeyDown, IsMouseButtonPressed, GetKeysPressed, GetMouseButtonsPressed,
  GetMouseLocation, GetMouseDelta, MouseBehavior (Default, LockCenter,
  LockCurrentPosition), MouseIconEnabled, MouseDeltaSensitivity, TouchEnabled,
  KeyboardEnabled, MouseEnabled, GamepadEnabled, TouchStarted/Moved/Ended, TouchTap,
  TouchLongPress, TouchPan, TouchPinch, TouchRotate, TouchSwipe, JumpRequest,
  GetFocusedTextBox, TextBoxFocused/FocusReleased, WindowFocused/FocusReleased,
  gamepad (GetConnectedGamepads, GetGamepadState, GamepadConnected/Disconnected,
  GetNavigationGamepads), PreferredInput, GetStringForKeyCode.
- ContextActionService: BindAction (con createTouchButton: pulsanti veri sul
  telefono), BindActionAtPriority, UnbindAction, UnbindAllActions, GetBoundActionInfo,
  SetTitle, SetDescription, SetImage, SetPosition, GetButton, BindActivate.
- Camera completa: CameraType (Custom, Scriptable, Follow, Attach, Fixed, Track, Watch,
  Orbital), CFrame, Focus, FieldOfView, FieldOfViewMode, CameraSubject, ViewportSize,
  NearPlaneZ, ViewportPointToRay, ScreenPointToRay, WorldToViewportPoint,
  WorldToScreenPoint, GetPartsObscuringTarget, Zoom, prima persona con
  Player.CameraMode = LockFirstPerson, Interpolate (vecchio).
- RunService: Heartbeat, Stepped, RenderStepped, PreSimulation, PostSimulation,
  PreRender, PreAnimation, BindToRenderStep (con Enum.RenderPriority),
  UnbindFromRenderStep, IsServer, IsClient, IsStudio, IsRunning, IsRunMode.
- TweenService completo: Create con TUTTI gli EasingStyle (Linear, Sine, Back, Quad,
  Quart, Quint, Bounce, Elastic, Exponential, Circular, Cubic) e EasingDirection (In,
  Out, InOut), su numeri, Vector3, CFrame, Color3, UDim2, Vector2, booleani;
  RepeatCount, Reverses, DelayTime; Tween:Play, Pause, Cancel, PlaybackState, Completed;
  GetValue. (Oggi Back/Quart/Quint/Bounce/Elastic sono tutti uguali a smoothstep:
  vanno fatti veri.)

P9 - Effetti, luci, atmosfera, suono (291 membri)
- ParticleEmitter completo: Rate, Lifetime, Speed, SpreadAngle, Acceleration, Drag,
  Size/Transparency/Color con le sequenze, Texture, LightEmission, LightInfluence,
  Brightness, Orientation, Rotation, RotSpeed, Squash, ZOffset, LockedToPart,
  EmissionDirection, Shape/ShapeStyle/ShapeInOut/ShapePartial, Enabled, Emit(n), Clear,
  Flipbook*, TimeScale, VelocityInheritance, WindAffectsDrag.
- Beam (Attachment0/1, Color, Transparency, Width0/1, CurveSize0/1, Segments, Texture,
  TextureSpeed, TextureLength, FaceCamera, LightEmission), Trail (Attachment0/1, Color,
  Transparency, Lifetime, MinLength, WidthScale, FaceCamera, Texture), Fire (Size, Heat,
  Color, SecondaryColor, Enabled), Smoke (Size, Opacity, RiseVelocity, Color), Sparkles
  (SparkleColor), Highlight (Adornee, FillColor, FillTransparency, OutlineColor,
  OutlineTransparency, DepthMode, Enabled), SelectionBox e gli HandleAdornment
  (Box, Sphere, Cylinder, Cone, Line: per mirini, aree, debug).
- Luci: PointLight, SpotLight, SurfaceLight (Brightness, Range, Color, Shadows, Enabled,
  Angle, Face), con il limite di luci del telefono.
- Lighting completo: ClockTime, TimeOfDay, GeographicLatitude, GetMinutesAfterMidnight,
  SetMinutesAfterMidnight, GetSunDirection, GetMoonDirection, Ambient, OutdoorAmbient,
  Brightness, ColorShift_Top/Bottom, EnvironmentDiffuseScale, EnvironmentSpecularScale,
  ExposureCompensation, FogStart, FogEnd, FogColor, GlobalShadows, ShadowSoftness,
  Technology, LightingChanged.
- Sky (SkyboxBk/Dn/Ft/Lf/Rt/Up, SunTextureId, MoonTextureId, StarCount,
  CelestialBodiesShown, SunAngularSize), Atmosphere (Density, Offset, Color, Decay,
  Glare, Haze), Clouds (Cover, Density, Color), post-effetti BloomEffect, BlurEffect,
  ColorCorrectionEffect, SunRaysEffect, DepthOfFieldEffect (oggi in parte: completarli).
- Sound completo: SoundId (file di Blocky), Volume, PlaybackSpeed, TimePosition,
  TimeLength, Looped, Playing, IsPlaying, IsPaused, IsLoaded, PlayOnRemove, Play, Stop,
  Pause, Resume, Played, Stopped, Paused, Resumed, Ended, DidLoop, Loaded, suono 3D
  (dentro una part o un Attachment: RollOffMode, RollOffMinDistance,
  RollOffMaxDistance), SoundGroup (Volume), gli effetti (Reverb, Equalizer, Echo,
  Distortion, Chorus, Compressor, Flange, PitchShift, Tremolo), SoundService
  (AmbientReverb, RolloffScale, PlayLocalSound, SetListener).

P10 - Dati, rete e servizi (244 membri)
- RemoteEvent (FireServer, FireClient, FireAllClients, OnServerEvent con il Player,
  OnClientEvent), RemoteFunction (InvokeServer, InvokeClient, OnServerInvoke,
  OnClientInvoke), UnreliableRemoteEvent; i tipi che passano (anche Instance, tabelle
  annidate, Vector3, CFrame, Enum) come Roblox; il client non vede ServerStorage e
  ServerScriptService.
- DataStoreService completo: GetDataStore (con scope), GetOrderedDataStore (GetSortedAsync
  con pagine, crescente/decrescente, min/max), GetGlobalDataStore, ListDataStoresAsync,
  GetRequestBudgetForRequestType; GlobalDataStore: GetAsync, SetAsync, UpdateAsync,
  IncrementAsync, RemoveAsync, OnUpdate; DataStore: ListKeysAsync, ListVersionsAsync,
  GetVersionAsync, RemoveVersionsAsync; DataStoreKeyInfo, DataStoreSetOptions; errori e
  limiti di dimensione come Roblox; salvataggio vero sul disco del dispositivo (o del
  server dedicato).
- MemoryStoreService (GetSortedMap, GetQueue, GetHashMap con le loro funzioni, anche solo
  in memoria del server), MessagingService (PublishAsync, SubscribeAsync tra i server di
  Blocky), HttpService (JSONEncode, JSONDecode, GenerateGUID, UrlEncode; GetAsync,
  PostAsync, RequestAsync solo se HttpEnabled e se lo abilito io).
- MarketplaceService (PromptGamePassPurchase, PromptProductPurchase,
  UserOwnsGamePassAsync, GetProductInfo, ProcessReceipt, PromptPurchaseFinished...:
  con i "B$" di Blocky in locale), BadgeService, GroupService, PolicyService,
  LocalizationService, SocialService (inviti finti o spenti, senza errori).
- TextChatService completo (TextChannels, RBXGeneral, SendAsync, DisplaySystemMessage,
  MessageReceived, OnIncomingMessage, TextChatCommand con PrimaryAlias e Triggered,
  ChatWindowConfiguration, BubbleChatConfiguration), Chat:Chat (le bolle sopra la testa).
- TeleportService (Teleport, TeleportAsync, TeleportOptions, GetLocalPlayerTeleportData,
  tra i place di Blocky).
- DataModel: GetService (tutti i servizi del dump: quelli che Blocky non ha danno un
  servizio "presente" senza errori), FindService, PlaceId, GameId, JobId, PlaceVersion,
  CreatorId, BindToClose, IsLoaded, Loaded, Shutdown.

=====================================================================
P11 - CORESCRIPTS (src/engine/lua/core)
=====================================================================
L'interfaccia di base di Blocky in Luau, come le CoreScripts di Roblox: barra della vita
(fatta: BarraVita.luau), zaino e barra 1-9 (con le icone dei Tool), lista dei giocatori
con leaderstats e squadre, chat con le bolle, prompt "E Apri" (ProximityPrompt), menu
(Esc: riprendi, impostazioni, esci), notifiche (SendNotification), pulsanti del telefono
(salto, joystick). Ognuna spegnibile con SetCoreGuiEnabled; leggere (si ridisegnano solo
quando cambia qualcosa); usano __blkCoreGui("Heartbeat", f). Quando una CoreScript
prende il posto di un pezzo del C++ quel pezzo non si disegna piu' (come la barra della
vita: __blkCoreGui("Health", true)), ma se il Luau non parte resta quello del C++.

=====================================================================
P12 - LUAU PARALLELO E SICUREZZA
=====================================================================
- Actor, task.desynchronize/synchronize, ConnectParallel, SharedTable: anche se tutto
  gira su un thread, gli script che li usano devono funzionare (senza errori, stesso
  risultato).
- La sandbox resta: niente loadstring/getfenv/setfenv, gli script non sostituiscono le
  funzioni del motore (lo scudo anti-executor vieta le FUNZIONI nelle tabelle del motore,
  non i dati). Ogni API nuova non deve aprire buchi: niente accesso ai file fuori dal
  gioco, niente rete senza permesso.

=====================================================================
GIOCHI-MODELLO (la prova che "ogni tipo di gioco" funziona)
=====================================================================
Per ogni genere un progetto Rojo piccolo (200-600 righe), ORIGINALE, in
giochi/modelli/<genere>/, scritto come lo scriverebbe uno sviluppatore Roblox con le API
tipiche del genere. NON usare le aggiunte di Blocky: solo API Roblox.
- obby: checkpoint (SpawnLocation/attributi), part che uccidono, piattaforme che si
  muovono (TweenService), timer in leaderstats, salvataggio dello stage (DataStore).
- tycoon: bottoni da comprare (Touched), soldi in leaderstats, nastri trasportatori
  (AssemblyLinearVelocity sulle part), DataStore, squadre per i lotti.
- simulator: raccogliere (ClickDetector/Tool), zaino che si riempie, vendere, pet che
  seguono (AlignPosition), classifica (OrderedDataStore), negozio GUI con ScrollingFrame.
- sparatutto: Tool con raycast, danni, mirino GUI, munizioni e ricarica, squadre e respawn,
  prima persona (CameraMode, MouseBehavior LockCenter), effetti (Beam, ParticleEmitter, suono 3D).
- corse: VehicleSeat con le ruote a HingeConstraint, giri e checkpoint, cronometro, classifica.
- tower defense: percorso con waypoint, ondate di NPC (Humanoid:MoveTo), torrette che
  ruotano (CFrame.lookAt), soldi, GUI per piazzare.
- horror: buio (Lighting), torcia (SpotLight), suono 3D, NPC che insegue con
  PathfindingService, porte con ProximityPrompt e TweenService, jumpscare GUI.
- survival e crafting: fame e sete (attributi), giorno e notte, albero che si taglia,
  crafting a ricette (GUI con UIGridLayout), oggetti trascinabili (DragDetector).
- picchiaduro: animazioni (AnimationTrack con marker), combo, hitbox con
  GetPartBoundsInBox, knockback (ApplyImpulse), barra della vita BillboardGui.
- sandbox: posare blocchi su griglia (Mouse.Hit, Raycast), colori, cancellare, salvare
  e ricaricare (DataStore con JSON).
- roleplay di citta': case con porte, lavori, soldi, auto, telefono GUI, chat (TextChatService).
- puzzle/escape: oggetti da raccogliere, codici con TextBox, leve, porte a tempo.
- platform 2D: camera Scriptable laterale, movimento solo su un asse, nemici che pattugliano.
- party game: lobby, minigiochi a turni (teletrasporti, punteggio, timer), vincitore.
Ognuno ha Config.PROVA_AUTOMATICA (se serve) e deve girare 30 s SENZA errori Lua nel test
dei giochi veri di tools/test_all.ps1 (aggiungili; se il test dei giochi non c'e' ancora,
crealo: avvia BlockyGameEngine con BLOCKY_SHOT/BLOCKY_ROJO e cerca gli errori nel log).
Un errore in un gioco-modello = un'API che manca o e' sbagliata: SI CORREGGE L'API,
non il gioco. Guarda anche il png: il gioco deve vedersi giusto.
I miei progetti veri restano il giudice finale: giochi/UltimoBinario e
C:\Users\User\Documents\dead-rails (quest'ultimo NON si modifica).

=====================================================================
COME SI MISURA
=====================================================================
- docs/API_CHECKLIST.md: quante caselle spuntate per pacchetto (scrivilo nello Stato).
- docs/api_coverage.md: "funzionante" delle classi piu' usate dai giochi sopra l'80%.
- tests/compat tutti verdi; tools/test_all.ps1 VERDE.
- Tutti i giochi-modello e i due giochi veri: 30 s senza errori, e il png giusto.
- Il banco di prova (calcoli, tabelle, Vector3, part.Position) non deve peggiorare.

=====================================================================
FINE SESSIONE
=====================================================================
- Rilancia tools/test_all.ps1 (VERDE) e i giochi-modello.
- Aggiorna la sezione "Stato" in fondo a docs/PROMPT_API_LUAU.md: caselle spuntate per
  pacchetto, copertura, giochi-modello che girano, il primo errore che resta in ognuno.
- Dimmi in breve cosa hai fatto, cosa non hai potuto provare e cosa e' pronto per il mio "salva".
```

---

## Stato (aggiornarlo a fine sessione)

Aggiornato al 19 novembre 2026.

| | |
|---|---|
| Lista di controllo | 2080 membri in `docs/API_CHECKLIST.md`, 0 spuntati: va fatta la prima passata che spunta quelli già funzionanti e provati |
| Copertura | funzionante 8,1% · presente 78,8% |
| Moduli API Luau | 17 in `src/engine/lua/api`, più 1 CoreScript (barra della vita) |
| Test | `tests/compat` fino all'81, tutti verdi; `tools/test_all.ps1` VERDE (53 test) |
| Primo bug da chiudere (P0) | `task.wait()` dentro gli eventi (PlayerAdded / CharacterAdded) viene rifiutato |
| Giochi-modello | nessuno ancora |
| Pezzi già fatti | P2 (attributi, discendenti, segnali di proprietà), P3 (ricerche nel mondo, CSG), P4 (gruppi di collisione, rete), P5 (LoadCharacter, Seat.Occupant, Humanoid.Sit), P7 (GUI di base, UICorner, UIStroke), P11 (barra della vita) |
