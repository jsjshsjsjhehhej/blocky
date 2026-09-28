#pragma once


#include "QualityProfile.h"
#include "platform/PlatformServices.h"
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
            "----------------------------------------\n",
            buildStamp.c_str(),
            targetPlatformName(platform),
            gfxApiName(gfxApi),
            quality.name,
            quality.renderScale,
            quality.msaaSamples,
            quality.maxVisibleParts);

        // Cosa questa piattaforma sa fare DAVVERO. Vedi PlatformServices.h:
        // e' l'informazione che e' mancata per tutta la porta Android, dove i
        // servizi assenti fallivano in silenzio invece di dirlo.
        plat::reportServices();
        std::fprintf(stdout, "========================================\n");
    }
};

struct IGfxDevice {
    virtual ~IGfxDevice() = default;
    virtual const char* name() const = 0;
    virtual bool beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void setVsync(bool on) = 0;
    virtual int  msaaSamples() const = 0;
};

