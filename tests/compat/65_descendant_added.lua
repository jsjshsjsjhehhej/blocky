-- workspace.DescendantAdded esiste e scatta quando arriva una Part o un figlio
-- dentro un Folder messo nel mondo.
local visti = {}
workspace.DescendantAdded:Connect(function(d)
	visti[d.Name] = d
end)

local p = Instance.new("Part")
p.Name = "NuovaPart"
p.Parent = workspace
assert(visti.NuovaPart == p, "DescendantAdded sulla Part")

local cartella = Instance.new("Folder")
cartella.Name = "Cartella"
cartella.Parent = workspace
assert(visti.Cartella == cartella, "DescendantAdded sul Folder")

local dentro = Instance.new("Part")
dentro.Name = "Dentro"
dentro.Parent = cartella
assert(visti.Dentro == dentro, "DescendantAdded attraverso il Folder")
COMPAT_OK = true
