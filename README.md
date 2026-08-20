# Blocky Engine

Game engine professionale in C++ per creare e giocare **solo su Blocky**.

I giochi fatti con Blocky Game Engine girano su **Blocky.exe** — non c’è export verso Roblox, Unity o altri runtime.

## Prodotti

L'engine vive in **`libblocky.dll`**: rendering, ImGui, Lua, netcode client.
I tre eseguibili sotto sono lanciatori minimi (poche decine di KB) che la
caricano a runtime e chiamano l'export giusto — vedi
[`src/engine/BlockyApi.h`](src/engine/BlockyApi.h).

| Eseguibile | Ruolo | Export chiamato |
|------------|--------|--------|
| **Blocky.exe** | Hub + Play (Games / Shop / Chat) | `Blocky_Run` |
| **BlockyGameEngine.exe** | Studio classico: costruisci, scripta, pubblica | `Blocky_RunStudio` |
| **BlockyStudio.exe** | Editor place separato (ribbon, viewport dedicata) | `Blocky_RunEditor` |

Server, in [`src/blocky_servers/`](src/blocky_servers/):

| Eseguibile | Ruolo |
|------------|--------|
| **BlockyServer.exe** | Server dedicato multiplayer LAN (relay di stato) |
| **blocky_hostd.exe** | Hosting HTTP: riceve un `.zip` pubblicato, lo valida e lo serve |

## Formato professionale

### Progetto Studio
```
MyGame/
  project.blocky.json
  places/main.place.json
  scripts/main.lua
  assets/...
```

### Gioco pubblicato (hub)
```
%APPDATA%/Blocky/games/<id>/
  game.blocky.json
  place.json
  scripts/
  assets/
```

`blockyExclusive: true` e `runtime: "Blocky"` sono obbligatori nei manifest.

## Compilare (Windows)

```powershell
cmake -S . -B build -G Ninja
cmake --build build --target blocky BlockyLauncher BlockyStudioLauncher BlockyEditorLauncher BlockyServer blocky_hostd
```

Output in `build/bin/`:
- `libblocky.dll` — l'engine, deve stare accanto a ogni lanciatore
- `Blocky.exe`, `BlockyGameEngine.exe`, `BlockyStudio.exe`
- `BlockyServer.exe`, `blocky_hostd.exe`

## Roadmap engine pro (solo Blocky)

1. **Formato progetto + game package** — fatto (base)
2. **Libreria condivisa** — fatto: `main.cpp` e `studio_main.cpp` vivono in
   `libblocky.dll`, tre lanciatori minimi la caricano. **Non fatto**: `main.cpp`
   resta un unico file da ~9800 righe — la dll condivisa toglie la
   duplicazione fra eseguibili, ma non lo spezza internamente.
3. **Asset pipeline** — `game_index.json` (mesh/collider/materiali/hash),
   `blocky_hostd` lo serve via HTTP. Cache cooked di mesh/texture non fatta.
4. **API Lua v1** — Touched, Humanoid, Remotes LAN
5. **Editor pro** — Explorer, undo/redo, prefab
6. **Server autoritativo** — tick fisso + replica mondo

## Controlli Play
- WASD muovi · Space salta · Shift Lock · Esc menu
- Pad Xbox / DualSense supportati

## Controlli Studio
- Mouse orbit · tools Select/Move/Scale/Rotate
- Pubblica su Blocky Games (pacchetto esclusivo Blocky)
