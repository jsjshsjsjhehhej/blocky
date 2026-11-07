-- Suono attaccato a una part.
local p = Instance.new("Part")
p.Parent = workspace
local s = Instance.new("Sound")
s.SoundId = "rbxasset://sounds/electronicpingshort.wav"
s.Volume = 0.5
s.Looped = false
s.Parent = p
s:Play()
s.Ended:Connect(function() end)
s:Stop()
COMPAT_OK = true
