#pragma once

#include <glad/gl.h>
#include "platform/Platform.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "stb_image.h"

#include "gfx/GfxCore.h"
#include "gfx/GlDevice.h"

/**
 * Le chiamate GL di questa struct sono state spostate in gfx/GlDevice.h:
 * qui resta solo il caricamento del file (stb_image, uguale a prima) e la
 * chiamata al backend attivo di GfxCore. Prima fetta del layer grafico.
 *
 * `id` resta un GLuint pubblico perche' decine di punti del motore lo
 * passano ancora direttamente agli shader (es. `drawCube(..., studTex.id)`):
 * e' un ponte temporaneo verso l'handle vero (`handle.backend`), che sparira'
 * quando anche quelle firme passeranno a gfx::TextureHandle.
 */
struct Texture2D {
    blk::gfx::TextureHandle handle;
    GLuint id = 0;
    int w = 0, h = 0;

    bool load(const std::string& path, bool generateMips = false) {
        blk::gfx::installGlDeviceIfNeeded();

        stbi_set_flip_vertically_on_load(true);
        unsigned char* px = stbi_load(path.c_str(), &w, &h, nullptr, 4);
        if (!px) {
            std::fprintf(stderr, "Texture2D: impossibile caricare '%s'\n", path.c_str());
            return false;
        }

        blk::gfx::TextureDesc desc;
        desc.pixels = px;
        desc.width = w;
        desc.height = h;
        desc.mipmaps = generateMips;
        const bool ok = blk::gfx::device().createTexture2D(desc, handle);
        stbi_image_free(px);
        if (!ok) {
            std::fprintf(stderr, "Texture2D: creazione GPU fallita per '%s'\n", path.c_str());
            return false;
        }
        id = (GLuint)handle.backend;
        return true;
    }

    /**
     * Come load, ma la texture viene ridotta a side x side (media dei pixel che
     * ogni pixel nuovo copre). Per gli stud della qualita' Low: stesso disegno,
     * molta meno memoria da leggere per ogni pixel dello schermo.
     */
    bool loadDownscaled(const std::string& path, int side) {
        blk::gfx::installGlDeviceIfNeeded();
        stbi_set_flip_vertically_on_load(true);
        int sw = 0, sh = 0;
        unsigned char* px = stbi_load(path.c_str(), &sw, &sh, nullptr, 4);
        if (!px) {
            std::fprintf(stderr, "Texture2D: impossibile caricare '%s'\n", path.c_str());
            return false;
        }
        side = std::max(1, std::min(side, std::min(sw, sh)));
        std::vector<unsigned char> out((size_t)side * side * 4);
        for (int y = 0; y < side; ++y)
            for (int x = 0; x < side; ++x) {
                const int x0 = x * sw / side, x1 = std::max(x0 + 1, (x + 1) * sw / side);
                const int y0 = y * sh / side, y1 = std::max(y0 + 1, (y + 1) * sh / side);
                unsigned sum[4] = {0, 0, 0, 0};
                for (int yy = y0; yy < y1; ++yy)
                    for (int xx = x0; xx < x1; ++xx)
                        for (int c = 0; c < 4; ++c) sum[c] += px[((size_t)yy * sw + xx) * 4 + c];
                const unsigned n = (unsigned)((x1 - x0) * (y1 - y0));
                for (int c = 0; c < 4; ++c) out[((size_t)y * side + x) * 4 + c] = (unsigned char)(sum[c] / n);
            }
        stbi_image_free(px);
        blk::gfx::TextureDesc desc;
        desc.pixels = out.data();
        desc.width = side;
        desc.height = side;
        desc.mipmaps = false;
        if (!blk::gfx::device().createTexture2D(desc, handle)) return false;
        w = h = side;
        id = (GLuint)handle.backend;
        return true;
    }

    void bind(int unit = 0) const {
        if (blk::gfx::hasDevice()) blk::gfx::device().bindTexture2D(handle, unit);
    }

    void destroy() {
        if (blk::gfx::hasDevice()) blk::gfx::device().destroyTexture2D(handle);
        id = 0;
    }
};

// Radice + percorso relativo, con ripiego sulla cartella di lavoro.
//
// Prima queste funzioni ricostruivano a mano la cartella dell'eseguibile con
// GetModuleFileNameA, e fuori da Windows restava il solo percorso relativo —
// che funziona unicamente se la cartella di lavoro e' quella giusta. Su
// Android non lo e' mai (la cwd e' "/"), ed e' per questo che stud.png non si
// trovava pur essendo sul disco. plat::selfDir() sa gia' rispondere su tutte
// le piattaforme, Android compreso, dove punta alla memoria privata dell'app
// in cui gli asset dell'APK vengono estratti all'avvio.
inline std::string findAssetFile(const std::string& rel) {
    const std::vector<std::string> paths = { plat::selfDir() + "/" + rel, rel };
    for (const auto& c : paths) {
        if (FILE* f = std::fopen(c.c_str(), "rb")) { std::fclose(f); return c; }
    }
    return paths.front();
}

inline std::string findStudTexture() {
    return findAssetFile("assets/textures/stud.png");
}

// Texture reale per un materiale: preferisce .png (senza fondo), poi .jpg.
inline std::string findMaterialTexture(const std::string& name) {
    for (const char* ext : {".png", ".jpg", ".jpeg"}) {
        const std::string rel = "assets/textures/materials/" + name + ext;
        const std::string path = findAssetFile(rel);
        if (FILE* f = std::fopen(path.c_str(), "rb")) {
            std::fclose(f);
            return path;
        }
    }
    return findAssetFile("assets/textures/materials/" + name + ".png");
}
