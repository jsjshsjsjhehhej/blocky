# Blocky Engine

Game engine professionale in C++ per creare e giocare **solo su Blocky**.

I giochi fatti con Blocky Game Engine girano su **Blocky.exe** — non c’è export verso Roblox, Unity o altri runtime.

## Prodotti

| Eseguibile | Ruolo |
|------------|--------|
| **Blocky.exe** | Hub + Play (Games / Shop / Chat) |
| **BlockyGameEngine.exe** | Studio: costruisci, scripta, pubblica |
| **BlockyServer.exe** | Server dedicato multiplayer LAN |

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
cmake --build build --target Roblox RobloxStudio BlockyServer
```

Output in `build/bin/`:
- `Blocky.exe`
- `BlockyGameEngine.exe`
- `BlockyServer.exe`

## Roadmap engine pro (solo Blocky)

1. **Formato progetto + game package** — fatto (base)
2. **Separare runtime / editor** — libreria condivisa, meno `main.cpp` monolitico
3. **Asset pipeline** — import → cache cooked (mesh/texture)
4. **API Lua v1** — Touched, Humanoid, Remotes LAN
5. **Editor pro** — Explorer, undo/redo, prefab
6. **Server autoritativo** — tick fisso + replica mondo

## Controlli Play
- WASD muovi · Space salta · Shift Lock · Esc menu
- Pad Xbox / DualSense supportati

## Controlli Studio
- Mouse orbit · tools Select/Move/Scale/Rotate
- Pubblica su Blocky Games (pacchetto esclusivo Blocky)
