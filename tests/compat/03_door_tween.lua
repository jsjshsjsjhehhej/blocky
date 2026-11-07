-- Porta che si apre con TweenService quando la clicchi.
local TweenService = game:GetService("TweenService")
local door = Instance.new("Part")
door.Name = "Door"
door.Size = Vector3.new(4, 7, 1)
door.Position = Vector3.new(10, 3.5, 0)
door.Anchored = true
door.Parent = workspace

local click = Instance.new("ClickDetector")
click.MaxActivationDistance = 16
click.Parent = door

local info = TweenInfo.new(0.5, Enum.EasingStyle.Quad, Enum.EasingDirection.Out)
local open = false
local function toggle()
	open = not open
	local goal = { Position = door.Position + Vector3.new(0, open and 7 or -7, 0) }
	local tween = TweenService:Create(door, info, goal)
	tween:Play()
	return tween
end
click.MouseClick:Connect(toggle)
toggle()
task.delay(1, function()
	assert(door.Position.Y > 9, "la porta deve essere salita: " .. tostring(door.Position.Y))
	COMPAT_OK = true
end)
