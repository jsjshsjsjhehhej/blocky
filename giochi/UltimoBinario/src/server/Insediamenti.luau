-- Citta' fantasma, forti e avamposti lungo i binari.
-- Costruiscono le part; casse, oro, mercanti e zombie li mettono i moduli
-- che si registrano con Insediamenti.usa{...} (cosi' questo file non dipende da loro).

local RS = game:GetService("ReplicatedStorage")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local C = require(Shared:WaitForChild("Costruisci"))

local Insediamenti = {}

local agganci = {
	cassa = function(_pos: Vector3, _padre: Instance, _ricca: boolean?) end,
	oro = function(_pos: Vector3, _padre: Instance) end,
	mercante = function(_pos: Vector3, _padre: Instance) end,
	bacheca = function(_pos: Vector3, _padre: Instance) end,
	zona = function(_centro: Vector3, _raggio: number, _quanti: number, _tipi: { string }) end,
}

function Insediamenti.usa(t)
	for k, f in pairs(t) do agganci[k] = f end
end

local L = Config.LATO_INSEDIAMENTI   -- da che parte dei binari

-- Se il pezzo e' grosso, ogni tanto lascia passare un frame (niente scatti).
local fatte = 0
local function respira()   -- (ora lo fa gia' Costruisci.part; resta per chiarezza)
	fatte += 1
	if fatte % 40 == 0 and coroutine.isyieldable() then task.wait() end
end

local function P(o, padre)
	respira()
	return C.part(o, padre)
end

-- ---------------------------------------------------------------- pezzi ----

--- Un edificio di legno con porta verso la strada (dirPorta = +1 o -1 lungo X).
local function edificio(padre: Instance, cx: number, cz: number, largo: number, fondo: number, alto: number,
	colore: string, dirPorta: number, insegna: string?)
	local m = C.modello(insegna or "Casa", padre)
	local y0 = 0
	P({ nome = "Pavimento", dimensione = Vector3.new(fondo, 0.6, largo), posizione = Vector3.new(cx, y0 + 0.3, cz),
		colore = "legnoScuro", materiale = Enum.Material.Wood }, m)
	-- muri: dietro, fianchi, davanti con la porta
	local xd, xp = cx - dirPorta * fondo / 2, cx + dirPorta * fondo / 2
	P({ nome = "Muro", dimensione = Vector3.new(0.5, alto, largo), posizione = Vector3.new(xd, alto / 2, cz),
		colore = colore, materiale = Enum.Material.Wood }, m)
	for s = -1, 1, 2 do
		P({ nome = "Muro", dimensione = Vector3.new(fondo, alto, 0.5), posizione = Vector3.new(cx, alto / 2, cz + s * largo / 2),
			colore = colore, materiale = Enum.Material.Wood }, m)
	end
	local porta = 4.2
	local lato = (largo - porta) / 2
	for s = -1, 1, 2 do
		P({ nome = "Facciata", dimensione = Vector3.new(0.5, alto, lato), posizione = Vector3.new(xp, alto / 2, cz + s * (porta / 2 + lato / 2)),
			colore = colore, materiale = Enum.Material.Wood }, m)
	end
	P({ nome = "SopraPorta", dimensione = Vector3.new(0.5, alto - 6, porta), posizione = Vector3.new(xp, 6 + (alto - 6) / 2, cz),
		colore = colore, materiale = Enum.Material.Wood }, m)
	-- finestre (vetro scuro) sulla facciata
	for s = -1, 1, 2 do
		P({ nome = "Finestra", dimensione = Vector3.new(0.3, 2.2, 2.4), posizione = Vector3.new(xp + dirPorta * 0.2, 4.2, cz + s * (porta / 2 + lato / 2)),
			colore = "vetro", materiale = Enum.Material.Glass, collisione = false }, m)
	end
	-- tetto a due falde: da lontano non e' una scatola
	local meta = fondo / 2 + 0.6
	local alza = meta * 0.42
	P({ nome = "Faldale", dimensione = Vector3.new(meta * 1.2, 0.35, largo + 1.4),
		posizione = Vector3.new(cx - meta * 0.42, alto + alza * 0.55, cz),
		rotazione = Vector3.new(0, 0, 32), colore = "tetto", materiale = Enum.Material.Wood, collisione = false }, m)
	P({ nome = "Faldale", dimensione = Vector3.new(meta * 1.2, 0.35, largo + 1.4),
		posizione = Vector3.new(cx + meta * 0.42, alto + alza * 0.55, cz),
		rotazione = Vector3.new(0, 0, -32), colore = "tetto", materiale = Enum.Material.Wood, collisione = false }, m)
	P({ nome = "Colmo", dimensione = Vector3.new(0.45, 0.4, largo + 1.6), posizione = Vector3.new(cx, alto + alza + 0.15, cz),
		colore = "legnoScuro", materiale = Enum.Material.Wood, collisione = false }, m)
	P({ nome = "Camino", dimensione = Vector3.new(1.2, 2.6, 1.2), posizione = Vector3.new(cx + fondo * 0.22, alto + 1.5, cz + largo * 0.28),
		colore = "mattone", materiale = Enum.Material.Slate, collisione = false }, m)
	P({ nome = "FacciataAlta", dimensione = Vector3.new(0.6, 3, largo + 1), posizione = Vector3.new(xp + dirPorta * 0.3, alto + 1.5, cz),
		colore = colore, materiale = Enum.Material.Wood }, m)
	-- portico
	P({ nome = "Portico", dimensione = Vector3.new(3, 0.4, largo + 1), posizione = Vector3.new(xp + dirPorta * 1.8, 0.2, cz),
		colore = "legnoChiaro", materiale = Enum.Material.Wood }, m)
	P({ nome = "TettoPortico", dimensione = Vector3.new(3.4, 0.3, largo + 1), posizione = Vector3.new(xp + dirPorta * 1.9, 6.6, cz),
		colore = "tetto", materiale = Enum.Material.Wood, collisione = false }, m)
	for s = -1, 1, 2 do
		P({ nome = "Palo", dimensione = Vector3.new(0.35, 6.4, 0.35), posizione = Vector3.new(xp + dirPorta * 3.3, 3.3, cz + s * largo / 2),
			colore = "legnoScuro", materiale = Enum.Material.Wood }, m)
	end
	if insegna then
		P({ nome = "Insegna", dimensione = Vector3.new(0.3, 1.4, largo * 0.6), posizione = Vector3.new(xp + dirPorta * 0.7, alto + 1.6, cz),
			colore = "rifiniture", materiale = Enum.Material.Wood, collisione = false }, m)
	end
	return m, Vector3.new(cx, 0, cz)
end

local function torreAcqua(padre: Instance, x: number, z: number)
	for _, d in ipairs({ { -1.6, -1.6 }, { 1.6, -1.6 }, { -1.6, 1.6 }, { 1.6, 1.6 } }) do
		P({ nome = "Gamba", dimensione = Vector3.new(0.5, 12, 0.5), posizione = Vector3.new(x + d[1], 6, z + d[2]),
			colore = "legnoScuro", materiale = Enum.Material.Wood }, padre)
	end
	P({ nome = "Serbatoio", dimensione = Vector3.new(6, 5, 6), posizione = Vector3.new(x, 14.5, z),
		colore = "legnoVecchio", materiale = Enum.Material.Wood, forma = Enum.PartType.Cylinder }, padre)
	P({ nome = "Cappello", dimensione = Vector3.new(6.6, 0.6, 6.6), posizione = Vector3.new(x, 17.3, z),
		colore = "tetto", materiale = Enum.Material.Wood, forma = Enum.PartType.Cylinder }, padre)
end

local function banchina(padre: Instance, z: number, lungo: number)
	P({ nome = "Banchina", dimensione = Vector3.new(8, 1.4, lungo), posizione = Vector3.new(L * 9.5, 0.7, z),
		colore = "legnoVecchio", materiale = Enum.Material.Wood }, padre)
	P({ nome = "Pensilina", dimensione = Vector3.new(7, 0.4, lungo * 0.5), posizione = Vector3.new(L * 10, 7.5, z),
		colore = "tetto", materiale = Enum.Material.Wood, collisione = false }, padre)
	for s = -1, 1, 2 do
		P({ nome = "Palo", dimensione = Vector3.new(0.4, 6.2, 0.4), posizione = Vector3.new(L * 12.5, 4.3, z + s * lungo * 0.22),
			colore = "legnoScuro", materiale = Enum.Material.Wood }, padre)
	end
end

-- ---------------------------------------------------------------- tipi ----

--- Avamposto: banchina, baracca, casse.
function Insediamenti.avamposto(padre: Instance, z: number, _rng: Random)
	local m = C.modello("Avamposto", padre)
	banchina(m, z, 30)
	edificio(m, L * 22, z, 10, 9, 7, "vagone", -L, nil)
	agganci.cassa(Vector3.new(L * 11, 2.3, z - 8), m)
	agganci.cassa(Vector3.new(L * 22, 1.5, z + 2), m)
	agganci.cassa(Vector3.new(L * 24, 1.5, z - 2), m)
	agganci.zona(Vector3.new(L * 24, 3, z), 18, 2, { "Zombie" })
end

--- Citta' fantasma: strada parallela ai binari, case sui due lati, banca con l'oro,
--- saloon, mercante alla stazione, torre dell'acqua. Piena di zombie.
function Insediamenti.citta(padre: Instance, z: number, rng: Random)
	local m = C.modello("Citta", padre)
	banchina(m, z, 40)
	local strada = L * 44
	P({ nome = "Strada", dimensione = Vector3.new(14, 0.2, 130), posizione = Vector3.new(strada, 0.1, z),
		colore = "strada", materiale = Enum.Material.Sand, collisione = false }, m)
	-- vialetto dalla stazione alla strada
	P({ nome = "Vialetto", dimensione = Vector3.new(30, 0.2, 6), posizione = Vector3.new(L * 26, 0.1, z),
		colore = "strada", materiale = Enum.Material.Sand, collisione = false }, m)
	agganci.mercante(Vector3.new(L * 13, 1.4, z + 12), m)
	agganci.bacheca(Vector3.new(L * 12, 1.4, z - 12), m)

	local colori = { "vagone", "legnoChiaro", "intonaco", "mattone", "legnoVecchio" }
	local nomi = { "Banca", "Saloon", "Emporio", "Sceriffo", "Hotel", "Casa", "Casa", "Fabbro" }
	-- mischia l'ordine (la banca non sempre nello stesso posto)
	for i = #nomi, 2, -1 do
		local j = rng:NextInteger(1, i)
		nomi[i], nomi[j] = nomi[j], nomi[i]
	end
	local k = 0
	for lato = -1, 1, 2 do
		for i = -2, 1 do
			k += 1
			local nome = nomi[k]
			local cz = z + i * 28 + 14 + rng:NextNumber(-2, 2)
			local largo = rng:NextNumber(14, 18)
			local fondo = rng:NextNumber(12, 16)
			local alto = rng:NextNumber(8, 11)
			local cx = strada + lato * (7 + fondo / 2 + 1.5)
			local col = if nome == "Banca" then "mattone" else colori[rng:NextInteger(1, #colori)]
			local casa = edificio(m, cx, cz, largo, fondo, alto, col, -lato, nome)
			-- dentro: casse, e l'oro nella banca
			if nome == "Banca" then
				for g = 1, 4 do
					agganci.oro(Vector3.new(cx - lato * (fondo / 2 - 2.5), 1.2, cz - 4 + g * 1.8), casa)
				end
				agganci.cassa(Vector3.new(cx - lato * (fondo / 2 - 2.5), 1.5, cz + 5), casa, true)
			else
				agganci.cassa(Vector3.new(cx - lato * (fondo / 2 - 2.5), 1.5, cz + rng:NextNumber(-4, 4)), casa)
				if rng:NextNumber() < 0.35 then
					agganci.oro(Vector3.new(cx, 1.0, cz - largo / 2 + 2), casa)
				end
			end
		end
	end
	torreAcqua(m, strada + L * 24, z - 40)
	-- carretto rotto in mezzo alla strada
	P({ nome = "Carretto", dimensione = Vector3.new(4, 1.6, 7), posizione = Vector3.new(strada + 2, 1.6, z + 30),
		rotazione = Vector3.new(0, 15, 8), colore = "legnoVecchio", materiale = Enum.Material.Wood }, m)
	agganci.zona(Vector3.new(strada, 3, z), 55, rng:NextInteger(8, 11), { "Zombie", "Zombie", "Zombie", "Corridore", "Bruto" })
end

--- Forte: mura di pietra con torri agli angoli, cancello verso i binari,
--- caserma e armeria (casse ricche, oro). Lo difendono i Soldati.
function Insediamenti.forte(padre: Instance, z: number, rng: Random)
	local m = C.modello("Forte", padre)
	banchina(m, z, 24)
	local lato = 60
	local cx = L * (24 + lato / 2)
	local alto = 9
	local spessore = 1.6
	local cancello = 12
	-- muro lontano e fianchi
	P({ nome = "Mura", dimensione = Vector3.new(spessore, alto, lato), posizione = Vector3.new(cx + L * lato / 2, alto / 2, z),
		colore = "pietraForte", materiale = Enum.Material.Slate }, m)
	for s = -1, 1, 2 do
		P({ nome = "Mura", dimensione = Vector3.new(lato, alto, spessore), posizione = Vector3.new(cx, alto / 2, z + s * lato / 2),
			colore = "pietraForte", materiale = Enum.Material.Slate }, m)
	end
	-- muro verso i binari, con il cancello
	local pezzo = (lato - cancello) / 2
	for s = -1, 1, 2 do
		P({ nome = "Mura", dimensione = Vector3.new(spessore, alto, pezzo), posizione = Vector3.new(cx - L * lato / 2, alto / 2, z + s * (cancello / 2 + pezzo / 2)),
			colore = "pietraForte", materiale = Enum.Material.Slate }, m)
	end
	P({ nome = "Architrave", dimensione = Vector3.new(spessore + 0.4, 2, cancello + 2), posizione = Vector3.new(cx - L * lato / 2, alto + 1, z),
		colore = "legnoScuro", materiale = Enum.Material.Wood }, m)
	-- merli
	for i = -lato / 2 + 2, lato / 2 - 2, 4 do
		P({ nome = "Merlo", dimensione = Vector3.new(spessore, 1.4, 1.8), posizione = Vector3.new(cx + L * lato / 2, alto + 0.7, z + i),
			colore = "pietraForte", materiale = Enum.Material.Slate, collisione = false }, m)
	end
	-- torri agli angoli
	for _, a in ipairs({ { -1, -1 }, { -1, 1 }, { 1, -1 }, { 1, 1 } }) do
		local tx, tz = cx + a[1] * lato / 2, z + a[2] * lato / 2
		P({ nome = "Torre", dimensione = Vector3.new(6, 15, 6), posizione = Vector3.new(tx, 7.5, tz),
			colore = "pietraForte", materiale = Enum.Material.Slate }, m)
		P({ nome = "TettoTorre", dimensione = Vector3.new(7, 1, 7), posizione = Vector3.new(tx, 15.5, tz),
			colore = "tetto", materiale = Enum.Material.Wood }, m)
		P({ nome = "Punta", dimensione = Vector3.new(4.2, 2.4, 4.2), posizione = Vector3.new(tx, 17.2, tz),
			colore = "pietraForte", materiale = Enum.Material.Slate, collisione = false }, m)
		P({ nome = "Punta", dimensione = Vector3.new(2.2, 2.2, 2.2), posizione = Vector3.new(tx, 19.2, tz),
			colore = "tetto", materiale = Enum.Material.Wood, collisione = false }, m)
	end
	-- pennone con bandiera
	P({ nome = "Pennone", dimensione = Vector3.new(0.4, 18, 0.4), posizione = Vector3.new(cx, 9, z),
		colore = "legnoScuro", materiale = Enum.Material.Wood }, m)
	P({ nome = "Bandiera", dimensione = Vector3.new(0.15, 2.4, 4), posizione = Vector3.new(cx, 16.6, z + 2.1),
		colore = "bandiera", materiale = Enum.Material.Fabric, collisione = false }, m)
	-- caserma e armeria dentro
	local caserma = edificio(m, cx + L * 16, z - 16, 16, 12, 8, "legnoVecchio", -L, "Caserma")
	local armeria = edificio(m, cx + L * 16, z + 16, 12, 10, 7, "pietraForte", -L, "Armeria")
	agganci.cassa(Vector3.new(cx + L * 19, 1.5, z - 18), caserma)
	agganci.cassa(Vector3.new(cx + L * 19, 1.5, z + 14), armeria, true)
	agganci.cassa(Vector3.new(cx + L * 19, 1.5, z + 18), armeria, true)
	for g = 1, 3 do
		agganci.oro(Vector3.new(cx + L * 13, 1.0, z + 12 + g * 1.8), armeria)
	end
	-- sacchi di sabbia nel cortile
	for i = 1, 4 do
		P({ nome = "Sacchi", dimensione = Vector3.new(4, 1.6, 1.6), posizione = Vector3.new(cx + rng:NextNumber(-12, 6), 0.8, z + rng:NextNumber(-10, 10)),
			rotazione = Vector3.new(0, rng:NextNumber(0, 90), 0), colore = "sabbiaScura", materiale = Enum.Material.Fabric }, m)
	end
	agganci.zona(Vector3.new(cx, 3, z), 28, rng:NextInteger(7, 9), { "Soldato", "Soldato", "Zombie", "Bruto" })
end

--- Accampamento dei banditi: tende, carri messi in cerchio, un fuoco acceso,
--- casse col bottino rubato. I Banditi sparano da lontano.
function Insediamenti.accampamento(padre: Instance, z: number, rng: Random)
	local m = C.modello("Accampamento", padre)
	local cx = L * 48
	-- fuoco al centro
	for i = 0, 5 do
		local a = i / 6 * math.pi * 2
		P({ nome = "Pietra", dimensione = Vector3.new(1, 0.6, 1), posizione = Vector3.new(cx + math.cos(a) * 1.6, 0.3, z + math.sin(a) * 1.6),
			colore = "roccia", materiale = Enum.Material.Slate }, m)
	end
	local fuoco = P({ nome = "Fuoco", dimensione = Vector3.new(1.4, 1.6, 1.4), posizione = Vector3.new(cx, 0.8, z),
		colore = "fuoco", neon = true, forma = Enum.PartType.Ball, collisione = false }, m)
	fuoco.PointLight = true
	fuoco.LightRange = 26
	fuoco.LightBrightness = 2
	-- tende a capanna (due falde inclinate) in cerchio
	for i = 0, 4 do
		local a = i / 5 * math.pi * 2 + 0.3
		local tx, tz = cx + math.cos(a) * 14, z + math.sin(a) * 14
		local giro = math.deg(-a) + 90
		local tela = if i % 2 == 0 then "intonaco" else "sabbiaScura"
		for s = -1, 1, 2 do
			P({ nome = "Tenda", dimensione = Vector3.new(3.4, 0.2, 5), posizione = Vector3.new(tx, 1.6, tz),
				rotazione = Vector3.new(0, giro, s * 52), colore = tela, materiale = Enum.Material.Fabric }, m)
		end
	end
	-- carri coperti a fare da muro
	for i = 0, 2 do
		local a = i / 3 * math.pi * 2 + 1.1
		local wx, wz = cx + math.cos(a) * 24, z + math.sin(a) * 24
		local giro = math.deg(-a)
		P({ nome = "Carro", dimensione = Vector3.new(3.6, 1.2, 7), posizione = Vector3.new(wx, 1.8, wz),
			rotazione = Vector3.new(0, giro, 0), colore = "legnoVecchio", materiale = Enum.Material.Wood }, m)
		P({ nome = "Telone", dimensione = Vector3.new(3.4, 2.8, 6), posizione = Vector3.new(wx, 3.6, wz),
			rotazione = Vector3.new(0, giro, 0), colore = "intonaco", materiale = Enum.Material.Fabric,
			forma = Enum.PartType.Cylinder }, m)
	end
	-- bottino rubato: casse ricche e qualche lingotto
	agganci.cassa(Vector3.new(cx + 5, 0.9, z + 4), m, true)
	agganci.cassa(Vector3.new(cx - 6, 0.9, z - 3), m)
	for g = 1, 2 do
		agganci.oro(Vector3.new(cx + 4 + g * 1.4, 0.4, z - 5), m)
	end
	agganci.zona(Vector3.new(cx, 3, z), 26, rng:NextInteger(5, 7), { "Bandito", "Bandito", "Bandito", "Zombie" })
end

--- La meta: un faro alto, acceso, alla fine dei binari.
function Insediamenti.faro(padre: Instance, z: number)
	local m = C.modello("Faro", padre)
	P({ nome = "Fermo", dimensione = Vector3.new(7, 2.5, 1.5), posizione = Vector3.new(0, 1.25, z - 4),
		colore = "rifiniture", materiale = Enum.Material.Metal }, m)
	P({ nome = "Base", dimensione = Vector3.new(14, 2, 14), posizione = Vector3.new(L * 18, 1, z + 8),
		colore = "roccia", materiale = Enum.Material.Slate }, m)
	for i = 0, 5 do
		local col = if i % 2 == 0 then "faro" else "bandiera"
		P({ nome = "Torre", dimensione = Vector3.new(8 - i * 0.6, 6, 8 - i * 0.6), posizione = Vector3.new(L * 18, 5 + i * 6, z + 8),
			colore = col, materiale = Enum.Material.Concrete }, m)
	end
	P({ nome = "Galleria", dimensione = Vector3.new(11, 0.45, 11), posizione = Vector3.new(L * 18, 37.2, z + 8),
		colore = "rifiniture", materiale = Enum.Material.Metal, collisione = false }, m)
	local luce = P({ nome = "Luce", dimensione = Vector3.new(4, 4, 4), posizione = Vector3.new(L * 18, 40, z + 8),
		colore = "fuoco", neon = true, forma = Enum.PartType.Ball }, m)
	luce.PointLight = true
	luce.LightRange = 48
	luce.LightBrightness = 2.4
	P({ nome = "Cupola", dimensione = Vector3.new(5.2, 2.4, 5.2), posizione = Vector3.new(L * 18, 43, z + 8),
		colore = "tetto", materiale = Enum.Material.Metal, forma = Enum.PartType.Ball, collisione = false }, m)
end

return Insediamenti
