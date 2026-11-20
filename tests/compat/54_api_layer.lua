-- API di Roblox in Blocky: comportamento vero dove c'e', strato "presente" (valori
-- iniziali giusti, eventi collegabili, metodi che dicono chiaro che mancano) altrove.

-- Tag sulle Instance (CollectionService sotto), sia su una Part sia su un oggetto Luau.
local p = Instance.new("Part")
p.Size = Vector3.new(2, 3, 4)
p.Parent = workspace
p:AddTag("Nemico")
assert(p:HasTag("Nemico"), "Part:HasTag dopo AddTag")
assert(table.find(p:GetTags(), "Nemico"), "Part:GetTags")
assert(#game:GetService("CollectionService"):GetTagged("Nemico") >= 1, "GetTagged vede il tag messo con AddTag")
p:RemoveTag("Nemico")
assert(not p:HasTag("Nemico"), "RemoveTag")

-- Massa = densita' x volume.
local mass = p.Mass
assert(type(mass) == "number" and mass > 0, "Part.Mass e' un numero: " .. tostring(mass))

-- Piattaforma vera.
local UIS = game:GetService("UserInputService")
assert(type(UIS.TouchEnabled) == "boolean" and UIS.KeyboardEnabled ~= UIS.TouchEnabled, "TouchEnabled / KeyboardEnabled")

-- Classe che il motore non implementa: valori iniziali di Roblox.
local h = Instance.new("Highlight")
assert(h.FillTransparency == 0.5, "Highlight.FillTransparency iniziale")
assert(h.DepthMode == Enum.HighlightDepthMode.AlwaysOnTop, "Highlight.DepthMode e' un Enum vero")
h.FillTransparency = 0.2
assert(h.FillTransparency == 0.2, "la proprieta' si scrive e si rilegge")

-- Enum con i numeri veri di Roblox.
assert(Enum.KeyCode.E ~= nil and Enum.CameraMode.LockFirstPerson.Value == 1, "Enum con i valori di Roblox")
assert(#Enum.EasingStyle:GetEnumItems() > 5, "GetEnumItems")

-- Metodo che non c'e' ancora: errore chiaro, non "attempt to call nil".
local ok, err = pcall(function() h:Destroy() h = Instance.new("Highlight") return game:GetService("MarketplaceService"):PromptBulkPurchase() end)
assert(not ok and tostring(err):find("non e' ancora in Blocky"), "errore chiaro per i metodi mancanti: " .. tostring(err))

-- Evento collegabile anche se il motore non lo fa ancora scattare.
local conn = game:GetService("MarketplaceService").PromptGamePassPurchaseFinished:Connect(function() end)
assert(conn ~= nil, "evento collegabile")

COMPAT_OK = true
