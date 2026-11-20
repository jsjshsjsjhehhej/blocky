-- Le armi sul server: ricarica, munizioni, chi viene colpito.
-- Il client manda solo da dove guarda e dove mira; qui si decide tutto il resto
-- (cosi' un client non puo' sparare piu' veloce o senza munizioni).

local RS = game:GetService("ReplicatedStorage")
local Debris = game:GetService("Debris")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local C = require(Shared:WaitForChild("Costruisci"))

local Armi = {}

type Bersaglio = {
	-- un sistema che si puo' colpire (gli zombie; la torretta usa lo stesso)
	lungoLaLinea: (Vector3, Vector3, number, number) -> (any, number, boolean),
	ferisci: (any, number, Player?) -> (),
	esplosione: (Vector3, number, number, Player?) -> number,
	davanti: (Vector3, Vector3, number) -> { any },
}

local bersagli: Bersaglio? = nil
local Bottino: any = nil
local avvisa: (Player?, string) -> () = function() end
local effetto: (string, ...any) -> () = function() end

function Armi.usa(t: { bersagli: Bersaglio, bottino: any, avvisa: (Player?, string) -> (), effetto: (string, ...any) -> () })
	bersagli = t.bersagli
	Bottino = t.bottino
	avvisa = t.avvisa
	effetto = t.effetto
end

local ultimo: { [Player]: { [string]: number } } = {}
local rng = Random.new(Config.SEME + 31)

local function pronto(g: Player, arma: string, ricarica: number): boolean
	local t = ultimo[g]
	if not t then t = {} ultimo[g] = t end
	local adesso = os.clock()
	if (t[arma] or 0) + ricarica > adesso then return false end
	t[arma] = adesso
	return true
end

local function origine(g: Player, da: any): Vector3?
	local char = g.Character
	local root = char and char:FindFirstChild("HumanoidRootPart")
	if not root then return nil end
	-- il colpo parte dalla camera del client, ma non lontano dal giocatore
	if typeof(da) == "Vector3" and (da - root.Position).Magnitude < 30 then return da end
	return root.Position + Vector3.new(0, 1.5, 0)
end

--- Ha quest'arma? (Revolver e Fucile li hanno tutti; la Pompa si compra)
function Armi.possiede(g: Player, arma: string): boolean
	if arma == "Pompa" then return g:GetAttribute("HaPompa") == true end
	return Config.ARMI[arma] ~= nil
end

--- Un colpo (o una rosata di pallini) con l'arma scelta.
function Armi.spara(g: Player, arma: string, da: any, verso: any)
	local dati = Config.ARMI[arma]
	if not dati or not bersagli or typeof(verso) ~= "Vector3" then return end
	if not Armi.possiede(g, arma) then return end
	if not pronto(g, arma, dati.ricarica) then return end
	if Bottino.prendi(g, "Munizioni") <= 0 then
		avvisa(g, "Niente munizioni: cercale nelle casse o comprale dal mercante")
		return
	end
	local o = origine(g, da)
	if not o then return end
	Bottino.aggiungi(g, "Munizioni", -1)
	local dir0 = verso - o
	if dir0.Magnitude < 0.01 then return end
	dir0 = dir0.Unit
	-- per i pallini: due direzioni di lato, perpendicolari al colpo
	local destra = dir0:Cross(Vector3.new(0, 1, 0))
	if destra.Magnitude < 0.01 then destra = Vector3.new(1, 0, 0) end
	destra = destra.Unit
	local su = destra:Cross(dir0).Unit
	local partenza = o
	local char = g.Character
	local root = char and char:FindFirstChild("HumanoidRootPart")
	local canna = if root then root.Position + Vector3.new(0, 1.5, 0) else o
	local unoPreso, unaTesta = false, false
	for p = 1, dati.pallini do
		local dir = dir0
		if dati.apertura > 0 then
			dir = (dir0 + destra * rng:NextNumber(-dati.apertura, dati.apertura) + su * rng:NextNumber(-dati.apertura, dati.apertura)).Unit
		end
		local z, t, testa = bersagli.lungoLaLinea(partenza, dir, dati.gittata, if dati.pallini > 1 then 0.8 else 1)
		local arrivo = partenza + dir * (if z then t else math.min(dati.gittata, (verso - o).Magnitude))
		if z then
			bersagli.ferisci(z, dati.danno * (if testa then Config.DANNO_TESTA else 1), g)
			unoPreso = true
			unaTesta = unaTesta or testa
		end
		if p <= 3 then effetto("colpo", canna, arrivo, z ~= nil, testa) end
	end
	effetto("suono", if arma == "Pompa" then "shotgun" else "shoot")
	effetto("rinculo", g, dati.colpo)
	return unoPreso, unaTesta
end

--- Lancia un candelotto: vola fino al punto (al massimo Config.DINAMITE.lancio
--- stud), la miccia brucia, poi esplode. Fa male anche a chi e' troppo vicino.
function Armi.dinamite(g: Player, verso: any)
	if typeof(verso) ~= "Vector3" or not bersagli then return end
	if not pronto(g, "Dinamite", 1.0) then return end
	if Bottino.prendi(g, "Dinamite") <= 0 then
		avvisa(g, "Niente dinamite: la vende il mercante")
		return
	end
	local char = g.Character
	local root = char and char:FindFirstChild("HumanoidRootPart")
	if not root then return end
	Bottino.aggiungi(g, "Dinamite", -1)
	local da = root.Position + Vector3.new(0, 2, 0)
	local fino = verso - da
	if fino.Magnitude > Config.DINAMITE.lancio then fino = fino.Unit * Config.DINAMITE.lancio end
	local arrivo = Vector3.new(da.X + fino.X, math.max(0.6, da.Y + fino.Y), da.Z + fino.Z)

	local candelotto = C.part({ nome = "Dinamite", dimensione = Vector3.new(0.4, 0.4, 1.1), posizione = da,
		colore = Color3.fromRGB(180, 40, 30), materiale = Enum.Material.SmoothPlastic, collisione = false }, workspace)
	local miccia = C.part({ nome = "Miccia", dimensione = Vector3.new(0.25, 0.25, 0.25), posizione = da + Vector3.new(0, 0.3, 0),
		colore = Color3.fromRGB(255, 200, 80), neon = true, forma = Enum.PartType.Ball, collisione = false }, workspace)
	effetto("suono", "throw")
	task.spawn(function()
		-- volo a parabola
		local durata = 0.55
		local t = 0
		while t < durata do
			local dt = task.wait()
			t = math.min(durata, t + dt)
			local k = t / durata
			local p = da:Lerp(arrivo, k) + Vector3.new(0, math.sin(k * math.pi) * 5, 0)
			candelotto.Position = p
			candelotto.Orientation = Vector3.new(k * 720, 0, 0)
			miccia.Position = p + Vector3.new(0, 0.4, 0)
		end
		-- miccia che lampeggia
		local fine = os.clock() + Config.DINAMITE.miccia
		while os.clock() < fine do
			miccia.Transparency = if miccia.Transparency > 0.5 then 0 else 1
			task.wait(0.12)
		end
		local centro = candelotto.Position
		miccia:Destroy()
		-- l'esplosione del motore (fiamme, spinta, danno a chi e' vicino) + quella sugli NPC
		pcall(function() candelotto:Explode(18) end)
		Debris:AddItem(candelotto, 0.05)
		local n = bersagli.esplosione(centro, Config.DINAMITE.raggio, Config.DINAMITE.danno, g)
		effetto("esplosione", centro)
		if n >= 3 then avvisa(g, "Boom! " .. n .. " nemici colpiti") end
	end)
end

--- Un fendente: niente munizioni. Colpisce chi ti e' davanti o addosso.
function Armi.pala(g: Player, da: any, verso: any)
	if not bersagli or typeof(verso) ~= "Vector3" then return end
	if not pronto(g, "Pala", Config.PALA.ricarica) then return end
	local o = origine(g, da)
	if not o then return end
	local dir = verso - o
	if dir.Magnitude < 0.01 then return end
	dir = dir.Unit
	local n = 0
	for _, z in ipairs(bersagli.davanti(o, dir, Config.PALA.raggio)) do
		bersagli.ferisci(z, Config.PALA.danno, g)
		n += 1
	end
	effetto("suono", "hit")
	effetto("rinculo", g, 0.6)
	if n > 0 then effetto("colpo", o, o + dir * 3, true, false) end
end

function Armi.dimentica(g: Player)
	ultimo[g] = nil
end

return Armi
