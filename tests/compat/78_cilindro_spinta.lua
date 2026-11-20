-- Part nuova non ancorata, cilindro lungo su X, percorso, raggio, spinta.
local p = Instance.new("Part")
assert(p.Anchored == false, "Anchored")
p.Parent = workspace

local c = Instance.new("Cylinder")
assert(c.Size.X == 4 and c.Size.Y == 2 and c.Size.Z == 2, "cilindro X")

local path = game:GetService("PathfindingService"):CreatePath()
assert(type(path.ComputeAsync) == "function", "ComputeAsync")
assert(type(path.GetWaypoints) == "function", "GetWaypoints")

assert(Instance.new("Beam").ClassName == "Beam", "Beam")
assert(Instance.new("Trail").ClassName == "Trail", "Trail")
assert(Instance.new("Highlight").ClassName == "Highlight", "Highlight")

local lv = Instance.new("LinearVelocity")
lv.VectorVelocity = Vector3.new(0, 8, 0)
lv.Parent = p
assert(lv.ClassName == "LinearVelocity", "LinearVelocity")
assert(lv.__moverSlot >= 0, "spinta attaccata")
local vf = Instance.new("VectorForce")
vf.Force = Vector3.new(12, 0, 0)
vf.Parent = p
assert(vf.__moverSlot >= 0 and vf.__moverSlot ~= lv.__moverSlot, "forza attaccata")

COMPAT_OK = true
