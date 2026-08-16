#pragma once

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "DataModel.h"
#include "GameHub.h"
#include "Humanoid.h"
#include "LightingService.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

/** Esporta place + Humanoid in JSON (formato Blocky). */
class PlaceExporter {
public:
    static std::string exportPlace(const DataModel& dm, const Humanoid& humanoid,
                                   const glm::vec3& playerPos, float playerYaw,
                                   const LightingService* lighting = nullptr) {
        std::ostringstream j;
        j << std::fixed << std::setprecision(3);
        j << "{\n";
        j << "  \"format\": \"BlockyPlace\",\n";
        j << "  \"version\": 1,\n";
        j << "  \"placeName\": \"" << dm.placeName << "\",\n";
        if (lighting) {
            j << "  \"lighting\": {\"dayNight\":" << (int)lighting->dayNight
              << ",\"dayLengthMinutes\":" << lighting->dayLengthMinutes
              << ",\"fixedDayClock\":" << lighting->fixedDayClock
              << ",\"fixedNightClock\":" << lighting->fixedNightClock
              << ",\"clockTime\":" << lighting->clockTime
              << ",\"brightness\":" << lighting->brightness
              << ",\"ambient\":" << lighting->ambient
              << ",\"globalShadows\":" << (lighting->globalShadows ? "true" : "false")
              << "},\n";
        }
        j << "  \"spawn\": {\"pos\":[" << dm.spawn.position.x << "," << dm.spawn.position.y
          << "," << dm.spawn.position.z << "],\"x\":" << dm.spawn.position.x
          << ",\"y\":" << dm.spawn.position.y << ",\"z\":" << dm.spawn.position.z
          << ",\"yaw\":" << dm.spawn.yaw << "},\n";
        j << "  \"player\": {\"x\":" << playerPos.x << ",\"y\":" << playerPos.y
          << ",\"z\":" << playerPos.z << ",\"yaw\":" << playerYaw << "},\n";
        j << "  \"humanoid\": {\"health\":" << humanoid.health << ",\"maxHealth\":" << humanoid.maxHealth
          << ",\"walkSpeed\":" << humanoid.walkSpeed << ",\"jumpPower\":" << humanoid.jumpPower << "},\n";
        j << "  \"baseplate\": {\"enabled\":" << (dm.hasBaseplate ? "true" : "false")
          << ",\"size\":[" << dm.baseplateSize.x << "," << dm.baseplateSize.y << "," << dm.baseplateSize.z << "]"
          << ",\"color\":[" << dm.baseplateColor.r << "," << dm.baseplateColor.g << "," << dm.baseplateColor.b << "]},\n";
        j << "  \"parts\": [\n";
        for (size_t i = 0; i < dm.parts.size(); ++i) {
            const auto& p = dm.parts[i];
            j << "    {\"name\":\"" << p.name << "\",\"pos\":[" << p.position.x << "," << p.position.y << "," << p.position.z
              << "],\"size\":[" << p.size.x << "," << p.size.y << "," << p.size.z
              << "],\"rot\":[" << p.rotation.x << "," << p.rotation.y << "," << p.rotation.z
              << "],\"color\":[" << p.color.r << "," << p.color.g << "," << p.color.b
              << "],\"anchored\":" << (p.anchored ? "true" : "false")
              << ",\"canCollide\":" << (p.canCollide ? "true" : "false")
              << ",\"shape\":\"" << partShapeName(p.shape) << "\""
              << ",\"material\":\"" << partMaterialName(p.material) << "\""
              << ",\"emissive\":" << p.emissive
              << ",\"pointLight\":" << (p.pointLight ? "true" : "false")
              << ",\"lightRange\":" << p.lightRange
              << ",\"lightBrightness\":" << p.lightBrightness
              << ",\"canDestroy\":" << (p.canDestroy ? "true" : "false")
              << ",\"health\":" << p.health
              << ",\"maxHealth\":" << p.maxHealth
              << ",\"hasScript\":" << (p.script.empty() ? "false" : "true") << "}";
            if (i + 1 < dm.parts.size()) j << ",";
            j << "\n";
        }
        j << "  ],\n";
        j << "  \"labels\": [\n";
        for (size_t i = 0; i < dm.labels.size(); ++i) {
            const auto& l = dm.labels[i];
            j << "    {\"name\":\"" << l.name << "\",\"text\":\"" << l.text
              << "\",\"className\":\"" << l.className
              << "\",\"anchor\":[" << l.anchorX << "," << l.anchorY << ",0"
              << "],\"offset\":[" << l.offsetX << "," << l.offsetY << ",0"
              << "],\"size\":[" << l.sizeW << "," << l.sizeH << ",0"
              << "],\"color\":[" << l.color.r << "," << l.color.g << "," << l.color.b
              << "],\"fontScale\":" << l.fontScale
              << ",\"bgColor\":[" << l.bgColor.r << "," << l.bgColor.g << "," << l.bgColor.b << "]"
              << ",\"bgTransparency\":" << l.bgTransparency
              << ",\"visible\":" << (l.visible ? "true" : "false") << "}";
            if (i + 1 < dm.labels.size()) j << ",";
            j << "\n";
        }
        j << "  ],\n";
        j << "  \"backpack\": [\n";
        for (size_t i = 0; i < dm.backpack.size(); ++i) {
            const auto& b = dm.backpack[i];
            j << "    {\"id\":\"" << b.id << "\",\"name\":\"" << b.name
              << "\",\"iconHue\":" << b.iconHue
              << ",\"tool\":" << (int)b.tool
              << ",\"meshPath\":\"" << b.meshPath << "\""
              << ",\"animId\":\"" << b.animId << "\"}";
            if (i + 1 < dm.backpack.size()) j << ",";
            j << "\n";
        }
        j << "  ],\n";
        j << "  \"friends\": [\n";
        for (size_t i = 0; i < dm.friends.size(); ++i) {
            const auto& f = dm.friends[i];
            j << "    {\"name\":\"" << f.name
              << "\",\"pos\":[" << f.position.x << "," << f.position.y << "," << f.position.z
              << "],\"yaw\":" << f.yaw << "}";
            if (i + 1 < dm.friends.size()) j << ",";
            j << "\n";
        }
        j << "  ],\n";
        j << "  \"equippedBackpack\":" << dm.equippedBackpack << ",\n";
        j << "  \"humanoidParts\": [\n";
        for (size_t i = 0; i < humanoid.parts.size(); ++i) {
            const auto& p = humanoid.parts[i];
            j << "    {\"name\":\"" << p.name << "\",\"size\":[" << p.size.x << "," << p.size.y << "," << p.size.z
              << "],\"color\":[" << p.color.r << "," << p.color.g << "," << p.color.b << "]}";
            if (i + 1 < humanoid.parts.size()) j << ",";
            j << "\n";
        }
        j << "  ]\n";
        j << "}\n";
        return j.str();
    }

    static std::string makeExportPath(const std::string& placeName) {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
        std::string safe = placeName;
        for (char& c : safe)
            if (c == ' ') c = '_';
        return assetRoot() + "/assets/exports/" + safe + "_" + buf + ".blocky";
    }

    static bool saveToFile(const std::string& path, const std::string& json) {
        std::string dir = path.substr(0, path.find_last_of("\\/"));
#ifdef _WIN32
        _mkdir((assetRoot() + "/assets/exports").c_str());
        _mkdir(dir.c_str());
#else
        mkdir((assetRoot() + "/assets/exports").c_str(), 0755);
#endif
        std::ofstream f(path);
        if (!f) return false;
        f << json;
        std::fprintf(stdout, "Place esportato: %s\n", path.c_str());
        return true;
    }
};

