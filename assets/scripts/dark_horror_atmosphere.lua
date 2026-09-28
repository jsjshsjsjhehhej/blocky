--[[============================================================================
  Blocky — Atmosfera dark horror
  ==============================================================================
  Versione funzionante dello script "stile Roblox" che avevi incollato. In
  Blocky game:GetService("Lighting") esiste, ma NON e' un oggetto con
  proprieta' assegnabili (niente Lighting.Ambient = ..., niente Instance.new
  di ColorCorrectionEffect/BloomEffect/DepthOfFieldEffect: quelle chiamate
  giravano senza errore ma non facevano nulla, perche' Instance.new sa creare
  solo Part/Ball/Cylinder/Wedge). Qui sotto solo funzioni che l'engine legge
  davvero.
============================================================================]]

local Lighting = game:GetService("Lighting")
local SoundService = game:GetService("SoundService")

Lighting.SetClockTime(0.2)   -- 0..24, quasi mezzanotte
Lighting.SetBrightness(0.6)  -- 0..10, luce del sole/luna molto bassa
Lighting.SetAmbient(0.08)    -- 0..1, ombre quasi nere

-- Suono ambientale in loop. Metti un tuo file in assets/sounds/ e cambia il
-- nome qui sotto — "rbxassetid://" non esiste in Blocky, serve un file vero.
SoundService.Play("sounds/groan.wav", true)

print("[Horror] Atmosfera applicata: notte, buio, ambient scuro")

--[[
Cosa manca DAVVERO in Blocky rispetto allo script originale (non e' un bug
tuo, sono funzionalita' non ancora esposte a Lua):
  - Tinta colore fredda/bluastra sull'ambient (oggi Ambient e' solo un
    numero 0..1, non un Color3)
  - Colore/distanza della nebbia da script (il fog esiste nel motore e si
    vede in Studio, ma non e' collegato a Lua)
  - Bloom e DepthOfField non sono regolabili da script

Se vuoi davvero quella tinta fredda e la nebbia controllabili da script,
sono poche righe da aggiungere lato motore (SetAmbientTint/SetFogColor) —
dimmelo e le aggiungo.
]]
