-- L'officina del treno: la vita del treno, i potenziamenti (un banco sul
-- tender, si paga in gettoni) e la torretta automatica sul vagone.
--
-- Vita a zero = guasto: il treno si ferma finche' qualcuno non lo ripara.

local RS = game:GetService("ReplicatedStorage")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local C = require(Shared:WaitForChild("Costruisci"))

local Officina = {}

local Treno: any = nil
local Bottino: any = nil
local Zombie: any = nil
local avvisa: (Player?, string) -> () = function() end
local effetto: (string, ...any) -> () = function() end

function Officina.usa(t)
	Treno = t.treno
	Bottino = t.bottino
	Zombie = t.zombie
	avvisa = t.avvisa
	effetto = t.effetto
end

local vita = Config.TRENO_VITA
local vitaMax = Config.TRENO_VITA
local presi: { [string]: boolean } = {}
local torretta: Part? = nil
local cannaTorretta: Part? = nil
local tempoTorretta = 0

function Officina.vita(): number return vita end
function Officina.vitaMax(): number return vitaMax end
function Officina.preso(nome: string): boolean return presi[nome] == true end

--- Il treno prende danno (zombie a bordo, esplosioni vicine...).
function Officina.danno(quanto: number)
	if vita <= 0 then return end
	vita = math.max(0, vita - quanto)
	if vita <= 0 then
		Treno.impostaGuasto(true)
		avvisa(nil, "Il treno e' guasto! Riparalo all'officina sul tender")
		effetto("suono", "hit")
	end
end

local function ripara(quanto: number)
	vita = math.min(vitaMax, vita + quanto)
	if vita > 0 and Treno.guasto() then
		Treno.impostaGuasto(false)
		avvisa(nil, "Treno riparato: si riparte!")
	end
end

-- ---------------------------------------------------------------- torretta ----

local function montaTorretta()
	local m = Treno.modello()
	if not m or torretta then return end
	-- sul retro del vagone, un affusto con una canna che gira verso il bersaglio
	local base = Treno.punto(-1.4, 3.3, 7.6)
	C.part({ nome = "Affusto", dimensione = Vector3.new(1.8, 0.8, 1.8), posizione = base,
		colore = Color3.fromRGB(50, 54, 60), materiale = Enum.Material.DiamondPlate }, m)
	torretta = C.part({ nome = "Torretta", dimensione = Vector3.new(1.3, 1.0, 1.3), posizione = base + Vector3.new(0, 0.9, 0),
		colore = Color3.fromRGB(70, 76, 84), materiale = Enum.Material.SmoothPlastic, forma = Enum.PartType.Ball }, m)
	cannaTorretta = C.part({ nome = "Canna", dimensione = Vector3.new(0.3, 0.3, 2.6), posizione = base + Vector3.new(0, 1.1, 1.4),
		colore = Color3.fromRGB(30, 30, 34), materiale = Enum.Material.SmoothPlastic, collisione = false }, m)
end

local function passoTorretta(dt: number)
	if not presi.Torretta or not torretta or not cannaTorretta then return end
	tempoTorretta += dt
	local centro = torretta.Position + Vector3.new(0, 0.3, 0)
	local z = Zombie.bersaglioVicino(centro, Config.TORRETTA.gittata)
	if not z then return end
	local p = z.npc.Position
	if not p then return end
	local mira = p + Vector3.new(0, 1, 0)
	-- la canna guarda il bersaglio
	local dir = (mira - centro)
	if dir.Magnitude > 0.1 then
		cannaTorretta.CFrame = CFrame.lookAt(centro + dir.Unit * 1.3, mira)
	end
	if tempoTorretta < Config.TORRETTA.ogni then return end
	tempoTorretta = 0
	Zombie.ferisci(z, Config.TORRETTA.danno, nil)
	effetto("colpo", centro + dir.Unit * 2.4, mira, true, false)
	effetto("suono", "shoot")
end

-- ---------------------------------------------------------------- banco ----

local function compra(g: Player, nome: string)
	local pot = Config.POTENZIAMENTI[nome]
	if nome ~= "Riparazione" and presi[nome] then
		avvisa(g, "Gia' montato")
		return
	end
	if nome == "Riparazione" and vita >= vitaMax then
		avvisa(g, "Il treno non ha danni")
		return
	end
	if Bottino.prendi(g, "Gettoni") < pot.prezzo then
		avvisa(g, "Servono " .. pot.prezzo .. " gettoni")
		return
	end
	Bottino.aggiungi(g, "Gettoni", -pot.prezzo)
	effetto("suono", "coin")
	if nome == "Riparazione" then
		ripara(120)
		avvisa(g, "Treno riparato (+120)")
		return
	end
	presi[nome] = true
	if nome == "Corazza" then
		vitaMax += 200
		vita += 200
		Treno.impostaGuasto(false)
	elseif nome == "Caldaia" then
		Treno.impostaCaldaiaMax(Config.CARBONE_MAX * 1.5)
	elseif nome == "Pistoni" then
		Treno.impostaPotenza(1.25)
	elseif nome == "Torretta" then
		montaTorretta()
	end
	avvisa(nil, "Officina: " .. pot.testo .. " montato!")
end

--- Il banco degli attrezzi sul tender, con un prompt per ogni potenziamento.
local function banco()
	local m = Treno.modello()
	if not m then return end
	local base = Treno.punto(2.65, 3.4, 19.2)
	C.part({ nome = "Banco", dimensione = Vector3.new(1.4, 1.0, 3.4), posizione = base,
		colore = Color3.fromRGB(110, 76, 46), materiale = Enum.Material.Wood }, m)
	local voci = { "Riparazione", "Corazza", "Caldaia", "Pistoni", "Torretta" }
	local colori = {
		Riparazione = Color3.fromRGB(200, 60, 50), Corazza = Color3.fromRGB(120, 124, 130), Caldaia = Color3.fromRGB(255, 140, 40),
		Pistoni = Color3.fromRGB(196, 150, 62), Torretta = Color3.fromRGB(60, 64, 70),
	}
	for i, nome in ipairs(voci) do
		local pot = Config.POTENZIAMENTI[nome]
		local attrezzo = C.part({ nome = nome, dimensione = Vector3.new(0.5, 0.5, 0.5),
			posizione = base + Vector3.new(0, 0.75, -1.4 + (i - 1) * 0.7),
			colore = colori[nome], materiale = Enum.Material.SmoothPlastic, collisione = false }, m)
		local p = Instance.new("ProximityPrompt")
		p.Name = nome
		p.ObjectText = "Officina"
		p.ActionText = pot.testo .. " - " .. pot.prezzo .. " G"
		p.HoldDuration = 0.5
		p.MaxActivationDistance = 4.5
		p.Parent = attrezzo
		p.Triggered:Connect(function(g)
			if type(g) ~= "table" then return end
			compra(g, nome)
			if nome ~= "Riparazione" and presi[nome] then p.ActionText = pot.testo .. " (montato)" end
		end)
	end
end

-- ---------------------------------------------------------------- API ----

function Officina.nuovaPartita()
	vita = Config.TRENO_VITA
	vitaMax = Config.TRENO_VITA
	table.clear(presi)
	torretta, cannaTorretta = nil, nil
	banco()
end

function Officina.passo(dt: number)
	passoTorretta(dt)
end

return Officina
