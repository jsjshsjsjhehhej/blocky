-- Come Roblox: si puo' aspettare nel corpo dello script, senza task.spawn.
-- (prima: "task.wait() va usato dentro task.spawn")
local giri = 0
local atteso = task.wait(0.1)
assert(type(atteso) == "number" and atteso >= 0.09, "task.wait restituisce il tempo atteso: " .. tostring(atteso))
while giri < 3 do
    giri += 1
    task.wait()
end
local p = Instance.new("Part")
p.Name = "ArrivaDopo"
task.delay(0.2, function() p.Parent = workspace end)
local trovata = workspace:WaitForChild("ArrivaDopo", 2)
assert(trovata and trovata.Name == "ArrivaDopo", "WaitForChild aspetta davvero nel corpo dello script")
assert(giri == 3)
COMPAT_OK = true
