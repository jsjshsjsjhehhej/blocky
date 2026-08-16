#pragma once

#include <glad/gl.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "stb_image.h"

/** Carica PNG/JPG in una texture OpenGL (repeat, linear). */
struct Texture2D {
    GLuint id = 0;
    int w = 0, h = 0;

    bool load(const std::string& path, bool generateMips = false) {
        stbi_set_flip_vertically_on_load(true);
        unsigned char* px = stbi_load(path.c_str(), &w, &h, nullptr, 4);
        if (!px) {
            std::fprintf(stderr, "Texture2D: impossibile caricare '%s'\n", path.c_str());
            return false;
        }
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        if (generateMips) {
            // mipmap = meno moiré/scintillio sugli stud in lontananza
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // anisotropia se disponibile
            GLfloat aniso = 0.f;
            glGetFloatv(0x84FF /*GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT*/, &aniso);
            if (aniso > 1.f)
                glTexParameterf(GL_TEXTURE_2D, 0x84FE /*GL_TEXTURE_MAX_ANISOTROPY_EXT*/,
                                std::min(aniso, 8.f));
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        stbi_image_free(px);
        return true;
    }

    void bind(int unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, id);
    }

    void destroy() {
        if (id) glDeleteTextures(1, &id);
        id = 0;
    }
};

inline std::string findStudTexture() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string p(buf);
    auto s = p.find_last_of("\\/");
    std::string dir = s == std::string::npos ? "." : p.substr(0, s);
    std::vector<std::string> paths = {
        dir + "/assets/textures/stud.png",
        "assets/textures/stud.png",
    };
#else
    std::vector<std::string> paths = { "assets/textures/stud.png" };
#endif
    for (const auto& c : paths) {
        if (FILE* f = std::fopen(c.c_str(), "rb")) { std::fclose(f); return c; }
    }
    return paths.front();
}
