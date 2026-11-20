-- Escape: una chiave da prendere, una leva, una porta che sta aperta poco.
local Config = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"):WaitForChild("Config"))

local chiave = Instance.new("Part")
chiave.Name = "Chiave"
chiave.Size = Vector3.new(1, 1, 2)
chiave.Position = Vector3.new(6, 2, -8)
chiave.Color = Color3.fromRGB(220, 180, 40)
chiave.Anchored = true
chiave.Parent = workspace

local leva = Instance.new("Part")
leva.Name = "Leva"
leva.Size = Vector3.new(0.6, 3, 0.6)
leva.Position = Vector3.new(-6, 3, -8)
leva.Color = Color3.fromRGB(180, 40, 40)
leva.Anchored = true
leva.Parent = workspace

local porta = Instance.new("Part")
porta.Name = "PortaTempo"
porta.Size = Vector3.new(6, 8, 1)
porta.Position = Vector3.new(0, 4, -18)
porta.Color = Color3.fromRGB(40, 80, 140)
porta.Anchored = true
porta.Parent = workspace

local presa = false
local function raccogli(part)
	if part ~= chiave or presa then return end
	presa = true
	chiave.Parent = nil
	print("[Escape] chiave presa")
end

chiave.Touched:Connect(raccogli)

local function tiraLeva()
	leva.CFrame = leva.CFrame * CFrame.Angles(0, 0, math.rad(-40))
	porta.CanCollide = false
	porta.Transparency = 0.7
	print("[Escape] porta aperta")
	task.wait(2)
	if not Config.PROVA_AUTOMATICA then
		porta.CanCollide = true
		porta.Transparency = 0
		print("[Escape] porta chiusa")
	end
end

if Config.PROVA_AUTOMATICA then
	raccogli(chiave)
	task.spawn(tiraLeva)
end
