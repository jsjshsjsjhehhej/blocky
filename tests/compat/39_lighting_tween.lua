-- Tramonto: tween sulle proprieta' di Lighting.
local Lighting = game:GetService("Lighting")
local TS = game:GetService("TweenService")
Lighting.ClockTime = 12
local t = TS:Create(Lighting, TweenInfo.new(0.4, Enum.EasingStyle.Linear), { ClockTime = 18 })
t:Play()
task.delay(0.7, function()
	assert(math.abs(Lighting.ClockTime - 18) < 0.01, "ClockTime: " .. tostring(Lighting.ClockTime))
	COMPAT_OK = true
end)
