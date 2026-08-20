-- ============================================================
--  JUMPSCARE TRIGGER — audio + flash + scossa camera
--
--  Script.Parent = una Part invisibile o mimetizzata (canCollide false)
--  messa dove vuoi che scatti il jumpscare. Un solo colpo, non si
--  riarma da solo — è così per una trigger volume da corridoio, non
--  per un nemico che colpisce più volte (per quello Camera.Shake si
--  chiama direttamente da mostro_ai.lua sul colpo d'attacco).
-- ============================================================

local part = script.Parent
local Lighting = game:GetService("Lighting")
local Sound = game:GetService("SoundService")

local fired = false
local baseBrightness = Lighting:GetBrightness()

part.Touched:Connect(function(hit)
    if fired or hit.Name ~= "Player" then return end
    fired = true

    -- Flash: un picco di luminosità che rientra da solo in una manciata
    -- di frame — non serve un timer separato, basta un valore alto ora
    -- e uno script che lo riabbassa al prossimo Heartbeat.
    Lighting:SetBrightness(baseBrightness * 6)

    Sound:Play("assets/sounds/beep.wav", false)

    -- Camera.Shake(gradi, secondi): 4° è un impatto, oltre 8-10° è un
    -- vero jumpscare — troppo oltre i 15° la vista diventa illeggibile.
    Camera.Shake(9, 0.5)

    print("[Jumpscare] scattato")
end)

game:GetService("RunService").Heartbeat:Connect(function(dt)
    if not fired then return end
    -- Rientro morbido della luminosità in circa mezzo secondo.
    local current = Lighting:GetBrightness()
    if current > baseBrightness then
        Lighting:SetBrightness(current - (current - baseBrightness) * math.min(1, dt * 6))
    end
end)

print("[Jumpscare] trigger pronto su " .. part.Name)
