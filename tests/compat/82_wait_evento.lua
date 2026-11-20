-- task.wait dentro un evento del motore (PlayerAdded), come uno script Roblox.
local Players = game:GetService("Players")
local atteso = nil
Players.PlayerAdded:Connect(function(p)
    atteso = task.wait(0.05)
    assert(type(p.Name) == "string" and #p.Name > 0, "il Player arriva all'evento")
    assert(type(atteso) == "number" and atteso >= 0.04, "task.wait nell'evento restituisce il tempo")
    COMPAT_OK = true
end)
