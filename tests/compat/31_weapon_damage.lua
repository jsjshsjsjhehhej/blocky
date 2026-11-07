-- Arma: raggio che colpisce un NPC e gli toglie vita.
local npc = Instance.new("Model")
npc.Name = "Bersaglio"
local torso = Instance.new("Part")
torso.Name = "Torso"
torso.Size = Vector3.new(2, 2, 1)
torso.Position = Vector3.new(0, 3, -15)
torso.Anchored = true
torso.Parent = npc
local hum = Instance.new("Humanoid")
hum.Parent = npc
npc.Parent = workspace
local died = false
hum.Died:Connect(function() died = true end)
local result = workspace:Raycast(Vector3.new(0, 3, 0), Vector3.new(0, 0, -30))
assert(result and result.Instance, "il colpo deve prendere qualcosa")
local model = result.Instance:FindFirstAncestorOfClass("Model") or result.Instance.Parent
local h = model and model:FindFirstChildOfClass("Humanoid")
assert(h == hum, "dal pezzo colpito si arriva al Humanoid")
h:TakeDamage(100)
assert(h.Health == 0 and died, "morto dopo 100 danni")
COMPAT_OK = true
