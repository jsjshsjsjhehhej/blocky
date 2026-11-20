-- I nemici: gli zombie (Zombie, Corridore, Bruto, Soldato) e i Banditi.
-- Nelle citta', nei forti e negli accampamenti aspettano finche' non ti vedono,
-- poi attaccano; di notte arriva l'orda attorno al treno. Gli zombie che
-- arrivano al treno ci salgono sopra; i Banditi stanno a distanza e sparano.

local Players = game:GetService("Players")
local RS = game:GetService("ReplicatedStorage")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))

local Actors = game:GetService("Actors")

local Zombie = {}

export type Nemico = {
	npc: any,             -- l'NPC del motore (Actors:Spawn)
	tipo: string,
	dati: any,
	viva: boolean,
	sveglio: boolean,
	aBordo: boolean,
	colpo: number,
	morte: number,
	vita: number,         -- vita nostra: a zero e' morto, anche se Died del motore non arriva
	casa: Vector3,        -- dove gira finche' dorme
	giro: number,
	ultimoColpitoDa: Player?,
}

type Zona = { centro: Vector3, raggio: number, tipi: { string }, quanti: number, fatta: boolean }

local nemici: { Nemico } = {}
local zone: { Zona } = {}
local rng = Random.new(Config.SEME + 7)

-- agganci (li collega Main)
local quandoUcciso: (Player?, string, Vector3) -> () = function() end
local suono: (string, Vector3) -> () = function() end
local scia: (Vector3, Vector3, boolean) -> () = function() end
local attaccaTreno: (number) -> () = function() end

function Zombie.quandoUcciso(f: (Player?, string, Vector3) -> ()) quandoUcciso = f end
function Zombie.quandoSuono(f: (string, Vector3) -> ()) suono = f end
function Zombie.quandoScia(f: (Vector3, Vector3, boolean) -> ()) scia = f end
function Zombie.quandoAttaccaTreno(f: (number) -> ()) attaccaTreno = f end

-- ---------------------------------------------------------------- aspetto ----

local function vesti(npc, d)
	local pelle = Config.rgb(d.pelle)
	local maglia = Instance.new("Shirt")
	maglia.Color3 = Config.rgb(d.maglia)
	maglia.Parent = npc
	local pantaloni = Instance.new("Pants")
	pantaloni.Color3 = Config.rgb(d.pantaloni)
	pantaloni.Parent = npc
	-- BodyColors dopo i vestiti: testa e braccia color pelle (maglie strappate)
	local bc = Instance.new("BodyColors")
	bc.HeadColor3 = pelle
	bc.LeftArmColor3 = pelle
	bc.RightArmColor3 = pelle
	bc.TorsoColor3 = Config.rgb(d.maglia)
	bc.LeftLegColor3 = Config.rgb(d.pantaloni)
	bc.RightLegColor3 = Config.rgb(d.pantaloni)
	bc.Parent = npc
	if d.cappello then
		local c = Instance.new("Hat")
		c.Color3 = Config.rgb(d.cappello)
		c.Parent = npc
	end
end

local function vivi(): number
	local n = 0
	for _, z in ipairs(nemici) do if z.viva then n += 1 end end
	return n
end

local function muori(z: Nemico)
	if not z.viva then return end
	z.viva = false
	z.vita = 0
	pcall(function() z.npc.Health = 0 end)
	local dove = z.npc.Position or z.casa
	pcall(function() suono("morte", dove) end)
	pcall(function() quandoUcciso(z.ultimoColpitoDa, z.tipo, dove) end)
end

local function nasci(tipo: string, pos: Vector3, sveglio: boolean): Nemico?
	if vivi() >= Config.ZOMBIE_MAX then return nil end
	local d = Config.ZOMBIE_TIPI[tipo]
	local npc = Actors:Spawn(tipo, Vector3.new(pos.X, pos.Y + 2, pos.Z))
	npc.WalkSpeed = d.v * rng:NextNumber(0.9, 1.08)
	npc.MaxHealth = d.vita
	npc.Health = d.vita
	vesti(npc, d)
	local z: Nemico = { npc = npc, tipo = tipo, dati = d, viva = true, sveglio = sveglio, aBordo = false,
		colpo = rng:NextNumber(0, d.ogni), morte = 0, casa = pos, giro = rng:NextNumber(0, 3), ultimoColpitoDa = nil,
		vita = d.vita }
	npc.Died:Connect(function()
		muori(z)
	end)
	table.insert(nemici, z)
	return z
end

--- Una zona (citta', forte, accampamento): i suoi nemici nascono quando arrivi vicino.
function Zombie.zona(centro: Vector3, raggio: number, quanti: number, tipi: { string })
	table.insert(zone, { centro = centro, raggio = raggio, tipi = tipi, quanti = quanti, fatta = false })
end

--- Il giocatore vivo piu' vicino a pos (e dove sta, e quanto e' lontano).
local function piuVicino(pos: Vector3): (Player?, Vector3?, number)
	local meglio, dove, d = nil, nil, math.huge
	for _, p in ipairs(Players:GetPlayers()) do
		local char = p.Character
		local root = char and char:FindFirstChild("HumanoidRootPart")
		local hum = char and char:FindFirstChild("Humanoid")
		if root and (not hum or hum.Health > 0) then
			local dd = (root.Position - pos).Magnitude
			if dd < d then meglio, dove, d = p, root.Position, dd end
		end
	end
	return meglio, dove, d
end

local function svegliaZone()
	for _, zn in ipairs(zone) do
		if not zn.fatta then
			local _, _, d = piuVicino(zn.centro)
			if d < zn.raggio + 70 then
				zn.fatta = true
				for _ = 1, zn.quanti do
					local a = rng:NextNumber(0, math.pi * 2)
					local r = rng:NextNumber(4, zn.raggio)
					local tipo = zn.tipi[rng:NextInteger(1, #zn.tipi)]
					nasci(tipo, zn.centro + Vector3.new(math.cos(a) * r, 0, math.sin(a) * r), false)
				end
			end
		end
	end
end

local function colpisciGiocatore(g: Player, danno: number, dove: Vector3)
	local hum = g.Character and g.Character:FindFirstChild("Humanoid")
	if hum and hum.Health > 0 then
		hum:TakeDamage(danno)
		suono("hit", dove)
	end
end

-- ---------------------------------------------------------------- passo ----

local tempoSuono = 0

--- Un passo: zone, orda di notte, inseguimento, colpi; i morti e i lontani se ne vanno.
--- treno: il modulo Treno; dzTreno: quanto si e' mosso (chi e' a bordo viaggia con lui).
function Zombie.passo(dt: number, notte: boolean, treno: any, dzTreno: number)
	svegliaZone()
	if notte and rng:NextNumber() < dt * Config.ZOMBIE_ORDA_NOTTE then
		local lato = if rng:NextNumber() < 0.5 then -1 else 1
		local z = treno.fronte() + rng:NextNumber(10, 80)
		local r = rng:NextNumber()
		local tipo = if r < 0.22 then "Corridore" elseif r < 0.3 then "Bruto" else "Zombie"
		nasci(tipo, Vector3.new(lato * rng:NextNumber(14, 40), 3, z), true)
	end
	tempoSuono -= dt

	for i = #nemici, 1, -1 do
		local z = nemici[i]
		local npc = z.npc
		if not z.viva then
			z.morte += dt
			if z.morte > 1.6 then
				npc:Destroy()
				table.remove(nemici, i)
			end
			continue
		end
		local pos = npc.Position
		if pos == nil then table.remove(nemici, i) continue end
		if z.aBordo and dzTreno > 0 then
			pos += Vector3.new(0, 0, dzTreno)
			npc.Position = pos
		end
		-- troppo indietro: sparisce (non si vedra' piu')
		if pos.Z < treno.retro() - 180 then
			npc:Destroy()
			table.remove(nemici, i)
			continue
		end
		local giocatore, dove, dist = piuVicino(pos)
		local vista = if notte then Config.ZOMBIE_SVEGLIA_NOTTE else Config.ZOMBIE_SVEGLIA
		if z.dati.distanza then vista *= 1.3 end   -- i banditi vedono piu' lontano
		if not z.sveglio and dist < vista then
			z.sveglio = true
			if tempoSuono <= 0 and not z.dati.distanza then
				tempoSuono = 1.5
				suono("groan", pos)
			end
		end
		z.colpo += dt
		if z.sveglio and dove and giocatore then
			if z.dati.distanza then
				-- BANDITO: si tiene a distanza e spara (non sempre prende)
				local voglio = z.dati.distanza
				local verso = (dove - pos) * Vector3.new(1, 0, 1)
				local unit = if verso.Magnitude > 0.1 then verso.Unit else Vector3.new(0, 0, 1)
				if dist > voglio + 6 then
					npc:MoveTo(Vector3.new(dove.X, pos.Y, dove.Z) - unit * voglio)
				elseif dist < voglio - 8 then
					npc:MoveTo(pos - unit * 8)
				else
					-- di lato, per non stare fermo
					local lato = Vector3.new(-unit.Z, 0, unit.X) * (if i % 2 == 0 then 6 else -6)
					npc:MoveTo(pos + lato)
				end
				if dist < voglio * 2.2 and z.colpo >= z.dati.ogni then
					z.colpo = 0
					local da = pos + Vector3.new(0, 1.6, 0)
					local prende = rng:NextNumber() < z.dati.mira * (if dist < voglio then 1.2 else 0.8)
					local arrivo = if prende then dove else dove + Vector3.new(rng:NextNumber(-4, 4), rng:NextNumber(-1, 2), rng:NextNumber(-4, 4))
					scia(da, arrivo, prende)
					suono("shoot", pos)
					if prende then colpisciGiocatore(giocatore, z.dati.danno, dove) end
				end
			else
				-- ZOMBIE: arrivato accanto al treno ci sale, poi insegue
				if not z.aBordo and treno.sopra(Vector3.new(pos.X * 0.75, treno.PIANO_Y + 1, pos.Z)) then
					z.aBordo = true
					npc.Position = Vector3.new(math.clamp(pos.X, -2.6, 2.6), treno.PIANO_Y, pos.Z)
					pos = npc.Position
				end
				-- Si fermano a un passo: la meta non e' il tuo busto, altrimenti ti
				-- salgono in testa. L'altezza la decide il terreno, non un numero fisso.
				local dx, dz = dove.X - pos.X, dove.Z - pos.Z
				local distOrizz = math.sqrt(dx * dx + dz * dz)
				local ux = if distOrizz > 0.2 then dx / distOrizz else 0
				local uz = if distOrizz > 0.2 then dz / distOrizz else 1
				if distOrizz < 1.8 then
					local nx = dove.X - ux * 2.6
					local nz = dove.Z - uz * 2.6
					if z.aBordo then nx = math.clamp(nx, -2.6, 2.6) end
					npc.Position = Vector3.new(nx, pos.Y, nz)
					pos = npc.Position
				end
				local mx, mz = dove.X - ux * 2.5, dove.Z - uz * 2.5
				if z.aBordo then mx = math.clamp(mx, -2.6, 2.6) end
				npc:MoveTo(Vector3.new(mx, pos.Y, mz))
				if z.colpo >= z.dati.ogni then
					if distOrizz < 4.2 then
						z.colpo = 0
						colpisciGiocatore(giocatore, z.dati.danno, dove)
					elseif z.aBordo and distOrizz > 12 then
						-- a bordo senza nessuno vicino: se la prende col treno
						z.colpo = 0
						attaccaTreno(Config.TRENO_DANNO_ZOMBIE * (if z.tipo == "Bruto" then 3 else 1))
					end
				end
			end
		else
			-- dorme: gira piano attorno a casa
			z.giro -= dt
			if z.giro <= 0 then
				z.giro = rng:NextNumber(2, 5)
				npc:MoveTo(z.casa + Vector3.new(rng:NextNumber(-6, 6), 0, rng:NextNumber(-6, 6)))
			end
		end
	end
end

-- ---------------------------------------------------------------- colpi ----

-- Dal collo dei piedi alla cima della testa. Il raggio e' largo quanto il busto:
-- un colpo all'altezza degli occhi deve prendere il corpo, non passargli sopra.
local CORPO_BASSO = 0.4
local CORPO_ALTO = 5.1
local CORPO_RAGGIO = 1.7
local TESTA_DA = 4.0

--- Il primo nemico lungo la linea da "da" (direzione unitaria dir), entro gittata.
--- larghezza: quanto si puo' sbagliare di lato. Restituisce (nemico, distanza, alla testa).
--- Se il nemico ti e' addosso, il raggio parte dentro il corpo: conta lo stesso.
-- Distanza fra il raggio (da + dir*t, t >= 0) e il segmento verticale del corpo.
-- Restituisce t lungo il raggio e quanto si e' in alto sul corpo (0 = piedi).
local function sulCorpo(da: Vector3, dir: Vector3, piedi: Vector3, gittata: number): (number?, number)
	local ab = Vector3.new(0, CORPO_ALTO - CORPO_BASSO, 0)
	local a = piedi + Vector3.new(0, CORPO_BASSO, 0)
	local w = da - a
	local b = dir:Dot(ab)
	local c = ab:Dot(ab)
	local d = dir:Dot(w)
	local e = ab:Dot(w)
	local denom = c - b * b
	local t = if math.abs(denom) > 1e-4 then (b * e - c * d) / denom else 0
	local u = if c > 1e-4 then (e + b * t) / c else 0
	t = math.clamp(t, 0, gittata)
	u = math.clamp(u, 0, 1)
	-- dopo il clamp, ricalcola l'altro parametro sul punto fissato
	local punto = a + ab * u
	local rel = punto - da
	t = math.clamp(rel:Dot(dir), 0, gittata)
	local lato = (punto - (da + dir * t)).Magnitude
	if lato > CORPO_RAGGIO then return nil, 0 end
	return t, CORPO_BASSO + (CORPO_ALTO - CORPO_BASSO) * u
end

function Zombie.lungoLaLinea(da: Vector3, dir: Vector3, gittata: number, _larghezza: number): (Nemico?, number, boolean)
	local meglio: Nemico? = nil
	local migliorT = gittata
	local testa = false
	for _, z in ipairs(nemici) do
		if z.viva then
			local piedi = z.npc.Position
			if piedi then
				local t, alto = sulCorpo(da, dir, piedi, gittata)
				if t and t < migliorT then
					meglio, migliorT, testa = z, math.max(t, 0.05), alto >= TESTA_DA
				end
			end
		end
	end
	return meglio, migliorT, testa
end

--- Pala: chi ti e' davanti, o proprio addosso, entro raggio.
function Zombie.davanti(da: Vector3, dir: Vector3, raggio: number): { Nemico }
	local out = {}
	for _, z in ipairs(nemici) do
		local piedi = z.viva and z.npc.Position
		if piedi then
			local centro = piedi + Vector3.new(0, 2.6, 0)
			local rel = centro - da
			local d = rel.Magnitude
			if d < raggio and d > 0.05 and (d < 3.4 or rel.Unit:Dot(dir) > 0.05) then
				table.insert(out, z)
			end
		end
	end
	return out
end

--- Ferisce un nemico (e lo sveglia). chi: a chi va il premio se muore.
function Zombie.ferisci(z: Nemico, danno: number, chi: Player?)
	if not z.viva then return end
	if chi then z.ultimoColpitoDa = chi end
	z.sveglio = true
	z.vita -= danno
	pcall(function()
		z.npc:TakeDamage(danno)
		z.npc.Health = math.max(0, z.vita)
	end)
	if z.vita <= 0 then muori(z) end
end

--- Il fucile (un colpo): restituisce (colpito, punto d'impatto, alla testa).
function Zombie.spara(chi: Player, da: Vector3, verso: Vector3, danno: number?, gittata: number?): (boolean, Vector3, boolean)
	local dir = verso - da
	if dir.Magnitude < 0.01 then return false, verso, false end
	dir = dir.Unit
	local g = gittata or Config.GITTATA
	local z, t, testa = Zombie.lungoLaLinea(da, dir, g, 1)
	if z then
		Zombie.ferisci(z, (danno or Config.DANNO_FUCILE) * (if testa then Config.DANNO_TESTA else 1), chi)
		return true, da + dir * t, testa
	end
	return false, da + dir * math.min(g, (verso - da).Magnitude), false
end

--- Esplosione: ferisce tutti nel raggio (meno ai bordi).
function Zombie.esplosione(centro: Vector3, raggio: number, danno: number, chi: Player?): number
	local colpiti = 0
	for _, z in ipairs(nemici) do
		local p = z.viva and z.npc.Position
		if p then
			local d = (p - centro).Magnitude
			if d < raggio then
				Zombie.ferisci(z, danno * (1 - 0.6 * d / raggio), chi)
				colpiti += 1
			end
		end
	end
	return colpiti
end

--- Il nemico sveglio piu' vicino a pos entro raggio (per la torretta).
function Zombie.bersaglioVicino(pos: Vector3, raggio: number): Nemico?
	local meglio, d = nil, raggio
	for _, z in ipairs(nemici) do
		local p = z.viva and z.sveglio and z.npc.Position
		if p then
			local dd = (p - pos).Magnitude
			if dd < d then meglio, d = z, dd end
		end
	end
	return meglio
end

function Zombie.quanti(): number return vivi() end

--- Le zone rimaste dietro non servono piu'.
function Zombie.dimenticaZoneDietro(z: number)
	for i = #zone, 1, -1 do
		if zone[i].centro.Z < z - 300 then table.remove(zone, i) end
	end
end

function Zombie.via()
	for _, z in ipairs(nemici) do z.npc:Destroy() end
	table.clear(nemici)
	table.clear(zone)
end

return Zombie
