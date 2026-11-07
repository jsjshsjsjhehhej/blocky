-- Destroying sulle part e Parent nil dopo Destroy.
local p = Instance.new("Part")
p.Name = "Temporanea"
p.Parent = workspace
local fired = false
p.Destroying:Connect(function() fired = true end)
p:Destroy()
assert(workspace:FindFirstChild("Temporanea") == nil, "tolta dal mondo")
assert(p.Parent == nil, "Parent nil dopo Destroy")
task.delay(0.05, function()
	assert(fired, "Destroying")
	COMPAT_OK = true
end)
