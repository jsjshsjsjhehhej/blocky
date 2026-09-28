-- ============================================================
--  FUCILE A POMPA — idle / mira / sparo
--
--  COME SI CREA L'ITEM (in Studio, prima di usare questo script):
--    Explorer -> Backpack -> aggiungi item, chiamalo "Fucile"
--    Properties -> Tool: Pistol   (usa la posa a due mani in mano)
--    Properties -> "Scegli modello (mesh in mano)..." -> il tuo .glb/.fbx
--  Il motore lo mette in mano da solo: questo script decide cosa FA.
--
--  NOTA ONESTA sulle animazioni: le tre "pose" qui sotto sono
--  idle / mira / rinculo ottenute con quello che il motore espone
--  davvero a Lua (clip corpo intero, velocita' di movimento, scossa
--  camera, particelle). L'animazione del braccio/arma e' disegnata
--  dal C++ per tipo di Tool e oggi non e' pilotabile da script:
--  per un rinculo dell'arma vero servirebbe un BackpackTool::Shotgun
--  dedicato nel motore, come esiste gia' per spada e torcia.
-- ============================================================

local ITEM_NAME = "Fucile"

local Gui    = game:GetService("StarterGui")
local Suoni  = game:GetService("SoundService")
local Fx     = game:GetService("Particles")

-- ---- Parametri dell'arma ----------------------------------
local PALLINI          = 8      -- pallini per colpo (e' un fucile a pompa)
local SPREAD_FIANCO    = 0.17   -- dispersione sparando dal fianco
local SPREAD_MIRA      = 0.05   -- dispersione in mira
local GITTATA          = 90
local DANNO_PALLINO    = 12
local CARICATORE       = 6
local TEMPO_RICARICA   = 1.8
local CADENZA          = 0.75   -- secondi tra un colpo e l'altro
local VEL_NORMALE      = 16
local VEL_MIRA         = 7      -- in mira si cammina piano

-- ---- Stato -------------------------------------------------
local equipaggiato = false
local inMira       = false
local occupato     = false
local colpi        = CARICATORE
local danniParte   = {}   -- quanto danno ha preso ogni parte colpita

-- ---- Interfaccia -------------------------------------------
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

local function aggiornaHud()
    Gui:SetLabelText("FucileMunizioni", colpi .. " / " .. CARICATORE)
end

-- ---- Azioni ------------------------------------------------
local function ricarica()
    if occupato or colpi == CARICATORE then return end
    occupato = true
    Gui:SetLabelText("FucileMunizioni", "RICARICO...")
    task.wait(TEMPO_RICARICA)
    colpi = CARICATORE
    aggiornaHud()
    occupato = false
end

local function colpisci(parte, punto)
    Fx:Sparks(punto, 6)
    local nome = parte.Name or "?"
    danniParte[nome] = (danniParte[nome] or 0) + DANNO_PALLINO
    -- oltre soglia la parte salta, se il motore le permette di essere distrutta
    if danniParte[nome] >= 100 then
        danniParte[nome] = 0
        parte:Explode()
    end
end

local function spara()
    if occupato then return end
    if colpi <= 0 then
        ricarica()
        return
    end

    occupato = true
    colpi = colpi - 1
    aggiornaHud()

    local origine = Humanoid.GetPosition() + Vector3.new(0, 2.2, 0)
    local avanti  = Humanoid.GetLook()
    local spread  = inMira and SPREAD_MIRA or SPREAD_FIANCO

    -- vampa alla bocca dell'arma
    Fx:Sparks(origine + avanti * 2.0, 5)
    Suoni:Play("assets/sounds/beep.wav", false)

    for _ = 1, PALLINI do
        local dir = Vector3.new(
            avanti.x + (math.random() - 0.5) * spread * 2,
            avanti.y + (math.random() - 0.5) * spread,
            avanti.z + (math.random() - 0.5) * spread * 2
        )
        local hit = workspace:Raycast(origine, dir, GITTATA)
        if hit and hit.Instance then
            colpisci(hit.Instance, hit.Position)
        end
    end

    -- rinculo: piu' secco dal fianco, controllato in mira
    Camera.Shake(inMira and 3.5 or 7.0, 0.22)

    task.wait(CADENZA)
    occupato = false
    if colpi <= 0 then ricarica() end
end

local function impostaMira(attiva)
    inMira = attiva
    if inMira then
        Humanoid.SetWalkSpeed(VEL_MIRA)
        Gui:SetLabelText("FucileMira", "IN MIRA")
        Gui:SetLabelBg("FucileMira", 0.15, 0.55, 0.30, 0.1)
    else
        Humanoid.SetWalkSpeed(VEL_NORMALE)
        Gui:SetLabelText("FucileMira", "MIRA")
        Gui:SetLabelBg("FucileMira", 0.20, 0.30, 0.45, 0.1)
    end
end

Gui:ConnectButton("FucileMira", function()
    if equipaggiato then impostaMira(not inMira) end
end)

-- ---- Collegamento all'item ---------------------------------
Events.Connect("ItemEquipped", function(_, itemName)
    if itemName ~= ITEM_NAME then return end
    equipaggiato = true
    Gui:SetLabelVisible("FucileMira", true)
    Gui:SetLabelVisible("FucileMunizioni", true)
    impostaMira(false)
    aggiornaHud()
    print("Fucile pronto —", colpi, "colpi")
end)

Events.Connect("ItemUnequipped", function(_, itemName)
    if itemName ~= ITEM_NAME then return end
    equipaggiato = false
    inMira = false
    Humanoid.SetWalkSpeed(VEL_NORMALE)
    Gui:SetLabelVisible("FucileMira", false)
    Gui:SetLabelVisible("FucileMunizioni", false)
end)

Events.Connect("ItemActivated", function(_, itemName)
    if itemName ~= ITEM_NAME then return end
    spara()
end)
