-- Città: case, porte, un lavoro, i soldi, un'auto.
local Config = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"):WaitForChild("Config"))
local Players = game:GetService("Players")

local function muro(nome, pos, size, colore)
	local p = Instance.new("Part")
	p.Name = nome
	p.Anchored = true
	p.Position = pos
	p.Size = size
	p.Color = colore
	p.Parent = workspace
	return p
end

local muroCol = Color3.fromRGB(230, 220, 200)
muro("Casa", Vector3.new(-16, 5, -16), Vector3.new(14, 8, 1), muroCol)
muro("CasaLato", Vector3.new(-23, 5, -10), Vector3.new(1, 8, 12), muroCol)
local porta = muro("Porta", Vector3.new(-16, 3.5, -10), Vector3.new(3, 7, 0.4), Color3.fromRGB(120, 70, 40))

local auto = muro("Auto", Vector3.new(14, 2, -12), Vector3.new(6, 2, 10), Color3.fromRGB(30, 90, 180))
local sedile = Instance.new("Seat")
sedile.Name = "Sedile"
sedile.Size = Vector3.new(2, 1, 2)
sedile.Position = Vector3.new(14, 3.2, -12)
sedile.Anchored = true
sedile.Parent = workspace

local function assumi(player)
	local soldi = Instance.new("IntValue")
	soldi.Name = "Soldi"
	soldi.Value = 0
	soldi.Parent = player
	local lavoro = Instance.new("StringValue")
	lavoro.Name = "Lavoro"
	lavoro.Value = "Muratore"
	lavoro.Parent = player
	if Config.PROVA_AUTOMATICA then
		soldi.Value = 25
		porta.CFrame = porta.CFrame * CFrame.Angles(0, math.rad(70), 0)
		print("[Citta] " .. player.Name .. " fa il " .. lavoro.Value .. " e ha " .. tostring(soldi.Value))
	end
end

Players.PlayerAdded:Connect(assumi)
for _, player in ipairs(Players:GetPlayers()) do
	task.spawn(assumi, player)
end
