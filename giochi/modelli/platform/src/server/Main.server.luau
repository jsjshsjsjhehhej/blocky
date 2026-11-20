-- Platform 2D: piattaforme in fila e un nemico che fa la ronda sull'asse X.
local Config = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"):WaitForChild("Config"))

for i = 0, 4 do
	local p = Instance.new("Part")
	p.Name = "Piattaforma"
	p.Anchored = true
	p.Size = Vector3.new(8, 1, 4)
	p.Position = Vector3.new(-16 + i * 10, 2, 0)
	p.Color = Color3.fromRGB(90, 170, 90)
	p.Parent = workspace
end

local nemico = Instance.new("Part")
nemico.Name = "Nemico"
nemico.Anchored = true
nemico.Size = Vector3.new(2, 3, 2)
nemico.Color = Color3.fromRGB(180, 40, 40)
nemico.Position = Vector3.new(-12, 4.5, 0)
nemico.Parent = workspace

if not Config.PROVA_AUTOMATICA then return end

local verso = 1
local x = -12
game:GetService("RunService").Heartbeat:Connect(function(dt)
	x += verso * 8 * dt
	if x > 16 then verso = -1 end
	if x < -16 then verso = 1 end
	local p = nemico.Position
	nemico.Position = Vector3.new(x, p.Y, 0)
end)
print("[Platform] nemico in ronda")
