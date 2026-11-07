-- Ciclo giorno/notte con Lighting.ClockTime.
local Lighting = game:GetService("Lighting")
Lighting.ClockTime = 14
Lighting.Brightness = 2
Lighting.FogEnd = 500
assert(Lighting.ClockTime == 14, "ClockTime")
local atm = Instance.new("Atmosphere")
atm.Density = 0.3
atm.Parent = Lighting
workspace.Gravity = 196.2
assert(workspace.Gravity == 196.2, "Gravity")
COMPAT_OK = true
