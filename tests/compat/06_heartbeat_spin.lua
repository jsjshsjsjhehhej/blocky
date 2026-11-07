-- Part che gira con Heartbeat e si ferma con Disconnect.
local RunService = game:GetService("RunService")
local part = Instance.new("Part")
part.Anchored = true
part.Position = Vector3.new(0, 5, 0)
part.Parent = workspace
local angle = 0
local conn
conn = RunService.Heartbeat:Connect(function(dt)
	angle += dt * 90
	part.CFrame = CFrame.new(part.Position) * CFrame.Angles(0, math.rad(angle), 0)
	if angle > 45 then conn:Disconnect() end
end)
task.delay(1.5, function()
	assert(not conn.Connected, "Disconnect")
	assert(math.abs(part.Orientation.Y) > 30, "la part deve aver girato: " .. tostring(part.Orientation))
	COMPAT_OK = true
end)
