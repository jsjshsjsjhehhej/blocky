-- Camera di lato: il personaggio si muove solo lungo X.
local Config = require(game:GetService("ReplicatedStorage"):WaitForChild("Shared"):WaitForChild("Config"))
local Players = game:GetService("Players")
local RunService = game:GetService("RunService")

local cam = workspace.CurrentCamera
cam.CameraType = Enum.CameraType.Scriptable
cam.CFrame = CFrame.lookAt(Vector3.new(0, 8, 28), Vector3.new(0, 4, 0))

if not Config.PROVA_AUTOMATICA then return end

RunService.Heartbeat:Connect(function()
	local char = Players.LocalPlayer.Character
	if not char then return end
	local root = char:FindFirstChild("HumanoidRootPart") or char:FindFirstChild("Torso")
	if not root then return end
	local p = root.Position
	root.CFrame = CFrame.new(p.X, p.Y, 0)
	cam.CFrame = CFrame.lookAt(Vector3.new(p.X, p.Y + 4, 28), Vector3.new(p.X, p.Y + 2, 0))
end)
