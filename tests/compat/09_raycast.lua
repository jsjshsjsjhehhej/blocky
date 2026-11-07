-- Raycast con RaycastParams (armi, mira).
local wall = Instance.new("Part")
wall.Size = Vector3.new(10, 10, 1)
wall.Position = Vector3.new(0, 5, -20)
wall.Anchored = true
wall.Parent = workspace

local params = RaycastParams.new()
params.FilterType = Enum.RaycastFilterType.Exclude
params.FilterDescendantsInstances = {}
local result = workspace:Raycast(Vector3.new(0, 5, 0), Vector3.new(0, 0, -50), params)
assert(result, "il raggio deve colpire il muro")
assert(math.abs(result.Position.Z + 19.5) < 0.6, "punto di impatto: " .. tostring(result.Position))
assert(result.Instance ~= nil and result.Normal ~= nil, "Instance e Normal")
COMPAT_OK = true
