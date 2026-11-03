# Blocky Engine Clean — audit

Obiettivo: sviluppo sensato (come Studio), non feature-count. Blocky-**gioco** resta; si ripulisce l’**engine**.

## KEEP (capitale)

- Hub / Play / touch / avatar in partita
- Luau ufficiale + sandbox `LuaRuntime`
- Parts / Models / Terrain / Lighting / Bloom / ShadowMap
- Pipeline documentata in `src/render/RenderPipeline.h`
- StarterPack tools + grip editor
- Place package PC + Android

## FIX (fatto in questo clean pass)

- Renderer: pass nominati (`passPrepare`…`passAvatars`), ombre `quality.shadows` collegate
- Grafica Studio: Performance / Studio Default / Beautiful
- Explorer: Workspace vs Services (StarterGui / StarterPack / Players fuori dalle Parts)
- Script: Script / LocalScript / ModuleScript (+ Legacy compat)
- Remoti disegnati anche con avatar GLB locale

## CUT / NASCONDERE (non accumulare)

- Chat / Debris / CollectionService → sotto “Altri servizi” (non in cima Explorer)
- Look AAA multipli restano; preferire Grafica Studio per look base
- `drawExplosionCubes` stub — lasciato, non esporre in UI
- AntiCheat panel in Engine: avanzato, non percorso creatore base

## NEXT (prossime settimane)

- ~~ModuleScript `require` path ReplicatedStorage~~ **FATTO**
- ~~RemoteEvent persistiti nel place~~ **FATTO** (instances[] BlockyPlace v2)
- ~~Hierarchy RS/SS/SSS~~ **FATTO**
- ~~Part Parent/Child + FindFirstChild/GetChildren~~ **FATTO**
- ~~WaitForChild con yield (task.spawn)~~ **FATTO**
- ~~RemoteFunction InvokeServer + OnServerInvoke (sync locale)~~ **FATTO**
- ~~Script in ServerScriptService auto-run~~ **FATTO**
- ~~BindableFunction Invoke/OnInvoke~~ **FATTO**
- ~~Humanoid.Died / HealthChanged / GetState~~ **FATTO**
- ~~ProximityPrompt (E + Triggered)~~ **FATTO**
- ~~leaderstats su LocalPlayer~~ **FATTO** (Classifica)
- ~~DataStore UpdateAsync~~ **FATTO**
- ~~UIListLayout base su Frame~~ **FATTO**
- ~~RemoteFunction rete (RFREQ/RFRES + yield)~~ **FATTO**
- ~~Camera FieldOfView / CameraType / CFrame~~ **FATTO**
- ~~PathfindingService:CreatePath~~ **FATTO**
- ~~ParticleEmitter Instance.new~~ **FATTO**
- ~~Sound:Play instance + ClickDetector~~ **FATTO**
- ~~Tool Instance + Equipped/Unequipped/Activated~~ **FATTO**
- ~~Players.PlayerAdded / CharacterAdded~~ **FATTO**
- ~~ChildAdded Part/Folder~~ **FATTO**
- ~~WeldConstraint / Motor6D~~ **FATTO**
- ~~BillboardGui / SurfaceGui~~ **FATTO**
- Deprecare eventi Item* se Tool lifecycle basta
