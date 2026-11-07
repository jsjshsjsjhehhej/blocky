-- Matematica di CFrame che usano tutti (camere, armi, NPC).
local origin = CFrame.new(0, 5, 0)
local look = CFrame.lookAt(Vector3.new(0, 5, 0), Vector3.new(10, 5, 0))
assert(look.LookVector:FuzzyEq(Vector3.new(1, 0, 0), 1e-4))
local offset = look * CFrame.new(0, 0, -5)
assert((offset.Position - Vector3.new(5, 5, 0)).Magnitude < 1e-3, tostring(offset.Position))
local rel = origin:ToObjectSpace(offset)
assert((origin * rel).Position:FuzzyEq(offset.Position, 1e-4))
local x, y, z = (CFrame.Angles(0, math.rad(90), 0)):ToOrientation()
assert(math.abs(math.deg(y) - 90) < 1e-3)
local mid = origin:Lerp(offset, 0.5)
assert(mid.Position:FuzzyEq(Vector3.new(2.5, 5, 0), 1e-3))
COMPAT_OK = true
