-- Baseplate — script di avvio
print("Benvenuto su Baseplate!")
print("Usa WASD per muoverti, Esc = menu")

local platform = Instance.new("Part")
platform.Name = "LuaPlatform"
platform.Position = Vector3.new(0, 1, 8)
platform.Size = Vector3.new(6, 1, 6)
platform.BrickColor = Color3.new(0.2, 0.6, 0.9)
workspace:Add(platform)

game.RunService.Heartbeat:Connect(function(dt)
end)
