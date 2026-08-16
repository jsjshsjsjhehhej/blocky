# Blocky — engine C++ multi-piattaforma

Un solo codebase. Le ottimizzazioni cambiano per **profilo piattaforma**.

## Target supportati

| Target | API grafica | Note |
|--------|-------------|------|
| **PC** | OpenGL 3.3 / Vulkan | Build attuale (Windows/Linux/macOS) |
| **Android** | OpenGL ES / Vulkan | Profilo Mobile + NDK |
| **iOS** | Metal | Richiede Xcode; profilo qualità pronto |
| **PS5** | AGC/GNM | Richiede Sony Prospero SDK (NDA) |

## Build rapida (PC)

```bash
cmake --preset pc-release
cmake --build --preset pc-release
```

Esegui: `build/bin/Blocky.exe`

Simula profilo mobile sul desktop:

```bash
cmake --preset mobile-gles
cmake --build --preset mobile-gles
```

## Branding

- Nome: **Blocky**
- Icona: lettera **B** (`assets/icon/b_icon.png`, `app.ico`)
