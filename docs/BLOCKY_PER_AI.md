# Scrivere giochi per Blocky (guida per persone e AI)

Generata da `tools/roblox_api/gen_blocky_defs.mjs`. Blocky esegue **Luau** con l'API di
Roblox: si scrive come su Roblox. Questa pagina dice cosa c'e', cosa no, e cosa ha in piu'.

## Regole
1. Scrivi come su Roblox (`game:GetService`, `Instance.new`, `:Connect`, `task.wait`...).
2. Prima di usare un'API poco comune guarda `docs/api_coverage.md`:
   - **funzionante** = fa davvero la cosa;
   - **presente** = esiste e non fa crashare lo script, ma non fa ancora niente
     (valore iniziale di Roblox; eventi che non scattano; i metodi danno l'errore
     "non e' ancora in Blocky").
3. Non usare `loadstring`, `setfenv`, `getfenv`: sono bloccati (sandbox).
4. Gli Enum hanno i numeri di Roblox (`Enum.KeyCode.E`, `Enum.EasingStyle.Quad`...).

## Progetti Rojo
Lo Studio legge i progetti Rojo (menu Script -> Rojo): `default.project.json` con
`tree` / `$path` / `$className`.
- `nome.server.luau` -> Script, `nome.client.luau` -> LocalScript, `nome.luau` -> ModuleScript
- cartella con `init.server.luau` / `init.client.luau` / `init.luau` -> la cartella e' quello script
- `nome.json` -> ModuleScript che restituisce i dati
- servizi: ReplicatedStorage, ServerStorage, ServerScriptService, StarterPlayer.StarterPlayerScripts, StarterGui
- non ancora: modelli (`.rbxm`, `.model.json`), Workspace con parti, Lighting
I file si sincronizzano da soli ogni mezzo secondo mentre lo Studio e' aperto.

## API in piu' di Blocky (globali)
- **AnimateFrame**
- **Workspace**
- **Events**: `Create`, `Connect`, `Once`, `Wait`, `Fire`, `DisconnectAll`
- **Humanoid**: `SetJumpEnabled`, `SetWalkEnabled`, `SetWalkSpeed`, `SetJumpPower`, `Get`, `GetPosition`, `TakeDamage`, `GetHealth`, `GetLook`, `SetPosition`, `GetState`, `LoadAnimation`, `GetPlayingAnimationTracks`
- **Backpack**: `Give`, `Equipped`, `Equip`, `Unequip`, `Drop`
- **Camera**: `Shake`, `ScreenRay`, `ScreenPointToRay`, `GetLook`, `GetCFrame`
- **UserInputService**: `DisableJump`, `EnableJump`, `IsKeyDown`, `IsSprintHeld`, `IsMoving`, `IsMouseButtonPressed`

Tipi per VS Code: `tools/blocky_luau/blocky.d.luau` (luau-lsp, piattaforma "roblox").
