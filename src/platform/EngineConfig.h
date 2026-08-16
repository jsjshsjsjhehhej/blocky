#pragma once

/**
 * Config runtime dell'engine — unico punto per ottimizzazioni cross-platform.
 */

#include "QualityProfile.h"
#include "TargetPlatform.h"

#include <cstdio>
#include <string>

struct EngineConfig {
    TargetPlatform platform = TargetPlatform::PC;
    GfxApi         gfxApi   = GfxApi::OpenGL33;
    QualityProfile quality{};
    QualityPreset  preset   = QualityPreset::PlatformDefault;

    std::string buildStamp = "Blocky Engine";
    bool        showPlatformHud = true;

    static EngineConfig createDefault() {
        EngineConfig c;
        c.platform = detectTargetPlatform();
        c.gfxApi   = recommendedGfxApi(c.platform);
        c.quality  = profileFor(c.platform);
        c.preset   = QualityPreset::PlatformDefault;
        return c;
    }

    void setPreset(QualityPreset p) {
        preset = p;
        quality = applyPreset(profileFor(platform), p);
    }

    void logBanner() const {
        std::fprintf(stdout,
            "========================================\n"
            "  %s\n"
            "  Platform : %s\n"
            "  Graphics : %s\n"
            "  Quality  : %s (scale=%.2f MSAA=%dx parts=%d)\n"
            "========================================\n",
            buildStamp.c_str(),
            targetPlatformName(platform),
            gfxApiName(gfxApi),
            quality.name,
            quality.renderScale,
            quality.msaaSamples,
            quality.maxVisibleParts);
    }
};

/** Backend grafico: interfaccia minima (OpenGL oggi; Vulkan/Metal/AGC dopo). */
struct IGfxDevice {
    virtual ~IGfxDevice() = default;
    virtual const char* name() const = 0;
    virtual bool beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void setVsync(bool on) = 0;
    virtual int  msaaSamples() const = 0;
};

/**
 * Note toolchain (non incluse nel repo open):
 *  - iOS/Metal : Xcode + Metal SDK, target arm64-apple-ios
 *  - Android   : NDK + GLES3/Vulkan, Gradle
 *  - PS5       : Sony Prospero SDK (NDA) — AGC/GNM, Prospero target
 * Il layer QualityProfile + EngineConfig resta identico su tutte.
 */
