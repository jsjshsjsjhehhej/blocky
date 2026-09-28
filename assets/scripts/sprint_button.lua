-- Bottone Sprint — clicca per correre più veloce, clicca di nuovo per tornare normale
local Gui = game:GetService("StarterGui")

local WALK_SPEED = 16
local SPRINT_SPEED = 26 -- max motore 28

local sprinting = false

Gui:CreateLabel({
    Name = "SprintButton",
    Text = "SPRINT",
    ClassName = "TextButton",
    AnchorX = 0.92,
    AnchorY = 0.92,
    SizeW = 110,
    SizeH = 40,
    BackgroundColor = {0.2, 0.6, 0.9},
    BackgroundTransparency = 0.1,
    TextColor = {1, 1, 1},
    Visible = true,
})

Humanoid.SetWalkSpeed(WALK_SPEED)

Gui:ConnectButton("SprintButton", function()
    sprinting = not sprinting
    if sprinting then
        Humanoid.SetWalkSpeed(SPRINT_SPEED)
        Gui:SetLabelText("SprintButton", "SPRINT!")
        Gui:SetLabelBg("SprintButton", 0.9, 0.35, 0.2, 0.1)
    else
        Humanoid.SetWalkSpeed(WALK_SPEED)
        Gui:SetLabelText("SprintButton", "SPRINT")
        Gui:SetLabelBg("SprintButton", 0.2, 0.6, 0.9, 0.1)
    end
end)
