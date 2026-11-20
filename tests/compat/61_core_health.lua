-- CoreScript della barra della vita (src/engine/lua/core/BarraVita.luau):
-- e' una GUI Roblox nel PlayerGui e StarterGui:SetCoreGuiEnabled la nasconde.
local plr = game:GetService("Players").LocalPlayer
local pg = plr:FindFirstChild("PlayerGui")
assert(pg, "PlayerGui")
local barra = pg:FindFirstChild("BlockyBarraVita")
assert(barra and barra.ClassName == "ScreenGui", "la CoreScript ha messo la sua ScreenGui")
local SG = game:GetService("StarterGui")
assert(SG:GetCoreGuiEnabled(Enum.CoreGuiType.Health) == true, "accesa di partenza")
SG:SetCoreGuiEnabled(Enum.CoreGuiType.Health, false)
assert(SG:GetCoreGuiEnabled(Enum.CoreGuiType.Health) == false and barra.Enabled == false, "SetCoreGuiEnabled la spegne")
SG:SetCoreGuiEnabled(Enum.CoreGuiType.Health, true)
assert(barra.Enabled == true, "e la riaccende")
COMPAT_OK = true
