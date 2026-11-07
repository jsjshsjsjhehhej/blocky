-- Tool con Handle nello zaino del giocatore.
local player = game:GetService("Players").LocalPlayer
local tool = Instance.new("Tool")
tool.Name = "Spada"
tool.RequiresHandle = true
local handle = Instance.new("Part")
handle.Name = "Handle"
handle.Size = Vector3.new(1, 4, 1)
handle.Parent = tool
tool.Activated:Connect(function() SWING = true end)
tool.Equipped:Connect(function() EQUIPPED = true end)
tool.Unequipped:Connect(function() end)
tool.Parent = player.Backpack
assert(player.Backpack:FindFirstChild("Spada") == tool, "Tool nel Backpack")
assert(tool:FindFirstChild("Handle") ~= nil, "Handle")
COMPAT_OK = true
