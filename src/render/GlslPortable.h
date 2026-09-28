#pragma once

/**
 * Gli shader del motore sono scritti in GLSL 3.30 core (OpenGL desktop).
 * GLES accetta al massimo "320 es" e rifiuta l'intero shader alla prima riga:
 * su Android tutto — mondo, bloom, particelle, UI — restava nero.
 *
 * Invece di mantenere due copie di ogni shader (che divergerebbero al primo
 * ritocco), qui si riscrive la sola riga #version e si aggiungono le
 * precisioni di default che GLES richiede e il desktop no. Il corpo dello
 * shader resta identico: la sintassi che il motore usa (layout(location=),
 * in/out, texture()) e' valida in entrambi.
 *
 * Su desktop la funzione non fa niente: ritorna la stringa com'e'.
 */

#include <string>

inline std::string glslForTarget(const char* src) {
    if (!src) return {};

#ifndef __ANDROID__
    return std::string(src);
#else
    std::string s(src);

    // GLES vuole una precisione di default dichiarata; senza, il fragment
    // shader non compila. sampler2DShadow non ne ha una nemmeno implicita.
    static const char* kPrecision =
        "\nprecision highp float;\n"
        "precision highp int;\n"
        "precision highp sampler2D;\n"
        "precision highp sampler2DShadow;\n";

    const size_t v = s.find("#version");
    if (v == std::string::npos)
        return std::string("#version 320 es") + kPrecision + s;

    size_t eol = s.find('\n', v);
    if (eol == std::string::npos) eol = s.size();
    s.replace(v, eol - v, "#version 320 es");

    const size_t insertAt = s.find('\n', v);
    if (insertAt != std::string::npos)
        s.insert(insertAt + 1, kPrecision + 1);   // +1: la riga nuova la mette gia' il \n
    return s;
#endif
}
