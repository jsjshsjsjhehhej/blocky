-- GUI: ScreenGui con un TextButton nel PlayerGui.
local player = game:GetService("Players").LocalPlayer
local playerGui = player:FindFirstChild("PlayerGui") or player.PlayerGui
assert(playerGui, "PlayerGui")
local gui = Instance.new("ScreenGui")
gui.Name = "ShopGui"
gui.ResetOnSpawn = false
gui.Parent = playerGui

local button = Instance.new("TextButton")
button.Size = UDim2.new(0, 200, 0, 50)
button.Position = UDim2.fromScale(0.5, 0.9)
button.AnchorPoint = Vector2.new(0.5, 0.5)
button.Text = "Compra"
button.BackgroundColor3 = Color3.fromRGB(40, 160, 80)
button.Parent = gui

local corner = Instance.new("UICorner")
corner.CornerRadius = UDim.new(0, 8)
corner.Parent = button

button.MouseButton1Click:Connect(function() BOUGHT = true end)
button.Activated:Connect(function() ACTIVATED = true end)
COMPAT_OK = true
