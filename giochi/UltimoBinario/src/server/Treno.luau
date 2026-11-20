-- La locomotiva, il tender e il vagone: si costruiscono una volta e poi scorrono
-- sui binari. Il treno va verso +Z. "retro" = la coda del vagone.
--
--   dz  0 .. 14    vagone (pianale con sponde a paletti, casse, barili, cassaforte)
--   dz 15 .. 23    tender (carbone, officina, passerella a destra per la cabina)
--   dz 24 .. 33    cabina (alta, con il sedile del macchinista e la leva)
--   dz 33 .. 49    caldaia, camera fumo, spazzaneve
--
-- Si guida col regolatore (0 = fermo, 1 = tutta forza): la leva lo porta a 0 o 1,
-- seduti al posto del macchinista W e S lo cambiano piano (Guida.client).
--
-- Nota sui cilindri: in Blocky l'asse del cilindro e' Y (su Roblox e' X), per
-- questo ruote e caldaia sono girate con Orientation. Sta tutto in cilindro().

local RS = game:GetService("ReplicatedStorage")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local C = require(Shared:WaitForChild("Costruisci"))

local Treno = {}

Treno.LUNGO = 49             -- dalla coda del vagone alla punta dello spazzaneve
Treno.PIANO_Y = 2.6          -- altezza del pavimento (dove si sta in piedi)
Treno.LARGO = 7
Treno.SOPRA_FINO = 33        -- fin dove si puo' stare (vagone, tender, cabina)
local L0 = 24                -- dove comincia la locomotiva
local CABINA = 9             -- lunghezza della cabina
local TETTO_Y = 10.4         -- altezza del tetto della cabina (dentro: 7.8 stud)

local modello: Model? = nil
local retroZ = Config.PARTENZA_Z
local velocita = 0
local regolatore = 0          -- 0..1
local carbone = Config.CARBONE_INIZIO
-- potenziamenti dell'officina e guasti
local carboneMax = Config.CARBONE_MAX
local mulVelocita = 1
local guasto = false

local leva: Part? = nil
local bocca: Part? = nil
local fumoDa: Part? = nil
local sedile: Part? = nil
local cassaforte: Part? = nil
local promptLeva: ProximityPrompt? = nil
local promptCaldaia: ProximityPrompt? = nil
local fumi: { Part } = {}
local luci: { Part } = {}
local bielle: { { part: Part, x: number, y: number, z: number } } = {}
local angoloRuote = 0

-- colori di questo treno
local NERO = Color3.fromRGB(34, 36, 40)
local GRIGIO = Color3.fromRGB(58, 62, 68)
local ROSSO = Color3.fromRGB(128, 34, 28)
local OTTONE = Color3.fromRGB(196, 150, 62)
local LEGNO = Color3.fromRGB(118, 76, 44)
local LEGNO_SCURO = Color3.fromRGB(76, 50, 32)
local LAMIERA = Color3.fromRGB(80, 84, 90)

local function part(m: Instance, nome: string, size: Vector3, pos: Vector3, col: Color3, mat: Enum.Material?,
	rot: Vector3?, collide: boolean?, forma: Enum.PartType?): Part
	-- (SmoothPlastic: il Metal di Blocky riflette il cielo e il ferro nero sembra azzurro)
	return C.part({ nome = nome, dimensione = size, posizione = pos, colore = col, materiale = mat or Enum.Material.SmoothPlastic,
		rotazione = rot, collisione = collide, forma = forma }, m)
end

--- Cilindro lungo l'asse X, Y o Z del mondo.
local function cilindro(m: Instance, nome: string, diametro: number, lungo: number, pos: Vector3, asse: string,
	col: Color3, mat: Enum.Material?, collide: boolean?): Part
	local rot = if asse == "X" then Vector3.new(0, 0, 90) elseif asse == "Z" then Vector3.new(90, 0, 0) else Vector3.new(0, 0, 0)
	return part(m, nome, Vector3.new(diametro, lungo, diametro), pos, col, mat, rot, collide, Enum.PartType.Cylinder)
end

-- ================================================================ pezzi ====

local function vagone(m: Model, at: (number, number, number) -> Vector3): Part
	local pianale = part(m, "Pianale", Vector3.new(7, 0.6, 14), at(0, 2.3, 7), LEGNO, Enum.Material.Wood)
	part(m, "Longherone", Vector3.new(7.2, 0.5, 14.4), at(0, 1.8, 7), NERO)
	for x = -1, 1, 2 do
		part(m, "Sponda", Vector3.new(0.25, 1.1, 14), at(x * 3.4, 3.15, 7), LEGNO_SCURO, Enum.Material.Wood)
		for dz = 0.6, 13.6, 2.6 do
			part(m, "Paletto", Vector3.new(0.35, 1.8, 0.35), at(x * 3.45, 3.5, dz), NERO, nil, nil, false)
		end
	end
	part(m, "Sponda", Vector3.new(7, 1.1, 0.25), at(0, 3.15, 0.15), LEGNO_SCURO, Enum.Material.Wood)
	-- carico: casse legate e barili (a sinistra: a destra resta spazio per quello che porti)
	part(m, "Cassa", Vector3.new(2.2, 2.2, 2.2), at(-1.9, 3.7, 2.0), Color3.fromRGB(150, 104, 58), Enum.Material.Wood)
	part(m, "Fascia", Vector3.new(2.3, 0.15, 2.3), at(-1.9, 4.1, 2.0), NERO, nil, nil, false)
	part(m, "Cassa", Vector3.new(1.4, 1.4, 1.4), at(-1.9, 5.5, 2.1), Color3.fromRGB(160, 116, 66), Enum.Material.Wood, Vector3.new(0, -12, 0))
	for i, d in ipairs({ { -2.3, 11.5 }, { -2.3, 12.9 } }) do
		cilindro(m, "Barile", 1.2, 1.6, at(d[1], 3.4, d[2]), "Y", if i == 2 then Color3.fromRGB(110, 70, 40) else Color3.fromRGB(96, 60, 36), Enum.Material.Wood)
		cilindro(m, "Cerchio", 1.25, 0.12, at(d[1], 3.9, d[2]), "Y", NERO, nil, false)
	end
	-- cassaforte (qui si vende l'oro: Bottino.cassaforte, e ci si trascinano sopra i lingotti)
	cassaforte = part(m, "Cassaforte", Vector3.new(1.8, 2.0, 1.8), at(2.2, 3.6, 1.4), GRIGIO, Enum.Material.DiamondPlate)
	part(m, "Ruota", Vector3.new(0.15, 0.7, 0.7), at(1.25, 3.8, 1.4), OTTONE, nil, nil, false, Enum.PartType.Cylinder)
	-- lanterna su un palo
	part(m, "PaloLanterna", Vector3.new(0.2, 3.2, 0.2), at(3.2, 4.2, 0.6), NERO, nil, nil, false)
	table.insert(luci, part(m, "Lanterna", Vector3.new(0.6, 0.7, 0.6), at(3.2, 6.0, 0.6), Color3.fromRGB(255, 190, 90), Enum.Material.Neon, nil, false))
	for _, dz in ipairs({ 2.6, 11.4 }) do
		part(m, "Carrello", Vector3.new(6.2, 0.6, 3.4), at(0, 1.55, dz), NERO)
		for _, o in ipairs({ -0.9, 0.9 }) do
			for x = -1, 1, 2 do
				cilindro(m, "Ruota", 1.5, 0.35, at(x * 3.0, 1.35, dz + o), "X", NERO, nil, false)
			end
		end
	end
	part(m, "Gancio", Vector3.new(1.4, 0.5, 1.6), at(0, 2.0, 14.6), NERO)
	return pianale
end

local function tender(m: Model, at: (number, number, number) -> Vector3)
	part(m, "PianaleTender", Vector3.new(7, 0.6, 8), at(0, 2.3, 19.2), NERO)
	local cx = -0.9
	part(m, "Cassone", Vector3.new(5.0, 2.8, 7.6), at(cx, 4.0, 19.2), Color3.fromRGB(44, 48, 54))
	part(m, "Bordo", Vector3.new(5.2, 0.25, 7.8), at(cx, 5.5, 19.2), ROSSO, nil, nil, false)
	for i = 1, 9 do
		local a = i * 1.7
		part(m, "Carbone", Vector3.new(1.6, 1.0, 1.6), at(cx + math.sin(a) * 1.4, 5.7 + (i % 3) * 0.25, 16.6 + (i % 5) * 1.25),
			Color3.fromRGB(26, 26, 30), Enum.Material.Slate, Vector3.new(i * 23 % 40, i * 37 % 90, i * 11 % 30), false)
	end
	cilindro(m, "Boccaporto", 1.2, 0.4, at(cx, 5.7, 22.2), "Y", OTTONE, nil, false)
	part(m, "Passerella", Vector3.new(1.7, 0.2, 8), at(2.65, 2.7, 19.2), LAMIERA, Enum.Material.DiamondPlate)
	part(m, "Corrimano", Vector3.new(0.12, 0.12, 8), at(3.4, 4.0, 19.2), OTTONE, nil, nil, false)
	for _, dz in ipairs({ 15.6, 19.2, 22.8 }) do
		part(m, "Montantino", Vector3.new(0.12, 1.3, 0.12), at(3.4, 3.35, dz), OTTONE, nil, nil, false)
	end
	for _, dz in ipairs({ 16.6, 21.8 }) do
		for x = -1, 1, 2 do
			cilindro(m, "Ruota", 1.6, 0.4, at(x * 3.0, 1.4, dz), "X", NERO, nil, false)
		end
	end
end

local function cabina(m: Model, at: (number, number, number) -> Vector3)
	local z0, z1 = L0, L0 + CABINA
	local zc = (z0 + z1) / 2
	local altoMuro = TETTO_Y - Treno.PIANO_Y
	for x = -1, 1, 2 do
		-- parete bassa, poi finestrini aperti tra i montanti
		part(m, "Fianco", Vector3.new(0.3, 2.4, CABINA), at(x * 3.35, Treno.PIANO_Y + 1.2, zc), NERO)
		part(m, "Filetto", Vector3.new(0.34, 0.18, CABINA), at(x * 3.37, Treno.PIANO_Y + 2.4, zc), OTTONE, nil, nil, false)
		part(m, "Architrave", Vector3.new(0.3, 0.8, CABINA), at(x * 3.35, TETTO_Y - 0.4, zc), NERO, nil, nil, false)
		for _, dz in ipairs({ 0.2, 3.2, 5.8, CABINA - 0.2 }) do
			part(m, "Montante", Vector3.new(0.32, altoMuro, 0.36), at(x * 3.35, Treno.PIANO_Y + altoMuro / 2, z0 + dz), NERO)
		end
	end
	-- tetto a due strati con la sporgenza e il gocciolatoio
	part(m, "Tetto", Vector3.new(8.2, 0.35, CABINA + 1.2), at(0, TETTO_Y + 0.15, zc), Color3.fromRGB(48, 50, 56))
	part(m, "Falda", Vector3.new(4.6, 0.28, CABINA + 1.5), at(-1.6, TETTO_Y + 0.85, zc), Color3.fromRGB(48, 50, 56), nil, Vector3.new(0, 0, 16), false)
	part(m, "Falda", Vector3.new(4.6, 0.28, CABINA + 1.5), at(1.6, TETTO_Y + 0.85, zc), Color3.fromRGB(48, 50, 56), nil, Vector3.new(0, 0, -16), false)
	part(m, "Gocciolatoio", Vector3.new(8.3, 0.12, 0.2), at(0, TETTO_Y - 0.05, z0 - 0.6), OTTONE, nil, nil, false)
	-- paratia della caldaia con la bocca del fuoco e i manometri
	local zp = z1 + 0.2
	part(m, "Paratia", Vector3.new(7, altoMuro, 0.4), at(0, Treno.PIANO_Y + altoMuro / 2, zp), Color3.fromRGB(52, 56, 62))
	bocca = part(m, "Bocca", Vector3.new(2, 1.5, 0.3), at(0, Treno.PIANO_Y + 1.4, zp - 0.3), Color3.fromRGB(255, 140, 40), Enum.Material.Neon, nil, false)
	part(m, "Sportello", Vector3.new(2.4, 0.25, 0.4), at(0, Treno.PIANO_Y + 2.3, zp - 0.25), OTTONE, nil, nil, false)
	for i, x in ipairs({ -1.6, -0.5 }) do
		cilindro(m, "Manometro", 0.7, 0.15, at(x, Treno.PIANO_Y + 4.0 + i * 0.1, zp - 0.25), "Z", Color3.fromRGB(230, 226, 210), Enum.Material.SmoothPlastic, false)
		cilindro(m, "Ghiera", 0.8, 0.1, at(x, Treno.PIANO_Y + 4.0 + i * 0.1, zp - 0.22), "Z", OTTONE, nil, false)
	end
	-- la leva di marcia, a destra della bocca
	part(m, "BaseLeva", Vector3.new(0.8, 0.6, 0.8), at(2.3, Treno.PIANO_Y + 0.3, z1 - 1.6), NERO)
	leva = part(m, "Leva", Vector3.new(0.22, 1.8, 0.22), at(2.3, Treno.PIANO_Y + 1.4, z1 - 1.6), OTTONE, nil, Vector3.new(-25, 0, 0), false)
	part(m, "Pomello", Vector3.new(0.4, 0.4, 0.4), at(2.3, Treno.PIANO_Y + 2.3, z1 - 2.0), Color3.fromRGB(130, 30, 26), Enum.Material.SmoothPlastic,
		nil, false, Enum.PartType.Ball)
	-- il sedile del macchinista (Seat: ci si siede camminandoci sopra, si scende col salto)
	local s = Instance.new("Seat")
	s.Name = "PostoMacchinista"
	s.Anchored = true
	s.Size = Vector3.new(1.8, 0.5, 1.8)
	s.Position = at(-2.0, Treno.PIANO_Y + 1.3, z1 - 3.0)
	s.Color = Color3.fromRGB(110, 40, 34)
	s.Material = Enum.Material.Fabric
	-- Blocky: un Seat creato da script non fa sedere finche' non ha la proprieta'
	-- Seat accesa (docs/limiti_trovati.md, n. 9)
	pcall(function() (s :: any).Seat = true end)
	s.Parent = m
	sedile = s
	part(m, "GambaSedile", Vector3.new(0.4, 1.0, 0.4), at(-2.0, Treno.PIANO_Y + 0.5, z1 - 3.0), NERO, nil, nil, false)
	part(m, "Schienale", Vector3.new(1.8, 1.8, 0.3), at(-2.0, Treno.PIANO_Y + 2.4, z1 - 3.9), Color3.fromRGB(110, 40, 34), Enum.Material.Fabric)
end

local function caldaia(m: Model, at: (number, number, number) -> Vector3)
	local B = L0 + CABINA      -- da qui in avanti la caldaia
	cilindro(m, "Caldaia", 4.2, 10.6, at(0, 4.9, B + 5.6), "Z", NERO)
	for _, dz in ipairs({ 0.8, 3.6, 6.4, 9.2 }) do
		cilindro(m, "Fascia", 4.35, 0.25, at(0, 4.9, B + dz), "Z", OTTONE, nil, false)
	end
	cilindro(m, "CameraFumo", 4.6, 3.6, at(0, 4.9, B + 12.6), "Z", Color3.fromRGB(28, 28, 32))
	cilindro(m, "Portello", 3.6, 0.3, at(0, 4.9, B + 14.5), "Z", Color3.fromRGB(40, 40, 46))
	cilindro(m, "Mozzo", 0.7, 0.35, at(0, 4.9, B + 14.7), "Z", OTTONE, nil, false)
	part(m, "Targa", Vector3.new(1.6, 0.6, 0.15), at(0, 3.5, B + 14.65), OTTONE, nil, nil, false)
	cilindro(m, "Camino", 1.3, 2.4, at(0, 8.1, B + 12.6), "Y", Color3.fromRGB(28, 28, 32))
	cilindro(m, "Camino", 1.7, 0.9, at(0, 9.6, B + 12.6), "Y", Color3.fromRGB(28, 28, 32))
	fumoDa = cilindro(m, "BoccaCamino", 2.1, 0.45, at(0, 10.25, B + 12.6), "Y", OTTONE, nil, false)
	cilindro(m, "BaseDuomo", 1.6, 0.9, at(0, 7.3, B + 4.4), "Y", NERO)
	part(m, "Duomo", Vector3.new(1.7, 1.7, 1.7), at(0, 7.8, B + 4.4), OTTONE, nil, nil, nil, Enum.PartType.Ball)
	cilindro(m, "BaseSabbiera", 1.4, 0.7, at(0, 7.2, B + 8.2), "Y", NERO)
	part(m, "Sabbiera", Vector3.new(1.5, 1.4, 1.5), at(0, 7.6, B + 8.2), NERO, nil, nil, nil, Enum.PartType.Ball)
	cilindro(m, "Fischietto", 0.35, 1.1, at(0.9, 7.6, B + 1.0), "Y", OTTONE, nil, false)
	for x = -1, 1, 2 do
		part(m, "Corrimano", Vector3.new(0.12, 0.12, 12), at(x * 2.35, 5.9, B + 6.6), OTTONE, nil, nil, false)
		part(m, "Passerella", Vector3.new(1.0, 0.18, 14), at(x * 3.0, 2.75, B + 7.4), LAMIERA, Enum.Material.DiamondPlate)
		part(m, "Bordo", Vector3.new(0.12, 0.5, 14), at(x * 3.52, 2.45, B + 7.4), ROSSO, nil, nil, false)
		cilindro(m, "Pistone", 1.3, 3.2, at(x * 3.15, 2.5, B + 11.4), "Z", NERO)
		cilindro(m, "Coperchio", 1.35, 0.15, at(x * 3.15, 2.5, B + 13.0), "Z", OTTONE, nil, false)
	end
	part(m, "Telaio", Vector3.new(7, 0.6, CABINA + 15), at(0, 2.3, L0 + (CABINA + 15) / 2), NERO)
	part(m, "Fascione", Vector3.new(7.3, 0.4, CABINA + 15.1), at(0, 1.95, L0 + (CABINA + 15) / 2), ROSSO, nil, nil, false)
	part(m, "Fanale", Vector3.new(1.3, 1.2, 1.2), at(0, 7.3, B + 13.8), NERO)
	table.insert(luci, cilindro(m, "Lente", 0.9, 0.15, at(0, 7.3, B + 14.45), "Z", Color3.fromRGB(255, 236, 180), Enum.Material.Neon, false))
	part(m, "Respingente", Vector3.new(7, 1.0, 0.6), at(0, 2.2, B + 14.0), ROSSO)
	for x = -1, 1, 2 do
		cilindro(m, "Tampone", 0.7, 0.6, at(x * 2.5, 2.2, B + 14.5), "Z", NERO, nil, false)
	end
	for i = -3, 3 do
		part(m, "Lamella", Vector3.new(0.3, 0.3, 2.6), at(i * 0.85, 1.35, B + 15.1 - math.abs(i) * 0.25), Color3.fromRGB(70, 70, 76),
			nil, Vector3.new(30, i * 7, 0), false)
	end
	-- ruote motrici sotto cabina e caldaia, con mozzo e contrappeso; bielle; carrello davanti
	for _, dz in ipairs({ 5.0, 9.0, 13.0 }) do
		for x = -1, 1, 2 do
			cilindro(m, "RuotaMotrice", 3.0, 0.45, at(x * 3.05, 2.6, L0 + dz), "X", ROSSO, nil, false)
			cilindro(m, "Cerchione", 3.1, 0.3, at(x * 3.0, 2.6, L0 + dz), "X", NERO, nil, false)
			cilindro(m, "Mozzo", 0.7, 0.6, at(x * 3.3, 2.6, L0 + dz), "X", OTTONE, nil, false)
			part(m, "Contrappeso", Vector3.new(0.2, 0.9, 1.4), at(x * 3.32, 2.0, L0 + dz), NERO, nil, nil, false)
		end
	end
	for x = -1, 1, 2 do
		local b = part(m, "Biella", Vector3.new(0.22, 0.3, 8.6), at(x * 3.45, 2.2, L0 + 9), Color3.fromRGB(170, 170, 178), nil, nil, false)
		table.insert(bielle, { part = b, x = x * 3.45, y = 2.6, z = L0 + 9 })
		part(m, "Stantuffo", Vector3.new(0.18, 0.18, 4.4), at(x * 3.4, 2.5, B + 9.8), Color3.fromRGB(170, 170, 178), nil, nil, false)
		cilindro(m, "RuotaPortante", 1.5, 0.35, at(x * 2.9, 1.35, B + 12.0), "X", NERO, nil, false)
	end
end

local function costruisci(z: number): Model
	local m = C.modello("Treno", workspace)
	local function at(x: number, y: number, dz: number): Vector3 return Vector3.new(x, y, z + dz) end
	table.clear(bielle)
	table.clear(luci)
	local pianale = vagone(m, at)
	tender(m, at)
	cabina(m, at)
	caldaia(m, at)
	m.PrimaryPart = pianale
	return m
end

local function mettiPrompt()
	assert(leva and bocca)
	promptLeva = Instance.new("ProximityPrompt")
	promptLeva.Name = "LevaMarcia"
	promptLeva.ObjectText = "Leva"
	promptLeva.ActionText = "Avvia"
	promptLeva.MaxActivationDistance = 6
	promptLeva.Parent = leva

	promptCaldaia = Instance.new("ProximityPrompt")
	promptCaldaia.Name = "Caldaia"
	promptCaldaia.ObjectText = "Caldaia"
	promptCaldaia.ActionText = "Butta carbone"
	promptCaldaia.HoldDuration = 0.35
	promptCaldaia.MaxActivationDistance = 6
	promptCaldaia.KeyboardKeyCode = Enum.KeyCode.F
	promptCaldaia.Parent = bocca
end

-- ---------------------------------------------------------------- fumo ----

local fumoAcc = 0
local function fumo(dt: number)
	-- sbuffi che salgono, si allargano e svaniscono; piu' veloce, piu' sbuffi
	for i = #fumi, 1, -1 do
		local f = fumi[i]
		local t = (f:GetAttribute("t") or 0) + dt
		f:SetAttribute("t", t)
		if t > 2.4 then
			f:Destroy()
			table.remove(fumi, i)
		else
			local s = 1.4 + t * 2.2
			f.Size = Vector3.new(s, s, s)
			f.Position = f.Position + Vector3.new(0, dt * 3.2, -velocita * dt * 0.55)
			f.Transparency = 0.25 + t / 2.4 * 0.75
		end
	end
	if regolatore <= 0 or carbone <= 0 or not fumoDa then return end
	fumoAcc += dt * (1.2 + velocita / Config.VELOCITA_MAX * 4)
	if guasto then fumoAcc += dt * 3 end
	if fumoAcc < 1 or #fumi > 10 then return end
	fumoAcc = 0
	local f = C.part({ nome = "Fumo", dimensione = Vector3.new(1.4, 1.4, 1.4), posizione = fumoDa.Position + Vector3.new(0, 0.8, 0),
		colore = if guasto then Color3.fromRGB(40, 40, 44) else Color3.fromRGB(200, 196, 190), materiale = Enum.Material.SmoothPlastic,
		forma = Enum.PartType.Ball, collisione = false, trasparenza = 0.25 }, workspace)
	table.insert(fumi, f)
end

-- ---------------------------------------------------------------- API ----

function Treno.crea()
	-- (niente modello mentre si costruisce: la costruzione lascia passare dei frame)
	local vecchio = modello
	modello = nil
	promptLeva, promptCaldaia = nil, nil
	if vecchio then vecchio:Destroy() end
	for _, f in ipairs(fumi) do f:Destroy() end
	table.clear(fumi)
	retroZ = Config.PARTENZA_Z
	velocita = 0
	regolatore = 0
	carbone = Config.CARBONE_INIZIO
	carboneMax = Config.CARBONE_MAX
	mulVelocita = 1
	guasto = false
	modello = costruisci(retroZ)
	mettiPrompt()
end

function Treno.modello(): Model? return modello end
function Treno.retro(): number return retroZ end
function Treno.fronte(): number return retroZ + Treno.LUNGO end
function Treno.velocita(): number return velocita end
function Treno.carbone(): number return carbone end
function Treno.inMarcia(): boolean return regolatore > 0 end
function Treno.regolatore(): number return regolatore end
function Treno.sedile(): Part? return sedile end
function Treno.cassaforte(): Part? return cassaforte end
function Treno.bocca(): Part? return bocca end

--- Dove si sale: sul vagone, a destra (a sinistra c'e' il carico).
function Treno.puntoSalita(): Vector3
	return Vector3.new(1.2, Treno.PIANO_Y + 3, retroZ + 8)
end

--- La posizione p e' sopra il treno (vagone, tender, cabina)?
function Treno.sopra(p: Vector3): boolean
	return math.abs(p.X) < Treno.LARGO / 2 + 0.6
		and p.Z > retroZ - 0.5 and p.Z < retroZ + Treno.SOPRA_FINO
		and p.Y > Treno.PIANO_Y - 0.5 and p.Y < TETTO_Y + 3
end

local function aggiornaLeva()
	if promptLeva then promptLeva.ActionText = if regolatore > 0 then "Ferma" else "Avvia" end
	if leva then
		local base = leva.Position
		leva.Orientation = Vector3.new(-25 + 50 * regolatore, 0, 0)
		leva.Position = base
	end
end

--- La leva: tutto avanti o tutto fermo.
function Treno.impostaMarcia(avanti: boolean)
	regolatore = if avanti then 1 else 0
	aggiornaLeva()
end

--- Il regolatore (dal posto del macchinista): 0..1.
function Treno.impostaRegolatore(x: number)
	local nuovo = math.clamp(x, 0, 1)
	if math.abs(nuovo - regolatore) < 0.001 then return end
	regolatore = nuovo
	aggiornaLeva()
end

function Treno.aggiungiCarbone(quanto: number)
	carbone = math.min(carboneMax, carbone + quanto)
end

function Treno.promptLeva(): ProximityPrompt? return promptLeva end
function Treno.promptCaldaia(): ProximityPrompt? return promptCaldaia end

--- Un passo di simulazione: velocita', carbone, movimento. Restituisce di quanto e' andato avanti.
function Treno.passo(dt: number, fermaObbligata: boolean): number
	local m = modello
	if not m then return 0 end
	local acceso = regolatore > 0 and carbone > 0 and not fermaObbligata and not guasto
	local obiettivo = if acceso then Config.VELOCITA_MAX * mulVelocita * regolatore else 0
	if velocita < obiettivo then
		velocita = math.min(obiettivo, velocita + Config.ACCELERAZIONE * dt)
	else
		velocita = math.max(obiettivo, velocita - Config.FRENATA * dt)
	end
	if acceso then
		carbone = math.max(0, carbone - Config.CONSUMO * dt * (0.25 + 0.75 * velocita / Config.VELOCITA_MAX))
	end
	if bocca then bocca.Transparency = 0.6 - 0.6 * math.min(1, carbone / Config.CARBONE_MAX) end
	local fine = Config.LUNGHEZZA_VIAGGIO - 6 - Treno.LUNGO
	local dz = velocita * dt
	if retroZ + dz > fine then
		dz = math.max(0, fine - retroZ)
		velocita = 0
		regolatore = 0
		aggiornaLeva()
	end
	if dz > 0 then
		m:TranslateBy(Vector3.new(0, 0, dz))
		retroZ += dz
		-- le bielle girano con le ruote motrici
		angoloRuote += dz / 1.5
		local ox, oy = math.sin(angoloRuote) * 0.55, math.cos(angoloRuote) * 0.55
		for _, b in ipairs(bielle) do
			b.part.Position = Vector3.new(b.x, b.y + oy - 0.4, retroZ + b.z + ox)
		end
	end
	fumo(dt)
	return dz
end

-- ---------------------------------------------------------------- officina ----

function Treno.carboneMax(): number return carboneMax end
function Treno.impostaCaldaiaMax(n: number) carboneMax = n end
function Treno.impostaPotenza(mul: number) mulVelocita = mul end
function Treno.guasto(): boolean return guasto end
function Treno.impostaGuasto(si: boolean) guasto = si end

--- Un punto sul treno (dx di lato, dy in alto, dz dalla coda): per attaccarci cose.
function Treno.punto(dx: number, dy: number, dz: number): Vector3
	return Vector3.new(dx, dy, retroZ + dz)
end

--- Le luci (fanale, lanterna) accese di notte.
function Treno.luci(accese: boolean)
	for _, l in ipairs(luci) do
		l.PointLight = accese
		l.LightRange = 22
		l.LightBrightness = if accese then 2.2 else 0
		l.Transparency = if accese then 0 else 0.35
	end
end

function Treno.alla_meta(): boolean
	return retroZ >= Config.LUNGHEZZA_VIAGGIO - 6 - Treno.LUNGO - 0.01
end

return Treno
