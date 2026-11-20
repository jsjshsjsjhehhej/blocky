#pragma once
// Generato da tools/roblox_api/gen_blocky_defs.mjs: NON modificare a mano.
// Definizioni luau-lsp delle API proprie di Blocky (le scrive l'export Rojo).
#include <string>
namespace blk {
inline const std::string& blockyLuauDefs() {
    static const std::string s = R"DEFS(-- Generato da tools/roblox_api/gen_blocky_defs.mjs: NON modificare a mano.
-- Quello che Blocky ha in PIU' rispetto a Roblox (l'API di Roblox luau-lsp la conosce gia').
-- In VS Code: luau-lsp.types.definitionFiles = ["blocky.d.luau"].

declare AnimateFrame: (...any) -> ...any

declare Workspace: any

declare Events: {
    Create: (...any) -> ...any,
    Connect: (...any) -> ...any,
    Once: (...any) -> ...any,
    Wait: (...any) -> ...any,
    Fire: (...any) -> ...any,
    DisconnectAll: (...any) -> ...any,
}

declare Humanoid: {
    SetJumpEnabled: (...any) -> ...any,
    SetWalkEnabled: (...any) -> ...any,
    SetWalkSpeed: (...any) -> ...any,
    SetJumpPower: (...any) -> ...any,
    Get: (...any) -> ...any,
    GetPosition: (...any) -> ...any,
    TakeDamage: (...any) -> ...any,
    GetHealth: (...any) -> ...any,
    GetLook: (...any) -> ...any,
    SetPosition: (...any) -> ...any,
    GetState: (...any) -> ...any,
    Died: {
        Connect: (...any) -> ...any,
        Once: (...any) -> ...any,
        Wait: (...any) -> ...any,
    },
    HealthChanged: {
        Connect: (...any) -> ...any,
        Once: (...any) -> ...any,
        Wait: (...any) -> ...any,
    },
    StateChanged: {
        Connect: (...any) -> ...any,
        Once: (...any) -> ...any,
        Wait: (...any) -> ...any,
    },
    LoadAnimation: (...any) -> ...any,
    GetPlayingAnimationTracks: (...any) -> ...any,
    Animator: {
        ClassName: any,
        Name: any,
        LoadAnimation: (...any) -> ...any,
        GetPlayingAnimationTracks: (...any) -> ...any,
    },
}

declare Backpack: {
    Give: (...any) -> ...any,
    Equipped: (...any) -> ...any,
    Equip: (...any) -> ...any,
    Unequip: (...any) -> ...any,
    Drop: (...any) -> ...any,
}

declare Camera: {
    Shake: (...any) -> ...any,
    ScreenRay: (...any) -> ...any,
    ScreenPointToRay: (...any) -> ...any,
    GetLook: (...any) -> ...any,
    GetCFrame: (...any) -> ...any,
    FieldOfView: any,
}

declare UserInputService: {
    DisableJump: (...any) -> ...any,
    EnableJump: (...any) -> ...any,
    IsKeyDown: (...any) -> ...any,
    IsSprintHeld: (...any) -> ...any,
    IsMoving: (...any) -> ...any,
    IsMouseButtonPressed: (...any) -> ...any,
}
)DEFS";
    return s;
}
} // namespace blk
