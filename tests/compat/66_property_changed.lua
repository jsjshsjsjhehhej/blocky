-- GetPropertyChangedSignal su workspace e sulla Camera, come su Roblox.
local n = 0
workspace:GetPropertyChangedSignal("Gravity"):Connect(function() n += 1 end)
local prima = workspace.Gravity
workspace.Gravity = (type(prima) == "number" and prima or 196) + 25
task.wait()
assert(n >= 1, "cambiare Gravity avvisa, n=" .. tostring(n))
workspace.Gravity = prima

local cam = workspace.CurrentCamera
assert(cam, "CurrentCamera")
local visto = false
cam:GetPropertyChangedSignal("ViewportSize"):Connect(function() visto = true end)
assert(cam:GetPropertyChangedSignal("ViewportSize") ~= nil, "ViewportSize")
-- lo stesso nome restituisce lo stesso segnale
assert(workspace:GetPropertyChangedSignal("Gravity") == workspace:GetPropertyChangedSignal("Gravity"), "segnale stabile")
COMPAT_OK = true
