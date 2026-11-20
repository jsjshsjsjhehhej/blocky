-- Gli oggetti che si prendono col clic e si trascinano (DragDetector):
-- carbone, lingotti, munizioni, bende. Si possono anche mettere nello zaino (E).
--
--   * lasciati sul treno viaggiano con lui;
--   * lasciati in aria cadono sul primo appoggio sotto;
--   * carbone vicino alla bocca della caldaia = caldaia piu' piena;
--   * lingotto sulla cassaforte = venduto (a chi e' piu' vicino).

local Players = game:GetService("Players")
local RS = game:GetService("ReplicatedStorage")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local C = require(Shared:WaitForChild("Costruisci"))

local Trasporto = {}

-- Ogni oggetto e' un pezzo principale (quello che prendi) piu' dei pezzi attaccati,
-- cosi' non e' un cubo solo.
local TIPI = {
	Carbone = {
		size = Vector3.new(0.9, 0.7, 0.8), colore = Color3.fromRGB(22, 22, 24), materiale = Enum.Material.Slate,
		forma = Enum.PartType.Ball, testo = "Carbone",
		pezzi = {
			{ off = Vector3.new(0.45, 0.05, 0.15), size = Vector3.new(0.7, 0.55, 0.6), colore = Color3.fromRGB(40, 40, 44), forma = Enum.PartType.Ball },
			{ off = Vector3.new(-0.3, 0.1, -0.3), size = Vector3.new(0.55, 0.45, 0.5), colore = Color3.fromRGB(14, 14, 16), forma = Enum.PartType.Ball },
		},
	},
	Oro = {
		size = Vector3.new(1.5, 0.28, 0.7), colore = Color3.fromRGB(212, 160, 40), materiale = Enum.Material.Metal, testo = "Lingotto",
		pezzi = {
			{ off = Vector3.new(0, 0.16, 0), size = Vector3.new(1.35, 0.08, 0.52), colore = Color3.fromRGB(255, 214, 90), materiale = Enum.Material.Metal },
		},
	},
	Munizioni = {
		size = Vector3.new(1.15, 0.7, 0.8), colore = Color3.fromRGB(110, 78, 46), materiale = Enum.Material.Wood, testo = "Munizioni",
		pezzi = {
			{ off = Vector3.new(0, 0.32, 0), size = Vector3.new(1.2, 0.12, 0.86), colore = Color3.fromRGB(70, 74, 68), materiale = Enum.Material.Metal },
			{ off = Vector3.new(0, 0.05, 0.42), size = Vector3.new(0.7, 0.35, 0.08), colore = Color3.fromRGB(160, 48, 36), materiale = Enum.Material.SmoothPlastic },
		},
	},
	Bende = {
		size = Vector3.new(0.7, 0.55, 0.7), colore = Color3.fromRGB(236, 230, 214), materiale = Enum.Material.Fabric,
		forma = Enum.PartType.Cylinder, testo = "Bende",
		pezzi = {
			{ off = Vector3.new(0, 0, 0), size = Vector3.new(0.72, 0.12, 0.72), colore = Color3.fromRGB(180, 40, 36), materiale = Enum.Material.Fabric, forma = Enum.PartType.Cylinder },
		},
	},
}

type Pezzo = { part: Part, off: Vector3 }
type Oggetto = {
	part: Part,
	tipo: string,
	ultima: Vector3,      -- dove l'abbiamo lasciato noi (se cambia, qualcuno lo trascina)
	fermoDa: number,      -- da quanto nessuno lo tocca
	appoggiato: boolean,
	pezzi: { Pezzo },
}

local cartella = Instance.new("Folder")
cartella.Name = "Oggetti"
cartella.Parent = workspace

local oggetti: { Oggetto } = {}
local Treno: any = nil
local Bottino: any = nil
local Missioni: any = nil
local avvisa: (Player?, string) -> () = function() end
local effetto: (string, ...any) -> () = function() end

function Trasporto.usa(t)
	Treno = t.treno
	Bottino = t.bottino
	Missioni = t.missioni
	avvisa = t.avvisa
	effetto = t.effetto
end

local function ruotaY(off: Vector3, gradi: number): Vector3
	local r = math.rad(gradi)
	local c, s = math.cos(r), math.sin(r)
	return Vector3.new(off.X * c - off.Z * s, off.Y, off.X * s + off.Z * c)
end

local function togli(i: number)
	local o = oggetti[i]
	if o.part then o.part:Destroy() end
	for _, d in ipairs(o.pezzi) do
		if d.part then d.part:Destroy() end
	end
	table.remove(oggetti, i)
end

local function piuVicino(pos: Vector3): Player?
	local meglio, d = nil, math.huge
	for _, g in ipairs(Players:GetPlayers()) do
		local root = g.Character and g.Character:FindFirstChild("HumanoidRootPart")
		if root then
			local dd = (root.Position - pos).Magnitude
			if dd < d then meglio, d = g, dd end
		end
	end
	return meglio
end

--- Un oggetto da trascinare a pos (padre ignorato: gli oggetti stanno in workspace.Oggetti,
--- cosi' non spariscono con il pezzo di mondo in cui li hai trovati).
function Trasporto.oggetto(tipo: string, pos: Vector3, _padre: Instance?)
	local t = TIPI[tipo]
	if not t then return end
	local yaw = math.random(0, 359)
	local p = C.part({ nome = tipo, dimensione = t.size, posizione = pos, colore = t.colore, materiale = t.materiale,
		forma = t.forma, rotazione = Vector3.new(0, yaw, 0) }, cartella)
	local pezzi: { Pezzo } = {}
	for _, d in ipairs(t.pezzi or {}) do
		local q = C.part({
			nome = tipo .. "Pezzo", dimensione = d.size, posizione = pos + ruotaY(d.off, yaw),
			colore = d.colore, materiale = d.materiale or t.materiale, forma = d.forma,
			rotazione = Vector3.new(0, yaw, 0), collisione = false,
		}, cartella)
		table.insert(pezzi, { part = q, off = d.off })
	end
	local drag = Instance.new("DragDetector")
	drag.Parent = p
	local prompt = Instance.new("ProximityPrompt")
	prompt.Name = "Prendi"
	prompt.ObjectText = t.testo
	prompt.ActionText = "Prendi"
	prompt.HoldDuration = 0.2
	prompt.MaxActivationDistance = 6
	prompt.Parent = p
	local o: Oggetto = { part = p, tipo = tipo, ultima = pos, fermoDa = 0, appoggiato = false, pezzi = pezzi }
	prompt.Triggered:Connect(function(g)
		if type(g) ~= "table" or not o.part then return end
		local quanto = if tipo == "Munizioni" then 8 else 1
		local prima = Bottino.prendi(g, tipo)
		local dopo = Bottino.aggiungi(g, tipo, quanto)
		if dopo == prima then
			avvisa(g, "Hai le mani piene: trascinalo col clic")
			return
		end
		effetto("suono", if tipo == "Oro" then "coin" else "open")
		avvisa(g, t.testo .. " preso (" .. dopo .. ")")
		for i, x in ipairs(oggetti) do
			if x == o then togli(i) break end
		end
		o.part = nil :: any
	end)
	table.insert(oggetti, o)
	return p
end

--- Quanti oggetti ci sono sopra il treno (per l'HUD).
function Trasporto.sulTreno(): number
	local n = 0
	for _, o in ipairs(oggetti) do
		if o.part and Treno.sopra(o.part.Position) then n += 1 end
	end
	return n
end

local raggioParams = RaycastParams.new()
raggioParams.FilterType = Enum.RaycastFilterType.Exclude

--- Un passo: chi e' sul treno viaggia, chi e' lasciato cade, le consegne.
function Trasporto.passo(dt: number, dzTreno: number)
	local bocca = Treno.bocca()
	local cassaforte = Treno.cassaforte()
	for i = #oggetti, 1, -1 do
		local o = oggetti[i]
		local p = o.part
		if not p or not p.Parent then
			for _, d in ipairs(o.pezzi) do
				if d.part then d.part:Destroy() end
			end
			table.remove(oggetti, i)
			continue
		end
		local pos = p.Position
		-- qualcuno lo sta trascinando? (la posizione non e' piu' quella che gli abbiamo dato)
		if (pos - o.ultima).Magnitude > 0.01 then
			o.fermoDa = 0
			o.appoggiato = false
		else
			o.fermoDa += dt
		end
		local trascinato = o.fermoDa < 0.15

		-- consegne
		if o.tipo == "Carbone" and bocca and (pos - bocca.Position).Magnitude < 3.4 then
			if Treno.carbone() < Treno.carboneMax() - 1 then
				Treno.aggiungiCarbone(Config.CARBONE_PER_PEZZO)
				effetto("suono", "coal")
				avvisa(nil, "Carbone in caldaia: " .. math.floor(Treno.carbone() / Treno.carboneMax() * 100) .. "%")
				togli(i)
				continue
			end
		elseif o.tipo == "Oro" and cassaforte and (pos - cassaforte.Position).Magnitude < 2.6 then
			local g = piuVicino(pos)
			if g then
				Bottino.aggiungi(g, "Gettoni", Config.ORO_VALORE)
				Bottino.aggiungi(g, "OroVenduto", 1)
				Missioni.venduto(g, 1)
				effetto("suono", "coin")
				avvisa(g, "Lingotto venduto: +" .. Config.ORO_VALORE .. " gettoni")
				togli(i)
				continue
			end
		end

		if not trascinato then
			-- sul treno: viaggia con lui
			if dzTreno > 0 and Treno.sopra(pos) then
				pos += Vector3.new(0, 0, dzTreno)
				p.Position = pos
			end
			-- lasciato a mezz'aria: giu' fino al primo appoggio
			if not o.appoggiato and o.fermoDa > 0.25 then
				local filtro = { p }
				for _, d in ipairs(o.pezzi) do
					table.insert(filtro, d.part)
				end
				raggioParams.FilterDescendantsInstances = filtro
				local r = workspace:Raycast(pos, Vector3.new(0, -40, 0), raggioParams)
				local y = if r then r.Position.Y + p.Size.Y / 2 else 0.5 + p.Size.Y / 2
				pos = Vector3.new(pos.X, y, pos.Z)
				p.Position = pos
				o.appoggiato = true
			end
			-- troppo indietro: non lo rivedra' nessuno
			if pos.Z < Treno.retro() - 220 then
				togli(i)
				continue
			end
		end
		o.ultima = pos
		local yaw = p.Orientation.Y
		for _, d in ipairs(o.pezzi) do
			if d.part then
				d.part.Position = pos + ruotaY(d.off, yaw)
				d.part.Orientation = Vector3.new(0, yaw, 0)
			end
		end
	end
end

function Trasporto.via()
	for i = #oggetti, 1, -1 do togli(i) end
end

return Trasporto
