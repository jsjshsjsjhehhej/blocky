-- Player:LoadCharacter sposta sul RespawnLocation e fa scattare CharacterAdded.
local Players = game:GetService("Players")
local player = Players.LocalPlayer
local character = player.Character
assert(character, "Character")

local spawn = Instance.new("Part")
spawn.Anchored = true
spawn.Size = Vector3.new(4, 1, 4)
spawn.Position = Vector3.new(30, 8, -12)
spawn.Parent = workspace

local visto = nil
player.CharacterAdded:Connect(function(char)
	visto = char
end)
player.RespawnLocation = spawn
local tornato = player:LoadCharacter()
assert(tornato == character, "LoadCharacter restituisce il Character")
assert(visto == character, "CharacterAdded")

local hum = character:FindFirstChild("Humanoid") or character.Humanoid
local pos = hum:GetPosition()
assert(math.abs(pos.X - 30) < 0.6, "X sullo spawn: " .. tostring(pos.X))
assert(math.abs(pos.Z - -12) < 0.6, "Z sullo spawn: " .. tostring(pos.Z))
assert(pos.Y > 8, "i piedi stanno sopra lo spawn: " .. tostring(pos.Y))
COMPAT_OK = true
