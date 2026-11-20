-- Guidare dal posto del macchinista (il Seat in cabina): seduti, W alza il
-- regolatore e S lo abbassa; il salto fa alzare. Il server accetta il
-- regolatore solo da chi e' davvero seduto (attributo Macchinista del Player).

local Players = game:GetService("Players")
local RunService = game:GetService("RunService")
local UserInputService = game:GetService("UserInputService")
local RS = game:GetService("ReplicatedStorage")

local stato = RS:WaitForChild("Stato")
local azione = RS:WaitForChild("Azione")
local io = Players.LocalPlayer

local VELOCITA_LEVA = 0.45      -- quanto sale o scende il regolatore al secondo
local regolatore = 0
local inviato = -1
local tempoInvio = 0
local eraSeduto = false

-- il cartello in basso quando sei seduto
local gui = Instance.new("ScreenGui")
gui.Name = "Guida"
gui.Parent = io:WaitForChild("PlayerGui")
local cartello = Instance.new("TextLabel")
cartello.AnchorPoint = Vector2.new(0.5, 1)
cartello.Position = UDim2.new(0.5, 0, 1, -150)
cartello.Size = UDim2.new(0, 460, 0, 34)
cartello.BackgroundColor3 = Color3.fromRGB(24, 20, 18)
cartello.BackgroundTransparency = 0.25
cartello.TextColor3 = Color3.fromRGB(240, 190, 50)
cartello.TextSize = 17
cartello.Visible = false
cartello.Parent = gui
local angolo = Instance.new("UICorner")
angolo.CornerRadius = UDim.new(0, 8)
angolo.Parent = cartello

local function giu(tasto: Enum.KeyCode): boolean
	local ok, si = pcall(function() return UserInputService:IsKeyDown(tasto) end)
	return ok and si == true
end

RunService.RenderStepped:Connect(function(dt)
	local seduto = io:GetAttribute("Macchinista") == true
	cartello.Visible = seduto
	if not seduto then
		eraSeduto = false
		return
	end
	if not eraSeduto then
		-- appena seduto: si parte da dove sta la leva
		eraSeduto = true
		regolatore = (stato:GetAttribute("Regolatore") or 0) / 100
		inviato = regolatore
	end
	if giu(Enum.KeyCode.W) or giu(Enum.KeyCode.Up) then
		regolatore = math.min(1, regolatore + VELOCITA_LEVA * dt)
	elseif giu(Enum.KeyCode.S) or giu(Enum.KeyCode.Down) then
		regolatore = math.max(0, regolatore - VELOCITA_LEVA * 1.6 * dt)
	end
	cartello.Text = string.format("MACCHINISTA   W/S regolatore %d%%   -   %d st/s   -   salta per alzarti",
		math.floor(regolatore * 100 + 0.5), stato:GetAttribute("Velocita") or 0)
	-- al server al massimo 10 volte al secondo, e solo se e' cambiato
	tempoInvio += dt
	if tempoInvio >= 0.1 and math.abs(regolatore - inviato) > 0.005 then
		tempoInvio = 0
		inviato = regolatore
		azione:FireServer("regolatore", regolatore)
	end
end)
