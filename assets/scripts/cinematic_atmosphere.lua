--[[============================================================================
  Blocky — Atmosfera cinematica "golden hour"
  ==============================================================================
  Uso: Pannello Script -> "Carica cinematic_atmosphere.lua" -> Test (F5)

  Usa solo funzioni che Blocky legge davvero (vedi Lighting piu' sotto: e' un
  gruppo di funzioni, non un oggetto con proprieta' assegnabili come in
  Roblox — Lighting.Ambient = ... non farebbe nulla).
============================================================================]]

local Lighting = game:GetService("Lighting")
local Particles = game:GetService("Particles")

-- Tramonto caldo: il ciclo giorno/notte del motore gia' scalda i colori
-- intorno a quest'ora, qui la spingiamo un filo oltre per il taglio dorato.
Lighting.SetClockTime(17.5)     -- 0..24
Lighting.SetBrightness(3.2)     -- 0..10, sole ancora forte ma basso
Lighting.SetAmbient(0.5)        -- 0..1, ombre morbide, non nere

print("[Atmosfera] Golden hour applicata: ClockTime 17.5, Brightness 3.2, Ambient 0.5")

-- Un pizzico di polvere in sospensione nell'aria intorno al player, per dare
-- profondita' ai raggi di luce — puramente estetico, si puo' togliere.
local Humanoid_ = Humanoid
task.spawn(function()
    while true do
        local p = Humanoid_.GetPosition()
        Particles:Emit(p.X + math.random(-6, 6), p.Y + math.random(1, 4), p.Z + math.random(-6, 6))
        task.wait(0.6)
    end
end)

--[[
Regolazioni pronte, cambia solo i numeri:
  - Alba fredda:      SetClockTime(6.5)  SetBrightness(1.8) SetAmbient(0.35)
  - Mezzogiorno pieno: SetClockTime(13)   SetBrightness(4.5) SetAmbient(0.7)
  - Notte con luna:    SetClockTime(1)    SetBrightness(0.4) SetAmbient(0.15)

Cose "da vera color-grading" che Roblox ha (ColorCorrection, Bloom, DOF da
script) Blocky non le espone ancora a Lua — solo questi tre numeri. Se in
Studio hai gia' attivato Bloom/ombre dal pannello Lighting, restano quelli:
questo script non li tocca ne' li spegne.
]]
