-- Humanoid del personaggio: vita, velocita, morte.
local player = game:GetService("Players").LocalPlayer
local character = player.Character
assert(character, "player.Character")
local humanoid = character:FindFirstChild("Humanoid") or character.Humanoid
humanoid.WalkSpeed = 24
humanoid.JumpPower = 60
assert(humanoid.WalkSpeed == 24, "WalkSpeed")
humanoid.HealthChanged:Connect(function() end)
local before = humanoid.Health
humanoid:TakeDamage(10)
assert(humanoid.Health <= before - 10 + 0.01, "TakeDamage: " .. tostring(humanoid.Health))
humanoid.Died:Connect(function() DIED = true end)
local root = character:FindFirstChild("HumanoidRootPart")
assert(root, "HumanoidRootPart")
COMPAT_OK = true
