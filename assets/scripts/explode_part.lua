-- Esplosione leggera (anti-freeze)
local part = script.Parent
part.CanDestroy = true
part.CanCollide = false

local esploso = false
part.Touched:Connect(function()
    if esploso then return end
    esploso = true
    print("BOOM!")
    part:Explode(16) -- danno player + flash (senza pioggia di particelle)
end)
