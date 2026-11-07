#pragma once

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "DataModel.h"
#include "GameHub.h"
#include "PartConstraints.h"
#include "PartGroups.h"
#include "Humanoid.h"
#include "LightingService.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

class PlaceExporter {
public:
    /**
     * Chiamato prima di ogni esportazione: chi tiene il mondo in uno stato
     * provvisorio (l'Animation Studio muove le parti nella posa) lo rimette a
     * posto, cosi' nel file finisce sempre il mondo vero e non la posa.
     */
    static void (*&beforeExportHook())() {
        static void (*hook)() = nullptr;
        return hook;
    }

    static std::string escapeScript(const std::string& s) {
        std::string o;
        o.reserve(s.size() + 8);
        for (unsigned char c : s) {
            if (c == '\\') o += "\\\\";
            else if (c == '"') o += "\\\"";
            else if (c == '\n') o += "\\n";
            else if (c == '\r') continue;
            else o.push_back((char)c);
        }
        return o;
    }

    static std::string exportPlace(const DataModel& dm, const Humanoid& humanoid,
                                   const glm::vec3& playerPos, float playerYaw,
                                   const LightingService* lighting = nullptr) {
        if (beforeExportHook()) beforeExportHook()();
        std::ostringstream j;
        j << std::fixed << std::setprecision(3);
        j << "{\n";
        j << "  \"format\": \"BlockyPlace\",\n";
        j << "  \"version\": 2,\n";
        j << "  \"placeName\": \"" << escapeScript(dm.placeName) << "\",\n";
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
        partgroups::writeJson(j, dm);
        partconstraints::writeJson(j, dm);
        j << "  \"parts\": [\n";
        for (size_t i = 0; i < dm.parts.size(); ++i) {
            const auto& p = dm.parts[i];
            j << "    {\"name\":\"" << escapeScript(p.name) << "\",\"pos\":[" << p.position.x << "," << p.position.y << "," << p.position.z
              << "],\"size\":[" << p.size.x << "," << p.size.y << "," << p.size.z
              << "],\"rot\":[" << p.rotation.x << "," << p.rotation.y << "," << p.rotation.z
              << "],\"color\":[" << p.color.r << "," << p.color.g << "," << p.color.b << "]";
            partprops::writeAll(j, p);
            j << ",\"shape\":\"" << partShapeName(p.shape) << "\""
              << ",\"material\":\"" << partMaterialName(p.material) << "\""
              << ",\"emissive\":" << p.emissive
              << ",\"pointLight\":" << (p.pointLight ? "true" : "false")
              << ",\"lightRange\":" << p.lightRange
              << ",\"lightBrightness\":" << p.lightBrightness
              << ",\"canDestroy\":" << (p.canDestroy ? "true" : "false")
              << ",\"health\":" << p.health
              << ",\"maxHealth\":" << p.maxHealth
              << ",\"parentIndex\":" << p.parentIndex
              << ",\"groupId\":" << p.groupId
              << ",\"hasScript\":" << (p.script.empty() ? "false" : "true")
              << ",\"script\":\"" << escapeScript(p.script) << "\""
              << ",\"scriptType\":" << (int)p.scriptType << "}";
            if (i + 1 < dm.parts.size()) j << ",";
            j << "\n";
        }
        j << "  ],\n";
        j << "  \"labels\": [\n";
        for (size_t i = 0; i < dm.labels.size(); ++i) {
            const auto& l = dm.labels[i];
            j << "    {\"name\":\"" << escapeScript(l.name) << "\",\"text\":\"" << escapeScript(l.text)
              << "\",\"className\":\"" << escapeScript(l.className)
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
        // Shared instances (ReplicatedStorage / ServerStorage / SSS) — ModuleScript, RemoteEvent, Folder.
        j << "  \"instances\": [\n";
        for (size_t i = 0; i < dm.instances.size(); ++i) {
            const auto& s = dm.instances[i];
            j << "    {\"name\":\"" << escapeScript(s.name)
              << "\",\"className\":\"" << escapeScript(s.className)
              << "\",\"service\":" << (int)s.service
              << ",\"parentIndex\":" << s.parentIndex
              << ",\"scriptType\":" << (int)s.scriptType
              << ",\"source\":\"" << escapeScript(s.source) << "\"}";
            if (i + 1 < dm.instances.size()) j << ",";
            j << "\n";
        }
        j << "  ],\n";
        j << "  \"backpack\": [\n";
        for (size_t i = 0; i < dm.backpack.size(); ++i) {
            const auto& b = dm.backpack[i];
            // meshPath DEVE essere escaped: altrimenti C:\Users\... rompe il JSON
            // e sul telefono l'inventario arriva vuoto / senza mesh.
            j << "    {\"id\":\"" << escapeScript(b.id) << "\",\"name\":\"" << escapeScript(b.name)
              << "\",\"iconHue\":" << b.iconHue
              << ",\"tool\":" << (int)b.tool
              << ",\"meshPath\":\"" << escapeScript(b.meshPath) << "\""
              << ",\"animId\":\"" << escapeScript(b.animId) << "\""
              << ",\"gripOffset\":[" << b.gripOffset.x << "," << b.gripOffset.y
              << "," << b.gripOffset.z << "]"
              << ",\"gripRotation\":[" << b.gripRotation.x << "," << b.gripRotation.y
              << "," << b.gripRotation.z << "]"
              << ",\"gripScale\":" << b.gripScale
              << ",\"holdPose\":[";
            for (size_t hi = 0; hi < b.holdPose.size(); ++hi) {
                const auto& hp = b.holdPose[hi];
                j << "{\"name\":\"" << escapeScript(hp.name) << "\""
                  << ",\"pos\":[" << hp.pos.x << "," << hp.pos.y << "," << hp.pos.z << "]"
                  << ",\"rot\":[" << hp.rot.x << "," << hp.rot.y << "," << hp.rot.z << "]}";
                if (hi + 1 < b.holdPose.size()) j << ",";
            }
            j << "]}";
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
            j << "    {\"name\":\"" << escapeScript(p.name) << "\",\"size\":[" << p.size.x << "," << p.size.y << "," << p.size.z
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

