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

    // Copia prompt, beam, tag dal gioco vivo dentro il DataModel, prima del file.
    static void (*&sessionHook())(DataModel&) {
        static void (*hook)(DataModel&) = nullptr;
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
        if (sessionHook()) sessionHook()(const_cast<DataModel&>(dm));
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
              << ",\"atmosphere\":" << (lighting->atmosphereEnabled ? "true" : "false")
              << ",\"atmDensity\":" << lighting->atmosphereDensity
              << ",\"atmHaze\":" << lighting->atmosphereHaze
              << ",\"sunRays\":" << (lighting->sunRaysEnabled ? "true" : "false")
              << ",\"sunRaysIntensity\":" << lighting->sunRaysIntensity
              << ",\"cc\":" << (lighting->ccEnabled ? "true" : "false")
              << ",\"ccBrightness\":" << lighting->ccBrightness
              << ",\"ccContrast\":" << lighting->ccContrast
              << ",\"ccSaturation\":" << lighting->ccSaturation
              << ",\"ccTint\":[" << lighting->ccTint.r << "," << lighting->ccTint.g << "," << lighting->ccTint.b << "]"
              << ",\"bloomFx\":" << (lighting->bloomFx ? "true" : "false")
              << ",\"bloomFxIntensity\":" << lighting->bloomFxIntensity
              << ",\"blurFx\":" << (lighting->blurFx ? "true" : "false")
              << ",\"blurFxSize\":" << lighting->blurFxSize
              << ",\"dofFx\":" << (lighting->dofFx ? "true" : "false")
              << ",\"dofFar\":" << lighting->dofFar
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
              << ",\"toolSlot\":" << p.toolSlot
              << ",\"groupId\":" << p.groupId
              << ",\"hasScript\":" << (p.script.empty() ? "false" : "true")
              << ",\"script\":\"" << escapeScript(p.script) << "\""
              << ",\"scriptType\":" << (int)p.scriptType
              << ",\"decalImage\":\"" << escapeScript(p.decalImage) << "\""
              << ",\"className\":\"" << escapeScript(p.className) << "\""
              << ",\"meshFile\":\"" << escapeScript(p.meshFile) << "\""
              << ",\"textureImage\":\"" << escapeScript(p.textureImage) << "\""
              << ",\"spawnTeam\":\"" << escapeScript(p.spawnTeam) << "\""
              << ",\"extraDecals\":[";
            for (size_t fi = 0; fi < p.extraDecals.size(); ++fi) {
                const auto& fd = p.extraDecals[fi];
                j << "{\"face\":" << fd.face
                  << ",\"image\":\"" << escapeScript(fd.image) << "\""
                  << ",\"color\":[" << fd.r << "," << fd.g << "," << fd.b << "]"
                  << ",\"transparency\":" << fd.transparency << "}";
                if (fi + 1 < p.extraDecals.size()) j << ",";
            }
            j << "]}";
            if (i + 1 < dm.parts.size()) j << ",";
            j << "\n";
        }
        j << "  ],\n";
        j << "  \"labels\": [\n";
        bool firstLabel = true;
        for (size_t i = 0; i < dm.labels.size(); ++i) {
            const auto& l = dm.labels[i];
            // GUI in stile Roblox create dagli script mentre si gioca (PlayerGui):
            // sono del giocatore e della partita, non del place.
            if (l.uiLayout) continue;
            if (!firstLabel) j << ",\n";
            firstLabel = false;
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
        }
        if (!firstLabel) j << "\n";
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
              << ",\"rojo\":\"" << escapeScript(s.rojoFile) << "\""
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
              << ",\"requiresHandle\":" << (b.requiresHandle ? "true" : "false")
              << ",\"canBeDropped\":" << (b.canBeDropped ? "true" : "false")
              << ",\"manualActivationOnly\":" << (b.manualActivationOnly ? "true" : "false")
              << ",\"enabled\":" << (b.enabled ? "true" : "false")
              << ",\"toolTip\":\"" << escapeScript(b.toolTip) << "\""
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
        j << "  ],\n";
        auto writeNamed = [&](const char* key, auto&& body) {
            j << "  \"" << key << "\": [\n";
            body();
            j << "  ],\n";
        };
        writeNamed("prompts", [&]() {
            for (size_t i = 0; i < dm.savedPrompts.size(); ++i) {
                const auto& p = dm.savedPrompts[i];
                j << "    {\"name\":\"" << escapeScript(p.name) << "\",\"text\":\"" << escapeScript(p.text)
                  << "\",\"object\":\"" << escapeScript(p.objectText)
                  << "\",\"pos\":[" << p.x << "," << p.y << "," << p.z << "]"
                  << ",\"radius\":" << p.radius << ",\"part\":" << p.partIndex
                  << ",\"key\":\"" << (p.key ? p.key : 'E') << "\",\"hold\":" << p.hold
                  << ",\"enabled\":" << (p.enabled ? "true" : "false") << "}";
                if (i + 1 < dm.savedPrompts.size()) j << ",";
                j << "\n";
            }
        });
        writeNamed("attachments", [&]() {
            for (size_t i = 0; i < dm.savedAttachments.size(); ++i) {
                const auto& a = dm.savedAttachments[i];
                j << "    {\"name\":\"" << escapeScript(a.name) << "\",\"part\":" << a.partIndex
                  << ",\"pos\":[" << a.ox << "," << a.oy << "," << a.oz << "]}";
                if (i + 1 < dm.savedAttachments.size()) j << ",";
                j << "\n";
            }
        });
        writeNamed("beams", [&]() {
            for (size_t i = 0; i < dm.savedBeams.size(); ++i) {
                const auto& b = dm.savedBeams[i];
                j << "    {\"name\":\"" << escapeScript(b.name) << "\",\"a0\":" << b.a0 << ",\"a1\":" << b.a1
                  << ",\"w0\":" << b.width0 << ",\"w1\":" << b.width1
                  << ",\"color\":[" << b.r << "," << b.g << "," << b.b << "]"
                  << ",\"enabled\":" << (b.enabled ? "true" : "false") << "}";
                if (i + 1 < dm.savedBeams.size()) j << ",";
                j << "\n";
            }
        });
        writeNamed("fx", [&]() {
            for (size_t i = 0; i < dm.savedEmitters.size(); ++i) {
                const auto& e = dm.savedEmitters[i];
                j << "    {\"name\":\"" << escapeScript(e.name) << "\",\"class\":\""
                  << escapeScript(e.className) << "\",\"part\":" << e.partIndex
                  << ",\"preset\":" << e.preset
                  << ",\"rate\":" << e.rate << ",\"speed\":" << e.speed << ",\"life\":" << e.life
                  << ",\"color\":[" << e.r << "," << e.g << "," << e.b << "]"
                  << ",\"enabled\":" << (e.enabled ? "true" : "false")
                  << ",\"fire\":" << (e.customMotion ? "true" : "false") << "}";
                if (i + 1 < dm.savedEmitters.size()) j << ",";
                j << "\n";
            }
        });
        writeNamed("clicks", [&]() {
            for (size_t i = 0; i < dm.savedClicks.size(); ++i) {
                const auto& c = dm.savedClicks[i];
                j << "    {\"part\":" << c.partIndex << ",\"reach\":" << c.reach << "}";
                if (i + 1 < dm.savedClicks.size()) j << ",";
                j << "\n";
            }
        });
        writeNamed("trails", [&]() {
            for (size_t i = 0; i < dm.savedTrails.size(); ++i) {
                const auto& t = dm.savedTrails[i];
                j << "    {\"name\":\"" << escapeScript(t.name) << "\",\"part\":" << t.partIndex
                  << ",\"width\":" << t.width << ",\"life\":" << t.life
                  << ",\"color\":[" << t.r << "," << t.g << "," << t.b << "]"
                  << ",\"enabled\":" << (t.enabled ? "true" : "false") << "}";
                if (i + 1 < dm.savedTrails.size()) j << ",";
                j << "\n";
            }
        });
        j << "  \"tags\": [\n";
        for (size_t i = 0; i < dm.savedTags.size(); ++i) {
            const auto& t = dm.savedTags[i];
            j << "    {\"tag\":\"" << escapeScript(t.tag) << "\",\"part\":" << t.partIndex << "}";
            if (i + 1 < dm.savedTags.size()) j << ",";
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

