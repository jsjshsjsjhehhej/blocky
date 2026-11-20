-- CoreScript: la barra della vita in alto a destra.
-- Prima era C++ (main_ui/PlayingHudPanel.h, drawHealthBarHud); ora e' una GUI
-- in stile Roblox fatta da qui. Quando parte dice al motore di non disegnare
-- piu' la sua (__blkCoreGui); se lo stato Lua non parte, resta quella del C++.
--
-- Come su Roblox: StarterGui:SetCoreGuiEnabled(Enum.CoreGuiType.Health, false)
-- la nasconde. Si aggiorna solo quando la vita cambia (leggera sui telefoni).

local Internal = ...

Internal.modulo("core/BarraVita", function(I)
	if type(__blkCoreGui) ~= "function" then return end
	local Players = game:GetService("Players")
	local io = Players.LocalPlayer
	if type(io) ~= "table" then return end
	local pg = rawget(io, "PlayerGui")
	if pg == nil then
		local ok, v = pcall(function() return io:FindFirstChild("PlayerGui") end)
		pg = ok and v or nil
	end
	if pg == nil then return end

	local LARGO, ALTO = 220, 18
	local gui = Instance.new("ScreenGui")
	gui.Name = "BlockyBarraVita"
	gui.DisplayOrder = 100                 -- sopra le GUI dei giochi
	gui.Parent = pg

	local fondo = Instance.new("Frame")
	fondo.Name = "Fondo"
	fondo.AnchorPoint = Vector2.new(1, 0)
	fondo.Position = UDim2.new(1, -16, 0, 20)   -- sotto la barra in alto (36 px di inset)
	fondo.Size = UDim2.new(0, LARGO, 0, ALTO)
	fondo.BackgroundColor3 = Color3.fromRGB(35, 35, 40)
	fondo.BackgroundTransparency = 0.15
	fondo.Parent = gui
	local angoli = Instance.new("UICorner")
	angoli.CornerRadius = UDim.new(0, 4)
	angoli.Parent = fondo
	local bordo = Instance.new("UIStroke")
	bordo.Color = Color3.fromRGB(0, 0, 0)
	bordo.Transparency = 0.35
	bordo.Thickness = 3
	bordo.ApplyStrokeMode = Enum.ApplyStrokeMode.Border
	bordo.Parent = fondo

	local pieno = Instance.new("Frame")
	pieno.Name = "Pieno"
	pieno.Size = UDim2.new(1, 0, 1, 0)
	pieno.BackgroundColor3 = Color3.fromRGB(70, 200, 90)
	pieno.Parent = fondo
	local angoli2 = Instance.new("UICorner")
	angoli2.CornerRadius = UDim.new(0, 3)
	angoli2.Parent = pieno

	local testo = Instance.new("TextLabel")
	testo.Name = "Testo"
	testo.Size = UDim2.new(1, 0, 1, 0)
	testo.BackgroundTransparency = 1
	testo.TextColor3 = Color3.fromRGB(255, 255, 255)
	testo.TextSize = 13
	testo.Text = ""
	testo.ZIndex = 3
	testo.Parent = fondo

	local function colore(pct: number): Color3
		if pct > 0.5 then return Color3.fromRGB(70, 200, 90) end
		if pct > 0.25 then return Color3.fromRGB(230, 190, 60) end
		return Color3.fromRGB(220, 70, 60)
	end

	local function humanoid()
		local char = io.Character
		if type(char) ~= "table" then return nil end
		local ok, h = pcall(function() return char:FindFirstChild("Humanoid") or char.Humanoid end)
		return ok and h or nil
	end

	local ultima, ultimaMax = -1, -1
	local function aggiorna()
		local h = humanoid()
		if not h then return end
		local vita, max = tonumber(h.Health) or 0, math.max(1, tonumber(h.MaxHealth) or 100)
		if vita == ultima and max == ultimaMax then return end
		ultima, ultimaMax = vita, max
		if vita <= 0 then
			pieno.Size = UDim2.new(1, 0, 1, 0)
			pieno.BackgroundColor3 = Color3.fromRGB(120, 30, 30)
			testo.Text = "Dead"
		else
			local pct = math.clamp(vita / max, 0, 1)
			pieno.Size = UDim2.new(pct, 0, 1, 0)
			pieno.BackgroundColor3 = colore(pct)
			testo.Text = string.format("Health  %d / %d", math.floor(vita + 0.5), math.floor(max + 0.5))
		end
	end

	-- Quattro volte al secondo, ma si ridisegna solo se la vita e' cambiata.
	-- (L'Heartbeat delle CoreScripts: quello di RunService lo azzera lo Studio
	-- a ogni avvio degli script del gioco.)
	local acc = 0
	__blkCoreGui("Heartbeat", function(dt)
		acc += dt
		if acc >= 0.25 then
			acc = 0
			aggiorna()
		end
	end)
	aggiorna()
	__blkCoreGui("Health", true)

	-- StarterGui:SetCoreGuiEnabled / GetCoreGuiEnabled (per ora: Health e All)
	local accese = { Health = true }
	local function nome(tipo): string
		if type(tipo) == "table" then return tostring(tipo.Name) end
		return tostring(tipo)
	end
	I.realFuncs.StarterGui = I.realFuncs.StarterGui or {}
	I.realFuncs.StarterGui.SetCoreGuiEnabled = function(_, tipo, on)
		local n = nome(tipo)
		if n == "Health" or n == "All" then
			accese.Health = on and true or false
			gui.Enabled = accese.Health
		end
	end
	I.realFuncs.StarterGui.GetCoreGuiEnabled = function(_, tipo)
		local n = nome(tipo)
		if n == "Health" or n == "All" then return accese.Health end
		return true
	end
end)
