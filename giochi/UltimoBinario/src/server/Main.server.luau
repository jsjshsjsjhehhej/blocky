-- Ultimo Binario — il server: tiene insieme treno, mondo, nemici, bottino,
-- armi, officina, missioni, meteo e record; decide quando si vince o si perde.
--
-- Lo stato che il client mostra (HUD) sta negli attributi della cartella
-- ReplicatedStorage.Stato; le cose di ogni giocatore negli attributi del Player.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local Lighting = game:GetService("Lighting")
local RS = game:GetService("ReplicatedStorage")

local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local Treno = require(script.Parent.Treno)
local Mondo = require(script.Parent.Mondo)
local Insediamenti = require(script.Parent.Insediamenti)
local Zombie = require(script.Parent.Zombie)
local Bottino = require(script.Parent.Bottino)
local Armi = require(script.Parent.Armi)
local Officina = require(script.Parent.Officina)
local Missioni = require(script.Parent.Missioni)
local Meteo = require(script.Parent.Meteo)
local Record = require(script.Parent.Record)
local Trasporto = require(script.Parent.Trasporto)

-- ---------------------------------------------------------------- stato condiviso ----

local stato = Instance.new("Folder")
stato.Name = "Stato"
stato.Parent = RS

local azione = Instance.new("RemoteEvent")      -- client -> server
azione.Name = "Azione"
azione.Parent = RS

local effettoRemoto = Instance.new("RemoteEvent")     -- server -> client: avvisi, colpi, suoni, esplosioni
effettoRemoto.Name = "Effetto"
effettoRemoto.Parent = RS

local function avvisa(g: Player?, testo: string)
	if g then
		pcall(function() effettoRemoto:FireClient(g, "avviso", testo) end)
	else
		pcall(function() effettoRemoto:FireAllClients("avviso", testo) end)
	end
end

--- Un effetto per tutti; "rinculo" solo per chi ha sparato.
local function effetto(cosa: string, ...: any)
	local args = { ... }
	if cosa == "rinculo" then
		local g = args[1]
		pcall(function() effettoRemoto:FireClient(g, "rinculo", args[2]) end)
		return
	end
	pcall(function() effettoRemoto:FireAllClients(cosa, table.unpack(args)) end)
end

local function suono(nome: string, _dove: Vector3?)
	effetto("suono", nome)
end

-- ---------------------------------------------------------------- collegamenti ----

local distanzaFatta = 0

Bottino.quandoAvviso(avvisa)
Bottino.quandoSuono(suono)
Bottino.quandoVenduto(function(g, n) Missioni.venduto(g, n) end)
Bottino.quandoOggetto(function(tipo, pos) Trasporto.oggetto(tipo, pos) end)
Zombie.quandoSuono(suono)
Zombie.quandoScia(function(da, a, preso) effetto("colpo", da, a, false, false, preso) end)
Zombie.quandoAttaccaTreno(function(danno) Officina.danno(danno) end)
Meteo.quandoAvviso(avvisa)
Insediamenti.usa({
	cassa = Bottino.cassa,
	oro = Bottino.oro,
	mercante = Bottino.mercante,
	bacheca = Missioni.bacheca,
	zona = Zombie.zona,
})
Armi.usa({ bersagli = Zombie, bottino = Bottino, avvisa = avvisa, effetto = effetto })
Officina.usa({ treno = Treno, bottino = Bottino, zombie = Zombie, avvisa = avvisa, effetto = effetto })
Missioni.usa({ bottino = Bottino, avvisa = avvisa, effetto = effetto, distanza = function() return distanzaFatta end })
Trasporto.usa({ treno = Treno, bottino = Bottino, missioni = Missioni, avvisa = avvisa, effetto = effetto })

Zombie.quandoUcciso(function(g: Player?, tipo: string, dove: Vector3)
	-- i banditi a volte lasciano un lingotto
	local dati = Config.ZOMBIE_TIPI[tipo]
	if dati.oro and math.random() < dati.oro then
		local cartella = workspace:FindFirstChild("Mondo") or workspace
		Bottino.oro(Vector3.new(dove.X, 0.5, dove.Z), cartella)
	end
	if not g then return end
	Bottino.aggiungi(g, "Gettoni", dati.premio)
	Bottino.aggiungi(g, "Uccisi", 1)
	Missioni.ucciso(g, tipo)
	avvisa(g, tipo .. " abbattuto: +" .. dati.premio .. " gettoni")
end)

-- ---------------------------------------------------------------- partita ----

local orologio = 0          -- secondi dall'inizio della partita
local fine = ""             -- "", "vinto", "morto"
local fermoPerChi = false   -- il treno aspetta chi e' rimasto a terra
local preparando = false

local function ora(): number
	return (Config.ORA_INIZIO + orologio / Config.DURATA_GIORNO * 24) % 24
end

local function eNotte(): boolean
	local h = ora()
	return h >= Config.NOTTE_DA or h < Config.NOTTE_A
end

local function sulTreno(g: Player)
	local char = g.Character
	local root = char and char:FindFirstChild("HumanoidRootPart")
	if root and Treno.modello() then root.CFrame = CFrame.new(Treno.puntoSalita()) end
end

--- All'inizio il motore puo' rimettere il personaggio sul suo punto di partenza:
--- per qualche secondo si controlla che stia davvero sul treno.
local function tieniSulTreno(g: Player)
	task.spawn(function()
		for i = 1, 6 do
			local root = g.Character and g.Character:FindFirstChild("HumanoidRootPart")
			if root and not Treno.sopra(root.Position) then
				print(string.format("[Treno] %s non e' sul treno (%s, coda a %.1f): lo rimetto su", g.Name, tostring(root.Position), Treno.retro()))
				sulTreno(g)
			end
			task.wait(0.5)
		end
	end)
end

local function preparaGiocatore(g: Player)
	Bottino.iniziale(g)
	local char = g.Character
	local hum = char and char:FindFirstChild("Humanoid")
	if hum then hum.Health = hum.MaxHealth end
	sulTreno(g)
	tieniSulTreno(g)
end

local collegaTreno: () -> ()

--- Una partita nuova. Costruire treno e mondo richiede qualche frame (Costruisci
--- lascia respirare il gioco), percio' gira in un task suo, una alla volta.
local function nuovaPartita()
	if preparando then return end
	preparando = true
	task.spawn(function()
		local ok, err = pcall(function()
			Zombie.via()
			Trasporto.via()
			Mondo.ricomincia()
			Treno.crea()
			Bottino.cassaforte(Treno.modello():FindFirstChild("Cassaforte") :: Part)
			Officina.nuovaPartita()
			collegaTreno()
			orologio = 0
			distanzaFatta = 0
			Meteo.nuovaPartita()
			Mondo.preparaSubito(Treno.retro())
			Missioni.nuovaPartita(Players:GetPlayers())
			for _, g in ipairs(Players:GetPlayers()) do preparaGiocatore(g) end
		end)
		if not ok then warn("[Partita] " .. tostring(err)) end
		fine = ""
		preparando = false
		if Config.PROVA_AUTOMATICA then
			-- (per chi sviluppa) parte da solo, con la caldaia piena
			Treno.aggiungiCarbone(Config.CARBONE_MAX)
			Treno.impostaMarcia(true)
			print("[Prova] treno avviato da solo (Config.PROVA_AUTOMATICA)")
		end
		avvisa(nil, "Siediti in cabina e tieni W. Tasto 1 e clic per sparare.")
	end)
end

Players.PlayerAdded:Connect(function(g)
	g.CharacterAdded:Connect(function()
		task.wait(0.2)
		tieniSulTreno(g)
	end)
	task.wait(0.2)
	Record.carica(g)
	preparaGiocatore(g)
end)

Players.PlayerRemoving:Connect(function(g)
	Armi.dimentica(g)
	Missioni.dimentica(g)
	Record.dimentica(g)
end)

for _, g in ipairs(Players:GetPlayers()) do
	task.spawn(Record.carica, g)
end

-- ---------------------------------------------------------------- comandi ----

azione.OnServerEvent:Connect(function(g: Player, cosa: string, a, b, c)
	if type(g) ~= "table" or preparando then return end
	if cosa == "spara" then
		-- a = arma, b = da dove guarda la camera, c = dove mira
		Armi.spara(g, tostring(a), b, c)
	elseif cosa == "dinamite" then
		Armi.dinamite(g, a)
	elseif cosa == "pala" then
		Armi.pala(g, a, b)
	elseif cosa == "benda" then
		Bottino.usaBenda(g)
	elseif cosa == "regolatore" then
		-- solo chi e' seduto al posto del macchinista
		if g:GetAttribute("Macchinista") == true and type(a) == "number" then
			if Treno.guasto() then return end
			Treno.impostaRegolatore(a)
		end
	elseif cosa == "ricomincia" then
		if fine ~= "" then nuovaPartita() end
	end
end)

-- leva e caldaia (a ogni partita: i prompt del treno sono nuovi)
function collegaTreno()
	local leva = Treno.promptLeva()
	local caldaia = Treno.promptCaldaia()
	if leva then
		leva.Triggered:Connect(function(g)
			if Treno.guasto() then
				avvisa(g, "Il treno e' guasto: riparalo all'officina sul tender")
				return
			end
			Treno.impostaMarcia(not Treno.inMarcia())
			if Treno.inMarcia() and Treno.carbone() <= 0 then
				avvisa(g, "La caldaia e' vuota: serve carbone")
			end
		end)
	end
	if caldaia then
		caldaia.Triggered:Connect(function(g)
			if type(g) ~= "table" then return end
			if Bottino.prendi(g, "Carbone") <= 0 then
				avvisa(g, "Non hai carbone in mano. Cercalo nelle casse")
				return
			end
			if Treno.carbone() >= Treno.carboneMax() - 1 then
				avvisa(g, "La caldaia e' piena")
				return
			end
			Bottino.aggiungi(g, "Carbone", -1)
			Treno.aggiungiCarbone(Config.CARBONE_PER_PEZZO)
			suono("coal")
			avvisa(g, "Caldaia: " .. math.floor(Treno.carbone() / Treno.carboneMax() * 100) .. "%")
		end)
	end
end

-- ---------------------------------------------------------------- cielo ----

local luciAccese = false

local function cielo()
	local h = ora()
	Lighting.ClockTime = h
	-- 0 = mezzogiorno, 1 = mezzanotte
	local buio = math.clamp((math.cos((h - 12) / 24 * math.pi * 2) * -1 + 1) / 2, 0, 1)
	buio = math.clamp((buio - 0.25) / 0.6, 0, 1)
	local sabbia = Meteo.intensita()
	Lighting.Brightness = (2.2 - 1.8 * buio) * (1 - 0.35 * sabbia)
	Lighting.FogStart = 40 * (1 - sabbia) + 5
	Lighting.FogEnd = (650 - 470 * buio) * (1 - 0.8 * sabbia) + 20
	local giorno = Color3.fromRGB(214, 190, 150)
	local notte = Color3.fromRGB(18, 20, 34)
	local tempesta = Color3.fromRGB(196, 140, 80)
	Lighting.FogColor = giorno:Lerp(notte, buio):Lerp(tempesta, sabbia * (1 - buio * 0.7))
	Lighting.OutdoorAmbient = Color3.fromRGB(150, 135, 115):Lerp(Color3.fromRGB(40, 44, 70), buio)
	local servono = buio > 0.45 or sabbia > 0.5
	if servono ~= luciAccese then
		luciAccese = servono
		Treno.luci(servono)
	end
end

-- ---------------------------------------------------------------- ciclo ----

local function vivi(): { Player }
	local out = {}
	for _, g in ipairs(Players:GetPlayers()) do
		local hum = g.Character and g.Character:FindFirstChild("Humanoid")
		if hum and hum.Health > 0 then table.insert(out, g) end
	end
	return out
end

local function qualcunoVicino(): boolean
	for _, g in ipairs(vivi()) do
		local root = g.Character:FindFirstChild("HumanoidRootPart")
		if root and root.Position.Z > Treno.retro() - 40 then return true end
	end
	return false
end

--- La prossima tappa davanti al treno (nome e distanza).
local NOMI_TAPPE = { citta = "Citta'", avamposto = "Avamposto", forte = "Forte", accampamento = "Accampamento dei banditi" }
local function prossimaTappa(): (string, number)
	local z = Treno.fronte()
	local n = math.max(1, math.ceil((z - 30) / Config.TAPPA))
	local zt = n * Config.TAPPA
	if zt >= Config.LUNGHEZZA_VIAGGIO - 200 then return "Il Faro", Config.LUNGHEZZA_VIAGGIO - z end
	return NOMI_TAPPE[Mondo.tipoTappa(n)] or "?", zt - z
end

local function finePartita(come: string)
	fine = come
	for _, g in ipairs(Players:GetPlayers()) do
		if Record.fine(g, Treno.fronte()) then
			avvisa(g, "Nuovo record: " .. math.floor(Treno.fronte()) .. " stud!")
		end
	end
	if come == "vinto" then avvisa(nil, "Sei arrivato al Faro!") end
end

--- Chi sta sul treno viaggia con lui: si sposta nello stesso momento del treno
--- (prima lo faceva il client in un altro momento del frame: microscatti).
--- E chi cade sotto il terreno torna sul treno invece di morire.
local function portaGiocatori(dz: number)
	local sedile = Treno.sedile()
	for _, g in ipairs(Players:GetPlayers()) do
		local root = g.Character and g.Character:FindFirstChild("HumanoidRootPart")
		if root then
			local p = root.Position
			if dz > 0 and Treno.sopra(p) then
				p += Vector3.new(0, 0, dz)
				root.Position = p
			end
			if p.Y < -4 then
				sulTreno(g)
				local hum = g.Character:FindFirstChild("Humanoid")
				if hum and hum.Health > 20 then hum:TakeDamage(15) end
				avvisa(g, "Sei caduto! Torni sul treno (-15 vita)")
			end
			-- seduto al posto del macchinista? (Blocky non ha Seat.Occupant: si misura)
			local seduto = false
			if sedile then
				local d = p - sedile.Position
				seduto = math.abs(d.X) < 1.2 and math.abs(d.Z) < 1.2 and d.Y > -0.5 and d.Y < 3.2
			end
			if g:GetAttribute("Macchinista") ~= seduto then g:SetAttribute("Macchinista", seduto) end
		end
	end
end

local accLento = 0
local accStato = 0

-- Cronometro (con Config.PROVA_AUTOMATICA): il frame piu' lento di ogni sistema,
-- stampato ogni 5 secondi. Serve a trovare da dove vengono gli scatti.
local misure: { [string]: number } = {}
local frameLenti, frameMax, tempoMisure = 0, 0, 0
local function misura(nome: string, t0: number)
	local ms = (os.clock() - t0) * 1000
	if ms > (misure[nome] or 0) then misure[nome] = ms end
end
local function stampaMisure(dt: number)
	if not Config.PROVA_AUTOMATICA then return end
	frameMax = math.max(frameMax, dt * 1000)
	if dt > 0.025 then frameLenti += 1 end
	tempoMisure += dt
	if tempoMisure < 5 then return end
	local parti = {}
	for _, nome in ipairs({ "treno", "giocatori", "oggetti", "nemici", "officina", "mondo", "stato" }) do
		table.insert(parti, string.format("%s %.2f", nome, misure[nome] or 0))
	end
	print(string.format("[Prestazioni] ms massimi: %s | frame piu' lungo %.1f ms, frame oltre 25 ms: %d | %.0f st/s",
		table.concat(parti, ", "), frameMax, frameLenti, Treno.velocita()))
	table.clear(misure)
	frameLenti, frameMax, tempoMisure = 0, 0, 0
end

RunService.Heartbeat:Connect(function(dt)
	dt = math.min(dt, 0.1)
	if preparando then return end
	if fine ~= "" then
		stato:SetAttribute("Fine", fine)
		return
	end
	stampaMisure(dt)
	orologio += dt
	local notte = eNotte()

	-- il treno non lascia indietro nessuno: se tutti sono lontani, frena
	fermoPerChi = not qualcunoVicino()
	local t0 = os.clock()
	local dz = Treno.passo(dt, fermoPerChi)
	misura("treno", t0)
	distanzaFatta += dz
	t0 = os.clock()
	portaGiocatori(dz)
	misura("giocatori", t0)
	t0 = os.clock()
	Trasporto.passo(dt, dz)
	misura("oggetti", t0)
	t0 = os.clock()
	Zombie.passo(dt, notte, Treno, dz)
	misura("nemici", t0)
	t0 = os.clock()
	Officina.passo(dt)
	Meteo.passo(dt, notte)
	misura("officina", t0)

	accLento += dt
	if accLento > 0.25 then
		accLento = 0
		t0 = os.clock()
		Mondo.aggiorna(Treno.retro())
		misura("mondo", t0)
		Zombie.dimenticaZoneDietro(Treno.retro())
		Missioni.passo()
		cielo()
		for _, g in ipairs(Players:GetPlayers()) do Record.aggiorna(g, Treno.fronte()) end
		local nome, quanto = prossimaTappa()
		stato:SetAttribute("Prossima", nome)
		stato:SetAttribute("ProssimaDist", math.max(0, math.floor(quanto)))
	end

	-- stato per l'HUD (10 volte al secondo bastano)
	accStato += dt
	if accStato < 0.1 then
		if Treno.alla_meta() then finePartita("vinto") end
		return
	end
	accStato = 0
	stato:SetAttribute("Regolatore", math.floor(Treno.regolatore() * 100 + 0.5))
	stato:SetAttribute("SulTreno", Trasporto.sulTreno())
	stato:SetAttribute("Distanza", math.floor(Treno.fronte()))
	stato:SetAttribute("Meta", Config.LUNGHEZZA_VIAGGIO)
	stato:SetAttribute("Velocita", math.floor(Treno.velocita() + 0.5))
	stato:SetAttribute("Caldaia", math.floor(Treno.carbone() / Treno.carboneMax() * 100))
	stato:SetAttribute("Marcia", Treno.inMarcia())
	stato:SetAttribute("Guasto", Treno.guasto())
	stato:SetAttribute("VitaTreno", math.floor(Officina.vita()))
	stato:SetAttribute("VitaTrenoMax", Officina.vitaMax())
	stato:SetAttribute("Ora", ora())
	stato:SetAttribute("Notte", notte)
	stato:SetAttribute("Tempesta", Meteo.intensita())
	stato:SetAttribute("Zombie", Zombie.quanti())
	stato:SetAttribute("Aspetta", fermoPerChi)
	stato:SetAttribute("TrenoZ", Treno.retro())
	stato:SetAttribute("Fine", "")

	-- fine partita
	if Treno.alla_meta() then
		finePartita("vinto")
	elseif #Players:GetPlayers() > 0 and #vivi() == 0 then
		finePartita("morto")
	end
end)

-- ---------------------------------------------------------------- via ----

nuovaPartita()
