-- SoundService e SoundGroup.
local SoundService = game:GetService("SoundService")
local group = Instance.new("SoundGroup")
group.Name = "Musica"
group.Volume = 0.4
group.Parent = SoundService
local s = Instance.new("Sound")
s.SoundGroup = group
s.Parent = SoundService
assert(SoundService:FindFirstChild("Musica") == group, "SoundGroup")
assert(s.SoundGroup == group)
COMPAT_OK = true
