#pragma once

/**
 * Controlli player/script esposti a Lua (nessun limite di gameplay).
 * Es: disattivare jump, velocità, eventi custom illimitati.
 */

#include <map>
#include <string>
#include <vector>

struct ScriptControls {
    bool jumpEnabled = true;
    bool walkEnabled = true;
    bool shiftLockAllowed = true;
    float walkSpeedOverride = -1.f; // <0 = usa humanoid
    float jumpPowerOverride = -1.f;

    /** Aggiornati ogni frame da main (input reale). */
    bool sprintHeld = false;
    bool moveHeld = false;

    // Eventi custom illimitati: nome → lista di lua registry refs
    std::map<std::string, std::vector<int>> events;

    // Part.Touched:Connect — partIndex → refs
    std::map<int, std::vector<int>> partTouched;

    // RemoteEvent OnServerEvent / OnClientEvent — name → refs
    std::map<std::string, std::vector<int>> remoteServer;
    std::map<std::string, std::vector<int>> remoteClient;

    // TextButton Activated — button name → refs
    std::map<std::string, std::vector<int>> guiButtonActivated;
    std::string lastGuiButton;
};

inline ScriptControls& scriptControls() {
    static ScriptControls c;
    return c;
}
