-- ProximityPrompt su una part (interagisci con E).
local p = Instance.new("Part")
p.Anchored = true
p.Parent = workspace
local prompt = Instance.new("ProximityPrompt")
prompt.ActionText = "Apri"
prompt.ObjectText = "Baule"
prompt.HoldDuration = 0.5
prompt.Parent = p
prompt.Triggered:Connect(function(player) OPENED = true end)
COMPAT_OK = true
