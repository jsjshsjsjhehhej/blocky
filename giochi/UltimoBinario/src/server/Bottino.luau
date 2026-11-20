-- Cosa si trova e cosa si compra.
-- Ogni giocatore tiene le sue cose come attributi del Player:
--   Carbone (pezzi in mano), Oro (lingotti in mano), Munizioni, Bende, Gettoni.
-- L'oro si vende alla cassaforte del treno; con i gettoni si compra dal mercante.

local RS = game:GetService("ReplicatedStorage")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local C = require(Shared:WaitForChild("Costruisci"))

local Bottino = {}

local rng = Random.new(Config.SEME + 99)
local avvisa: (Player, string) -> () = function() end
local suono: (string, Vector3) -> () = function() end

function Bottino.quandoAvviso(f: (Player, string) -> ()) avvisa = f end
function Bottino.quandoSuono(f: (string, Vector3) -> ()) suono = f end
local venduto: (Player, number) -> () = function() end
function Bottino.quandoVenduto(f: (Player, number) -> ()) venduto = f end
-- gli oggetti da trascinare li fa Trasporto (cosi' Bottino non dipende da lui)
local oggetto: (string, Vector3) -> () = function() end
function Bottino.quandoOggetto(f: (string, Vector3) -> ()) oggetto = f end

local MASSIMI = { Carbone = Config.PEZZI_IN_MANO_MAX, Oro = Config.ORO_MAX }

function Bottino.prendi(g: Player, cosa: string): number
	return g:GetAttribute(cosa) or 0
end

function Bottino.metti(g: Player, cosa: string, quanto: number)
	g:SetAttribute(cosa, quanto)
end

--- Aggiunge (o toglie, se negativo) e rispetta i massimi. Restituisce quanti ne ha ora.
function Bottino.aggiungi(g: Player, cosa: string, quanto: number): number
	local n = Bottino.prendi(g, cosa) + quanto
	local max = MASSIMI[cosa]
	n = math.max(0, if max then math.min(n, max) else n)
	Bottino.metti(g, cosa, n)
	return n
end

function Bottino.iniziale(g: Player)
	Bottino.metti(g, "Carbone", 1)
	Bottino.metti(g, "Oro", 0)
	Bottino.metti(g, "Munizioni", Config.MUNIZIONI_INIZIO)
	Bottino.metti(g, "Bende", 1)
	Bottino.metti(g, "Gettoni", 0)
	Bottino.metti(g, "Uccisi", 0)
	Bottino.metti(g, "Dinamite", 1)
	Bottino.metti(g, "Taglie", 0)
	Bottino.metti(g, "OroVenduto", 0)
	g:SetAttribute("HaPompa", false)
end

local function prompt(padre: Instance, azione: string, oggetto: string, tieni: number, distanza: number?): ProximityPrompt
	local p = Instance.new("ProximityPrompt")
	p.Name = oggetto
	p.ActionText = azione
	p.ObjectText = oggetto
	p.HoldDuration = tieni
	p.MaxActivationDistance = distanza or 7
	p.Parent = padre
	return p
end

local function vita(g: Player, quanta: number)
	local char = g.Character
	local hum = char and char:FindFirstChild("Humanoid")
	if hum then hum.Health = math.min(hum.MaxHealth, hum.Health + quanta) end
end

-- ---------------------------------------------------------------- casse ----

local function apri(g: Player, cassa: Part, coperchio: Part, p: ProximityPrompt, ricca: boolean)
	p.Enabled = false
	coperchio.Orientation = Vector3.new(0, 0, 70)
	coperchio.Position = cassa.Position + Vector3.new(cassa.Size.X / 2 + 0.2, cassa.Size.Y / 2 + 0.3, 0)
	cassa.Color = Config.colore("legnoScuro")
	suono("open", cassa.Position)

	-- la roba esce dalla cassa: oggetti veri da trascinare (o da prendere con E)
	local trovato = {}
	local giri = if ricca then 3 else rng:NextInteger(1, 2)
	local k = 0
	local function fuori(tipo: string)
		k += 1
		local a = k * 2.1
		oggetto(tipo, cassa.Position + Vector3.new(math.cos(a) * 2.2, 1.2, math.sin(a) * 2.2))
	end
	for _ = 1, giri do
		local r = rng:NextNumber()
		if r < 0.34 then
			for _ = 1, rng:NextInteger(1, 2) do fuori("Carbone") end
			table.insert(trovato, "carbone")
		elseif r < 0.62 then
			fuori("Munizioni")
			table.insert(trovato, "munizioni")
		elseif r < 0.8 then
			for _ = 1, rng:NextInteger(if ricca then 2 else 1, if ricca then 3 else 1) do fuori("Oro") end
			table.insert(trovato, "oro")
		elseif r < 0.92 then
			fuori("Bende")
			table.insert(trovato, "bende")
		else
			local n = rng:NextInteger(4, 12)
			Bottino.aggiungi(g, "Gettoni", n)
			table.insert(trovato, n .. " gettoni")
		end
	end
	avvisa(g, "Nella cassa: " .. table.concat(trovato, ", ") .. " (clic e trascina, o E per prendere)")
end

--- Una cassa chiusa (ricca: nelle banche e nelle armerie, piu' roba).
function Bottino.cassa(pos: Vector3, padre: Instance, ricca: boolean?)
	local cassa = C.part({ nome = "Cassa", dimensione = Vector3.new(2.6, 1.8, 1.8), posizione = pos,
		colore = if ricca then "rifiniture" else "cassa", materiale = Enum.Material.Wood }, padre)
	local coperchio = C.part({ nome = "Coperchio", dimensione = Vector3.new(2.8, 0.3, 2.0), posizione = pos + Vector3.new(0, 1.05, 0),
		colore = "legnoVecchio", materiale = Enum.Material.Wood, collisione = false }, padre)
	local p = prompt(cassa, "Apri", if ricca then "Cassa blindata" else "Cassa", if ricca then 1.2 else 0.6)
	p.Triggered:Connect(function(g: Player)
		if type(g) ~= "table" then return end
		apri(g, cassa, coperchio, p, ricca == true)
	end)
end

-- ---------------------------------------------------------------- oro ----

--- Un lingotto d'oro (nelle banche, nelle armerie, lasciato dai banditi):
--- un oggetto da trascinare fino alla cassaforte del treno, o da prendere con E.
function Bottino.oro(pos: Vector3, _padre: Instance?)
	oggetto("Oro", pos)
end

-- ---------------------------------------------------------------- mercante ----

--- Un banco del mercante: munizioni, carbone, bende in cambio di gettoni.
function Bottino.mercante(pos: Vector3, padre: Instance)
	local m = C.modello("Mercante", padre)
	C.part({ nome = "Banco", dimensione = Vector3.new(3, 2.4, 8), posizione = pos + Vector3.new(0, 1.2, 0),
		colore = "legnoChiaro", materiale = Enum.Material.Wood }, m)
	C.part({ nome = "Tenda", dimensione = Vector3.new(4, 0.3, 9), posizione = pos + Vector3.new(0, 5.6, 0),
		colore = "bandiera", materiale = Enum.Material.Fabric, collisione = false }, m)
	for s = -1, 1, 2 do
		C.part({ nome = "Palo", dimensione = Vector3.new(0.3, 5.4, 0.3), posizione = pos + Vector3.new(1.6, 2.8, s * 4.2),
			colore = "legnoScuro", materiale = Enum.Material.Wood }, m)
	end
	-- armi: fucile a pompa (una volta) e dinamite, sul lato del banco
	for i, arma in ipairs({ "Pompa", "Dinamite" }) do
		local merce = C.part({ nome = arma, dimensione = if arma == "Pompa" then Vector3.new(0.4, 0.4, 2.4) else Vector3.new(0.5, 0.5, 1.0),
			posizione = pos + Vector3.new(-0.4, 2.7, -4.6 - i * 0.9), rotazione = Vector3.new(0, 90, 0),
			colore = if arma == "Pompa" then "legnoScuro" else Color3.fromRGB(180, 40, 30), materiale = Enum.Material.Wood }, m)
		local prezzo = Config.PREZZI_ARMI[arma]
		local p = prompt(merce, if arma == "Pompa" then "Fucile a pompa (" .. prezzo .. " G)" else "2 dinamite (" .. prezzo .. " G)", "Armaiolo", 0.3, 6)
		p.Triggered:Connect(function(g: Player)
			if type(g) ~= "table" then return end
			if arma == "Pompa" and g:GetAttribute("HaPompa") then
				avvisa(g, "Hai gia' il fucile a pompa (tasto 4)")
				return
			end
			if Bottino.prendi(g, "Gettoni") < prezzo then
				avvisa(g, "Servono " .. prezzo .. " gettoni")
				return
			end
			Bottino.aggiungi(g, "Gettoni", -prezzo)
			if arma == "Pompa" then
				g:SetAttribute("HaPompa", true)
				avvisa(g, "Fucile a pompa! Tasto 4: devastante da vicino")
			else
				Bottino.aggiungi(g, "Dinamite", 2)
				avvisa(g, "+2 dinamite (tasto 3, clic per lanciare)")
			end
			suono("coin", pos)
		end)
	end

	local voci = { "Munizioni", "Carbone", "Bende" }
	for i, cosa in ipairs(voci) do
		local merce = C.part({ nome = cosa, dimensione = Vector3.new(1.2, 1.2, 1.2), posizione = pos + Vector3.new(0, 3.0, (i - 2) * 2.6),
			colore = if cosa == "Munizioni" then "rifiniture" elseif cosa == "Carbone" then "ruota" else "faro",
			materiale = Enum.Material.Fabric }, m)
		local prezzo = Config.PREZZI[cosa]
		local n = Config.CONFEZIONE[cosa]
		local p = prompt(merce, "Compra " .. n .. " (" .. prezzo .. " G)", cosa, 0, 6)
		p.Triggered:Connect(function(g: Player)
			if type(g) ~= "table" then return end
			if Bottino.prendi(g, "Gettoni") < prezzo then
				avvisa(g, "Servono " .. prezzo .. " gettoni (vendi l'oro alla cassaforte del treno)")
				return
			end
			if cosa == "Carbone" and Bottino.prendi(g, "Carbone") >= Config.PEZZI_IN_MANO_MAX then
				avvisa(g, "Hai le mani piene di carbone")
				return
			end
			Bottino.aggiungi(g, "Gettoni", -prezzo)
			Bottino.aggiungi(g, cosa, n)
			suono("coin", pos)
			avvisa(g, "Comprato: " .. n .. " " .. string.lower(cosa))
		end)
	end
end

-- ---------------------------------------------------------------- sul treno ----

--- Cassaforte sul vagone: l'oro diventa gettoni.
function Bottino.cassaforte(part: Part)
	local p = prompt(part, "Vendi l'oro", "Cassaforte", 0.5, 6)
	p.Triggered:Connect(function(g: Player)
		if type(g) ~= "table" then return end
		local oro = Bottino.prendi(g, "Oro")
		if oro <= 0 then
			avvisa(g, "Non hai oro. Cercalo nelle banche e nei forti")
			return
		end
		Bottino.metti(g, "Oro", 0)
		local guadagno = oro * Config.ORO_VALORE
		Bottino.aggiungi(g, "Gettoni", guadagno)
		Bottino.aggiungi(g, "OroVenduto", oro)
		venduto(g, oro)
		suono("coin", part.Position)
		avvisa(g, oro .. " lingotti venduti: +" .. guadagno .. " gettoni")
	end)
end

--- Bende: si usano con un tasto (lo manda il client).
function Bottino.usaBenda(g: Player)
	if Bottino.prendi(g, "Bende") <= 0 then
		avvisa(g, "Nessuna benda")
		return
	end
	Bottino.aggiungi(g, "Bende", -1)
	vita(g, Config.BENDE_VITA)
	avvisa(g, "+" .. Config.BENDE_VITA .. " vita")
end

return Bottino
