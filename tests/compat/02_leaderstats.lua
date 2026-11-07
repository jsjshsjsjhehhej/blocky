-- leaderstats con Folder e IntValue sotto il giocatore.
local Players = game:GetService("Players")

local function setup(player)
	local leaderstats = Instance.new("Folder")
	leaderstats.Name = "leaderstats"
	leaderstats.Parent = player
	local coins = Instance.new("IntValue")
	coins.Name = "Coins"
	coins.Value = 0
	coins.Parent = leaderstats
	coins.Value += 10
	return coins
end

local player = Players.LocalPlayer
assert(player, "LocalPlayer")
local coins = setup(player)
assert(coins.Value == 10, "IntValue.Value")
local stats = player:FindFirstChild("leaderstats")
assert(stats and stats:FindFirstChild("Coins"), "leaderstats.Coins")
Players.PlayerAdded:Connect(setup)
COMPAT_OK = true
