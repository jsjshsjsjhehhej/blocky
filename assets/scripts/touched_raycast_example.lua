-- Demo Touched + Raycast (metti su una Part e premi Test)
local part = script.Parent

part.Touched:Connect(function(other, partName)
    print("Touched!", partName or part.Name, "da", other.Name)
    game.Particles:Sparks(part.Position)
end)

-- Raycast ogni secondo dal cielo verso il basso
local t = 0
game.RunService.Heartbeat:Connect(function(dt)
    t = t + dt
    if t < 1.0 then return end
    t = 0
    local origin = Vector3.new(0, 40, 0)
    local dir = Vector3.new(0, -1, 0)
    local hit = workspace:Raycast(origin, dir, 100)
    if hit then
        print("Raycast hit", hit.Instance.Name, "dist", hit.Distance)
    end
end)
