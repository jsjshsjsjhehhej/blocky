# Blocky Engine — visione professionale

## Principio
Blocky è un **game engine completo** e un **runtime chiuso**:
- si crea con **Blocky Game Engine**
- si gioca solo con **Blocky.exe**
- niente export Roblox / Unity / Unreal

## Stack attuale
- C++17, OpenGL 3.3 (Vulkan opzionale)
- Lua 5.4 sandboxed (`LuaRuntime`)
- Place JSON `BlockyPlace`
- Project `BlockyProject` + package `BlockyGame`
- Hub, Shop, Chat, avatar, LAN multiplayer

## Prossimi pilastri
1. Core library (`blocky_runtime`) condivisa da Player e Studio
2. Asset cook (GUID + blob cache)
3. Script API gameplay (Touched, Humanoid, RemoteEvent)
4. Editor: Explorer tree + undo stack
5. Net authority (host/server tick)

## File chiave
- `src/engine/BlockyProject.h` — progetti e package
- `src/engine/GameHub.h` — hub / places
- `src/engine/LuaRuntime.h` — scripting
- `src/main.cpp` — Player + Studio (da spezzare)
