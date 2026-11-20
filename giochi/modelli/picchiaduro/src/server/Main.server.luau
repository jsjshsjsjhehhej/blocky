-- Picchiaduro: combo, marker di animazione, hitbox, spinta, barra della vita.
local Config = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"):WaitForChild("Config"))

local bersaglio = Instance.new("Part")
bersaglio.Name = "Bersaglio"
bersaglio.Size = Vector3.new(4, 6, 2)
bersaglio.Position = Vector3.new(0, 4, -14)
bersaglio.Anchored = true
bersaglio.BrickColor = BrickColor.new("Bright red")
bersaglio.Parent = workspace

local gui = Instance.new("BillboardGui")
gui.Name = "Vita"
gui.Size = UDim2.fromOffset(80, 14)
gui.StudsOffset = Vector3.new(0, 4, 0)
gui.Parent = bersaglio
local bar = Instance.new("Frame")
bar.Name = "Barra"
bar.Size = UDim2.fromScale(1, 1)
bar.BackgroundColor3 = Color3.fromRGB(40, 180, 60)
bar.Parent = gui

if not Config.PROVA_AUTOMATICA then return end

local Players = game:GetService("Players")
local function prova(player)
	local char = player.Character
	local hum = char and (char:FindFirstChildOfClass("Humanoid") or char:FindFirstChild("Humanoid"))
	if hum then
		local anim = Instance.new("Animation")
		anim.AnimationId = "blkanim://blocky_r6"
		local track = hum:LoadAnimation(anim)
		track:GetMarkerReachedSignal("Hit"):Connect(function()
			print("[Picchiaduro] marker Hit")
		end)
		track:Play()
	end
	task.wait(0.2)
	local colpiti = workspace:GetPartBoundsInBox(CFrame.new(0, 4, -14), Vector3.new(10, 10, 10))
	local preso = false
	for _, part in ipairs(colpiti) do
		if part == bersaglio or part.Name == "Bersaglio" then preso = true end
	end
	assert(preso, "la hitbox prende il bersaglio")
	bersaglio.Anchored = false
	bersaglio:ApplyImpulse(Vector3.new(0, 120, -80))
	bar.Size = UDim2.fromScale(0.4, 1)
	print("[Picchiaduro] combo 1")
end

Players.PlayerAdded:Connect(function(player)
	prova(player)
end)
for _, player in ipairs(Players:GetPlayers()) do
	task.spawn(prova, player)
end
