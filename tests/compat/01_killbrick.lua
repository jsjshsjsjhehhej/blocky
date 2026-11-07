-- Kill brick classico: tocchi la part e il Humanoid muore.
local part = Instance.new("Part")
part.Name = "KillBrick"
part.Anchored = true
part.Size = Vector3.new(8, 1, 8)
part.Position = Vector3.new(0, 0.5, 20)
part.BrickColor = BrickColor.new("Really red")
part.Parent = workspace

part.Touched:Connect(function(hit)
	local humanoid = hit.Parent and hit.Parent:FindFirstChildOfClass("Humanoid")
	if humanoid then
		humanoid.Health = 0
	end
end)
assert(workspace:FindFirstChild("KillBrick") ~= nil, "la part deve stare in workspace")
COMPAT_OK = true
