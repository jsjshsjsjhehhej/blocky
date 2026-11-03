--[[============================================================================
  Blocky — Carrello + bottone "VIA!"
  ==============================================================================
  Uso:
    1) Pannello Script → "Carica cart_lontano.lua"
    2) Test (F5)
    3) Clicca VIA! — il carrello parte lontanissimo.

  In Play: avvicinati al carrello e premi F per salire, poi VIA!
============================================================================]]

local Gui = game:GetService("StarterGui")
local FAR = 12000  -- stud (dentro il limite mondo, ma lontanissimo)

local cart = workspace:FindFirstChild("CartLontano")
if not cart then
    local p = Instance.new("Part")
    p.Name = "CartLontano"
    p.Size = Vector3.new(6, 2, 10)
    p.Position = Vector3.new(0, 3, 8)
    p.Color = Color3.new(0.25, 0.55, 0.85)
    p.Anchored = false
    p.VehicleSeat = true
    p.CanCollide = true
    cart = workspace:Add(p)
end

Gui:CreateLabel({
    Name = "CartViaBtn",
    Text = "VIA!",
    ClassName = "TextButton",
    AnchorX = 0.5,
    AnchorY = 0.88,
    SizeW = 160,
    SizeH = 48,
    FontScale = 1.15,
    BackgroundColor = {0.95, 0.35, 0.12},
    BackgroundTransparency = 0.05,
    TextColor = {1, 1, 1},
    Visible = true,
})

print("[Cart] Pronto — clicca VIA! per mandarlo lontanissimo")

Gui:ConnectButton("CartViaBtn", function()
    local c = workspace:FindFirstChild("CartLontano")
    if not c then
        print("[Cart] Carrello non trovato")
        return
    end
    local dest = Vector3.new(FAR, 40, -FAR)
    c.Position = dest
    if Veicoli and Veicoli.IsDriving() then
        Humanoid.SetPosition(dest.x, dest.y + 4, dest.z)
    end
    Gui:SetLabelText("CartViaBtn", "PARTITO!")
    Gui:SetLabelBg("CartViaBtn", 0.2, 0.75, 0.35, 0.05)
    print("[Cart] Partito a " .. FAR .. ", 40, " .. -FAR)
end)
