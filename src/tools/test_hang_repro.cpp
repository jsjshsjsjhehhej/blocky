// Riproduzione isolata del possibile blocco segnalato: lo script
// "Atmosfera dark horror" (stile Roblox, non riscritto) eseguito esattamente
// come farebbe runAllPartScripts in Test. Se questo programma non stampa
// "DONE" e va in timeout, il blocco e' riproducibile qui, fuori dalla GUI.
#include "engine/DataModel.h"
#include "engine/LuaRuntime.h"
#include "engine/GameHub.h"

#include <cstdio>
#include <string>

int main() {
    DataModel dm;
    dm.loadBaseplatePlace();
    LuaRuntime lua;
    if (!lua.init(&dm)) {
        std::fprintf(stderr, "FAIL init: %s\n", lua.lastError.c_str());
        return 1;
    }

    WorkspacePart p;
    p.name = "Part";
    p.position = {0, 1, 0};
    dm.parts.push_back(p);
    int idx = (int)dm.parts.size() - 1;

    const char* script =
        "local Lighting = game:GetService(\"Lighting\")\n"
        "Lighting.Ambient = Color3.fromRGB(10, 10, 15)\n"
        "Lighting.OutdoorAmbient = Color3.fromRGB(0, 0, 0)\n"
        "Lighting.Brightness = 1\n"
        "Lighting.ClockTime = 0.2\n"
        "Lighting.FogColor = Color3.fromRGB(5, 5, 10)\n"
        "Lighting.FogStart = 50\n"
        "Lighting.FogEnd = 200\n"
        "local cc = Instance.new(\"ColorCorrectionEffect\")\n"
        "cc.TintColor = Color3.fromRGB(180, 190, 255)\n"
        "cc.Contrast = 0.2\n"
        "cc.Brightness = -0.1\n"
        "cc.Saturation = -0.4\n"
        "cc.Parent = Lighting\n"
        "local bloom = Instance.new(\"BloomEffect\")\n"
        "bloom.Intensity = 0.4\n"
        "bloom.Size = 24\n"
        "bloom.Threshold = 0.8\n"
        "bloom.Parent = Lighting\n"
        "local dof = Instance.new(\"DepthOfFieldEffect\")\n"
        "dof.FocusDistance = 15\n"
        "dof.InFocusRadius = 10\n"
        "dof.NearIntensity = 0.4\n"
        "dof.FarIntensity = 0.6\n"
        "dof.Parent = Lighting\n"
        "local sound = Instance.new(\"Sound\")\n"
        "sound.SoundId = \"rbxassetid://1844662344\"\n"
        "sound.Looped = true\n"
        "sound.Volume = 0.6\n"
        "sound.Parent = workspace\n"
        "sound:Play()\n";

    std::fprintf(stderr, "[repro] pre runPartScript\n"); std::fflush(stderr);
    bool r = lua.runPartScript(script, idx);
    std::fprintf(stderr, "[repro] post runPartScript r=%d err=%s\n",
                 (int)r, lua.lastError.c_str()); std::fflush(stderr);

    for (int i = 0; i < 30; ++i) {
        lua.fireHeartbeat(0.016f);
    }
    std::fprintf(stderr, "[repro] post 30 heartbeat\n"); std::fflush(stderr);

    std::fprintf(stdout, "DONE\n");
    return 0;
}
