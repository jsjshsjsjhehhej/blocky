#pragma once

/**
 * Backend OpenGL di GfxCore — l'unico implementato oggi. Le chiamate GL qui
 * dentro sono le STESSE di prima, spostate parola per parola da
 * render/Texture2D.h: nessun cambiamento di comportamento, solo di posto.
 *
 * Questo e' l'unico file (per le texture) che sa cosa sia un GLuint: e'
 * esattamente lo scopo dell'esercizio — quando un domani ci sara' un
 * D3d11Device.h, sara' il suo gemello, e nient'altro nel motore dovra'
 * cambiare.
 */

#include <glad/gl.h>

#include <algorithm>

#include "gfx/GfxCore.h"

namespace blk::gfx {

class GlDevice final : public Device {
public:
    const char* apiName() const override { return "OpenGL"; }

    bool createTexture2D(const TextureDesc& desc, TextureHandle& out) override {
        if (!desc.pixels || desc.width <= 0 || desc.height <= 0) return false;

        GLuint id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, desc.width, desc.height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, desc.pixels);
        if (desc.mipmaps) {
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            GLfloat aniso = 0.f;
            glGetFloatv(0x84FF, &aniso);   // GL_MAX_TEXTURE_MAX_ANISOTROPY
            if (aniso > 1.f)
                glTexParameterf(GL_TEXTURE_2D, 0x84FE, std::min(aniso, 8.f)); // GL_TEXTURE_MAX_ANISOTROPY
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        out.backend = id;
        out.width = desc.width;
        out.height = desc.height;
        return true;
    }

    void bindTexture2D(const TextureHandle& tex, int unit) override {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, tex.backend);
    }

    void destroyTexture2D(TextureHandle& tex) override {
        if (tex.backend) glDeleteTextures(1, &tex.backend);
        tex.backend = 0;
        tex.width = tex.height = 0;
    }
};

inline GlDevice& glDevice() {
    static GlDevice d;
    return d;
}

/** Installa OpenGL come backend attivo se nessuno l'ha ancora scelto. */
inline void installGlDeviceIfNeeded() {
    if (!hasDevice()) setDevice(&glDevice());
}

} // namespace blk::gfx
