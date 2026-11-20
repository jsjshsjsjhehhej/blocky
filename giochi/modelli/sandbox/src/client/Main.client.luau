-- Sandbox: mirino, raggio, griglia, colori, cancella, salva e ricarica.
local Config = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"):WaitForChild("Config"))
local Players = game:GetService("Players")
local player = Players.LocalPlayer
local mouse = player:GetMouse()
local ds = game:GetService("DataStoreService"):GetDataStore("Sandbox")

local colori = {
	Color3.fromRGB(220, 60, 50),
	Color3.fromRGB(50, 140, 220),
	Color3.fromRGB(240, 200, 40),
	Color3.fromRGB(60, 180, 80),
}
local blocchi = {}

local function posa(dove, colore, nome)
	local p = Instance.new("Part")
	p.Name = nome or "Blocco"
	p.Size = Vector3.new(4, 4, 4)
	p.Anchored = true
	local g = 4
	p.Position = Vector3.new(math.floor(dove.X / g) * g, 2, math.floor(dove.Z / g) * g)
	p.Color = colore
	p.Parent = workspace
	table.insert(blocchi, p)
	return p
end

local function salva()
	local dati = {}
	for _, p in ipairs(blocchi) do
		if p.Parent then
			table.insert(dati, { p.Position.X, p.Position.Y, p.Position.Z, p.Color.R, p.Color.G, p.Color.B })
		end
	end
	ds:SetAsync("mondo", dati)
	return dati
end

if Config.PROVA_AUTOMATICA then
	local hit = mouse.Hit
	local origine = hit.Position + Vector3.new(0, 20, 0)
	local raggio = workspace:Raycast(origine, Vector3.new(0, -40, 0))
	local dove = raggio and raggio.Position or Vector3.new(8, 0, -8)
	posa(dove, colori[1], "Rosso")
	posa(dove + Vector3.new(4, 0, 0), colori[2], "Blu")
	posa(dove + Vector3.new(8, 0, 0), colori[3], "Giallo")
	local copia = salva()
	blocchi[3]:Destroy()
	local letti = ds:GetAsync("mondo")
	assert(type(letti) == "table" and #letti == #copia, "il DataStore tiene i blocchi")
	for _, b in ipairs(letti) do
		posa(Vector3.new(b[1], b[2], b[3]), Color3.new(b[4], b[5], b[6]), "Ricaricato")
	end
	print("[Sandbox] " .. tostring(#letti) .. " blocchi ricaricati")
end
