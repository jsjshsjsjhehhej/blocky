-- Il mondo a pezzi lungo i binari: si costruisce davanti al treno e si toglie
-- dietro, cosi' le part restano poche anche su un telefono.
-- Ogni Config.TAPPA stud c'e' un insediamento (Config.SEQUENZA): citta', forte, avamposto.

local RS = game:GetService("ReplicatedStorage")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local C = require(Shared:WaitForChild("Costruisci"))
local Insediamenti = require(script.Parent.Insediamenti)

local Mondo = {}

local cartella = Instance.new("Folder")
cartella.Name = "Mondo"
cartella.Parent = workspace

local pezzi: { [number]: Model } = {}
local inCostruzione = false

-- Numeri a caso ma sempre uguali per lo stesso pezzo: il mondo non cambia se lo rifai.
local function casoPer(indice: number): Random
	return Random.new(Config.SEME + indice * 7919)
end

local function binari(m: Model, z0: number, lungo: number)
	local zc = z0 + lungo / 2
	C.part({ nome = "Terreno", dimensione = Vector3.new(800, 1, lungo), posizione = Vector3.new(0, -0.5, zc),
		colore = "sabbia", materiale = Enum.Material.Sand }, m)
	-- bordi invisibili: dal deserto non si cade fuori
	for s = -1, 1, 2 do
		C.part({ nome = "Bordo", dimensione = Vector3.new(2, 40, lungo), posizione = Vector3.new(s * 395, 20, zc),
			colore = "sabbia", trasparenza = 1 }, m)
	end
	C.part({ nome = "Massicciata", dimensione = Vector3.new(8, 0.4, lungo), posizione = Vector3.new(0, 0.2, zc),
		colore = "pietrisco", materiale = Enum.Material.Slate }, m)
	for x = -1, 1, 2 do
		C.part({ nome = "Rotaia", dimensione = Vector3.new(0.35, 0.45, lungo), posizione = Vector3.new(x * 2.4, 0.85, zc),
			colore = "rotaia", materiale = Enum.Material.Metal, collisione = false }, m)
	end
	for z = z0 + 2, z0 + lungo - 1, 4 do
		C.part({ nome = "Traversina", dimensione = Vector3.new(6.6, 0.3, 1), posizione = Vector3.new(0, 0.55, z),
			colore = "traversina", materiale = Enum.Material.Wood, collisione = false }, m)
	end
	-- pali del telegrafo (a destra, gli insediamenti sono a sinistra): fanno sentire la velocita'
	for z = z0 + 20, z0 + lungo, 40 do
		C.part({ nome = "Palo", dimensione = Vector3.new(0.5, 11, 0.5), posizione = Vector3.new(9, 5.5, z),
			colore = "legnoVecchio", materiale = Enum.Material.Wood }, m)
		C.part({ nome = "Traverso", dimensione = Vector3.new(3, 0.35, 0.35), posizione = Vector3.new(9, 10.2, z),
			colore = "legnoVecchio", materiale = Enum.Material.Wood, collisione = false }, m)
	end
end

local function decorazioni(m: Model, z0: number, lungo: number, rng: Random, libero: (number, number) -> boolean)
	for _ = 1, 16 do
		local lato = if rng:NextNumber() < 0.5 then -1 else 1
		local x = lato * rng:NextNumber(16, 170)
		local z = z0 + rng:NextNumber(0, lungo)
		if not libero(x, z) then continue end
		local tipo = rng:NextInteger(1, 10)
		if tipo <= 4 then
			local s = rng:NextNumber(2, 7)
			C.part({ nome = "Roccia", dimensione = Vector3.new(s, s * rng:NextNumber(0.5, 1.1), s * rng:NextNumber(0.7, 1.4)),
				posizione = Vector3.new(x, s * 0.35, z), rotazione = Vector3.new(0, rng:NextNumber(0, 90), 0),
				colore = "roccia", materiale = Enum.Material.Slate }, m)
		elseif tipo <= 8 then
			local h = rng:NextNumber(5, 9)
			C.part({ nome = "Cactus", dimensione = Vector3.new(1.4, h, 1.4), posizione = Vector3.new(x, h / 2, z),
				colore = "cactus", materiale = Enum.Material.Grass }, m)
			C.part({ nome = "Braccio", dimensione = Vector3.new(0.9, h * 0.35, 0.9), posizione = Vector3.new(x + 1.4, h * 0.62, z),
				colore = "cactus", materiale = Enum.Material.Grass, collisione = false }, m)
			C.part({ nome = "Braccio", dimensione = Vector3.new(0.9, h * 0.28, 0.9), posizione = Vector3.new(x - 1.3, h * 0.5, z),
				colore = "cactus", materiale = Enum.Material.Grass, collisione = false }, m)
		else
			local h = rng:NextNumber(7, 11)
			C.part({ nome = "Tronco", dimensione = Vector3.new(0.9, h, 0.9), posizione = Vector3.new(x, h / 2, z),
				colore = "legnoVecchio", materiale = Enum.Material.Wood }, m)
			C.part({ nome = "Ramo", dimensione = Vector3.new(4, 0.5, 0.5), posizione = Vector3.new(x + 1.5, h * 0.8, z),
				rotazione = Vector3.new(0, 0, 25), colore = "legnoVecchio", materiale = Enum.Material.Wood, collisione = false }, m)
		end
	end
end

--- Un fiume che taglia il deserto, con un ponte a traliccio sotto i binari.
local function fiume(m: Model, z: number)
	local largo = 26
	-- l'acqua sopra la sabbia (il terreno e' una part sola), con le rive piu' scure
	C.part({ nome = "Fiume", dimensione = Vector3.new(420, 0.3, largo), posizione = Vector3.new(0, 0.05, z),
		colore = Color3.fromRGB(58, 104, 128), materiale = Enum.Material.Water, collisione = false }, m)
	for s = -1, 1, 2 do
		C.part({ nome = "Riva", dimensione = Vector3.new(420, 0.25, 3), posizione = Vector3.new(0, 0.08, z + s * (largo / 2 + 1.2)),
			colore = "sabbiaScura", materiale = Enum.Material.Sand, collisione = false }, m)
	end
	-- il ponte: travi lungo i binari, piloni, croci di Sant'Andrea ai lati
	for x = -1, 1, 2 do
		C.part({ nome = "Trave", dimensione = Vector3.new(0.8, 1.0, largo + 8), posizione = Vector3.new(x * 3.2, 0.1, z),
			colore = "legnoScuro", materiale = Enum.Material.Wood }, m)
		C.part({ nome = "Parapetto", dimensione = Vector3.new(0.3, 0.3, largo + 8), posizione = Vector3.new(x * 3.9, 2.0, z),
			colore = "legnoScuro", materiale = Enum.Material.Wood, collisione = false }, m)
	end
	for dz = -largo / 2, largo / 2, 6.5 do
		for x = -1, 1, 2 do
			C.part({ nome = "Pilone", dimensione = Vector3.new(0.7, 4.4, 0.7), posizione = Vector3.new(x * 3.2, -1.6, z + dz),
				colore = "legnoScuro", materiale = Enum.Material.Wood }, m)
			C.part({ nome = "Ritto", dimensione = Vector3.new(0.25, 2.0, 0.25), posizione = Vector3.new(x * 3.9, 1.0, z + dz),
				colore = "legnoScuro", materiale = Enum.Material.Wood, collisione = false }, m)
			C.part({ nome = "Croce", dimensione = Vector3.new(0.25, 0.25, 7.4), posizione = Vector3.new(x * 3.4, -1.4, z + dz + 3.2),
				rotazione = Vector3.new(30, 0, 0), colore = "legnoScuro", materiale = Enum.Material.Wood, collisione = false }, m)
		end
	end
end

--- Una mesa all'orizzonte: rocce rosse a strati, piatte in cima.
local function mesa(m: Model, x: number, z: number, rng: Random)
	local larg = rng:NextNumber(30, 60)
	local alto = rng:NextNumber(22, 40)
	local strati = 4
	for i = 0, strati - 1 do
		local k = 1 - i * 0.12
		local col = if i % 2 == 0 then Color3.fromRGB(178, 104, 70) else Color3.fromRGB(196, 124, 84)
		C.part({ nome = "Mesa", dimensione = Vector3.new(larg * k, alto / strati, larg * 0.8 * k),
			posizione = Vector3.new(x, alto / strati * (i + 0.5), z), rotazione = Vector3.new(0, rng:NextNumber(-8, 8), 0),
			colore = col, materiale = Enum.Material.Sand }, m)
	end
	-- un sasso caduto ai piedi
	C.part({ nome = "Masso", dimensione = Vector3.new(6, 4, 5), posizione = Vector3.new(x + larg * 0.55, 2, z + rng:NextNumber(-10, 10)),
		rotazione = Vector3.new(0, rng:NextNumber(0, 90), 12), colore = Color3.fromRGB(170, 100, 68), materiale = Enum.Material.Sand }, m)
end

--- Che insediamento c'e' alla tappa n (1, 2, 3...).
function Mondo.tipoTappa(n: number): string
	return Config.SEQUENZA[((n - 1) % #Config.SEQUENZA) + 1]
end

--- Le tappe che cadono nel pezzo [z0, z0 + lungo).
local function tappeIn(z0: number, lungo: number): { { z: number, tipo: string } }
	local out = {}
	local n = math.max(1, math.ceil(z0 / Config.TAPPA))
	while n * Config.TAPPA < z0 + lungo do
		local z = n * Config.TAPPA
		if z < Config.LUNGHEZZA_VIAGGIO - 200 then
			table.insert(out, { z = z, tipo = Mondo.tipoTappa(n) })
		end
		n += 1
	end
	return out
end

local function costruisci(indice: number)
	if pezzi[indice] or indice < 0 then return end
	local lungo = Config.PEZZO_LUNGO
	local z0 = indice * lungo
	if z0 > Config.LUNGHEZZA_VIAGGIO + lungo then return end
	local m = C.modello("Pezzo" .. indice, cartella)
	pezzi[indice] = m
	binari(m, z0, lungo)
	local rng = casoPer(indice)
	local tappe = tappeIn(z0, lungo)
	-- niente cactus in mezzo alle case
	local function libero(x: number, z: number): boolean
		for _, t in ipairs(tappe) do
			if math.sign(x) == Config.LATO_INSEDIAMENTI and math.abs(z - t.z) < 80 and math.abs(x) < 110 then return false end
		end
		return true
	end
	decorazioni(m, z0, lungo, rng, libero)
	-- fiumi con il ponte
	local n = math.max(1, math.ceil(z0 / Config.FIUME_OGNI))
	local zf = n * Config.FIUME_OGNI + 300
	if zf >= z0 and zf < z0 + lungo and zf < Config.LUNGHEZZA_VIAGGIO - 200 then fiume(m, zf) end
	-- mese all'orizzonte (dal lato senza insediamenti, lontano)
	if rng:NextNumber() < Config.MESA_PROB then
		mesa(m, -Config.LATO_INSEDIAMENTI * rng:NextNumber(120, 190), z0 + rng:NextNumber(20, lungo - 20), rng)
	end
	for _, t in ipairs(tappe) do
		local f = Insediamenti[t.tipo]
		if f then f(m, t.z, casoPer(t.z)) end
	end
	local meta = Config.LUNGHEZZA_VIAGGIO
	if meta >= z0 and meta < z0 + lungo then Insediamenti.faro(m, meta) end
end

--- Tiene pronti i pezzi attorno a z (la posizione del treno): uno alla volta,
--- in un task suo, cosi' le citta' grandi non bloccano il gioco.
function Mondo.aggiorna(z: number)
	local qui = math.floor(z / Config.PEZZO_LUNGO)
	for i, m in pairs(pezzi) do
		if i < qui - Config.PEZZI_DIETRO then
			m:Destroy()
			pezzi[i] = nil
		end
	end
	if inCostruzione then return end
	for i = qui - Config.PEZZI_DIETRO, qui + Config.PEZZI_AVANTI do
		if i >= 0 and not pezzi[i] then
			inCostruzione = true
			task.spawn(function()
				local ok, err = pcall(costruisci, i)
				if not ok then warn("[Mondo] pezzo " .. i .. ": " .. tostring(err)) end
				inCostruzione = false
			end)
			return
		end
	end
end

--- Tutto subito (all'inizio, prima che il giocatore veda).
function Mondo.preparaSubito(z: number)
	local qui = math.floor(z / Config.PEZZO_LUNGO)
	for i = math.max(0, qui - Config.PEZZI_DIETRO), qui + 1 do
		costruisci(i)
	end
end

function Mondo.ricomincia()
	for i, m in pairs(pezzi) do
		m:Destroy()
		pezzi[i] = nil
	end
end

return Mondo
