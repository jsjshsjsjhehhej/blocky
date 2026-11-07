-- Albero: GetDescendants + IsA("BasePart"), ricerca ricorsiva, Destroy.
local folder = Instance.new("Folder")
folder.Name = "Mappa"
folder.Parent = workspace
for i = 1, 4 do
	local p = Instance.new("Part")
	p.Name = "Blocco" .. i
	p.Parent = folder
end
local parts = 0
for _, d in ipairs(folder:GetDescendants()) do
	if d:IsA("BasePart") then parts += 1 end
end
assert(parts == 4, "parti nel folder: " .. parts)
assert(workspace:FindFirstChild("Blocco3", true) ~= nil, "FindFirstChild ricorsivo")
folder:ClearAllChildren()
assert(#folder:GetChildren() == 0)
folder:Destroy()
assert(workspace:FindFirstChild("Mappa") == nil)
COMPAT_OK = true
