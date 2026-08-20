-- ============================================================
--  TORCIA — esempio di comportamento oggetto stile Roblox Tool
--
--  Qualunque item nell'inventario appare già in mano da solo se ha
--  un meshPath (comportamento automatico del motore). Questo script
--  mostra come reagire a "l'oggetto è stato usato" (LMB / pad X) e
--  a "l'oggetto è stato equipaggiato" — la parte che decide COSA fa
--  la torcia, non che gli fa tenere in mano l'oggetto.
--
--  Vale per qualunque item: basta cambiare itemName e la reazione
--  dentro l'if. Una chiave che apre una porta e un medikit che cura
--  sono lo stesso schema con un corpo diverso.
-- ============================================================

local ITEM_NAME = "Torcia"

local Lighting = game:GetService("Lighting")
local torchOn = false
local baseBrightness = Lighting:GetBrightness()

-- Scatta per QUALSIASI item usato: si filtra per nome subito, cosi'
-- lo stesso listener puo' restare attivo anche con altri oggetti in giro.
Events.Connect("ItemActivated", function(itemId, itemName)
    if itemName ~= ITEM_NAME then return end

    torchOn = not torchOn
    if torchOn then
        Lighting:SetBrightness(baseBrightness + 1.4)
        print("[Torcia] accesa")
    else
        Lighting:SetBrightness(baseBrightness)
        print("[Torcia] spenta")
    end
end)

print("[Torcia] pronta — equipaggia '" .. ITEM_NAME .. "' e premi il tasto usa")
