-- Un tool equipaggiato non deve fermare il raggio: i pezzi sono in mano.
local tool = (Instance.new :: any)("Tool", "ProvaRaggio")
local p = Instance.new("Part")
p.Name = "MuroTool"
p.Size = Vector3.new(10, 10, 10)
p.Position = Vector3.new(0, 5, 0)
p.Anchored = true
p.CanCollide = true
p.Parent = tool
local hit = workspace:Raycast(Vector3.new(0, 5, -30), Vector3.new(0, 0, 60), 80)
if hit and hit.Instance and hit.Instance.Name == "MuroTool" then
	error("il raggio ha colpito il tool in mano")
end
COMPAT_OK = true
