#pragma once

/**
 * GfxCore — Graphics Abstraction Layer di Blocky, stesso ruolo del modulo
 * omonimo di Roblox: il codice che disegna non deve MAI sapere se sotto c'e'
 * OpenGL, DirectX o Vulkan. Lo sa solo il backend.
 *
 * Questo header e' deliberatamente puro: nessun #include di glad/GL/D3D.
 * Chi lo include vede solo handle opachi e un'interfaccia astratta — e' la
 * regola che tiene in piedi tutta l'idea: se qui dentro entra un GLuint,
 * il layer non serve piu' a niente.
 *
 * Stato: prima fetta. Oggi copre solo le texture 2D (vedi Texture2D.h, che
 * ora ci passa attraverso) e l'unico backend implementato e' OpenGL
 * (gfx/GlDevice.h). Mesh, shader e draw call sono ancora chiamate GL dirette
 * sparse nel motore: si spostano qui una famiglia alla volta, ognuna
 * verificabile da sola.
 */

namespace blk::gfx {

/**
 * Riferimento opaco a una texture sulla GPU. `backend` e' il numero che
 * conta per il backend attivo (un GLuint su OpenGL, un indice a una
 * ShaderResourceView su D3D11): chi disegna non deve interpretarlo, solo
 * ripassarlo al device.
 */
struct TextureHandle {
    unsigned int backend = 0;
    int width = 0;
    int height = 0;

    bool valid() const { return backend != 0; }
};

struct TextureDesc {
    const unsigned char* pixels = nullptr; // RGBA8, width*height*4 byte
    int width = 0;
    int height = 0;
    bool mipmaps = false;
};

class Device {
public:
    virtual ~Device() = default;

    /** Nome dell'API attiva ("OpenGL", "DirectX 11", ...) — per log e UI. */
    virtual const char* apiName() const = 0;

    virtual bool createTexture2D(const TextureDesc& desc, TextureHandle& out) = 0;
    virtual void bindTexture2D(const TextureHandle& tex, int unit) = 0;
    virtual void destroyTexture2D(TextureHandle& tex) = 0;
};

/**
 * Backend attivo. Uno solo per processo, scelto all'avvio — cambiarlo a
 * gioco avviato non e' previsto (ne' possibile: finestra e risorse GPU
 * appartengono all'API, e' il motivo per cui anche Roblox chiede di
 * riavviare quando cambi "Modalita' grafica").
 */
inline Device*& deviceSlot() {
    static Device* d = nullptr;
    return d;
}

inline bool hasDevice() { return deviceSlot() != nullptr; }
inline void setDevice(Device* d) { deviceSlot() = d; }
inline Device& device() { return *deviceSlot(); }

} // namespace blk::gfx
