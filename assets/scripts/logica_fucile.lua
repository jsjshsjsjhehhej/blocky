--[[ logica_fucile.lua — w_shotgun_spas: sparo verso il click, solo particelle ]]

local ITEM_NAME = "w_shotgun_spas"
local Gui = game:GetService("StarterGui")
local Suoni = game:GetService("SoundService")
local Fx = game:GetService("Particles")

local CARICATORE = 6
local RICARICA = 1.6
local CADENZA = 0.70
local GITTATA = 220

local equipaggiato = false
local inMira = false
local occupato = false
local colpi = CARICATORE

Gui:CreateLabel({
    Name = "FucileMira", Text = "MIRA", ClassName = "TextButton",
    AnchorX = 0.92, AnchorY = 0.56, SizeW = 110, SizeH = 40,
    BackgroundColor = {0.20, 0.30, 0.45}, BackgroundTransparency = 0.1,
    TextColor = {1, 1, 1}, Visible = false,
})
Gui:CreateLabel({
    Name = "FucileMunizioni", Text = "", ClassName = "TextLabel",
    AnchorX = 0.92, AnchorY = 0.50, SizeW = 110, SizeH = 28,
    BackgroundColor = {0.08, 0.10, 0.16}, BackgroundTransparency = 0.25,
    TextColor = {1, 0.85, 0.4}, Visible = false,
})

local function vec(v, fallback)
    if type(v) == "table" then
        return Vector3.new(v.x or v.X or 0, v.y or v.Y or 0, v.z or v.Z or 0)
    end
    return fallback or Vector3.new(0, 0, 0)
end

local function norma(v)
    local L = math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
    if L < 0.0001 then
        return Vector3.new(0, 0, -1)
    end
    return Vector3.new(v.x / L, v.y / L, v.z / L)
end

local function hud()
    Gui:SetLabelText("FucileMunizioni", colpi .. " / " .. CARICATORE)
end

local function ricarica()
    if occupato or colpi == CARICATORE then return end
    occupato = true
    Gui:SetLabelText("FucileMunizioni", "RICARICO...")
    task.wait(RICARICA)
    colpi = CARICATORE
    hud()
    occupato = false
end

local function miraClick()
    local origine, direzione
    local ok = pcall(function()
        origine, direzione = Camera.ScreenRay()
    end)
    if not ok or type(origine) ~= "table" then
        local piedi = vec(Humanoid.GetPosition())
        origine = Vector3.new(piedi.x, piedi.y + 2.2, piedi.z)
        direzione = vec(Humanoid.GetLook(), Vector3.new(0, 0, -1))
    end
    return vec(origine), norma(vec(direzione, Vector3.new(0, 0, -1)))
end

local function spara()
    if occupato then return end
    if colpi <= 0 then
        task.spawn(ricarica)
        return
    end
    occupato = true
    colpi = colpi - 1
    hud()

    local camOrigine, mira = miraClick()
    local piedi = vec(Humanoid.GetPosition())
    local petto = Vector3.new(piedi.x, piedi.y + 2.2, piedi.z)

    local colpo = nil
    pcall(function()
        colpo = workspace:Raycast(camOrigine, mira, GITTATA)
    end)

    local fine
    if type(colpo) == "table" and colpo.Position then
        fine = vec(colpo.Position)
        mira = norma(Vector3.new(fine.x - petto.x, fine.y - petto.y, fine.z - petto.z))
    else
        fine = Vector3.new(
            camOrigine.x + mira.x * 48,
            camOrigine.y + mira.y * 48,
            camOrigine.z + mira.z * 48
        )
    end

    local bocca = Vector3.new(
        petto.x + mira.x * 2.2,
        petto.y + mira.y * 2.2,
        petto.z + mira.z * 2.2
    )

    Fx:Sparks(bocca, 8, mira)
    Fx:Smoke(bocca, 3)
    Fx:Sparks(fine, 6, mira)
    for i = 1, 3 do
        local t = i / 4
        Fx:Sparks(Vector3.new(
            bocca.x + (fine.x - bocca.x) * t,
            bocca.y + (fine.y - bocca.y) * t,
            bocca.z + (fine.z - bocca.z) * t
        ), 3, mira)
    end

    pcall(function()
        Suoni:Play("sounds/shoot.wav", false)
    end)
    Camera.Shake(inMira and 2.5 or 5.0, 0.18)

    task.wait(CADENZA)
    occupato = false
    if colpi <= 0 then ricarica() end
end

local function impostaMira(on)
    inMira = on
    if inMira then
        Humanoid.SetWalkSpeed(7)
        Gui:SetLabelText("FucileMira", "IN MIRA")
        Gui:SetLabelBg("FucileMira", 0.15, 0.55, 0.30, 0.1)
    else
        Humanoid.SetWalkSpeed(16)
        Gui:SetLabelText("FucileMira", "MIRA")
        Gui:SetLabelBg("FucileMira", 0.20, 0.30, 0.45, 0.1)
    end
end

Gui:ConnectButton("FucileMira", function()
    if equipaggiato then impostaMira(not inMira) end
end)

Events.Connect("ItemEquipped", function(_, name)
    if name ~= ITEM_NAME then return end
    equipaggiato = true
    Gui:SetLabelVisible("FucileMira", true)
    Gui:SetLabelVisible("FucileMunizioni", true)
    impostaMira(false)
    hud()
end)

Events.Connect("ItemUnequipped", function(_, name)
    if name ~= ITEM_NAME then return end
    equipaggiato = false
    inMira = false
    Humanoid.SetWalkSpeed(16)
    Gui:SetLabelVisible("FucileMira", false)
    Gui:SetLabelVisible("FucileMunizioni", false)
end)

Events.Connect("ItemActivated", function(_, name)
    if name ~= ITEM_NAME then return end
    task.spawn(spara)
end)
