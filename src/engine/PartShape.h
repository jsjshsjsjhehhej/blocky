#pragma once

#include <cstring>
#include <string>

/** Forme Part stile Roblox (Enum.PartType). */
enum class PartShape : int {
    Block = 0,
    Ball = 1,      // sfera
    Cylinder = 2,  // cilindro (asse Y)
    Wedge = 3,     // triangolo / rampa
    Count
};

inline const char* partShapeName(PartShape s) {
    switch (s) {
    case PartShape::Ball: return "Ball";
    case PartShape::Cylinder: return "Cylinder";
    case PartShape::Wedge: return "Wedge";
    default: return "Block";
    }
}

inline PartShape partShapeFromName(const std::string& name) {
    if (name.empty()) return PartShape::Block;
    // case-insensitive short compare
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
