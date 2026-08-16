#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

enum class GameScreen {
    Menu,
    Loading,
    Playing,
    Shop
};

enum class PlantType {
    None,
    Carrot,
    Tomato
};

struct ShopItem {
    PlantType plant;
    std::string name;
    int price;
    glm::vec3 previewColor;
};

struct Plot {
    glm::vec3 center;
    PlantType type = PlantType::None;
    float growth = 0.0f; // 0..1, 1 = pronto al raccolto
};

struct Player {
    glm::vec3 position{ 0.0f, 0.0f, 8.0f };
    float yawDeg = -180.0f;
    float moveSpeed = 8.0f;
    int coins = 50;
    int seedsCarrot = 1;
    int seedsTomato = 0;
};

inline float plantGrowSeconds(PlantType type) {
    switch (type) {
        case PlantType::Carrot: return 8.0f;
        case PlantType::Tomato: return 14.0f;
        default: return 1.0f;
    }
}

inline int plantSellValue(PlantType type) {
    switch (type) {
        case PlantType::Carrot: return 15;
        case PlantType::Tomato: return 35;
        default: return 0;
    }
}

inline glm::vec3 plantColor(PlantType type, float growth) {
    switch (type) {
        case PlantType::Carrot:
            return glm::mix(glm::vec3(0.18f, 0.55f, 0.18f), glm::vec3(1.0f, 0.55f, 0.1f), growth);
        case PlantType::Tomato:
            return glm::mix(glm::vec3(0.15f, 0.45f, 0.15f), glm::vec3(0.85f, 0.15f, 0.12f), growth);
        default:
            return glm::vec3(0.4f);
    }
}

inline std::vector<Plot> makeGardenPlots() {
    std::vector<Plot> plots;
    for (int z = 0; z < 4; ++z) {
        for (int x = 0; x < 4; ++x) {
            Plot p;
            p.center = glm::vec3(-10.5f + x * 7.0f, 0.0f, -10.5f + z * 7.0f);
            plots.push_back(p);
        }
    }
    return plots;
}

inline std::vector<ShopItem> defaultShop() {
    return {
        { PlantType::Carrot, "Semi Carota", 10, { 1.0f, 0.55f, 0.1f } },
        { PlantType::Tomato, "Semi Pomodoro", 25, { 0.85f, 0.15f, 0.12f } },
    };
}
