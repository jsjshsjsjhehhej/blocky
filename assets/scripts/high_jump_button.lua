-- Bottone Salto Alto — clicca per saltare più in alto (potenza di salto vera,
-- non un arco finto: usa la fisica del motore, quindi collisioni ok), clicca
-- di nuovo per tornare normale.
local Gui = game:GetService("StarterGui")

local NORMAL_POWER = 50  -- salto di default del motore
local HIGH_POWER    = 70  -- massimo consentito (kMaxJumpPower)

local highJump = false

Gui:CreateLabel({
    Name = "JumpButton",
    Text = "SALTO",
    ClassName = "TextButton",
    AnchorX = 0.92,
    AnchorY = 0.68,
    SizeW = 110,
    SizeH = 40,
    BackgroundColor = {0.55, 0.25, 0.85},
    BackgroundTransparency = 0.1,
    TextColor = {1, 1, 1},
    Visible = true,
})

Humanoid.SetJumpPower(NORMAL_POWER)

Gui:ConnectButton("JumpButton", function()
    highJump = not highJump
    if highJump then
        Humanoid.SetJumpPower(HIGH_POWER)
        Gui:SetLabelText("JumpButton", "SALTO ALTO!")
        Gui:SetLabelBg("JumpButton", 0.85, 0.2, 0.55, 0.1)
    else
        Humanoid.SetJumpPower(NORMAL_POWER)
        Gui:SetLabelText("JumpButton", "SALTO")
        Gui:SetLabelBg("JumpButton", 0.55, 0.25, 0.85, 0.1)
    end
end)
