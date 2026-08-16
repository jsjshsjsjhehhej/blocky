-- Blocco che salta — solo questa Part (Lua 5.4 / script.Parent)
-- Studio: seleziona Part → Properties → "Blocco che salta" → Test (F5)

local part = script.Parent
local base = part.Position
local altezza = 5
local velocita = 2.5
local t = 0

print("Salto: " .. part.Name)

game.RunService.Heartbeat:Connect(function(dt)
    t = t + dt
    local y = base.y + math.abs(math.sin(t * velocita)) * altezza
    part.Position = Vector3.new(base.x, y, base.z)
end)
