-- Personaggio, NPC e modelli che si spostano (servono a giochi come un treno).
local Players = game:GetService("Players")
local plr = Players.LocalPlayer
local root = plr.Character:FindFirstChild("HumanoidRootPart")
assert(root, "HumanoidRootPart")

-- Scrivere Position / CFrame sulla HumanoidRootPart sposta davvero il personaggio.
root.Position = Vector3.new(10, 5, 10)
assert(root.Position:FuzzyEq(Vector3.new(10, 5, 10)), "Position: " .. tostring(root.Position))
root.CFrame = root.CFrame + Vector3.new(0, 0, 3)
assert(root.Position:FuzzyEq(Vector3.new(10, 5, 13)), "CFrame + Vector3: " .. tostring(root.Position))
root.CFrame = CFrame.lookAt(root.Position, root.Position + Vector3.new(1, 0, 0))
assert(root.CFrame.LookVector:FuzzyEq(Vector3.new(1, 0, 0), 1e-3), "CFrame gira il personaggio: " .. tostring(root.CFrame.LookVector))

-- Model:PivotTo con solo spostamento (via veloce) e con rotazione (via completa).
local m = Instance.new("Model")
local a = Instance.new("Part") a.Anchored = true a.Size = Vector3.new(4, 1, 8) a.Position = Vector3.new(0, 1, 0) a.Parent = m
local b = Instance.new("Part") b.Anchored = true b.Size = Vector3.new(1, 1, 1) b.Position = Vector3.new(0, 2, 3) b.Parent = m
m.PrimaryPart = a
m.Parent = workspace
m:PivotTo(m:GetPivot() + Vector3.new(0, 0, 5))
assert(a.Position:FuzzyEq(Vector3.new(0, 1, 5)) and b.Position:FuzzyEq(Vector3.new(0, 2, 8)), "PivotTo spostamento")
m:PivotTo(m:GetPivot() * CFrame.Angles(0, math.rad(90), 0))
assert(a.Position:FuzzyEq(Vector3.new(0, 1, 5), 1e-3), "PivotTo rotazione: il perno resta")
assert(b.Position:FuzzyEq(Vector3.new(3, 2, 5), 1e-3), "PivotTo rotazione: la part gira attorno al perno " .. tostring(b.Position))

-- NPC: Position li sposta, Destroy li toglie.
local Actors = game:GetService("Actors")
local npc = Actors:Spawn("Prova", Vector3.new(0, 3, 30))
npc.Position = Vector3.new(4, 3, 4)
assert(npc.Position:FuzzyEq(Vector3.new(4, 3, 4)), "npc.Position")
local died = false
npc.Died:Connect(function() died = true end)
npc:Destroy()
assert(npc.Position == nil and npc.Health == 0, "npc:Destroy")
npc:MoveTo(Vector3.new(0, 3, 0))   -- su un NPC tolto non fa niente (e non va in errore)
npc:TakeDamage(10)
local other = Actors:Spawn("Altro", Vector3.new(1, 3, 1))
assert(other.Health == 100 and other.Position:FuzzyEq(Vector3.new(1, 3, 1)), "un NPC nuovo dopo un Destroy")
other.Parent = nil
assert(other.Position == nil, "npc.Parent = nil lo toglie")
assert(not died, "Destroy non fa scattare Died")

-- Shape e Material con gli Enum, in scrittura e in lettura.
local c = Instance.new("Part")
c.Shape = Enum.PartType.Cylinder
c.Material = Enum.Material.Wood
c.Parent = workspace
assert(c.Shape == Enum.PartType.Cylinder, "Shape = Enum.PartType.Cylinder: " .. tostring(c.Shape))
assert(c.Material == Enum.Material.Wood, "Material == Enum.Material.Wood: " .. tostring(c.Material))
assert(Enum.Material.Wood.Name == "Wood", "EnumItem.Name")
assert(typeof(Enum.Material.Wood) == "EnumItem", "typeof EnumItem: " .. typeof(Enum.Material.Wood))
assert(Enum.KeyCode.F.Name == "F" and Enum.KeyCode.F.Value == 70, "KeyCode EnumItem")
assert(Enum.KeyCode:FromValue(70).Name == "F", "KeyCode FromValue")
assert(Enum.PartType.Cylinder.Name == "Cylinder", "PartType.Name")
c.Shape = "Ball"
assert(c.Shape == Enum.PartType.Ball, "Shape anche col nome")
local blk = Instance.new("Part")
blk.Parent = workspace
assert(blk.Shape == Enum.PartType.Block, "una Part nuova e' un Block: " .. tostring(blk.Shape))

COMPAT_OK = true
