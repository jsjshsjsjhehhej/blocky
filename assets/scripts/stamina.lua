--[[============================================================================
  Blocky — Stamina Sprint + barra UI
  ==============================================================================
  Uso:
    1) Pannello Script → "Carica stamina.lua"
    2) Test (F5)
    3) WASD + CTRL (o RT sul pad) per correre.

  Correndo: WalkSpeed alto e la barra stamina si svuota.
  Stamina a 0: torni a camminare finché non si ricarica un po'.
============================================================================]]

local UIS = game:GetService("UserInputService")
local RS  = game:GetService("RunService")
local Gui = game:GetService("StarterGui")

local MAX_STAMINA   = 100
local WALK_SPEED    = 16
local SPRINT_SPEED  = 26   -- max engine 28
local DRAIN_PER_SEC = 22
local REGEN_PER_SEC = 12
local MIN_TO_SPRINT = 8

local BAR_W = 220
local BAR_H = 16
local FILL_H = 12

local stamina = MAX_STAMINA
local exhausted = false

Gui:CreateLabel({
    Name = "StaminaBg",
    Text = "",
    ClassName = "TextLabel",
    AnchorX = 0.5,
    AnchorY = 0.92,
    SizeW = BAR_W,
    SizeH = BAR_H,
    BackgroundColor = {0.05, 0.05, 0.08},
    BackgroundTransparency = 0.25,
    TextColor = {1, 1, 1},
    Visible = true,
})

Gui:CreateLabel({
    Name = "StaminaFill",
    Text = "",
    ClassName = "TextLabel",
    AnchorX = 0.5,
    AnchorY = 0.92,
    SizeW = BAR_W - 4,
    SizeH = FILL_H,
    OffsetY = 0,
    BackgroundColor = {0.25, 0.85, 0.35},
    BackgroundTransparency = 0.0,
    TextColor = {1, 1, 1},
    Visible = true,
})

Gui:CreateLabel({
    Name = "StaminaText",
    Text = "STAMINA",
    ClassName = "TextLabel",
    AnchorX = 0.5,
    AnchorY = 0.92,
    OffsetY = -22,
    FontScale = 0.9,
    BackgroundTransparency = 1,
    TextColor = {1, 1, 1},
    Visible = true,
})

local function updateBar()
    local pct = math.clamp(stamina / MAX_STAMINA, 0, 1)
    local fillW = math.max(2, (BAR_W - 4) * pct)
    local ox = -((BAR_W - 4) * 0.5) + fillW * 0.5
    Gui:SetLabelSize("StaminaFill", fillW, FILL_H)
    Gui:SetLabelOffset("StaminaFill", ox, 0)

    local r, g, b
    if pct > 0.5 then
        local t = (pct - 0.5) * 2
        r = 0.85 - 0.60 * t
        g = 0.85
        b = 0.25 + 0.10 * t
    else
        local t = pct * 2
        r = 0.85
        g = 0.25 + 0.60 * t
        b = 0.20
    end
    Gui:SetLabelBg("StaminaFill", r, g, b, 0)

    if exhausted then
        Gui:SetLabelText("StaminaText", "STAMINA (riposa)")
        Gui:SetLabelColor("StaminaText", 1, 0.45, 0.35)
    else
        Gui:SetLabelText("StaminaText", string.format("STAMINA  %d%%", math.floor(pct * 100 + 0.5)))
        Gui:SetLabelColor("StaminaText", 1, 1, 1)
    end
end

updateBar()
Humanoid.SetWalkSpeed(WALK_SPEED)
print("[Stamina] Pronto — CTRL (o RT) + WASD per correre")

RS.Heartbeat:Connect(function(dt)
    if type(dt) ~= "number" or dt ~= dt or dt <= 0 then
        dt = 1 / 60
    end
    if dt > 0.1 then dt = 0.1 end

    local moving = UIS:IsMoving()
    local wantSprint = UIS:IsSprintHeld() and moving

    if exhausted then
        wantSprint = false
        if stamina >= MIN_TO_SPRINT + 12 then
            exhausted = false
        end
    end

    if wantSprint and stamina > 0 then
        Humanoid.SetWalkSpeed(SPRINT_SPEED)
        stamina = math.max(0, stamina - DRAIN_PER_SEC * dt)
        if stamina <= 0 then
            stamina = 0
            exhausted = true
            Humanoid.SetWalkSpeed(WALK_SPEED)
        end
    else
        Humanoid.SetWalkSpeed(WALK_SPEED)
        local regen = REGEN_PER_SEC
        if not moving then
            regen = regen * 1.35
        end
        stamina = math.min(MAX_STAMINA, stamina + regen * dt)
    end

    updateBar()
end)
