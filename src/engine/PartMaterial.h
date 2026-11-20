#pragma once

#include <algorithm>
#include <cstring>
#include <string>

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
    // Texture reali (PNG in assets/textures/materials/)
    Fabric,
    Marble,
    Sand,
    Slate,
    DiamondPlate,
    CorrodedMetal,
    Granite,
    Asphalt,
    Water,
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
    case PartMaterial::Fabric: return "Fabric";
    case PartMaterial::Marble: return "Marble";
    case PartMaterial::Sand: return "Sand";
    case PartMaterial::Slate: return "Slate";
    case PartMaterial::DiamondPlate: return "DiamondPlate";
    case PartMaterial::CorrodedMetal: return "CorrodedMetal";
    case PartMaterial::Granite: return "Granite";
    case PartMaterial::Asphalt: return "Asphalt";
    case PartMaterial::Water: return "Water";
    default: return "Plastic";
    }
}

/** Nome file texture (senza estensione) sotto assets/textures/materials/. */
inline const char* partMaterialTextureStem(PartMaterial m) {
    switch (m) {
    case PartMaterial::Wood: return "wood";
    case PartMaterial::Metal: return "metal";
    case PartMaterial::Concrete: return "concrete";
    case PartMaterial::Grass: return "grass";
    case PartMaterial::Brick: return "brick";
    case PartMaterial::Fabric: return "fabric";
    case PartMaterial::Marble: return "marble";
    case PartMaterial::Sand: return "sand";
    case PartMaterial::Slate: return "slate";
    case PartMaterial::DiamondPlate: return "diamondplate";
    case PartMaterial::CorrodedMetal: return "corrodedmetal";
    case PartMaterial::Granite: return "granite";
    case PartMaterial::Asphalt: return "asphalt";
    case PartMaterial::Water: return nullptr;
    default: return nullptr;
    }
}

inline PartMaterial partMaterialFromName(const std::string& name) {
    std::string s = name;
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    s.erase(std::remove_if(s.begin(), s.end(),
                            [](char c) { return c == ' ' || c == '_' || c == '-'; }),
            s.end());
    if (s == "neon") return PartMaterial::Neon;
    if (s == "metal") return PartMaterial::Metal;
    if (s == "diamondplate" || s == "diamond") return PartMaterial::DiamondPlate;
    if (s == "corrodedmetal" || s == "corroded" || s == "rust") return PartMaterial::CorrodedMetal;
    if (s == "wood" || s == "woodplanks") return PartMaterial::Wood;
    if (s == "concrete" || s == "cobblestone") return PartMaterial::Concrete;
    if (s == "slate") return PartMaterial::Slate;
    if (s == "grass" || s == "ground") return PartMaterial::Grass;
    if (s == "sand") return PartMaterial::Sand;
    if (s == "glass") return PartMaterial::Glass;
    if (s == "ice") return PartMaterial::Ice;
    if (s == "brick") return PartMaterial::Brick;
    if (s == "forcefield" || s == "force") return PartMaterial::ForceField;
    if (s == "smoothplastic" || s == "smooth") return PartMaterial::SmoothPlastic;
    if (s == "fabric" || s == "cloth") return PartMaterial::Fabric;
    if (s == "marble") return PartMaterial::Marble;
    if (s == "granite") return PartMaterial::Granite;
    if (s == "asphalt" || s == "pavement" || s == "road") return PartMaterial::Asphalt;
    if (s == "water") return PartMaterial::Water;
    // Materiali di Roblox che Blocky non ha: il piu' simile, invece della plastica.
    if (s == "sandstone" || s == "salt" || s == "mud") return PartMaterial::Sand;
    if (s == "pebble" || s == "basalt" || s == "rock" || s == "crackedlava" || s == "cobble") return PartMaterial::Slate;
    if (s == "limestone" || s == "plaster" || s == "pavement2") return PartMaterial::Concrete;
    if (s == "leafygrass" || s == "moss") return PartMaterial::Grass;
    if (s == "snow" || s == "glacier") return PartMaterial::Ice;
    if (s == "carpet" || s == "cardboard" || s == "leather") return PartMaterial::Fabric;
    if (s == "ceramictiles") return PartMaterial::Marble;
    if (s == "claytiles" || s == "roofshingles") return PartMaterial::Brick;
    if (s == "foil" || s == "aluminum" || s == "steel") return PartMaterial::Metal;
    if (s == "rubber" || s == "cardboardsmooth") return PartMaterial::SmoothPlastic;
    return PartMaterial::Plastic;
}

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
    case PartMaterial::DiamondPlate:
        specular = 0.95f;
        roughness = 0.18f;
        break;
    case PartMaterial::CorrodedMetal:
        specular = 0.35f;
        roughness = 0.72f;
        break;
    case PartMaterial::Wood:
    case PartMaterial::Fabric:
        specular = 0.08f;
        roughness = 0.85f;
        break;
    case PartMaterial::Concrete:
    case PartMaterial::Asphalt:
    case PartMaterial::Slate:
        specular = 0.06f;
        roughness = 0.92f;
        break;
    case PartMaterial::Grass:
    case PartMaterial::Sand:
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
    case PartMaterial::Granite:
        specular = 0.1f;
        roughness = 0.88f;
        break;
    case PartMaterial::Marble:
        specular = 0.55f;
        roughness = 0.28f;
        break;
    case PartMaterial::Water:
        specular = 0.8f;
        roughness = 0.08f;
        alpha = 0.42f;
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
