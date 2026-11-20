-- L'HUD: viaggio e prossima tappa, ora, taglia, caldaia e vita del treno,
-- cosa hai addosso, avvisi, mirino, tempesta, fine partita.
-- Legge ReplicatedStorage.Stato (dal server) e gli attributi del proprio Player.

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local RS = game:GetService("ReplicatedStorage")

local stato = RS:WaitForChild("Stato")
local effetto = RS:WaitForChild("Effetto")
local azione = RS:WaitForChild("Azione")
local io = Players.LocalPlayer

-- ---------------------------------------------------------------- aiuti ----

local SCURO = Color3.fromRGB(24, 20, 18)
local CARTA = Color3.fromRGB(236, 222, 196)
local ORO = Color3.fromRGB(240, 190, 50)
local FUOCO = Color3.fromRGB(255, 140, 40)
local ROSSO = Color3.fromRGB(220, 70, 50)
local VERDE = Color3.fromRGB(110, 190, 90)

local function angoli(g: GuiObject, r: number)
	local c = Instance.new("UICorner")
	c.CornerRadius = UDim.new(0, r)
	c.Parent = g
end

local function bordo(g: GuiObject, col: Color3, spessore: number)
	local s = Instance.new("UIStroke")
	s.Color = col
	s.Thickness = spessore
	s.ApplyStrokeMode = Enum.ApplyStrokeMode.Border
	s.Parent = g
end

local function pannello(padre: Instance, pos: UDim2, size: UDim2, trasp: number?): Frame
	local f = Instance.new("Frame")
	f.Position = pos
	f.Size = size
	f.BackgroundColor3 = SCURO
	f.BackgroundTransparency = trasp or 0.25
	angoli(f, 10)
	bordo(f, Color3.fromRGB(120, 92, 60), 2)
	f.Parent = padre
	return f
end

local function testo(padre: Instance, pos: UDim2, size: UDim2, t: string, dim: number, col: Color3?, allinea: string?): TextLabel
	local l = Instance.new("TextLabel")
	l.Position = pos
	l.Size = size
	l.BackgroundTransparency = 1
	l.Text = t
	l.TextSize = dim
	l.TextColor3 = col or CARTA
	l.TextXAlignment = if allinea == "sx" then Enum.TextXAlignment.Left elseif allinea == "dx" then Enum.TextXAlignment.Right else Enum.TextXAlignment.Center
	l.Parent = padre
	return l
end

local function barra(padre: Instance, pos: UDim2, size: UDim2, col: Color3): (Frame, Frame)
	local fondo = Instance.new("Frame")
	fondo.Position = pos
	fondo.Size = size
	fondo.BackgroundColor3 = Color3.fromRGB(50, 42, 36)
	angoli(fondo, 6)
	fondo.Parent = padre
	local pieno = Instance.new("Frame")
	pieno.Size = UDim2.fromScale(0.5, 1)
	pieno.BackgroundColor3 = col
	angoli(pieno, 6)
	pieno.Parent = fondo
	return fondo, pieno
end

-- ---------------------------------------------------------------- schermo ----

local gui = Instance.new("ScreenGui")
gui.Name = "HUD"
gui.Parent = io:WaitForChild("PlayerGui")

-- polvere della tempesta (sotto a tutto)
local polvere = Instance.new("Frame")
polvere.Size = UDim2.fromScale(1, 1)
polvere.BackgroundColor3 = Color3.fromRGB(196, 140, 80)
polvere.BackgroundTransparency = 1
polvere.ZIndex = 0
polvere.Parent = gui

-- rosso quando ti colpiscono
local ferita = Instance.new("Frame")
ferita.Size = UDim2.fromScale(1, 1)
ferita.BackgroundColor3 = Color3.fromRGB(160, 0, 0)
ferita.BackgroundTransparency = 1
ferita.ZIndex = 0
ferita.Parent = gui
local ultimaVita = 100

-- viaggio (in alto a sinistra)
local viaggio = pannello(gui, UDim2.new(0, 12, 0, 12), UDim2.new(0, 320, 0, 134))
testo(viaggio, UDim2.new(0, 14, 0, 6), UDim2.new(1, -28, 0, 24), "ULTIMO BINARIO", 20, ORO, "sx")
local tRecord = testo(viaggio, UDim2.new(0, 14, 0, 6), UDim2.new(1, -28, 0, 24), "", 13, CARTA, "dx")
local _, pienoViaggio = barra(viaggio, UDim2.new(0, 14, 0, 36), UDim2.new(1, -28, 0, 12), ORO)
local tDistanza = testo(viaggio, UDim2.new(0, 14, 0, 52), UDim2.new(1, -28, 0, 22), "", 15, CARTA, "sx")
local tProssima = testo(viaggio, UDim2.new(0, 14, 0, 76), UDim2.new(1, -28, 0, 22), "", 15, CARTA, "sx")
local tOra = testo(viaggio, UDim2.new(0, 14, 0, 100), UDim2.new(1, -28, 0, 22), "", 15, CARTA, "sx")

-- taglia (sotto il viaggio, solo se ne hai una)
local taglia = pannello(gui, UDim2.new(0, 12, 0, 154), UDim2.new(0, 320, 0, 66))
testo(taglia, UDim2.new(0, 14, 0, 4), UDim2.new(1, -28, 0, 20), "TAGLIA", 14, ORO, "sx")
local tTaglia = testo(taglia, UDim2.new(0, 14, 0, 22), UDim2.new(1, -28, 0, 20), "", 15, CARTA, "sx")
local _, pienoTaglia = barra(taglia, UDim2.new(0, 14, 0, 46), UDim2.new(1, -28, 0, 10), VERDE)
taglia.Visible = false

-- caldaia e vita del treno (in basso al centro)
local caldaia = pannello(gui, UDim2.new(0.5, -200, 1, -142), UDim2.new(0, 400, 0, 92))
local tCaldaia = testo(caldaia, UDim2.new(0, 14, 0, 4), UDim2.new(1, -28, 0, 22), "", 15, CARTA, "sx")
local tVelocita = testo(caldaia, UDim2.new(0, 14, 0, 4), UDim2.new(1, -28, 0, 22), "", 15, CARTA, "dx")
local _, pienoCaldaia = barra(caldaia, UDim2.new(0, 14, 0, 30), UDim2.new(1, -28, 0, 14), FUOCO)
local tTreno = testo(caldaia, UDim2.new(0, 14, 0, 48), UDim2.new(1, -28, 0, 20), "", 14, CARTA, "sx")
local _, pienoTreno = barra(caldaia, UDim2.new(0, 14, 0, 70), UDim2.new(1, -28, 0, 10), VERDE)

-- cose addosso (in basso a sinistra)
local VOCI = { "Carbone", "Oro", "Munizioni", "Dinamite", "Bende", "Gettoni" }
local zaino = pannello(gui, UDim2.new(0, 12, 1, -220), UDim2.new(0, 214, 0, 178))
local righe = {}
for i, nome in ipairs(VOCI) do
	righe[nome] = testo(zaino, UDim2.new(0, 14, 0, 8 + (i - 1) * 27), UDim2.new(1, -28, 0, 24), "", 16,
		if nome == "Oro" or nome == "Gettoni" then ORO else CARTA, "sx")
end

-- tasti (in basso a destra)
local aiuto = pannello(gui, UDim2.new(1, -292, 1, -168), UDim2.new(0, 280, 0, 156), 0.45)
local tAiuto = testo(aiuto, UDim2.new(0, 12, 0, 8), UDim2.new(1, -24, 0, 140),
	"Torcia gia' in mano\nCabina: siediti, tieni W\n1 revolver  2 fucile  3 dinamite\n4 pala    E casse    F carbone\nH benda", 15, CARTA, "sx")
tAiuto.TextYAlignment = Enum.TextYAlignment.Top

-- avvisi (in alto al centro)
local avviso = Instance.new("TextLabel")
avviso.AnchorPoint = Vector2.new(0.5, 0)
avviso.Position = UDim2.new(0.5, 0, 0, 130)
avviso.Size = UDim2.new(0, 720, 0, 36)
avviso.BackgroundColor3 = SCURO
avviso.BackgroundTransparency = 0.3
avviso.TextColor3 = CARTA
avviso.TextSize = 18
avviso.Text = ""
avviso.Visible = false
angoli(avviso, 8)
avviso.Parent = gui
local avvisoFino = 0

-- treno che aspetta / guasto
local aspetta = testo(gui, UDim2.new(0.5, -260, 0, 172), UDim2.new(0, 520, 0, 28), "Il treno ti aspetta: torna a bordo!", 18, FUOCO)
aspetta.Visible = false
local guasto = testo(gui, UDim2.new(0.5, -300, 0, 200), UDim2.new(0, 600, 0, 30), "TRENO GUASTO - riparalo all'officina sul tender (E)", 20, ROSSO)
guasto.Visible = false

-- mirino
local mirino = testo(gui, UDim2.new(0.5, -10, 0.5, -10), UDim2.new(0, 20, 0, 20), "+", 22, CARTA)
local colpitoX = testo(gui, UDim2.new(0.5, -14, 0.5, -14), UDim2.new(0, 28, 0, 28), "x", 26, ROSSO)
colpitoX.Visible = false
local colpitoFino = 0

-- fine partita
local fine = Instance.new("Frame")
fine.Size = UDim2.fromScale(1, 1)
fine.BackgroundColor3 = Color3.fromRGB(10, 8, 6)
fine.BackgroundTransparency = 0.25
fine.Visible = false
fine.ZIndex = 10
fine.Parent = gui
local tFine = testo(fine, UDim2.new(0.5, -400, 0.3, -40), UDim2.new(0, 800, 0, 70), "", 48, ORO)
local tStatistiche = testo(fine, UDim2.new(0.5, -400, 0.3, 40), UDim2.new(0, 800, 0, 30), "", 20, CARTA)
local tRecordFine = testo(fine, UDim2.new(0.5, -400, 0.3, 74), UDim2.new(0, 800, 0, 30), "", 18, ORO)
local bottone = Instance.new("TextButton")
bottone.AnchorPoint = Vector2.new(0.5, 0)
bottone.Position = UDim2.new(0.5, 0, 0.3, 130)
bottone.Size = UDim2.new(0, 240, 0, 52)
bottone.BackgroundColor3 = Color3.fromRGB(150, 96, 40)
bottone.TextColor3 = CARTA
bottone.TextSize = 22
bottone.Text = "Ricomincia (R)"
angoli(bottone, 10)
bordo(bottone, ORO, 2)
bottone.Parent = fine
bottone.MouseButton1Click:Connect(function() azione:FireServer("ricomincia") end)

-- ---------------------------------------------------------------- eventi ----

effetto.OnClientEvent:Connect(function(cosa, a, _b, colpito, testa)
	if cosa == "avviso" and type(a) == "string" then
		avviso.Text = a
		avviso.Visible = true
		avvisoFino = os.clock() + 3.5
	elseif cosa == "colpo" and colpito then
		colpitoX.Visible = true
		colpitoX.TextColor3 = if testa then ORO else ROSSO
		colpitoFino = os.clock() + 0.18
	end
end)

-- ---------------------------------------------------------------- ogni frame ----

local function ora(h: number): string
	local hh = math.floor(h)
	local mm = math.floor((h - hh) * 60 / 10) * 10
	return string.format("%02d:%02d", hh, mm)
end

local function attr(nome: string): number
	local v = io:GetAttribute(nome)
	return if type(v) == "number" then v else 0
end

local function num(nome: string, iniziale: number): number
	local v = stato:GetAttribute(nome)
	return if type(v) == "number" then v else iniziale
end

RunService.RenderStepped:Connect(function(dt)
	-- viaggio
	local distanza = num("Distanza", 0)
	local meta = num("Meta", 1)
	pienoViaggio.Size = UDim2.fromScale(math.clamp(distanza / meta, 0, 1), 1)
	tDistanza.Text = string.format("Faro: %d / %d", distanza, meta)
	local prossima = stato:GetAttribute("Prossima")
	tProssima.Text = if type(prossima) == "string" then string.format("Prossima: %s fra %d", prossima, num("ProssimaDist", 0)) else ""
	local notte = stato:GetAttribute("Notte") == true
	local zombie = num("Zombie", 0)
	tOra.Text = "Ora " .. ora(num("Ora", 8)) .. (if notte then "  -  NOTTE" else "  -  giorno")
		.. (if zombie > 0 then "   nemici: " .. zombie else "")
	tOra.TextColor3 = if notte then Color3.fromRGB(255, 120, 100) else CARTA
	tRecord.Text = if attr("Record") > 0 then "record " .. attr("Record") else ""

	-- taglia
	local m = io:GetAttribute("Missione")
	taglia.Visible = type(m) == "string" and m ~= ""
	if taglia.Visible then
		local serve = math.max(1, attr("MissioneServe"))
		local fatto = attr("MissioneFatto")
		tTaglia.Text = string.format("%s  (%d/%d)", m, fatto, serve)
		pienoTaglia.Size = UDim2.fromScale(math.clamp(fatto / serve, 0, 1), 1)
	end

	-- caldaia e treno
	local c = num("Caldaia", 0)
	pienoCaldaia.Size = UDim2.fromScale(math.clamp(c / 100, 0, 1), 1)
	pienoCaldaia.BackgroundColor3 = if c < 20 then ROSSO else FUOCO
	tCaldaia.Text = "Caldaia " .. c .. "%" .. (if c <= 0 then "  - VUOTA" else "")
	tVelocita.Text = (if stato:GetAttribute("Marcia") then "Leva: avanti   " else "Leva: ferma   ") .. num("Velocita", 0) .. " st/s"
	local vt, vtMax = num("VitaTreno", 1), math.max(1, num("VitaTrenoMax", 1))
	pienoTreno.Size = UDim2.fromScale(math.clamp(vt / vtMax, 0, 1), 1)
	pienoTreno.BackgroundColor3 = if vt / vtMax < 0.3 then ROSSO else VERDE
	tTreno.Text = "Treno " .. vt .. " / " .. vtMax
	guasto.Visible = stato:GetAttribute("Guasto") == true

	-- zaino
	righe.Carbone.Text = "Carbone in mano  " .. attr("Carbone") .. " / 3"
	righe.Oro.Text = "Oro  " .. attr("Oro")
	righe.Munizioni.Text = "Munizioni  " .. attr("Munizioni")
	righe.Dinamite.Text = "Dinamite  " .. attr("Dinamite") .. "   (3)"
	righe.Bende.Text = "Bende  " .. attr("Bende") .. "   (H)"
	righe.Gettoni.Text = "Gettoni  " .. attr("Gettoni")

	aspetta.Visible = stato:GetAttribute("Aspetta") == true
	if avviso.Visible and os.clock() > avvisoFino then avviso.Visible = false end
	if colpitoX.Visible and os.clock() > colpitoFino then colpitoX.Visible = false end

	-- tempesta: lo schermo si vela di sabbia
	polvere.BackgroundTransparency = 1 - 0.45 * num("Tempesta", 0)

	-- ferite: rosso che sfuma
	local hum = io.Character and io.Character:FindFirstChild("Humanoid")
	local vita = if hum then hum.Health else 100
	if vita < ultimaVita - 0.5 then ferita.BackgroundTransparency = 0.55 end
	ultimaVita = vita
	ferita.BackgroundTransparency = math.min(1, ferita.BackgroundTransparency + dt * 1.6)

	-- fine partita
	local f = stato:GetAttribute("Fine") or ""
	fine.Visible = f ~= ""
	mirino.Visible = f == ""
	if f ~= "" then
		tFine.Text = if f == "vinto" then "Sei arrivato al Faro!" else "Il viaggio finisce qui..."
		tStatistiche.Text = string.format("Distanza %d   -   nemici abbattuti %d   -   taglie %d   -   gettoni %d",
			distanza, attr("Uccisi"), attr("Taglie"), attr("Gettoni"))
		tRecordFine.Text = "Il tuo record: " .. attr("Record")
	end
end)
