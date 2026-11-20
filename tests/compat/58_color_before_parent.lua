-- Color e BrickColor scritti prima del Parent arrivano nel mondo.
local a = Instance.new("Part")
a.Color = Color3.fromRGB(200, 40, 30)
a.Anchored = true
a.Parent = workspace
assert(math.abs(a.Color.R - 200 / 255) < 0.01 and math.abs(a.Color.G - 40 / 255) < 0.01, "Color prima del Parent: " .. tostring(a.Color))
local b = Instance.new("Part")
b.Color = Color3.new(0, 0, 1)
b.Material = Enum.Material.Wood
b.Transparency = 0.5
b.Parent = workspace
assert(b.Color.B > 0.99 and b.Color.R < 0.01, "Color blu")
assert(b.Material == Enum.Material.Wood and math.abs(b.Transparency - 0.5) < 1e-3, "Material e Transparency prima del Parent")
COMPAT_OK = true
