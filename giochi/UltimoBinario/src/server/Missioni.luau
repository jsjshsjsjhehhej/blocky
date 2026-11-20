-- Le taglie dello sceriffo: una bacheca in ogni citta' da' una missione
-- (Config.MISSIONI). Una per volta per giocatore; finita, si incassa il premio.
--
-- Il client legge gli attributi del Player: Missione (testo), MissioneFatto,
-- MissioneServe.

local RS = game:GetService("ReplicatedStorage")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local C = require(Shared:WaitForChild("Costruisci"))

local Missioni = {}

type Attiva = { tipo: string, serve: number, fatto: number, premio: number, testo: string, partenza: number }

local attive: { [Player]: Attiva } = {}
local rng = Random.new(Config.SEME + 404)
local Bottino: any = nil
local avvisa: (Player?, string) -> () = function() end
local effetto: (string, ...any) -> () = function() end
local distanzaTreno: () -> number = function() return 0 end

function Missioni.usa(t)
	Bottino = t.bottino
	avvisa = t.avvisa
	effetto = t.effetto
	distanzaTreno = t.distanza
end

local function mostra(g: Player)
	local a = attive[g]
	if a then
		g:SetAttribute("Missione", a.testo)
		g:SetAttribute("MissioneFatto", math.min(a.fatto, a.serve))
		g:SetAttribute("MissioneServe", a.serve)
	else
		g:SetAttribute("Missione", "")
		g:SetAttribute("MissioneFatto", 0)
		g:SetAttribute("MissioneServe", 0)
	end
end

local function completa(g: Player, a: Attiva)
	attive[g] = nil
	Bottino.aggiungi(g, "Gettoni", a.premio)
	-- un regalo in piu': un candelotto
	Bottino.aggiungi(g, "Dinamite", 1)
	Bottino.aggiungi(g, "Taglie", 1)
	effetto("suono", "coin")
	avvisa(g, "Taglia riscossa: +" .. a.premio .. " gettoni e 1 dinamite!")
	mostra(g)
end

local function avanza(g: Player, tipo: string, quanto: number)
	local a = attive[g]
	if not a or a.tipo ~= tipo then return end
	a.fatto += quanto
	mostra(g)
	if a.fatto >= a.serve then completa(g, a) end
end

--- Un nemico abbattuto da g.
function Missioni.ucciso(g: Player?, tipo: string)
	if not g then return end
	if tipo == "Bandito" then avanza(g, "banditi", 1) end
	avanza(g, "uccidi", 1)
end

--- g ha venduto n lingotti.
function Missioni.venduto(g: Player, n: number)
	avanza(g, "oro", n)
end

--- Ogni tanto: le missioni "strada" contano i binari fatti.
function Missioni.passo()
	local d = distanzaTreno()
	for g, a in pairs(attive) do
		if a.tipo == "strada" then
			local fatto = math.floor(d - a.partenza)
			if fatto ~= a.fatto then
				a.fatto = fatto
				mostra(g)
				if a.fatto >= a.serve then completa(g, a) end
			end
		end
	end
end

local function prendi(g: Player)
	if attive[g] then
		local a = attive[g]
		avvisa(g, "Hai gia' una taglia: " .. a.testo .. " (" .. math.min(a.fatto, a.serve) .. "/" .. a.serve .. ")")
		return
	end
	local m = Config.MISSIONI[rng:NextInteger(1, #Config.MISSIONI)]
	local a: Attiva = { tipo = m.tipo, serve = m.quanti, fatto = 0, premio = m.premio,
		testo = string.format(m.testo, m.quanti), partenza = distanzaTreno() }
	attive[g] = a
	mostra(g)
	effetto("suono", "open")
	avvisa(g, "Nuova taglia: " .. a.testo .. " (premio " .. a.premio .. " G)")
end

--- La bacheca dello sceriffo (la mette Insediamenti nelle citta').
function Missioni.bacheca(pos: Vector3, padre: Instance)
	local m = C.modello("Bacheca", padre)
	for s = -1, 1, 2 do
		C.part({ nome = "Palo", dimensione = Vector3.new(0.35, 5, 0.35), posizione = pos + Vector3.new(0, 2.5, s * 1.6),
			colore = "legnoScuro", materiale = Enum.Material.Wood }, m)
	end
	local tavola = C.part({ nome = "Tavola", dimensione = Vector3.new(0.3, 2.4, 3.6), posizione = pos + Vector3.new(0, 3.4, 0),
		colore = "legnoChiaro", materiale = Enum.Material.Wood }, m)
	C.part({ nome = "Tettuccio", dimensione = Vector3.new(1.2, 0.25, 4.2), posizione = pos + Vector3.new(0, 4.8, 0),
		colore = "tetto", materiale = Enum.Material.Wood, collisione = false }, m)
	-- i fogli delle taglie
	for i = 0, 2 do
		C.part({ nome = "Foglio", dimensione = Vector3.new(0.05, 0.9, 0.7), posizione = pos + Vector3.new(0.18, 3.3 + (i % 2) * 0.25, -1.1 + i * 1.1),
			colore = Color3.fromRGB(232, 220, 190), materiale = Enum.Material.SmoothPlastic, collisione = false }, m)
	end
	local p = Instance.new("ProximityPrompt")
	p.Name = "Taglie"
	p.ObjectText = "Bacheca dello sceriffo"
	p.ActionText = "Prendi una taglia"
	p.HoldDuration = 0.4
	p.MaxActivationDistance = 7
	p.Parent = tavola
	p.Triggered:Connect(function(g)
		if type(g) ~= "table" then return end
		prendi(g)
	end)
end

function Missioni.nuovaPartita(giocatori: { Player })
	table.clear(attive)
	for _, g in ipairs(giocatori) do mostra(g) end
end

function Missioni.dimentica(g: Player)
	attive[g] = nil
end

return Missioni
