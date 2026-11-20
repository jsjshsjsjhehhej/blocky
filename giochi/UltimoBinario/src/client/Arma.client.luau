-- Le armi e i tasti.
-- Il client dice al server con che arma spara e dove mira (Azione "spara");
-- il server decide chi e' colpito e lo racconta a tutti (Effetto "colpo"),
-- e qui si disegnano scia, schizzi, rinculo ed esplosioni.
--
-- Zaino: 1 Revolver, 2 Fucile a leva, 3 Dinamite, 4 Fucile a pompa (se comprato).

local Players = game:GetService("Players")
local UserInputService = game:GetService("UserInputService")
local RunService = game:GetService("RunService")
local RS = game:GetService("ReplicatedStorage")
local Debris = game:GetService("Debris")

local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))
local azione = RS:WaitForChild("Azione")
local effetto = RS:WaitForChild("Effetto")

local io = Players.LocalPlayer
local mouse = io:GetMouse()
local zaino = io:WaitForChild("Backpack")

-- Un pezzo dell'arma. In mano l'asse Y segue il braccio: Y positivo = avanti.
local function pezzo(t: Tool, nome: string, size: Vector3, col: Color3, mat: Enum.Material, pos: Vector3)
	local p = Instance.new("Part")
	p.Name = nome
	p.Size = size
	p.Color = col
	p.Material = mat
	p.CanCollide = false
	p.Position = pos
	p.Parent = t
	return p
end

local function attrezzo(nome: string, suggerimento: string): Tool
	-- Blocky: Instance.new("Tool") + Parent crea il Tool due volte nella barra
	-- (un "Tool" in piu'). La forma di Blocky con il nome come 2o argomento lo
	-- crea una volta sola, gia' nello zaino (vedi docs/limiti_trovati.md, n. 8).
	local t = (Instance.new :: any)("Tool", nome)
	t.ToolTip = suggerimento
	t.CanBeDropped = false
	return t
end

local METALLO = Color3.fromRGB(70, 72, 78)
local METALLO_SCURO = Color3.fromRGB(36, 36, 40)
local LEGNO = Color3.fromRGB(92, 58, 36)

local function revolver(t: Tool)
	pezzo(t, "Handle", Vector3.new(0.2, 0.22, 0.48), LEGNO, Enum.Material.Wood, Vector3.zero)
	pezzo(t, "Castello", Vector3.new(0.26, 0.42, 0.28), METALLO_SCURO, Enum.Material.Metal, Vector3.new(0, 0.28, 0.12))
	pezzo(t, "Tamburo", Vector3.new(0.32, 0.22, 0.32), METALLO, Enum.Material.Metal, Vector3.new(0, 0.22, 0.14))
	pezzo(t, "Canna", Vector3.new(0.12, 0.85, 0.12), METALLO, Enum.Material.Metal, Vector3.new(0, 0.78, 0.16))
end

local function fucile(t: Tool)
	pezzo(t, "Handle", Vector3.new(0.22, 0.9, 0.26), LEGNO, Enum.Material.Wood, Vector3.zero)
	pezzo(t, "Calcio", Vector3.new(0.24, 0.7, 0.55), Color3.fromRGB(70, 42, 26), Enum.Material.Wood, Vector3.new(0, -0.55, -0.1))
	pezzo(t, "Canna", Vector3.new(0.1, 1.7, 0.1), METALLO, Enum.Material.Metal, Vector3.new(0, 1.05, 0.1))
	pezzo(t, "Leva", Vector3.new(0.08, 0.4, 0.22), Color3.fromRGB(180, 140, 60), Enum.Material.Metal, Vector3.new(0, 0.15, -0.12))
end

local function disegnaPompa(t: Tool)
	pezzo(t, "Handle", Vector3.new(0.22, 0.85, 0.24), LEGNO, Enum.Material.Wood, Vector3.zero)
	pezzo(t, "Canna", Vector3.new(0.14, 1.35, 0.14), METALLO, Enum.Material.Metal, Vector3.new(0, 0.85, 0.1))
	pezzo(t, "Serbatoio", Vector3.new(0.16, 0.9, 0.16), METALLO_SCURO, Enum.Material.Metal, Vector3.new(0, 0.45, -0.08))
end

local function pala(t: Tool)
	pezzo(t, "Handle", Vector3.new(0.16, 1.8, 0.16), Color3.fromRGB(120, 78, 42), Enum.Material.Wood, Vector3.zero)
	pezzo(t, "Collo", Vector3.new(0.18, 0.22, 0.18), METALLO_SCURO, Enum.Material.Metal, Vector3.new(0, 1.0, 0))
	pezzo(t, "Lama", Vector3.new(0.72, 0.12, 0.85), Color3.fromRGB(176, 178, 182), Enum.Material.Metal, Vector3.new(0, 1.35, 0.05))
end

-- ---------------------------------------------------------------- sparare ----

local function mira(): (Vector3, Vector3)
	local cam = workspace.CurrentCamera
	local ok, da = pcall(function() return cam.CFrame.Position end)
	if not ok or typeof(da) ~= "Vector3" then
		local root = io.Character and io.Character:FindFirstChild("HumanoidRootPart")
		da = if root then root.Position + Vector3.new(0, 1.5, 0) else Vector3.zero
	end
	return da, mouse.Hit.Position
end

local ultimo: { [string]: number } = {}
local function pronto(arma: string, ricarica: number): boolean
	local adesso = os.clock()
	if adesso - (ultimo[arma] or 0) < ricarica then return false end
	ultimo[arma] = adesso
	return true
end

local function armaDaFuoco(nome: string, suggerimento: string, disegna: (Tool) -> ())
	local t = attrezzo(nome, suggerimento)
	disegna(t)
	local dati = Config.ARMI[nome]
	t.Activated:Connect(function()
		if not pronto(nome, dati.ricarica) then return end
		local da, verso = mira()
		azione:FireServer("spara", nome, da, verso)
	end)
	return t
end

armaDaFuoco("Revolver", "Revolver: veloce. Clic per sparare", revolver)
armaDaFuoco("Fucile", "Fucile a leva: alla testa fa il doppio", fucile)

local dinamite = attrezzo("Dinamite", "Dinamite: clic per lanciare")
pezzo(dinamite, "Handle", Vector3.new(0.28, 0.85, 0.28), Color3.fromRGB(170, 36, 28), Enum.Material.SmoothPlastic, Vector3.zero)
pezzo(dinamite, "Miccia", Vector3.new(0.08, 0.28, 0.08), Color3.fromRGB(40, 40, 40), Enum.Material.Fabric, Vector3.new(0, 0.52, 0))
dinamite.Activated:Connect(function()
	if not pronto("Dinamite", 1.0) then return end
	local _, verso = mira()
	azione:FireServer("dinamite", verso)
end)

local vanga = attrezzo("Pala", "Pala: clic, colpisce chi ti e' vicino")
pala(vanga)
vanga.Activated:Connect(function()
	if not pronto("Pala", Config.PALA.ricarica) then return end
	local da, verso = mira()
	azione:FireServer("pala", da, verso)
end)

local torcia = attrezzo("Torcia", "Torcia")
Backpack:Equip(torcia)

-- La torcia del motore non ha una Part: la luce e' una fiammella accanto a te.
local fiamma = Instance.new("Part")
fiamma.Name = "LuceTorcia"
fiamma.Anchored = true
fiamma.CanCollide = false
fiamma.Size = Vector3.new(0.35, 0.35, 0.35)
fiamma.Color = Color3.fromRGB(255, 170, 60)
fiamma.Material = Enum.Material.Neon
fiamma.Shape = Enum.PartType.Ball
fiamma.PointLight = true
fiamma.LightRange = 20
fiamma.LightBrightness = 0
fiamma.Parent = workspace

RunService.Heartbeat:Connect(function()
	local accesa = Backpack:Equipped() == "Torcia"
	local root = io.Character and io.Character:FindFirstChild("HumanoidRootPart")
	if accesa and root then
		fiamma.LightBrightness = 1.5 + math.sin(os.clock() * 9) * 0.25
		fiamma.Position = root.Position + Vector3.new(1.3, 0.4, 0)
	else
		fiamma.LightBrightness = 0
		fiamma.Position = Vector3.new(0, -80, 0)
	end
end)

-- il fucile a pompa arriva quando lo compri (attributo HaPompa)
local pompa: Tool? = nil
local function controllaPompa()
	if io:GetAttribute("HaPompa") == true and not pompa then
		pompa = armaDaFuoco("Pompa", "Fucile a pompa: sette pallini, da vicino", disegnaPompa)
	elseif io:GetAttribute("HaPompa") ~= true and pompa then
		pompa:Destroy()
		pompa = nil
	end
end
-- (Blocky: il Player non ha GetAttributeChangedSignal, vedi docs/limiti_trovati.md:
-- si controlla ogni mezzo secondo)
task.spawn(function()
	while true do
		controllaPompa()
		task.wait(0.5)
	end
end)

UserInputService.InputBegan:Connect(function(input, giaUsato)
	if giaUsato then return end
	if input.KeyCode == Enum.KeyCode.H then
		azione:FireServer("benda")
	elseif input.KeyCode == Enum.KeyCode.R then
		azione:FireServer("ricomincia")
	end
end)

-- ---------------------------------------------------------------- effetti ----

local function scintilla(pos: Vector3, col: Color3, s: number, dura: number)
	local p = Instance.new("Part")
	p.Anchored = true
	p.CanCollide = false
	p.Shape = Enum.PartType.Ball
	p.Material = Enum.Material.Neon
	p.Color = col
	p.Size = Vector3.new(s, s, s)
	p.Position = pos
	p.Parent = workspace
	Debris:AddItem(p, dura)
end

effetto.OnClientEvent:Connect(function(cosa, a, b, colpito, testa, banditoPrende)
	if cosa == "colpo" and typeof(a) == "Vector3" and typeof(b) == "Vector3" then
		local lungo = (b - a).Magnitude
		if lungo < 0.5 then return end
		local scia = Instance.new("Part")
		scia.Name = "Scia"
		scia.Anchored = true
		scia.CanCollide = false
		scia.Material = Enum.Material.Neon
		scia.Color = if banditoPrende ~= nil then Color3.fromRGB(255, 90, 60)
			elseif colpito then Color3.fromRGB(255, 160, 90) else Color3.fromRGB(255, 232, 170)
		scia.Size = Vector3.new(0.07, 0.07, lungo)
		scia.CFrame = CFrame.lookAt((a + b) / 2, b)
		scia.Parent = workspace
		Debris:AddItem(scia, 0.07)
		scintilla(a, Color3.fromRGB(255, 210, 120), 0.5, 0.05)      -- vampata alla canna
		if colpito then
			scintilla(b, if testa then Color3.fromRGB(240, 200, 60) else Color3.fromRGB(110, 190, 80), if testa then 1.0 else 0.7, 0.15)
		end
	elseif cosa == "rinculo" then
		pcall(function() Camera.Shake(tonumber(a) or 1, 0.1) end)   -- (Blocky)
	elseif cosa == "esplosione" and typeof(a) == "Vector3" then
		local root = io.Character and io.Character:FindFirstChild("HumanoidRootPart")
		local d = if root then (root.Position - a).Magnitude else 999
		if d < 70 then pcall(function() Camera.Shake(math.clamp(10 - d / 7, 1, 8), 0.5) end) end
		scintilla(a, Color3.fromRGB(255, 150, 40), 6, 0.2)
	end
end)
