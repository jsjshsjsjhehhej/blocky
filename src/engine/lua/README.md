# API Luau del motore

Qui ci sono le API Roblox di Blocky scritte **in Luau** invece che in C++.

| File | Cosa contiene |
|---|---|
| `prelude.luau` | la base: Instance Luau, CFrame, segnali, Enum, lo strato "API presente" |
| `api/*.luau` | un file per ogni API aggiunta in Luau (es. `api/rete.luau`) |
| `core/*.luau` | le CoreScripts: l'interfaccia di base di Blocky, come le CoreScripts di Roblox (es. `core/BarraVita.luau`) |

## CoreScripts
Sono moduli come quelli di `api/` (stesso `Internal.modulo`), ma disegnano l'interfaccia
di base con la GUI in stile Roblox (ScreenGui nel PlayerGui) invece che in C++.
Quando una CoreScript parte chiama `__blkCoreGui("Health", true)` e il C++ smette di
disegnare quel pezzo; se lo stato Lua non parte resta il disegno del C++.
I giochi le spengono come su Roblox: `StarterGui:SetCoreGuiEnabled(Enum.CoreGuiType.Health, false)`.
Regola per i telefoni: aggiornare solo quando qualcosa cambia, non a ogni frame.

## Aggiungere un'API: un file
1. Crea `api/nome.luau`:
   ```lua
   local Internal = ...
   Internal.modulo("nome", function(I)
       -- un metodo vero su una classe del motore (e sulle sue figlie)
       I.realFuncs.BasePart = I.realFuncs.BasePart or {}
       I.realFuncs.BasePart.MioMetodo = function(self, x) ... end
       -- una proprieta' calcolata
       I.realProps.Humanoid = I.realProps.Humanoid or {}
       I.realProps.Humanoid.MiaProprieta = function(h) return ... end
   end)
   ```
2. Scrivi il test in `tests/compat/NN_nome.lua` (`COMPAT_OK = true` alla fine).
3. Premi Play nello Studio (o lancia `build/bin/test_compat.exe`): **senza ricompilare**.

Cosa si trova in `I` (Internal):
| | |
|---|---|
| `I.realFuncs[classe][metodo]` | metodi veri; valgono per la classe e le sue figlie (BasePart vale per Part, Seat...) |
| `I.realProps[classe][prop]` | proprieta' in lettura: `function(obj) return valore end` |
| `I.Signal.new()` | un evento Luau (`:Connect`, `:Fire`, `:Wait`) |
| `I.isLuaInstance(v)`, `I.store[v]` | le Instance fatte in Luau e i loro dati |

I moduli girano in ordine di nome, dopo che `game`, `workspace`, `Enum` e lo strato
"presente" sono pronti. Un errore in un modulo si scrive in Output e non ferma gli altri.

## Da dove li legge il motore
- **In sviluppo** (programma avviato da `build/bin`, oppure `BLOCKY_LUA_DIR=<cartella>`):
  dal disco, a ogni Play. In Output: "API Luau lette dal disco (sviluppo)".
- **Release e APK**: dalla copia incorporata nel programma, che CMake rigenera da questi
  file a ogni build (`cmake/BlockyEmbedLua.cmake`).

Quando serve il C++: solo per dare al Luau qualcosa che non ha (leggere o scrivere un
dato del motore, disegnare, fisica). Tutto il resto conviene scriverlo qui.
