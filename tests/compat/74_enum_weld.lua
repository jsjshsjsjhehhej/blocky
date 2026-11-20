-- EnumItem: .Name .Value typeof. WeldConstraint lega due Part.
assert(typeof(Enum.Material.Wood) == "EnumItem", typeof(Enum.Material.Wood))
assert(Enum.Material.Wood.Name == "Wood")
assert(tostring(Enum.KeyCode.F) == "Enum.KeyCode.F")
assert(Enum.KeyCode:FromName("Space").Value == 32)

local a = Instance.new("Part")
a.Name = "PezzoA"
a.Anchored = true
a.Position = Vector3.new(0, 5, 0)
a.Parent = workspace
local b = Instance.new("Part")
b.Name = "PezzoB"
b.Anchored = true
b.Position = Vector3.new(0, 8, 0)
b.Parent = workspace

local w = Instance.new("WeldConstraint")
w.Part0 = a
w.Part1 = b
assert(w.Part0 ~= nil and w.Part0.Name == "PezzoA", "Part0")
assert(w.Part1 ~= nil and w.Part1.Name == "PezzoB", "Part1")
assert(w.Enabled == true, "Weld acceso")
w.Enabled = false
assert(w.Enabled == false, "Weld spento")

COMPAT_OK = true
