-- workspace.FallenPartsDestroyHeight: si legge e si cambia come su Roblox.
-- (Blocky non ha piu' il limite di 144 stud in orizzontale: si muore solo sotto questa altezza.)
assert(type(workspace.FallenPartsDestroyHeight) == "number", "FallenPartsDestroyHeight c'e'")
assert(workspace.FallenPartsDestroyHeight == -12, "di partenza -12 (Blocky): " .. tostring(workspace.FallenPartsDestroyHeight))
workspace.FallenPartsDestroyHeight = -500
assert(workspace.FallenPartsDestroyHeight == -500, "si cambia")
-- un giocatore lontano dal centro resta vivo (prima: morto oltre 144 stud)
local root = game:GetService("Players").LocalPlayer.Character:FindFirstChild("HumanoidRootPart")
root.Position = Vector3.new(0, 5, 1000)
assert(root.Position.Z > 999, "il giocatore puo' stare a 1000 stud")
COMPAT_OK = true
