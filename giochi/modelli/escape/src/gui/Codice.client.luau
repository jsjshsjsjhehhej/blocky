-- La cassaforte chiede un codice.
local Config = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"):WaitForChild("Config"))
local Players = game:GetService("Players")

local gui = Instance.new("ScreenGui")
gui.Name = "Cassaforte"
gui.Parent = Players.LocalPlayer:WaitForChild("PlayerGui")

local box = Instance.new("TextBox")
box.Name = "Codice"
box.Size = UDim2.fromOffset(120, 36)
box.Position = UDim2.fromOffset(24, 24)
box.PlaceholderText = "codice"
box.Text = ""
box.Parent = gui

local function controlla()
	if box.Text == "1492" then
		local porta = workspace:FindFirstChild("PortaTempo")
		if porta then porta.Transparency = 0.85 porta.CanCollide = false end
		print("[Escape] codice giusto")
	end
end
box.FocusLost:Connect(function()
	controlla()
end)

if Config.PROVA_AUTOMATICA then
	box.Text = "1492"
	controlla()
end
