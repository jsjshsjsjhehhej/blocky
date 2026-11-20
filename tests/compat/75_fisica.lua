-- Gruppi di collisione per nome, ricerche nel mondo, Region3.
local PS = game:GetService("PhysicsService")

assert(PS:IsCollisionGroupRegistered("Default"), "Default")
assert(PS:GetMaxCollisionGroups() == 32, "max")

PS:RegisterCollisionGroup("Train")
PS:RegisterCollisionGroup("World")
PS:RegisterCollisionGroup("Train")
assert(PS:IsCollisionGroupRegistered("Train"), "Train registrato")
PS:CollisionGroupSetCollidable("Train", "World", false)
assert(PS:CollisionGroupsAreCollidable("Train", "World") == false, "non collidono")
assert(PS:CollisionGroupsAreCollidable("Train", "Default") == true, "Train e Default")

local lista = PS:GetRegisteredCollisionGroups()
local haTrain = false
for _, g in ipairs(lista) do if g.name == "Train" then haTrain = true end end
assert(haTrain, "lista gruppi")

local id = PS:CreateCollisionGroup("Crea")
assert(type(id) == "number" and id >= 1, "CreateCollisionGroup")
assert(PS:GetCollisionGroupId("Crea") == id, "GetCollisionGroupId")

PS:RenameCollisionGroup("Crea", "Creato")
assert(PS:IsCollisionGroupRegistered("Creato"), "rinomina")
assert(not PS:IsCollisionGroupRegistered("Crea"), "vecchio nome")
PS:UnregisterCollisionGroup("Creato")
assert(not PS:IsCollisionGroupRegistered("Creato"), "tolto")
local ok = pcall(function() PS:CollisionGroupSetCollidable("Creato", "Default", false) end)
assert(not ok, "gruppo tolto non si usa")

local a = Instance.new("Part")
a.Name = "FisicaA"
a.Anchored = true
a.Size = Vector3.new(4, 4, 4)
a.Position = Vector3.new(400, 50, 400)
a.Parent = workspace
a.CollisionGroup = "Train"
assert(a.CollisionGroup == "Train", "CollisionGroup letto: " .. tostring(a.CollisionGroup))

local b = Instance.new("Part")
b.Name = "FisicaB"
b.Anchored = true
b.Size = Vector3.new(4, 4, 4)
b.Position = Vector3.new(400, 50, 400)
b.Parent = workspace
b.CollisionGroup = "World"
assert(a:CanCollideWith(b) == false, "CanCollideWith")
assert(type(a:GetMass()) == "number", "GetMass")
assert(workspace:ArePartsTouchingOthers({ a }) == true, "si toccano")

local vicini = workspace:GetPartBoundsInRadius(Vector3.new(400, 50, 400), 3)
local trovato = false
for _, p in ipairs(vicini) do if p.Name == "FisicaA" then trovato = true end end
assert(trovato, "GetPartBoundsInRadius")

local hit = workspace:Spherecast(Vector3.new(400, 50, 370), 1, Vector3.new(0, 0, 40))
assert(hit and hit.Instance and hit.Instance.Name == "FisicaA", "Spherecast")
assert(hit.Distance > 0, "Spherecast distanza")

local box = workspace:Blockcast(CFrame.new(400, 50, 370), Vector3.new(2, 2, 2), Vector3.new(0, 0, 40))
assert(box and box.Instance and box.Instance.Name == "FisicaA", "Blockcast")

workspace:BulkMoveTo({ a }, { CFrame.new(410, 50, 400) })
assert(math.abs(a.Position.X - 410) < 0.2, "BulkMoveTo " .. tostring(a.Position.X))

local reg = Region3.new(Vector3.new(405, 45, 395), Vector3.new(415, 55, 405))
assert(typeof(reg) == "Region3", "typeof Region3")
local esp = reg:ExpandToGrid(4)
assert(esp.Min.X <= reg.Min.X and esp.Max.X >= reg.Max.X, "ExpandToGrid")
local dentro = workspace:FindPartsInRegion3(reg)
local nel = false
for _, p in ipairs(dentro) do if p.Name == "FisicaA" then nel = true end end
assert(nel, "FindPartsInRegion3")

local w = Instance.new("WeldConstraint")
w.Part0 = a
w.Part1 = b
w.Parent = workspace
local uniti = a:GetConnectedParts()
local saldato = false
for _, p in ipairs(uniti) do if p.Name == "FisicaB" then saldato = true end end
assert(saldato, "GetConnectedParts")

a:Destroy()
b:Destroy()
w:Destroy()

COMPAT_OK = true
