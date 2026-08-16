-- Grow a Garden — script place
print("Grow a Garden caricato!")

for x = -1, 1 do
    for z = -1, 1 do
        local plot = Instance.new("Part")
        plot.Name = "GardenPlot_" .. x .. "_" .. z
        plot.Position = Vector3.new(x * 14, 0.5, z * 14)
        plot.Size = Vector3.new(5, 1, 5)
        plot.BrickColor = Color3.new(0.45, 0.28, 0.12)
        workspace:Add(plot)
    end
end

local sign = Instance.new("Part")
sign.Name = "ShopSign"
sign.Position = Vector3.new(0, 3, -18)
sign.Size = Vector3.new(4, 2, 1)
sign.BrickColor = Color3.new(0.95, 0.75, 0.1)
workspace:Add(sign)

print("Garden pronto!")
