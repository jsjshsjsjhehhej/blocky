-- API aggiunte dopo Ultimo Binario: rete, attributi del Player, Seat, Tool, leaderstats,
-- materiali, typeof.
local Players = game:GetService("Players")
local plr = Players.LocalPlayer

-- SetNetworkOwner & co.: non fermano piu' lo script (la fisica e' del server)
local p = Instance.new("Part")
p.Parent = workspace
p:SetNetworkOwner(nil)
p:SetNetworkOwnershipAuto()
assert(p:GetNetworkOwner() == nil, "GetNetworkOwner")
assert(p:CanSetNetworkOwnership() == true, "CanSetNetworkOwnership")

-- attributi del Player con gli eventi
local volte, nomi = 0, {}
plr:GetAttributeChangedSignal("Oro"):Connect(function() volte += 1 end)
plr.AttributeChanged:Connect(function(n) table.insert(nomi, n) end)
plr:SetAttribute("Oro", 3)
plr:SetAttribute("Oro", 3)        -- uguale: niente evento
plr:SetAttribute("Oro", 5)
assert(volte == 2, "GetAttributeChangedSignal: " .. volte)
assert(#nomi == 2 and nomi[1] == "Oro", "AttributeChanged")
assert(plr:GetAttributes().Oro == 5, "GetAttributes")

-- Seat: fa sedere, e nessuno e' seduto finche' non ci si siede
local s = Instance.new("Seat")
s.Anchored = true
s.Position = Vector3.new(30, 2, 30)
s.Parent = workspace
assert(s.Seat == true, "Instance.new('Seat') e' un sedile vero")
assert(s.Occupant == nil, "Seat.Occupant libero")
local hum = plr.Character.Humanoid
assert(hum.Sit == false and hum.SeatPart == nil, "Humanoid.Sit / SeatPart")
hum.Sit = false   -- alzarsi quando non si e' seduti: non fa niente

-- Tool: Name prima del Parent, rinominare dopo, niente doppioni nello zaino
local t = Instance.new("Tool")
t.Name = "Spada"
local h = Instance.new("Part")
h.Name = "Handle"
h.Parent = t
t.Parent = plr.Backpack
assert(t.Name == "Spada", "Tool.Name: " .. tostring(t.Name))
local attivato = 0
t.Activated:Connect(function() attivato += 1 end)
t.Name = "Lama"
assert(t.Name == "Lama", "Tool rinominato")

-- leaderstats: la cartella di Blocky non e' un figlio; quella dello script si'
assert(plr:FindFirstChild("leaderstats") == nil or typeof(plr:FindFirstChild("leaderstats").GetChildren) == "function",
	"FindFirstChild('leaderstats') non da' la tabella finta")
local ls = Instance.new("Folder")
ls.Name = "leaderstats"
ls.Parent = plr
local monete = Instance.new("IntValue")
monete.Name = "Monete"
monete.Parent = ls
assert(plr:FindFirstChild("leaderstats") == ls and ls:FindFirstChild("Monete") == monete, "leaderstats vera")
assert(plr.leaderstats == ls, "player.leaderstats e' quella dello script")

-- materiali di Roblox che Blocky non ha: il piu' simile
local m = Instance.new("Part")
m.Material = Enum.Material.Sandstone
m.Parent = workspace
assert(m.Material == Enum.Material.Sand, "Sandstone -> Sand: " .. tostring(m.Material))

-- typeof
assert(typeof(UDim2.new(0.5, 1, 0.5, 2)) == "UDim2", "typeof UDim2: " .. typeof(UDim2.new()))
assert(typeof(UDim.new(1, 2)) == "UDim", "typeof UDim: " .. typeof(UDim.new(1, 2)))
assert(typeof(Vector2.new(1, 2)) == "Vector2", "typeof Vector2")

COMPAT_OK = true
