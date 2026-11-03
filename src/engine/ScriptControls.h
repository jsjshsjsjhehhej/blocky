#pragma once


#include <map>
#include <string>
#include <vector>

struct ScriptControls {
    bool jumpEnabled = true;
    bool walkEnabled = true;
    bool shiftLockAllowed = true;
    float walkSpeedOverride = -1.f;
    float jumpPowerOverride = -1.f;

    bool sprintHeld = false;
    bool moveHeld = false;

    std::map<std::string, std::vector<int>> events;

    std::map<int, std::vector<int>> partTouched;

    std::map<std::string, std::vector<int>> remoteServer;
    std::map<std::string, std::vector<int>> remoteClient;
    // RemoteFunction.OnServerInvoke — una sola function per nome.
    std::map<std::string, int> remoteInvoke;
    // BindableFunction.OnInvoke — locale, sync.
    std::map<std::string, int> bindableInvoke;

    std::map<std::string, std::vector<int>> guiButtonActivated;
    std::string lastGuiButton;

    // Humanoid.Died / HealthChanged / StateChanged
    std::vector<int> humanoidDied;
    std::vector<int> humanoidHealthChanged;
    std::vector<int> humanoidStateChanged;

    // ProximityPrompt.Triggered[name]
    std::map<std::string, std::vector<int>> proximityTriggered;

    // RemoteFunction.OnClientInvoke
    std::map<std::string, int> remoteInvokeClient;

    // Invoke async pending (rete): corrId -> coRef
    struct PendingInvoke {
        int coRef = 0; // LUA_NOREF set at push time
        std::string corrId;
        float remaining = 8.f;
    };
    std::vector<PendingInvoke> pendingInvokes;
    int nextInvokeId = 1;

    // Players.PlayerAdded / LocalPlayer.CharacterAdded
    std::vector<int> playerAdded;
    std::vector<int> characterAdded;

    // Part/Folder ChildAdded[parentIndex] — parentIndex=-1 = workspace root parts
    std::map<int, std::vector<int>> partChildAdded;
    // SharedInstance ChildAdded[sharedIndex] — -1 = service root unused; use service maps
    std::map<int, std::vector<int>> sharedChildAdded;

    // Tool.Equipped / Unequipped / Activated keyed by tool Name
    std::map<std::string, std::vector<int>> toolEquipped;
    std::map<std::string, std::vector<int>> toolUnequipped;
    std::map<std::string, std::vector<int>> toolActivated;

    std::vector<int> playerRemoving;
    std::vector<int> inputBegan;
    std::vector<int> inputEnded;

    struct DebrisItem {
        int partIndex = -1;
        float remaining = 1.f;
    };
    std::vector<DebrisItem> debris;

    struct RenderStepBind {
        std::string name;
        int priority = 2000;
        int fnRef = -1;
    };
    std::vector<RenderStepBind> renderSteps;

    // CollectionService GetInstanceAddedSignal(tag) -> refs
    std::map<std::string, std::vector<int>> tagAdded;
    std::map<std::string, std::vector<int>> tagRemoved;

    // AttributeChanged[partIndex:key]
    std::map<std::string, std::vector<int>> attributeChanged;

    // Lightweight Beam/Attachment store (script-side)
    struct ScriptAttachment {
        std::string name = "Attachment";
        int partIndex = -1;
        float ox = 0, oy = 0, oz = 0;
    };
    struct ScriptBeam {
        std::string name = "Beam";
        int a0 = -1, a1 = -1;
        float width0 = 0.2f, width1 = 0.2f;
        bool enabled = true;
    };
    std::vector<ScriptAttachment> attachments;
    std::vector<ScriptBeam> beams;

    float workspaceGravity = 196.2f;
};

inline ScriptControls& scriptControls() {
    static ScriptControls c;
    return c;
}
