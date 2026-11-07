-- PlayerAdded + giocatori gia' presenti, CharacterAdded.
local Players = game:GetService("Players")
local seen = {}
local function onPlayer(p)
	seen[p.Name] = true
	p.CharacterAdded:Connect(function(char) end)
	if p.Character then seen.char = true end
end
Players.PlayerAdded:Connect(onPlayer)
for _, p in ipairs(Players:GetPlayers()) do onPlayer(p) end
assert(#Players:GetPlayers() >= 1, "GetPlayers")
assert(seen[Players.LocalPlayer.Name] and seen.char, "giocatore e personaggio")
assert(Players:GetPlayerFromCharacter(Players.LocalPlayer.Character) == Players.LocalPlayer, "GetPlayerFromCharacter")
assert(type(Players.MaxPlayers) == "number", "MaxPlayers")
COMPAT_OK = true
