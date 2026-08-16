-- Mega Obby — genera piattaforme via Lua
print("Mega Obby — buona fortuna!")

local colors = {
    {0.9, 0.2, 0.15},
    {0.95, 0.5, 0.1},
    {0.2, 0.7, 0.95},
    {0.6, 0.2, 0.85},
}

for i = 0, 10 do
    local step = Instance.new("Part")
    step.Name = "ObbyStep" .. i
    step.Position = Vector3.new(i * 3.5, 0.5 + i * 1.2, math.sin(i) * 2)
    step.Size = Vector3.new(3, 1, 3)
    local c = colors[(i % #colors) + 1]
    step.BrickColor = Color3.new(c[1], c[2], c[3])
    workspace:Add(step)
end

print("Obby pronto!")
