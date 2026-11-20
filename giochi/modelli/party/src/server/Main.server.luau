-- Party: lobby, due minigiochi a tempo, punteggio, un vincitore.
local Config = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"):WaitForChild("Config"))
local Players = game:GetService("Players")

local function pad(nome, pos, colore)
	local p = Instance.new("Part")
	p.Name = nome
	p.Anchored = true
	p.Size = Vector3.new(8, 1, 8)
	p.Position = pos
	p.Color = colore
	p.Parent = workspace
	return p
end

local lobby = pad("Lobby", Vector3.new(0, 1, 0), Color3.fromRGB(200, 200, 210))
local arena = pad("Arena", Vector3.new(0, 1, -30), Color3.fromRGB(220, 120, 40))
local traguardo = pad("Traguardo", Vector3.new(24, 1, -30), Color3.fromRGB(80, 200, 120))

local cartello = Instance.new("Part")
cartello.Name = "Cartello"
cartello.Anchored = true
cartello.Size = Vector3.new(6, 3, 0.4)
cartello.Position = Vector3.new(0, 6, -4)
cartello.Color = Color3.fromRGB(30, 30, 40)
cartello.Parent = workspace

if not Config.PROVA_AUTOMATICA then return end

local punti = {}
local function nome(player) return player.Name end

local function teletrasporta(player, pos)
	local char = player.Character
	if not char then return end
	local root = char:FindFirstChild("HumanoidRootPart") or char:FindFirstChild("Torso") or char.PrimaryPart
	if root then root.CFrame = CFrame.new(pos) end
end

local function round(titolo, secondi, dove, premio)
	cartello.Color = premio
	print("[Party] " .. titolo .. " " .. tostring(secondi) .. "s")
	for _, player in ipairs(Players:GetPlayers()) do
		teletrasporta(player, dove)
	end
	task.wait(secondi)
	for _, player in ipairs(Players:GetPlayers()) do
		punti[nome(player)] = (punti[nome(player)] or 0) + 1
	end
end

local function partita(player)
	round("Corsa", 1, arena.Position + Vector3.new(0, 4, 0), Color3.fromRGB(220, 120, 40))
	round("Traguardo", 1, traguardo.Position + Vector3.new(0, 4, 0), Color3.fromRGB(80, 200, 120))
	local vincitore, maxp = nil, -1
	for n, p in pairs(punti) do
		if p > maxp then vincitore, maxp = n, p end
	end
	cartello.Color = Color3.fromRGB(240, 210, 40)
	print("[Party] vince " .. tostring(vincitore) .. " con " .. tostring(maxp))
end

Players.PlayerAdded:Connect(partita)
for _, player in ipairs(Players:GetPlayers()) do
	task.spawn(partita, player)
end
