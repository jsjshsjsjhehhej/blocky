#pragma once

/**
 * Blocky Studio — service catalog + real dock panel ids.
 * Only lists things creators can actually open or GetService today.
 */

namespace studio {
namespace platform {

/** Stable Explorer / Properties service ids (extend only at the end). */
enum ServiceId : int {
    Svc_Lighting = 0,
    Svc_RunService = 1,
    Svc_TweenService = 2,
    Svc_UserInputService = 3,
    Svc_Chat = 4,
    Svc_Debris = 5,
    Svc_ReplicatedStorage = 6,
    Svc_StarterGui = 7,
    Svc_DataStoreService = 8,
    Svc_CollectionService = 9,
    Svc_Players = 10,
    Svc_ServerScriptService = 11,
    Svc_ServerStorage = 12,
    Svc_StarterPlayer = 13,
    Svc_StarterCharacter = 14,
    Svc_StarterPack = 15,
    Svc_SoundService = 30,
    Svc_COUNT
};

struct ServiceInfo {
    ServiceId id;
    const char* name;
    const char* luaName;
    const char* category;
    const char* hint;
};

inline const ServiceInfo* services(int* outCount = nullptr) {
    static const ServiceInfo k[] = {
        {Svc_Lighting, "Lighting", "Lighting", "World", "Day/night, fog, ambient, graphics"},
        {Svc_RunService, "RunService", "RunService", "Core", "Heartbeat / Stepped / RenderStepped"},
        {Svc_TweenService, "TweenService", "TweenService", "Core", "TweenService:Create():Play()"},
        {Svc_UserInputService, "UserInputService", "UserInputService", "Core", "Input, jump lock"},
        {Svc_Chat, "Chat", "Chat", "Social", "In-game chat"},
        {Svc_Debris, "Debris", "Debris", "Core", "Debris:AddItem(instance, lifetime)"},
        {Svc_ReplicatedStorage, "ReplicatedStorage", "ReplicatedStorage", "Storage", "Shared remotes / assets"},
        {Svc_StarterGui, "StarterGui", "StarterGui", "Player", "ScreenGui · HUD · buttons"},
        {Svc_DataStoreService, "DataStoreService", "DataStoreService", "Cloud", "GetAsync / SetAsync"},
        {Svc_CollectionService, "CollectionService", "CollectionService", "Core", "Tags on instances"},
        {Svc_Players, "Players", "Players", "Player", "LocalPlayer · Character · Humanoid"},
        {Svc_ServerScriptService, "ServerScriptService", "ServerScriptService", "Storage", "Server scripts"},
        {Svc_ServerStorage, "ServerStorage", "ServerStorage", "Storage", "Server-only assets"},
        {Svc_StarterPlayer, "StarterPlayer", "StarterPlayer", "Player", "Player defaults"},
        {Svc_StarterCharacter, "StarterCharacter", "StarterCharacter", "Player", "Default character"},
        {Svc_StarterPack, "StarterPack", "StarterPack", "Player", "Tools → hotbar"},
        {Svc_SoundService, "SoundService", "SoundService", "World", "3D emitters · music"},
    };
    if (outCount) *outCount = (int)(sizeof(k) / sizeof(k[0]));
    return k;
}

inline const ServiceInfo* findService(int id) {
    int n = 0;
    const ServiceInfo* all = services(&n);
    for (int i = 0; i < n; ++i)
        if ((int)all[i].id == id) return &all[i];
    return nullptr;
}

/** Only panels that exist as real Studio windows. */
enum class PanelId : int {
    Explorer = 0,
    Properties,
    Toolbox,
    Assets,
    ScriptEditor,
    Output,
    Console,
    AnimationEditor,
    Profiler,
    Count
};

struct PanelInfo {
    PanelId id;
    const char* title;
    const char* searchKeys;
};

inline const PanelInfo* panels(int* outCount = nullptr) {
    static const PanelInfo k[] = {
        {PanelId::Explorer, "Explorer", "explorer datamodel"},
        {PanelId::Properties, "Properties", "properties inspector"},
        {PanelId::Toolbox, "Toolbox", "toolbox prefabs"},
        {PanelId::Assets, "Assets", "assets models sounds"},
        {PanelId::ScriptEditor, "Script Editor", "script luau"},
        {PanelId::Output, "Output", "output print warn"},
        {PanelId::Console, "Console", "console command"},
        {PanelId::AnimationEditor, "Animation Editor", "animation"},
        {PanelId::Profiler, "Profiler", "profiler fps"},
    };
    if (outCount) *outCount = (int)(sizeof(k) / sizeof(k[0]));
    return k;
}

} // namespace platform
} // namespace studio
