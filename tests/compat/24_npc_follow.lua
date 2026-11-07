-- NPC che segue il giocatore (Humanoid:MoveTo).
local npc = Instance.new("Model")
npc.Name = "Zombie"
local root = Instance.new("Part")
root.Name = "HumanoidRootPart"
root.Size = Vector3.new(2, 2, 1)
root.Position = Vector3.new(30, 3, 0)
root.Parent = npc
local hum = Instance.new("Humanoid")
hum.Parent = npc
npc.PrimaryPart = root
npc.Parent = workspace
hum:MoveTo(Vector3.new(0, 3, 0))
hum.MoveToFinished:Connect(function(reached) ARRIVED = reached end)
COMPAT_OK = true
