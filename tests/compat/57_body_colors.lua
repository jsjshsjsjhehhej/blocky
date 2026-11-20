-- BodyColors / Shirt su un NPC, con il Parent messo dopo (come su Roblox):
-- colorano l'NPC, non il giocatore; prima del Parent non colorano nessuno.
local Actors = game:GetService("Actors")
local npc = Actors:Spawn("Colorato", Vector3.new(0, 3, 40))
local bc = Instance.new("BodyColors")
bc.HeadColor3 = Color3.new(0, 0, 0)
bc.TorsoColor3 = Color3.new(0.1, 0.1, 0.2)
bc.Parent = npc
local shirt = Instance.new("Shirt")
shirt.Color3 = Color3.new(1, 0, 0)
local orphan = Instance.new("BodyColors")      -- senza Parent: non tocca nessuno
orphan.HeadColor3 = Color3.new(1, 1, 0)
shirt.Parent = npc
assert(bc.Parent == npc and shirt.Parent == npc, "Parent dei vestiti")
-- MaxHealth di un NPC: si puo' alzare, e la vita non lo supera
local bruto = Actors:Spawn("Bruto", Vector3.new(4, 3, 40))
bruto.MaxHealth = 320
bruto.Health = 320
assert(bruto.MaxHealth == 320 and bruto.Health == 320, "MaxHealth alzato: " .. tostring(bruto.Health))
bruto.Health = 999
assert(bruto.Health == 320, "la vita si ferma al massimo")
bruto.MaxHealth = 50
assert(bruto.Health == 50, "abbassando il massimo scende anche la vita")
COMPAT_OK = true
