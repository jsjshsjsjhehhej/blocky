-- Telefono in mano e un messaggio in chat.
local Config = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"):WaitForChild("Config"))

local gui = Instance.new("ScreenGui")
gui.Name = "Telefono"
gui.Parent = game:GetService("Players").LocalPlayer:WaitForChild("PlayerGui")

local telefono = Instance.new("Frame")
telefono.Name = "Cornice"
telefono.Size = UDim2.fromOffset(160, 260)
telefono.Position = UDim2.new(1, -180, 1, -280)
telefono.BackgroundColor3 = Color3.fromRGB(20, 20, 24)
telefono.Parent = gui

local titolo = Instance.new("TextLabel")
titolo.Size = UDim2.new(1, 0, 0, 36)
titolo.BackgroundTransparency = 1
titolo.Text = "Telefono"
titolo.TextColor3 = Color3.fromRGB(240, 240, 240)
titolo.Parent = telefono

if Config.PROVA_AUTOMATICA then
	local canale = game:GetService("TextChatService").TextChannels.RBXGeneral
	canale:SendAsync("C'e' qualcuno in citta?")
end
