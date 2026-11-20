-- Poligono: un revolver fatto a pezzi (come Dead Rails) che si equipaggia e spara.
-- In mano -Y e' avanti, +Z e' su, +X e' a destra. Il cilindro e' lungo su Size.X.

local Players = game:GetService("Players")
local Debris = game:GetService("Debris")
local SoundService = game:GetService("SoundService")
local Particles = game:GetService("Particles")

local io = Players.LocalPlayer
local mouse = io:GetMouse()

local STEEL = Color3.fromRGB(70, 70, 76)
local BLUED = Color3.fromRGB(42, 45, 54)
local DARK = Color3.fromRGB(30, 30, 35)
local WALNUT = Color3.fromRGB(100, 66, 40)
local DARK_WALNUT = Color3.fromRGB(68, 44, 28)
local BRASS = Color3.fromRGB(205, 160, 60)
local SAND = Color3.fromRGB(194, 164, 112)
local WOOD = Color3.fromRGB(120, 78, 46)

local CYL = Enum.PartType.Cylinder
local AVANTI = CFrame.Angles(0, 0, math.rad(90)) -- in mano: lunghezza del cilindro verso +Y

local function mondo(nome, size, cf, colore, materiale, forma)
	local p = Instance.new("Part")
	p.Name = nome
	p.Size = size
	p.CFrame = cf
	p.Color = colore
	p.Material = materiale or Enum.Material.Wood
	p.Anchored = true
	p.CanCollide = true
	if forma then p.Shape = forma end
	p.Parent = workspace
	return p
end

local function pezzo(tool, nome, size, cf, colore, materiale, forma)
	local p = Instance.new("Part")
	p.Name = nome
	p.Size = size
	p.CFrame = cf
	p.Color = colore
	p.Material = materiale or Enum.Material.Metal
	p.Anchored = true
	p.CanCollide = false
	if forma then p.Shape = forma end
	p.Parent = tool
	return p
end

-- Il terreno e i bersagli, in cerchio: si spara da tutte le parti.
local function campo()
	mondo("Sabbia", Vector3.new(180, 1, 180), CFrame.new(0, -0.5, 0), SAND, Enum.Material.Sand)
	for i = 1, 10 do
		local a = (i / 10) * math.pi * 2
		local r = 22 + (i % 3) * 6
		local x, z = math.cos(a) * r, math.sin(a) * r
		local palo = mondo("Palo", Vector3.new(0.4, 3.2, 0.4), CFrame.new(x, 1.6, z), WOOD, Enum.Material.Wood)
		-- la faccia sottile guarda il centro
		local yaw = math.deg(math.atan2(-math.cos(a), -math.sin(a)))
		local guarda = CFrame.Angles(0, math.rad(yaw), 0)
		local vx, vz = -math.cos(a), -math.sin(a)
		mondo("Tavola", Vector3.new(2.4, 2.4, 0.28), CFrame.new(x, 3.5, z) * guarda, Color3.fromRGB(150, 100, 58), Enum.Material.Wood)
		mondo("Anello", Vector3.new(1.35, 1.35, 0.1), CFrame.new(x + vx * 0.16, 3.5, z + vz * 0.16) * guarda, Color3.fromRGB(236, 236, 232), Enum.Material.SmoothPlastic)
		mondo("Centro", Vector3.new(0.5, 0.5, 0.1), CFrame.new(x + vx * 0.24, 3.5, z + vz * 0.24) * guarda, Color3.fromRGB(176, 42, 36), Enum.Material.SmoothPlastic)
		palo.CFrame = CFrame.new(x, 1.6, z)
		-- bottiglia accanto al palo
		local bx = x + math.cos(a + 1.2) * 2.2
		local bz = z + math.sin(a + 1.2) * 2.2
		mondo("Bottiglia", Vector3.new(0.45, 1.15, 0.45), CFrame.new(bx, 0.7, bz), Color3.fromRGB(150, 190, 160), Enum.Material.Glass, CYL)
		mondo("Collo", Vector3.new(0.18, 0.4, 0.18), CFrame.new(bx, 1.45, bz), Color3.fromRGB(170, 210, 180), Enum.Material.Glass, CYL)
	end
	-- qualche cassa, per avere qualcosa in mezzo
	for _, dove in ipairs({ Vector3.new(8, 0.6, 6), Vector3.new(-7, 0.6, 9), Vector3.new(4, 0.6, -8) }) do
		mondo("Cassa", Vector3.new(2.2, 1.2, 1.6), CFrame.new(dove), Color3.fromRGB(112, 74, 42), Enum.Material.Wood)
	end
end

-- Revolver a singola azione. In mano -Y e' avanti (la canna) e +Z e' su.
-- L'impugnatura scende quasi dritta sotto il castello.
local function revolver()
	local tool = (Instance.new :: any)("Tool", "Revolver")
	tool.ToolTip = "Revolver — clic per sparare"
	tool.CanBeDropped = false

	local grip = CFrame.Angles(math.rad(-78), 0, 0)
	pezzo(tool, "Handle", Vector3.new(0.32, 0.9, 0.34), CFrame.new(0, 0.12, -0.38) * grip, WALNUT, Enum.Material.Wood)
	pezzo(tool, "GripPanelL", Vector3.new(0.06, 0.72, 0.28), CFrame.new(-0.17, 0.12, -0.38) * grip, DARK_WALNUT, Enum.Material.Wood)
	pezzo(tool, "GripPanelR", Vector3.new(0.06, 0.72, 0.28), CFrame.new(0.17, 0.12, -0.38) * grip, DARK_WALNUT, Enum.Material.Wood)
	pezzo(tool, "GripScrew", Vector3.new(0.12, 0.1, 0.1), CFrame.new(0, 0.12, -0.38), BRASS, Enum.Material.Metal, CYL)
	pezzo(tool, "ButtCap", Vector3.new(0.36, 0.08, 0.36), CFrame.new(0, 0.22, -0.85) * grip, BRASS, Enum.Material.Metal)

	pezzo(tool, "Frame", Vector3.new(0.36, 0.85, 0.42), CFrame.new(0, -0.35, 0.12), BLUED, Enum.Material.Metal)
	pezzo(tool, "TopStrap", Vector3.new(0.26, 1.05, 0.1), CFrame.new(0, -0.45, 0.36), BLUED, Enum.Material.Metal)
	pezzo(tool, "Drum", Vector3.new(0.58, 0.62, 0.62), CFrame.new(0, -0.42, 0.18) * AVANTI, STEEL, Enum.Material.Metal, CYL)
	for k = 0, 5 do
		local a = k / 6 * math.pi * 2
		pezzo(tool, "Chamber", Vector3.new(0.08, 0.12, 0.12),
			CFrame.new(math.cos(a) * 0.18, -0.74, 0.18 + math.sin(a) * 0.18) * AVANTI, DARK, Enum.Material.Metal, CYL)
	end
	pezzo(tool, "Barrel", Vector3.new(1.35, 0.2, 0.2), CFrame.new(0, -1.35, 0.28) * AVANTI, BLUED, Enum.Material.Metal, CYL)
	pezzo(tool, "Ejector", Vector3.new(0.95, 0.1, 0.1), CFrame.new(0, -1.15, 0.08) * AVANTI, STEEL, Enum.Material.Metal, CYL)
	pezzo(tool, "EjectorHead", Vector3.new(0.14, 0.14, 0.14), CFrame.new(0, -1.68, 0.08), BRASS, Enum.Material.Metal)
	pezzo(tool, "Muzzle", Vector3.new(0.08, 0.28, 0.28), CFrame.new(0, -2.02, 0.28) * AVANTI, DARK, Enum.Material.Metal, CYL)
	pezzo(tool, "Sight", Vector3.new(0.06, 0.08, 0.14), CFrame.new(0, -1.95, 0.44), BRASS, Enum.Material.Metal)
	pezzo(tool, "RearSight", Vector3.new(0.1, 0.08, 0.1), CFrame.new(0, -0.12, 0.44), BRASS, Enum.Material.Metal)

	local cane = pezzo(tool, "Hammer", Vector3.new(0.1, 0.28, 0.16), CFrame.new(0, 0.05, 0.42) * CFrame.Angles(math.rad(18), 0, 0), BLUED, Enum.Material.Metal)
	pezzo(tool, "HammerSpur", Vector3.new(0.08, 0.08, 0.12), CFrame.new(0, 0.16, 0.52), BRASS, Enum.Material.Metal)
	pezzo(tool, "Trigger", Vector3.new(0.04, 0.16, 0.06), CFrame.new(0, -0.08, -0.02), BRASS, Enum.Material.Metal)
	pezzo(tool, "Guard", Vector3.new(0.1, 0.42, 0.08), CFrame.new(0, -0.02, -0.08), BLUED, Enum.Material.Metal)
	return tool, cane
end

local function scia(da: Vector3, a: Vector3)
	local n = 7
	for i = 0, n do
		local p = da:Lerp(a, i / n)
		local s = Instance.new("Part")
		s.Name = "Scia"
		s.Anchored = true
		s.CanCollide = false
		s.Shape = Enum.PartType.Ball
		s.Material = Enum.Material.Neon
		s.Color = if i == n then Color3.fromRGB(255, 220, 120) else Color3.fromRGB(255, 196, 80)
		local g = if i == n then 0.35 else 0.12
		s.Size = Vector3.new(g, g, g)
		s.Position = p
		s.Parent = workspace
		Debris:AddItem(s, 0.12)
	end
end

local function colpito(part: Instance)
	local nome = part.Name
	if nome == "Bottiglia" or nome == "Collo" then
		local pos = part.Position
		Particles:Sparks(pos, 14)
		pcall(function() SoundService:Play("sounds/hit.wav") end)
		part.Transparency = 1
		part.CanCollide = false
		task.delay(2.5, function()
			part.Transparency = 0
			part.CanCollide = true
		end)
		return
	end
	if nome == "Centro" then
		Particles:Sparks(part.Position, 10)
		local prima = part.Color
		part.Color = Color3.fromRGB(255, 230, 80)
		task.delay(0.15, function() part.Color = prima end)
	elseif nome == "Anello" or nome == "Tavola" or nome == "Cassa" then
		Particles:Sparks(part.Position, 4)
	end
end

campo()
local tool, cane = revolver()
local canePos = cane.Position
local caneRot = cane.Orientation

task.spawn(function()
	task.wait(0.4)
	pcall(function()
		local char = io.Character or io.CharacterAdded:Wait()
		local hum = char:FindFirstChild("Humanoid") or char:WaitForChild("Humanoid")
		hum:EquipTool(tool)
	end)
	pcall(function() Backpack:Equip(tool) end)
end)

local ultimo = 0
tool.Activated:Connect(function()
	local adesso = os.clock()
	if adesso - ultimo < 0.42 then return end
	ultimo = adesso

	local cam = workspace.CurrentCamera
	local da = cam.CFrame.Position
	local verso = mouse.Hit.Position
	local dir = verso - da
	if dir.Magnitude < 0.05 then return end
	local result = workspace:Raycast(da, dir, 250)
	local arrivo = if result then result.Position else da + dir.Unit * 80
	scia(da + dir.Unit * 1.5, arrivo)
	pcall(function() SoundService:Play("sounds/shoot.wav") end)
	pcall(function() Particles:Sparks(arrivo, 6) end)
	if result and result.Instance then colpito(result.Instance) end

	cane.Orientation = caneRot + Vector3.new(-35, 0, 0)
	task.delay(0.07, function()
		cane.Orientation = caneRot
		cane.Position = canePos
	end)
end)

print("Poligono: Revolver in mano. Clic sinistro per sparare, intorno ai bersagli.")
