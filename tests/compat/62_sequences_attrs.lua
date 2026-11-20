-- Trovati provando il progetto dead-rails: NumberSequenceKeypoint con 2 argomenti,
-- attributi su workspace, proprieta' dei servizi (Players.CharacterAutoLoads).
local kp = NumberSequenceKeypoint.new(0, 1.5)
assert(kp.Time == 0 and kp.Value == 1.5 and kp.Envelope == 0, "NumberSequenceKeypoint.new(t, v)")
local ns = NumberSequence.new({ NumberSequenceKeypoint.new(0, 1.5), NumberSequenceKeypoint.new(1, 8) })
assert(#ns.Keypoints == 2 and ns.Keypoints[2].Value == 8, "NumberSequence da keypoint")

local volte = 0
workspace:GetAttributeChangedSignal("Night"):Connect(function() volte += 1 end)
workspace:SetAttribute("Night", true)
assert(workspace:GetAttribute("Night") == true and volte == 1, "attributi su workspace")
game:GetService("Lighting"):SetAttribute("Ora", 12)
assert(game:GetService("Lighting"):GetAttribute("Ora") == 12, "attributi su Lighting")

local Players = game:GetService("Players")
Players.CharacterAutoLoads = false
Players.RespawnTime = 3
assert(Players.CharacterAutoLoads == false and Players.RespawnTime == 3, "proprieta' dei servizi")
assert(not pcall(function() Players.GetPlayers2 = function() end end), "una funzione nuova nel servizio resta vietata")
COMPAT_OK = true
