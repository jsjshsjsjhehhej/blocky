-- Particelle Blocky (Play / Test)
-- game.Particles:Emit / Sparks / Smoke / Coins

local t = 0
game.RunService.Heartbeat:Connect(function(dt)
    t = t + dt
    if t > 2.5 then
        t = 0
        local p = Vector3.new(0, 4, 6)
        game.Particles:Sparks(p)
        -- game.Particles:Smoke(p)
        -- game.Particles:Coins(p, 20, Color3.new(1, 0.9, 0.2))
        -- game.Particles:Emit(p, 24, Color3.new(0.4, 0.8, 1))
    end
end)

print("Particles demo: sparks ogni 2.5s a (0,4,6)")
