#pragma once

#include <cstring>
#include <string>

enum class PartShape : int {
    Block = 0,
    Ball = 1,
    Cylinder = 2,
    Wedge = 3,
    Count
};


/**
 * UNA tabella sola per nome, dimensione e colore di default di ogni forma.
 *
 * Prima queste tre cose vivevano ripetute a mano in file diversi (il nome
 * qui, la dimensione in WorldEditingTools.h, il colore anche li'): aggiungere
 * una forma nuova significava ricordarsi di tenerle in sincronia a memoria,
 * la stessa classe di bug della lista dei giochi built-in duplicata in tre
 * posti. Ora chi aggiunge una forma tocca UNA riga qui, e tutto il resto la
 * legge da qui.
 */
struct PartShapeInfo {
    const char* name;
    float sizeX, sizeY, sizeZ;
    float r, g, b;
};

inline const PartShapeInfo& partShapeInfo(PartShape s) {
    static constexpr PartShapeInfo kInfo[(int)PartShape::Count] = {
        /* Block    */ {"Part",     4.f, 1.f, 4.f,  0.64f, 0.64f, 0.64f},
        /* Ball     */ {"Ball",     4.f, 4.f, 4.f,  0.95f, 0.45f, 0.35f},
        /* Cylinder */ {"Cylinder", 2.f, 5.f, 2.f,  0.35f, 0.65f, 0.95f},
        /* Wedge    */ {"Wedge",    4.f, 2.f, 6.f,  0.45f, 0.85f, 0.40f},
    };
    const int i = (int)s;
    return (i >= 0 && i < (int)PartShape::Count) ? kInfo[i] : kInfo[0];
}

inline const char* partShapeName(PartShape s) { return partShapeInfo(s).name; }

inline PartShape partShapeFromName(const std::string& name) {
    if (name.empty()) return PartShape::Block;
    auto eq = [&](const char* a) {
        if (name.size() != std::strlen(a)) return false;
        for (size_t i = 0; i < name.size(); ++i) {
            char c = name[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            char d = a[i];
            if (d >= 'A' && d <= 'Z') d = (char)(d - 'A' + 'a');
            if (c != d) return false;
        }
        return true;
    };
    if (eq("ball") || eq("sphere") || eq("sfera")) return PartShape::Ball;
    if (eq("cylinder") || eq("cilindro")) return PartShape::Cylinder;
    if (eq("wedge") || eq("triangle") || eq("triangolo") || eq("ramp")) return PartShape::Wedge;
    return PartShape::Block;
}
