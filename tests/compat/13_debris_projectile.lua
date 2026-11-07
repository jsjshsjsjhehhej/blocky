-- Proiettile con Debris:AddItem.
local Debris = game:GetService("Debris")
local bullet = Instance.new("Part")
bullet.Name = "Bullet"
bullet.Size = Vector3.new(0.3, 0.3, 1)
bullet.Position = Vector3.new(0, 5, 0)
bullet.CanCollide = false
bullet.Parent = workspace
Debris:AddItem(bullet, 0.3)
task.delay(1, function()
	assert(workspace:FindFirstChild("Bullet") == nil, "Debris doveva togliere il proiettile")
	COMPAT_OK = true
end)
