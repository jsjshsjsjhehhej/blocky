--[[============================================================================
  BlockyPlace Shooter — stile Phantom Forces (SOLO Luau, niente mesh)
  ==============================================================================
  Arena baseplate + cover. Viewmodel = braccio pesca + fucile a Part, aggiornato
  ogni Heartbeat su Camera.ScreenRay(). Nessuna API C++ aggiunta.

  Controlli:
    Slot 1 (AssaultGun)     = equipaggia
    Click / Activated       = spara
    Bottone MIRA            = ADS
    Bottone RICARICA        = ricarica
    Menu → Prima persona ON = look PF (già nel client)

  I colpi partono oltre il fucile (origin + look * 3.2) cosi' il raycast
  non colpisce il viewmodel.
============================================================================]]

local RS    = game:GetService("RunService")
local Gui   = game:GetService("StarterGui")
local Fx    = game:GetService("Particles")
local Light = game:GetService("Lighting")
local Suoni = game:GetService("SoundService")

local ITEM = "AssaultGun"

local function V(x, y, z) return Vector3.new(x, y, z) end
local function C(r, g, b) return Color3.new(r, g, b) end

local function nrm(v)
    local m = v.Magnitude
    if not m or m < 1e-4 then return V(0, 0, -1) end
    return V(v.x / m, v.y / m, v.z / m)
end

local function add(a, b) return V(a.x + b.x, a.y + b.y, a.z + b.z) end
local function mul(a, s) return V(a.x * s, a.y * s, a.z * s) end
local function cross(a, b)
    return V(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x)
end

local function lookBasis(fwd)
    local f = nrm(fwd)
    local r = nrm(cross(f, V(0, 1, 0)))
    if r.Magnitude < 0.2 then r = nrm(cross(f, V(1, 0, 0))) end
    local u = cross(r, f)
    return f, r, u
end

local function makePart(name, pos, size, color, opts)
    opts = opts or {}
    local p = Instance.new("Part")
    p.Name = name
    p.Position = pos
    p.Size = size
    p.BrickColor = color
    p.Anchored = true
    p.CanCollide = opts.collide == true
    if opts.smooth then p.Material = "SmoothPlastic" end
    if opts.material then p.Material = opts.material end
    return workspace:Add(p)
end

------------------------------------------------------------------ arena ------

print("[PF] BlockyPlace Shooter — solo Part / Luau")

pcall(function()
    Light:SetClockTime(14)
    Light:SetBrightness(1.15)
    -- Ambient e' uno scalare in Blocky (non Color3)
    Light:SetAmbient(0.55)
    Light:SetOutdoorAmbient(C(0.7, 0.78, 0.9))
    Light:SetFogStart(180)
    Light:SetFogEnd(420)
    Light:SetFogColor(C(0.55, 0.72, 0.95))
end)

makePart("Baseplate", V(0, -1, 0), V(120, 2, 120), C(0.55, 0.55, 0.58), {
    collide = true, material = "Plastic",
})

local wallCol = C(0.45, 0.22, 0.22)
makePart("WallN", V(0, 4, -58), V(120, 8, 2), wallCol, { collide = true })
makePart("WallS", V(0, 4, 58), V(120, 8, 2), wallCol, { collide = true })
makePart("WallE", V(58, 4, 0), V(2, 8, 120), wallCol, { collide = true })
makePart("WallW", V(-58, 4, 0), V(2, 8, 120), wallCol, { collide = true })

local cover = C(0.42, 0.44, 0.48)
local crates = {
    { V(-18, 2, -8), V(6, 4, 3) },
    { V(14, 2, 10), V(5, 4, 4) },
    { V(-6, 1.5, 18), V(8, 3, 2) },
    { V(22, 2.5, -16), V(3, 5, 6) },
    { V(-28, 2, 12), V(4, 4, 4) },
    { V(8, 1.2, -22), V(10, 2.4, 2.5) },
    { V(0, 3, 0), V(2, 6, 2) },
}
for i, c in ipairs(crates) do
    makePart("Cover" .. i, c[1], c[2], cover, { collide = true, smooth = true })
end

makePart("SpawnA", V(0, 0.6, 28), V(8, 1.2, 8), C(0.2, 0.45, 0.9), { collide = true })
makePart("SpawnB", V(0, 0.6, -28), V(8, 1.2, 8), C(0.9, 0.35, 0.2), { collide = true })
pcall(function() Humanoid.SetPosition(V(0, 3, 26)) end)

------------------------------------------------------------------ viewmodel ------

local vm = {}
local SKIN  = C(0.95, 0.78, 0.62)
local GUN   = C(0.22, 0.24, 0.22)
local OLIVE = C(0.35, 0.40, 0.28)
local METAL = C(0.55, 0.55, 0.52)

local function vmPart(suffix, size, color)
    vm[suffix] = makePart("__vm_" .. suffix, V(0, -80, 0), size, color, {
        collide = false, smooth = true,
    })
end

vmPart("Arm",    V(0.9, 0.9, 2.4), SKIN)
vmPart("Hand",   V(1.0, 0.85, 0.9), SKIN)
vmPart("Body",   V(0.7, 0.85, 3.2), GUN)
vmPart("Stock",  V(0.55, 0.7, 1.4), OLIVE)
vmPart("Mag",    V(0.45, 1.1, 0.7), METAL)
vmPart("Barrel", V(0.28, 0.28, 2.0), METAL)
vmPart("Sight",  V(0.2, 0.45, 0.55), METAL)

local recoil = 0
local sway = 0

local function updateViewmodel(dt)
    local show = (Backpack.Equipped() == ITEM)
    if not show then
        for _, p in pairs(vm) do
            if p then p.Position = V(0, -120, 0) end
        end
        return
    end

    local origin, dir
    local ok = pcall(function()
        origin, dir = Camera.ScreenRay()
    end)
    if not ok or type(origin) ~= "table" then
        local feet = Humanoid.GetPosition()
        origin = V(feet.x, feet.y + 2.4, feet.z)
        dir = Humanoid.GetLook()
    end
    origin = V(origin.x or origin.X, origin.y or origin.Y, origin.z or origin.Z)
    dir = nrm(V(dir.x or dir.X, dir.y or dir.Y, dir.z or dir.Z))

    local f, r, u = lookBasis(dir)
    sway = sway + dt * 6
    local bob = math.sin(sway) * 0.04
    local rk = recoil

    local function put(name, ox, oy, oz)
        local p = vm[name]
        if not p then return end
        p.Position = add(origin,
            add(mul(f, oz - rk * 0.35),
            add(mul(r, ox), mul(u, oy + bob))))
    end

    put("Arm",    0.85, -0.55, 1.6)
    put("Hand",   0.95, -0.50, 2.5)
    put("Stock",  0.70, -0.35, 2.0)
    put("Body",   0.75, -0.32, 3.2)
    put("Mag",    0.75, -0.85, 3.0)
    put("Barrel", 0.75, -0.28, 4.6)
    put("Sight",  0.75,  0.15, 3.5)

    if recoil > 0 then recoil = math.max(0, recoil - dt * 14) end
end

------------------------------------------------------------------ arma ------

local MAG, RESERVE = 30, 90
local CADENZA, GITTATA, DANNO = 0.11, 220, 18
local SPREAD_HIP, SPREAD_ADS = 0.085, 0.028
local VEL_NORM, VEL_ADS = 16, 8

local equipaggiato, ads, busy = false, false, false
local ammo, reserve, health = MAG, RESERVE, 100

Gui:CreateLabel({
    Name = "PFTitle", Text = "Phantom Forces Update 1", ClassName = "TextLabel",
    AnchorX = 0.02, AnchorY = 0.02, SizeW = 280, SizeH = 28,
    BackgroundColor = C(0, 0, 0), BackgroundTransparency = 0.55, TextColor = C(1, 1, 1),
})
Gui:CreateLabel({
    Name = "PFHealth", Text = "Health  100", ClassName = "TextLabel",
    AnchorX = 0.35, AnchorY = 0.90, SizeW = 220, SizeH = 28,
    BackgroundColor = C(0.15, 0.55, 0.22), BackgroundTransparency = 0.15, TextColor = C(1, 1, 1),
})
Gui:CreateLabel({
    Name = "PFAmmo", Text = "30 | 90", ClassName = "TextLabel",
    AnchorX = 0.88, AnchorY = 0.88, SizeW = 120, SizeH = 32,
    BackgroundColor = C(0.08, 0.1, 0.14), BackgroundTransparency = 0.2,
    TextColor = C(1, 0.9, 0.45), Visible = false,
})
Gui:CreateLabel({
    Name = "PFMira", Text = "MIRA", ClassName = "TextButton",
    AnchorX = 0.88, AnchorY = 0.78, SizeW = 110, SizeH = 36,
    BackgroundColor = C(0.2, 0.3, 0.45), BackgroundTransparency = 0.1,
    TextColor = C(1, 1, 1), Visible = false,
})
Gui:CreateLabel({
    Name = "PFReload", Text = "RICARICA", ClassName = "TextButton",
    AnchorX = 0.88, AnchorY = 0.68, SizeW = 110, SizeH = 36,
    BackgroundColor = C(0.35, 0.25, 0.2), BackgroundTransparency = 0.1,
    TextColor = C(1, 1, 1), Visible = false,
})
Gui:CreateLabel({
    Name = "PFHint", Text = "Menu → Prima persona: ON  ·  slot 1 = arma",
    ClassName = "TextLabel",
    AnchorX = 0.02, AnchorY = 0.94, SizeW = 360, SizeH = 24,
    BackgroundColor = C(0, 0, 0), BackgroundTransparency = 0.5, TextColor = C(0.85, 0.9, 1),
})

local function hudAmmo()
    Gui:SetLabelText("PFAmmo", ammo .. " | " .. reserve)
end

local function hudHp()
    Gui:SetLabelText("PFHealth", "Health  " .. math.floor(health))
    local t = math.max(0, math.min(1, health / 100))
    Gui:SetLabelBg("PFHealth", 0.15 + (1 - t) * 0.5, 0.15 + t * 0.4, 0.18, 0.12)
end

local function setAds(on)
    ads = on
    if ads then
        Humanoid.SetWalkSpeed(VEL_ADS)
        Gui:SetLabelText("PFMira", "IN MIRA")
        Gui:SetLabelBg("PFMira", 0.15, 0.55, 0.3, 0.1)
    else
        Humanoid.SetWalkSpeed(VEL_NORM)
        Gui:SetLabelText("PFMira", "MIRA")
        Gui:SetLabelBg("PFMira", 0.2, 0.3, 0.45, 0.1)
    end
end

local function ricarica()
    if busy or ammo >= MAG or reserve <= 0 then return end
    busy = true
    Gui:SetLabelText("PFAmmo", "...")
    task.wait(1.55)
    local take = math.min(MAG - ammo, reserve)
    ammo = ammo + take
    reserve = reserve - take
    hudAmmo()
    busy = false
end

local function spara()
    if not equipaggiato or busy then return end
    if ammo <= 0 then
        task.spawn(ricarica)
        return
    end
    busy = true
    ammo = ammo - 1
    hudAmmo()
    recoil = math.min(1.2, recoil + 0.55)

    local origin, dir
    pcall(function() origin, dir = Camera.ScreenRay() end)
    if type(origin) ~= "table" then
        local feet = Humanoid.GetPosition()
        origin = V(feet.x, feet.y + 2.2, feet.z)
        dir = Humanoid.GetLook()
    end
    origin = V(origin.x or origin.X, origin.y or origin.Y, origin.z or origin.Z)
    dir = nrm(V(dir.x or dir.X, dir.y or dir.Y, dir.z or dir.Z))

    local muzzle = add(origin, mul(dir, 3.2))
    local spread = ads and SPREAD_ADS or SPREAD_HIP
    dir = nrm(V(
        dir.x + (math.random() - 0.5) * spread * 2,
        dir.y + (math.random() - 0.5) * spread,
        dir.z + (math.random() - 0.5) * spread * 2
    ))

    pcall(function() Fx:Sparks(muzzle, 5) end)
    pcall(function() Suoni:Play("assets/sounds/shoot.wav", false) end)
    pcall(function() Camera.Shake(ads and 2.2 or 4.5, 0.12) end)

    local hit = workspace:Raycast(muzzle, dir, GITTATA)
    if type(hit) == "table" and hit.Position then
        local hp = V(hit.Position.x or hit.Position.X,
                     hit.Position.y or hit.Position.Y,
                     hit.Position.z or hit.Position.Z)
        pcall(function() Fx:Sparks(hp, 8) end)
        local inst = hit.Instance
        if inst and inst.CanDestroy then
            local h = (inst.Health or 100) - DANNO
            inst.Health = h
            if h <= 0 then pcall(function() inst:Explode() end) end
        end
    end

    task.wait(CADENZA)
    busy = false
    if ammo <= 0 then task.spawn(ricarica) end
end

Gui:ConnectButton("PFMira", function()
    if equipaggiato then setAds(not ads) end
end)
Gui:ConnectButton("PFReload", function()
    if equipaggiato then task.spawn(ricarica) end
end)

Events.Connect("ItemEquipped", function(_, name)
    if name ~= ITEM then return end
    equipaggiato = true
    Gui:SetLabelVisible("PFAmmo", true)
    Gui:SetLabelVisible("PFMira", true)
    Gui:SetLabelVisible("PFReload", true)
    setAds(false)
    hudAmmo()
    print("[PF] AssaultGun pronto —", ammo, "colpi")
end)

Events.Connect("ItemUnequipped", function(_, name)
    if name ~= ITEM then return end
    equipaggiato = false
    ads = false
    Humanoid.SetWalkSpeed(VEL_NORM)
    Gui:SetLabelVisible("PFAmmo", false)
    Gui:SetLabelVisible("PFMira", false)
    Gui:SetLabelVisible("PFReload", false)
end)

Events.Connect("ItemActivated", function(_, name)
    if name == ITEM then spara() end
end)

Backpack.Give(ITEM, "")
hudHp()
hudAmmo()

RS.Heartbeat:Connect(function(dt)
    updateViewmodel(dt or 0.016)
end)

print("[PF] Slot 1 = arma. Menu → Prima persona: ON per il look classico.")
