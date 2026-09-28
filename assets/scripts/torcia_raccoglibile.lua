-- Torcia raccoglibile: crea una part "TorciaPickup", toccala per riceverla
-- nello zaino. Poi carica anche item_torcia.lua per la reazione all'uso.
local part = Instance.new("Part")
part.Name = "TorciaPickup"
part.Position = Vector3.new(4, 1, 4)
part.Size = Vector3.new(1, 1, 1)
part.Material = "Neon"
part.BrickColor = Color3.new(1, 0.8, 0.3)
part.Emissive = 1.2
part.PointLight = true
part.LightRange = 12
part.LightBrightness = 1.4
-- workspace:Add restituisce la part VIVA, gia' collegata al mondo — quella
-- creata da Instance.new e' solo un descrittore, non ha Touched/Destroy.
local livePart = workspace:Add(part)

livePart.Touched:Connect(function()
    Backpack.Give("Torcia")
    livePart.Destroy()
    print("[Torcia] raccolta!")
end)
