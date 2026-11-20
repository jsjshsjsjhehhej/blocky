-- I suoni del viaggio. I colpi arrivano dal server (Effetto "suono").
-- Treno, notte e tempesta li decide il client dallo Stato, cosi' non
-- si manda un remote a ogni battito.

local RS = game:GetService("ReplicatedStorage")
local SoundService = game:GetService("SoundService")
local RunService = game:GetService("RunService")
local effetto = RS:WaitForChild("Effetto")
local stato = RS:WaitForChild("Stato")

local FILE = {
	shoot = { "sounds/shoot.wav", 0.55 },
	hit = { "sounds/hit.wav", 0.7 },
	groan = { "sounds/groan.wav", 0.6 },
	shotgun = { "sounds/shotgun.wav", 0.7 },
	coal = { "sounds/coal.wav", 0.75 },
	open = { "sounds/open.wav", 0.7 },
	coin = { "sounds/coin.wav", 0.55 },
	throw = { "sounds/throw.wav", 0.5 },
	morte = { "sounds/morte.wav", 0.8 },
	chug = { "sounds/chug.wav", 0.4 },
	wind = { "sounds/wind.wav", 0.35 },
	night = { "sounds/night.wav", 0.3 },
}

local suoni: { [string]: Sound } = {}
for nome, f in pairs(FILE) do
	local s = Instance.new("Sound")
	s.Name = nome
	s.SoundId = f[1]
	s.Volume = f[2]
	s.Parent = SoundService
	suoni[nome] = s
end

effetto.OnClientEvent:Connect(function(cosa, nome)
	if cosa ~= "suono" then return end
	local s = suoni[nome]
	if s then pcall(function() s:Play() end) end
end)

local function gira(s: Sound, acceso: boolean, volume: number)
	s.Volume = volume
	s.Looped = true
	if acceso then
		pcall(function() s:Play() end)
	else
		pcall(function() s:Stop() end)
	end
end

local battito = 0
local fasciaVento = -1
local eraNotte = false

RunService.Heartbeat:Connect(function(dt)
	local vel = stato:GetAttribute("Velocita") or 0
	if vel > 2 then
		battito += dt
		local ogni = 0.52 - math.clamp(vel / 50, 0, 0.28)
		if battito >= ogni then
			battito = 0
			local c = suoni.chug
			c.Volume = 0.25 + math.clamp(vel / 40, 0, 0.45)
			pcall(function() c:Play() end)
		end
	else
		battito = 0
	end

	local tempesta = stato:GetAttribute("Tempesta") or 0
	local fascia = if tempesta > 0.15 then math.floor(tempesta * 4) else 0
	if fascia ~= fasciaVento then
		fasciaVento = fascia
		gira(suoni.wind, fascia > 0, 0.15 + fascia * 0.12)
	end

	local notte = stato:GetAttribute("Notte") == true
	if notte ~= eraNotte then
		eraNotte = notte
		gira(suoni.night, notte, 0.28)
	end
end)
