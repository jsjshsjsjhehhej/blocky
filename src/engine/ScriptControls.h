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

    std::map<std::string, std::vector<int>> guiButtonActivated;
    std::string lastGuiButton;
};

inline ScriptControls& scriptControls() {
    static ScriptControls c;
    return c;
}
