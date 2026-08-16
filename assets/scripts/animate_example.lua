-- AnimateFrame — animazione professionale (frame + posizione)
-- Usa il primo modello nel Workspace (importa un Model in Studio prima).
--
-- API:
--   AnimateFrame(frame, Vector3.new(x, y, z))
--   AnimateFrame("NomeModello", frame, Vector3.new(x, y, z))

print("[AnimateFrame] Script animazione avviato")

-- Keyframe: frame → posizione
AnimateFrame(0,  Vector3.new(0, 2, 0))
AnimateFrame(10, Vector3.new(4, 3, 0))
AnimateFrame(20, Vector3.new(8, 2, 4))
AnimateFrame(30, Vector3.new(4, 4, 8))
AnimateFrame(40, Vector3.new(0, 2, 0))

print("[AnimateFrame] Clip impostata (F0 → F40). Apri Animator Model e premi Play.")

-- Opzionale: anima ogni Heartbeat leggendo il tempo
-- (i key restano sul modello; lo scrubber Studio li riproduce)
local t = 0
game.RunService.Heartbeat:Connect(function(dt)
    t = t + dt
    -- esempio: ogni 2 secondi log
    if t > 2 then
        t = 0
        print("[AnimateFrame] in esecuzione — usa Play nell'Animator")
    end
end)
