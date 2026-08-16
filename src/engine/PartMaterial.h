#pragma once

#include <algorithm>
#include <cstring>
#include <string>

/** Materiali Part stile Roblox (semplificati). */
enum class PartMaterial : int {
    Plastic = 0,
    SmoothPlastic,
    Neon,
    Metal,
    Wood,
    Concrete,
    Grass,
    Glass,
    Ice,
    Brick,
    ForceField,
    Count
};

inline const char* partMaterialName(PartMaterial m) {
    switch (m) {
    case PartMaterial::Plastic: return "Plastic";
    case PartMaterial::SmoothPlastic: return "SmoothPlastic";
    case PartMaterial::Neon: return "Neon";
    case PartMaterial::Metal: return "Metal";
    case PartMaterial::Wood: return "Wood";
    case PartMaterial::Concrete: return "Concrete";
    case PartMaterial::Grass: return "Grass";
    case PartMaterial::Glass: return "Glass";
    case PartMaterial::Ice: return "Ice";
    case PartMaterial::Brick: return "Brick";
    case PartMaterial::ForceField: return "ForceField";
    default: return "Plastic";
    }
}

inline PartMaterial partMaterialFromName(const std::string& name) {
    std::string s = name;
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    // rimuovi spazi / underscore
    s.erase(std::remove_if(s.begin(), s.end(),
                            [](char c) { return c == ' ' || c == '_' || c == '-'; }),
            s.end());
    if (s == "neon") return PartMaterial::Neon;
    if (s == "metal" || s == "diamondplate" || s == "corrodedmetal") return PartMaterial::Metal;
    if (s == "wood" || s == "woodplanks") return PartMaterial::Wood;
    if (s == "concrete" || s == "slate" || s == "cobblestone") return PartMaterial::Concrete;
    if (s == "grass" || s == "ground" || s == "sand") return PartMaterial::Grass;
    if (s == "glass") return PartMaterial::Glass;
    if (s == "ice") return PartMaterial::Ice;
    if (s == "brick") return PartMaterial::Brick;
    if (s == "forcefield" || s == "force") return PartMaterial::ForceField;
    if (s == "smoothplastic" || s == "smooth") return PartMaterial::SmoothPlastic;
    return PartMaterial::Plastic;
}

/** Parametri shading per materiale → shader (specular, roughness, emissive bias, alpha). */
inline void partMaterialShading(PartMaterial m, float& specular, float& roughness,
                                float& emissiveBias, float& alpha) {
    specular = 0.12f;
    roughness = 0.75f;
    emissiveBias = 0.f;
    alpha = 1.f;
    switch (m) {
    case PartMaterial::SmoothPlastic:
        specular = 0.35f;
        roughness = 0.35f;
        break;
    case PartMaterial::Neon:
        specular = 0.05f;
        roughness = 0.9f;
        emissiveBias = 1.f;
        break;
    case PartMaterial::Metal:
        specular = 0.95f;
        roughness = 0.18f;
        break;
    case PartMaterial::Wood:
        specular = 0.08f;
        roughness = 0.85f;
        break;
    case PartMaterial::Concrete:
        specular = 0.06f;
        roughness = 0.92f;
        break;
    case PartMaterial::Grass:
        specular = 0.04f;
        roughness = 0.95f;
        break;
    case PartMaterial::Glass:
        specular = 0.85f;
        roughness = 0.12f;
        alpha = 0.38f;
        break;
    case PartMaterial::Ice:
        specular = 0.75f;
        roughness = 0.2f;
        alpha = 0.55f;
        break;
    case PartMaterial::Brick:
        specular = 0.1f;
        roughness = 0.88f;
        break;
    case PartMaterial::ForceField:
        specular = 0.6f;
        roughness = 0.25f;
        emissiveBias = 0.55f;
        alpha = 0.55f;
        break;
    default:
        break;
    }
}
