# Limiti trovati scrivendo un gioco solo in Luau

Prova: si può fare un gioco vero in Blocky scrivendo **solo Luau**, come su Roblox,
senza toccare il motore? Il gioco di prova è `giochi/UltimoBinario` (progetto Rojo).

Regola: mentre si scrive il gioco il motore non si tocca. Quello che non va come su
Roblox si scrive qui, con lo script che lo dimostra, e poi si decide se correggerlo
(in C++ si aggiungono solo API).

## Già corretti nel motore (14 novembre 2026, prima della regola)
Erano bug veri che nessuno script poteva aggirare. Ognuno ha un test in `tests/compat/`.

| Script Roblox normale | Cosa succedeva | Test |
|---|---|---|
| `p.Color = ...` e poi `p.Parent = workspace` | il colore si perdeva (vinceva il BrickColor grigio di partenza) | 58 |
| più `ProximityPrompt` con lo stesso Name | diventavano uno solo; `Triggered` scattava per tutti | 56 |
| una proprietà del prompt scritta due volte | la seconda scrittura era ignorata | 56 |
| `prompt.Triggered:Connect(function(player)` | arrivava il nome del prompt, non il Player | 56 |
| `prompt:Destroy()` / la part del prompt distrutta | non esisteva / il prompt restava a galleggiare | 56 |
| `BodyColors` / `Shirt` con `.Parent = npc` dopo | coloravano il giocatore | 57 |
| `bc.HeadColor3 = ...` | ignorato (si leggeva solo `HeadColor`) | 57 |
| `npc.MaxHealth = 320` | ignorato | 57 |
| `root.CFrame = ...` / `root.Position = ...` | il giocatore restava fermo | 55 |
| `npc:Destroy()`, `npc.Position = ...` | non esistevano | 55 |
| `p.Shape = Enum.PartType.Cylinder` | ignorato | 55 |
| `p.Material == Enum.Material.Wood` | sempre falso (si leggeva un testo) | 55 |
| `Humanoid.WalkSpeed` letto prima di cambiarlo | -1 invece di 16 | — |
| `Model:PivotTo` di un modello che si sposta | 1,8 ms per 40 part (ora 0,2 ms) | 55 |

## Ancora da correggere (il gioco li aggira in Luau)

### 1. Cilindri: l'asse è Y, su Roblox è X
```lua
local ruota = Instance.new("Part")
ruota.Shape = Enum.PartType.Cylinder
ruota.Size = Vector3.new(0.5, 3, 3)   -- su Roblox: ruota sottile, asse lungo X
ruota.Parent = workspace              -- in Blocky: un cilindro alto 3 e largo 0.5
```
Come lo aggira il gioco: `Treno.luau`, funzione `cilindro()`, con `Size` (diametro, lungo, diametro)
e `Orientation` (0, 0, 90). Il codice Roblox che arriva da fuori viene sbagliato.
Correggerlo rompe i place di Blocky già fatti, quindi serve una regola di migrazione.

### 2. Limite di istruzioni per chiamata — corretto
Prima uno script veniva fermato dopo 50 ms (`Lua budget: troppe istruzioni`).
Ora il timeout è 10 secondi, come uno Script su Roblox: un ciclo infinito si
ferma ancora, una mappa costruita tutta in una volta no. Test `63`.

### 3. Materiali che non esistono diventano plastica senza avviso
```lua
p.Material = Enum.Material.WoodPlanks   -- e Sandstone, Cobblestone, Pebble...
print(p.Material)                       -- Plastic
```
`Enum.Material.WoodPlanks` esiste (sono gli Enum di Roblox), ma il motore non ha quel
materiale. Proposta: ripiegare sul materiale più simile (WoodPlanks → Wood,
Sandstone → Sand, Cobblestone → Slate) e scriverlo una volta in Output.

### 4. `Enum.Material.X` e `Enum.KeyCode.X` non sono EnumItem — corretto
`Enum.Material.Wood.Name`, `.Value` e `typeof(...) == "EnumItem"` funzionano.
Il confronto `part.Material == Enum.Material.Wood` resta vero. Test `55` e `74`.

### 5. `typeof(UDim2.new())` restituisce "Vector2"
Già noto, ancora da correggere.

### 6. Il Player non ha `GetAttributeChangedSignal` né `AttributeChanged`
```lua
local io = game:GetService("Players").LocalPlayer
io:GetAttributeChangedSignal("Oro"):Connect(print)   -- errore: metodo mancante
```
Un Folder creato dagli script li ha; il LocalPlayer (oggetto del motore) no.
Come lo aggira il gioco: `Arma.client.luau` controlla l'attributo ogni mezzo secondo.

### 7. La `leaderstats` che Blocky mette sul giocatore non è un Folder vero
```lua
local ls = game:GetService("Players").LocalPlayer:FindFirstChild("leaderstats")
print(typeof(ls))                   -- "Instance"
ls:FindFirstChild("Coins")          -- errore: "attempt to call a table value"
```
Ha un campo `FindFirstChild` che è una tabella (e un `Create` tutto suo). Il codice
Roblox normale (cerco leaderstats, se c'è ci aggiungo i valori) si rompe.
Il test `02_leaderstats` non se ne accorge perché crea sempre una cartella nuova.
Come lo aggira il gioco: `Record.luau` crea sempre la sua cartella e se la ricorda.

### 8. `Instance.new("Tool")` con il Parent alla fine: il Tool compare due volte
```lua
local t = Instance.new("Tool")
t.Name = "Spada"
t.Parent = game.Players.LocalPlayer.Backpack
-- nella barra: "Tool" e "Spada"
```
Il motore aggiunge il Tool allo zaino quando lo crea, e poi di nuovo quando arriva
il Parent (con il nome nuovo). Mettere il Parent subito non aiuta: il nome resta
"Tool" per il motore, e gli eventi (Activated...) sono legati al nome, quindi tutti
i Tool chiamati "Tool" li condividono. In più un Tool creato con il Parent subito
non compare in `Backpack:GetChildren()`.
Come lo aggira il gioco: `Arma.client.luau` usa la forma di Blocky
`Instance.new("Tool", "Spada")` (nome come 2o argomento), che non è Roblox.

### 9. `Instance.new("Seat")` non fa sedere
```lua
local s = Instance.new("Seat")
s.Anchored = true
s.Parent = workspace
print(s.Seat)   -- false: camminarci sopra non fa sedere
```
Il motore crea una part normale; si siede solo sulle part con la proprietà di Blocky
`Seat = true` (quella che lo Studio mette con la spunta).
Come lo aggira il gioco: `Treno.luau` mette `s.Seat = true`.

### 10. Non si sa chi è seduto: niente `Seat.Occupant`, `Humanoid.Sit`, `Humanoid.SeatPart`
```lua
s:GetPropertyChangedSignal("Occupant"):Connect(...)   -- Occupant non esiste
print(humanoid.Sit, humanoid.SeatPart)                -- nil, nil
```
Su Roblox è il modo normale per sapere chi guida un veicolo.
Come lo aggira il gioco: `Main.server.luau` (`portaGiocatori`) misura se il personaggio
sta sopra il sedile e scrive l'attributo `Macchinista` del Player.

### 12. BLOCCANTE: il giocatore muore a 144 stud dal centro
```cpp
// src/main_ui/ScriptsAndPhysics.h:1015
const float killY = -12.f;
const float mapHalfExtent = 144.f;
bool fellOffMap = g_playerPos.y < killY || std::fabs(g_playerPos.x) > mapHalfExtent
                  || std::fabs(g_playerPos.z) > mapHalfExtent || ...
```
Qualsiasi mappa più grande di 288×288 stud uccide il giocatore ai bordi, anche con il
pavimento sotto i piedi. Nel gioco: si muore sempre quando il treno arriva a 173.
Su Roblox non c'è un limite in orizzontale; si muore solo sotto
`Workspace.FallenPartsDestroyHeight` (di partenza -500).
Da Luau non si aggira, se non spostando tutto il mondo indietro ogni tanto
("origine mobile"), con scatti a ogni spostamento.
Proposta: togliere il limite in X e Z e usare `workspace.FallenPartsDestroyHeight`
per l'altezza.

### 13. Scatti quando tante part ancorate si muovono insieme
Misurato nel gioco (cronometro in `Main.server.luau`, con `Config.PROVA_AUTOMATICA`),
treno in movimento con circa 200 part:
- Luau: `Treno.passo` al massimo 2,2 ms per frame; giocatori, oggetti e nemici meno di 0,1 ms.
- Frame oltre 25 ms: 8-14 ogni 5 secondi, il più lungo 50 ms, anche quando gli script
  non fanno quasi niente.
Il costo è nel motore (disegno, fisica o griglia delle part, quando le part si spostano),
non negli script. Da guardare nel motore con il profiler.

### 14. `part:SetNetworkOwner(nil)` va in errore e ferma lo script
```lua
root:SetNetworkOwner(nil)   -- "Part:SetNetworkOwner() non e' ancora in Blocky"
```
Molto comune negli script Roblox che muovono veicoli dal server. In Blocky la fisica
è già tutta del server, quindi il metodo potrebbe semplicemente non fare niente
(lo stesso vale per `GetNetworkOwner` e `SetNetworkOwnershipAuto`).
Trovato provando uno script di treno fisico (LinearVelocity + WeldConstraint).

### 15. `Instance.new("Part")` parte ancorata (su Roblox no)
```lua
local p = Instance.new("Part")
print(p.Anchored)   -- Blocky: true, Roblox: false
```
Gli script Roblox che costruiscono veicoli o oggetti fisici non scrivono `Anchored = false`
perché è già così: in Blocky restano fermi. Cambiarlo è una decisione grossa (tutti gli
script di Blocky che contano su `true` vedrebbero cadere le part): forse un'impostazione
per place ("compatibilità Roblox").

### 16. Il `Name` degli elementi GUI non arriva al motore (e i clic vanno per nome)
```lua
local b = Instance.new("TextButton")
b.Name = "Gioca"        -- lo script lo rilegge, ma per il motore resta "TextButton"
b.MouseButton1Click:Connect(...)
```
Il motore smista i clic dei bottoni per nome (`guiButtonActivated[nome]`): due bottoni con
lo stesso nome condividono gli eventi, e rinominare un bottone dopo averlo collegato
lo stacca. Da correggere collegando gli eventi al singolo elemento (indice della label),
poi il `Name` si può passare al motore senza rischi.

### 11. Lo spostamento delle part ancorate non porta con sé chi ci sta sopra
Anche su Roblox è così per le part spostate via CFrame, quindi non è un errore di
compatibilità: chi fa treni e ascensori deve portare il giocatore da sé.
Da sapere: va fatto **nello stesso momento** in cui si sposta la part (nel gioco:
`portaGiocatori` subito dopo `Treno.passo`, nello stesso Heartbeat). Farlo in un
altro momento del frame (prima: RenderStepped del client) dà dei microscatti.
