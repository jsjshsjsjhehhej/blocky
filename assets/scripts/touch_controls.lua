--[[============================================================================
  Blocky — Comandi a schermo: joystick direzionale + bottone salto
  ==============================================================================
  Uso:
    1) Pannello Script -> "Carica touch_controls.lua"
    2) Test (F5)
    3) Tocca le frecce per camminare, il centro per fermarti, JUMP per saltare.

  NOTA ONESTA sui limiti (l'API Lua di Blocky oggi non espone di piu'):
    - I bottoni mandano un evento al CLICK, non "mentre sono premuti": quindi
      le frecce funzionano a interruttore (tocchi = parti, ritocchi = ti fermi)
      invece che come una leva analogica.
    - Non esiste una funzione per far saltare il player, quindi il salto qui e'
      un arco calcolato dallo script: sale e ridiscende da solo. Va bene su
      terreno piano; su dislivelli forti puo' sembrare rigido.
    - Muovendo la posizione da script si passa sopra alle collisioni: non
      contare su questi comandi per attraversare muri "legalmente".
============================================================================]]

local Gui = game:GetService("StarterGui")
local RS  = game:GetService("RunService")

local SPEED      = 14      -- studs al secondo
local JUMP_H     = 5.0     -- altezza dell'arco di salto
local JUMP_TIME  = 0.55    -- durata del salto in secondi

-- Posizione dei comandi sullo schermo (0..1 = frazione di schermo)
local PAD_X, PAD_Y   = 0.13, 0.80   -- joystick in basso a sinistra
local JUMP_X, JUMP_Y = 0.88, 0.82   -- salto in basso a destra
local BTN            = 62           -- lato dei bottoni direzionali

-- Direzione attiva, in spazio locale del player: 1 = avanti, -1 = indietro
local moveFwd, moveSide = 0, 0
-- Stato del salto
local jumping, jumpT, jumpBaseY = false, 0, 0

------------------------------------------------------------------- UI --------

local function mkButton(name, text, ax, ay, ox, oy, w, h, r, g, b)
    Gui:CreateLabel({
        Name = name,
        Text = text,
        ClassName = "TextButton",
        AnchorX = ax, AnchorY = ay,
        OffsetX = ox, OffsetY = oy,
        SizeW = w, SizeH = h,
        BackgroundColor = {r, g, b},
        BackgroundTransparency = 0.15,
        TextColor = {1, 1, 1},
        FontScale = 1.4,
        Visible = true,
    })
end

-- Base del joystick (solo decorativa, non cliccabile)
Gui:CreateLabel({
    Name = "PadBase",
    Text = "",
    ClassName = "TextLabel",
    AnchorX = PAD_X, AnchorY = PAD_Y,
    SizeW = BTN * 3 + 16, SizeH = BTN * 3 + 16,
    BackgroundColor = {0.06, 0.09, 0.16},
    BackgroundTransparency = 0.55,
    TextColor = {1, 1, 1},
    Visible = true,
})

mkButton("PadUp",    "^", PAD_X, PAD_Y,      0, -BTN - 4, BTN, BTN, 0.18, 0.42, 0.78)
mkButton("PadDown",  "v", PAD_X, PAD_Y,      0,  BTN + 4, BTN, BTN, 0.18, 0.42, 0.78)
mkButton("PadLeft",  "<", PAD_X, PAD_Y, -BTN - 4,       0, BTN, BTN, 0.18, 0.42, 0.78)
mkButton("PadRight", ">", PAD_X, PAD_Y,  BTN + 4,       0, BTN, BTN, 0.18, 0.42, 0.78)
mkButton("PadStop",  "O", PAD_X, PAD_Y,      0,       0, BTN, BTN, 0.45, 0.20, 0.20)

mkButton("JumpBtn", "JUMP", JUMP_X, JUMP_Y, 0, 0, 108, 108, 0.15, 0.60, 0.35)

Gui:CreateLabel({
    Name = "PadState",
    Text = "fermo",
    ClassName = "TextLabel",
    AnchorX = PAD_X, AnchorY = PAD_Y,
    OffsetY = -(BTN * 1.5 + 26),
    FontScale = 1.0,
    BackgroundTransparency = 1,
    TextColor = {0.8, 0.9, 1},
    Visible = true,
})

--------------------------------------------------------------- comandi -------

local function setDir(f, s, label)
    -- Ritoccare la stessa direzione ferma il player: con eventi "al click"
    -- e' il modo piu' comodo per non restare bloccati a camminare.
    if moveFwd == f and moveSide == s then
        moveFwd, moveSide = 0, 0
        Gui:SetLabelText("PadState", "fermo")
    else
        moveFwd, moveSide = f, s
        Gui:SetLabelText("PadState", label)
    end
end

Gui:ConnectButton("PadUp",    function() setDir( 1,  0, "avanti")   end)
Gui:ConnectButton("PadDown",  function() setDir(-1,  0, "indietro") end)
Gui:ConnectButton("PadLeft",  function() setDir( 0, -1, "sinistra") end)
Gui:ConnectButton("PadRight", function() setDir( 0,  1, "destra")   end)

Gui:ConnectButton("PadStop", function()
    moveFwd, moveSide = 0, 0
    Gui:SetLabelText("PadState", "fermo")
end)

Gui:ConnectButton("JumpBtn", function()
    if jumping then return end
    local p = Humanoid.GetPosition()
    jumping, jumpT, jumpBaseY = true, 0, p.Y
end)

Humanoid.SetWalkSpeed(SPEED)
print("[Comandi] Frecce = cammina (ritocca per fermarti) · JUMP = salto")

------------------------------------------------------------------ loop -------

RS.Heartbeat:Connect(function(dt)
    if type(dt) ~= "number" or dt ~= dt or dt <= 0 then dt = 1 / 60 end
    if dt > 0.1 then dt = 0.1 end

    local p = Humanoid.GetPosition()
    local x, y, z = p.X, p.Y, p.Z

    if moveFwd ~= 0 or moveSide ~= 0 then
        local look = Humanoid.GetLook()
        -- Vettore "destra" perpendicolare all'avanti sul piano orizzontale.
        -- Se lo strafe ti sembra invertito, scambia i segni qui sotto.
        local rx, rz = look.Z, -look.X

        local dx = look.X * moveFwd + rx * moveSide
        local dz = look.Z * moveFwd + rz * moveSide
        local len = math.sqrt(dx * dx + dz * dz)
        if len > 0.0001 then
            x = x + (dx / len) * SPEED * dt
            z = z + (dz / len) * SPEED * dt
        end
    end

    if jumping then
        jumpT = jumpT + dt
        local t = jumpT / JUMP_TIME
        if t >= 1 then
            jumping = false
            y = jumpBaseY
        else
            -- parabola: 0 a terra, 1 al culmine, 0 di nuovo a terra
            y = jumpBaseY + JUMP_H * (4 * t * (1 - t))
        end
    end

    if moveFwd ~= 0 or moveSide ~= 0 or jumping then
        Humanoid.SetPosition(x, y, z)
    end
end)
