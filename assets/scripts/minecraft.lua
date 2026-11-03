--[[============================================================================
  Mini Minecraft — place Blocky (solo Luau)
  ==============================================================================
  Mondo voxel (noise + Terrain), scava/piazza, craft, giorno/notte, cave,
  biomi, spawn, hotbar touch. Il lag mobile si risolve nel renderer terrain
  (chunk VBO + distanza corta + shading lite) — questo script resta leggero:
  mondo piu' piccolo, meno alberi, niente SetLabel ogni frame se non serve.
============================================================================]]

local Terrain = workspace.Terrain
local RS      = game:GetService("RunService")
local Gui     = game:GetService("StarterGui")
local Fx      = game:GetService("Particles")
local Input   = game:GetService("UserInputService")
local Light   = game:GetService("Lighting")

------------------------------------------------------------------ mondo ------

local SEED      = 1337
local HALF      = 14          -- 28x28: meno voxel = piu' fps su telefono
local SEA       = 6
local MAX_H     = 18
local ROWS_TICK = 2

local B
do
    local x0 = Terrain:CellCenterToWorld(0, 0, 0)
    local x1 = Terrain:CellCenterToWorld(1, 0, 0)
    B = math.abs(x1 - x0)
    if not B or B < 0.5 then B = 3 end
end

local M = Enum.Material
local HARDNESS = {
    [M.Grass] = 0.55, [M.Ground] = 0.45, [M.Sand] = 0.45, [M.Snow] = 0.35,
    [M.Mud] = 0.45,   [M.LeafyGrass] = 0.2, [M.Wood] = 0.9, [M.WoodPlanks] = 0.8,
    [M.Rock] = 1.4,   [M.Cobblestone] = 1.3, [M.Brick] = 1.3, [M.Glass] = 0.25,
    [M.Pebble] = 0.45, [M.Ice] = 0.4, [M.Water] = nil,
    Coal = 1.6, Iron = 2.4, Gold = 2.4, Diamond = 3.0, Flower = 0.05,
    Cactus = 0.7, Torch = 0.05, Mushroom = 0.05, Clay = 0.5,
    Lava = nil, Bedrock = nil,
}
local DROP = { [M.Grass] = M.Ground, [M.Rock] = M.Cobblestone }
local NO_DROP = { [M.LeafyGrass] = true, Flower = true, Mushroom = true }

-- Slot hotbar (si puo' cambiare con craft / raccolto)
local HOTBAR = {
    M.Ground, M.Cobblestone, M.Wood, M.WoodPlanks, M.Sand,
    M.Glass, M.Brick, "Torch", "Coal",
}
local HOTBAR_LABEL = {
    [M.Ground] = "Terra", [M.Cobblestone] = "Pietra", [M.Wood] = "Tronco",
    [M.WoodPlanks] = "Assi", [M.Sand] = "Sabbia", [M.Glass] = "Vetro",
    [M.Brick] = "Mattoni", [M.LeafyGrass] = "Foglie", Torch = "Torcia",
    [M.Rock] = "Roccia", [M.Grass] = "Erba", [M.Snow] = "Neve",
    Coal = "Carbone", Iron = "Ferro", Gold = "Oro", Diamond = "Diamante",
    Mushroom = "Fungo", Cactus = "Cactus", Clay = "Argilla", Flower = "Fiore",
}

local inventory = {}
local selected  = 1
local heightMap = {}
local worldReady = false
local clockTime = 9
local food = 20
local lastTargetText = ""
local lastHotbarText = ""
local lastFoodText = ""

------------------------------------------------------------ util ------

local function hash(x, z, salt)
    local h = math.sin(x * 127.1 + z * 311.7 + salt * 74.7 + SEED) * 43758.5453
    return h - math.floor(h)
end

local function fbm(x, z)
    local a, f, amp, n = 0, 1, 1, 0
    for i = 0, 3 do
        a = a + math.noise(x * f + SEED * 0.01, i * 0.37, z * f - SEED * 0.013) * amp
        n = n + amp
        f = f * 2
        amp = amp * 0.5
    end
    return a / n
end

local function heightAt(x, z)
    local n = fbm(x * 0.05, z * 0.05)
    local mountain = math.max(0, fbm(x * 0.022 + 50, z * 0.022 - 50)) ^ 2 * 7
    return math.clamp(5 + math.floor((n * 0.5 + 0.5) * (MAX_H - 7) + mountain), 2, MAX_H)
end

local function key(x, z) return x .. "," .. z end
local function labelOf(mat) return HOTBAR_LABEL[mat] or tostring(mat) end

local function setLabel(name, text)
    if name == "McTarget" then
        if text == lastTargetText then return end
        lastTargetText = text
    elseif name == "McHotbar" then
        if text == lastHotbarText then return end
        lastHotbarText = text
    elseif name == "McFood" then
        if text == lastFoodText then return end
        lastFoodText = text
    end
    Gui:SetLabelText(name, text)
end

------------------------------------------------------------ generazione ------

local function regionForColumn(x, z, y0, y1)
    local ax, ay, az = Terrain:CellCenterToWorld(x, y0, z)
    local bx, by, bz = Terrain:CellCenterToWorld(x, y1, z)
    return Region3.new(Vector3.new(ax, ay, az), Vector3.new(bx, by, bz))
end

local function buildColumn(x, z)
    local h = heightAt(x, z)
    heightMap[key(x, z)] = h

    local wet = fbm(x * 0.03 + 200, z * 0.03 + 200)
    local top = M.Grass
    if h <= SEA + 1 or wet < -0.32 then top = M.Sand end
    if h >= MAX_H - 2 then top = M.Snow end
    if top == M.Grass and hash(x, z, 9) < 0.1 then top = M.LeafyGrass end

    local topY = math.max(h, SEA)
    local mats = {}
    for y = 0, topY do
        local id
        if y == 0 then
            id = "Bedrock"
        elseif y < h then
            if y >= h - 3 then
                id = (top == M.Sand) and M.Sand or M.Ground
            else
                id = M.Rock
                local r = hash(x + y * 7, z - y * 3, 3)
                if y < h - 4 and r < 0.05 then id = "Coal" end
                if y < 8 and r > 0.982 then id = "Iron" end
                if y < 5 and r > 0.972 and r <= 0.982 then id = "Gold" end
                if y < 3 and r > 0.995 then id = "Diamond" end
                if hash(x, z + y, 5) < 0.025 then id = M.Pebble end
                if hash(x, y, z) < 0.02 and y < h - 2 then id = "Clay" end
            end
        elseif y == h then
            id = top
        else
            id = M.Water
        end
        -- Cave: tunnel di rumore 3D sotto la superficie
        if y > 1 and y < h - 1 and id ~= M.Water and id ~= "Bedrock" then
            local cave = math.noise(x * 0.09 + 3, y * 0.12, z * 0.09 - 2)
            if cave > 0.52 then id = M.Air end
        end
        mats[#mats + 1] = id
    end
    Terrain:WriteVoxels(regionForColumn(x, z, 0, topY), 4, mats)
    return h, top, wet
end

local function plantTree(x, z, h)
    local trunk = 3 + math.floor(hash(x, z, 11) * 2)
    local cx, _, cz = Terrain:CellCenterToWorld(x, 0, z)
    local baseY = select(2, Terrain:CellCenterToWorld(x, h + 1, z))
    Terrain:FillBlock(
        CFrame.new(cx, baseY + (trunk - 1) * B * 0.5, cz),
        Vector3.new(B, B * trunk, B), M.Wood)
    local _, topY = Terrain:CellCenterToWorld(x, h + trunk, z)
    Terrain:FillBall(Vector3.new(cx, topY + B * 0.6, cz), B * 1.9, M.LeafyGrass)
    Terrain:FillBlock(CFrame.new(cx, topY, cz), Vector3.new(B, B, B), M.Wood)
end

local function decorate(x, z, h, top, wet)
    if h <= SEA then return end
    local r = hash(x, z, 21)
    if top == M.Grass and r < 0.018 then
        plantTree(x, z, h)
    elseif top == M.Grass and r < 0.05 then
        Terrain:FillBlock(CFrame.new(Terrain:CellCenterToWorld(x, h + 1, z)),
                          Vector3.new(B, B, B), "Flower")
    elseif top == M.Grass and r < 0.065 then
        Terrain:FillBlock(CFrame.new(Terrain:CellCenterToWorld(x, h + 1, z)),
                          Vector3.new(B, B, B), "Mushroom")
    elseif top == M.Sand and wet < -0.4 and r < 0.04 then
        local ch = 1 + math.floor(hash(x, z, 8) * 2)
        for i = 1, ch do
            Terrain:FillBlock(CFrame.new(Terrain:CellCenterToWorld(x, h + i, z)),
                              Vector3.new(B, B, B), "Cactus")
        end
    elseif top ~= M.Sand and h > SEA + 2 and r > 0.992 then
        Terrain:FillBlock(CFrame.new(Terrain:CellCenterToWorld(x, h, z)),
                          Vector3.new(B, B, B), "Lava")
    end
end

local function surfaceAt(x, z)
    return heightMap[key(x, z)] or heightAt(x, z)
end

local function spawnPlayer()
    local x, z, h = 0, 0, surfaceAt(0, 0)
    for r = 0, 10 do
        local found = false
        for dx = -r, r do
            for dz = -r, r do
                local hh = surfaceAt(dx, dz)
                if hh > SEA + 1 then x, z, h, found = dx, dz, hh, true break end
            end
            if found then break end
        end
        if found then break end
    end
    local wx, wy, wz = Terrain:CellCenterToWorld(x, h + 1, z)
    Humanoid.SetPosition(wx, wy + B + 2, wz)
    food = math.max(food, 12)
end

local function generateWorld()
    Terrain:Clear()
    heightMap = {}
    local total = (HALF * 2) * (HALF * 2)
    local done, pending = 0, {}

    for z = -HALF, HALF - 1 do
        for x = -HALF, HALF - 1 do
            local h, top, wet = buildColumn(x, z)
            pending[#pending + 1] = { x, z, h, top, wet }
            done = done + 1
        end
        if (z + HALF) % ROWS_TICK == ROWS_TICK - 1 then
            setLabel("McStatus", string.format("Genero il mondo... %d%%",
                math.floor(done / total * 100)))
            task.wait()
        end
    end

    for i, c in ipairs(pending) do
        decorate(c[1], c[2], c[3], c[4], c[5])
        if i % 250 == 0 then task.wait() end
    end

    -- Kit di partenza
    inventory[M.Ground] = 8
    inventory[M.Wood] = 4
    inventory[M.Cobblestone] = 4
    inventory["Torch"] = 4
    inventory["Coal"] = 2

    worldReady = true
    spawnPlayer()
    Gui:SetLabelVisible("McStatus", false)
    print(string.format("[Minecraft] mondo %dx%d seed %d", HALF * 2, HALF * 2, SEED))
end

---------------------------------------------------------------------- UI -----

Gui:SetLabelText("Welcome", "MINI MINECRAFT — scava · piazza · craft · giorno/notte")

Gui:CreateLabel({
    Name = "McStatus", Text = "Genero il mondo...", ClassName = "TextLabel",
    AnchorX = 0.5, AnchorY = 0.5, SizeW = 320, SizeH = 44,
    BackgroundColor = {0.05, 0.08, 0.14}, BackgroundTransparency = 0.25,
    TextColor = {1, 1, 1}, FontScale = 1.3, Visible = true,
})
Gui:CreateLabel({
    Name = "McHotbar", Text = "", ClassName = "TextLabel",
    AnchorX = 0.5, AnchorY = 0.93, SizeW = 580, SizeH = 32,
    BackgroundColor = {0.05, 0.05, 0.08}, BackgroundTransparency = 0.35,
    TextColor = {1, 1, 1}, FontScale = 0.95, Visible = true,
})
Gui:CreateLabel({
    Name = "McTarget", Text = "", ClassName = "TextLabel",
    AnchorX = 0.5, AnchorY = 0.58, SizeW = 240, SizeH = 26,
    BackgroundTransparency = 1, TextColor = {1, 0.9, 0.5}, FontScale = 1.0, Visible = true,
})
Gui:CreateLabel({
    Name = "McFood", Text = "Cibo 20", ClassName = "TextLabel",
    AnchorX = 0.12, AnchorY = 0.08, SizeW = 100, SizeH = 24,
    BackgroundColor = {0.15, 0.1, 0.05}, BackgroundTransparency = 0.35,
    TextColor = {1, 0.85, 0.4}, FontScale = 1.0, Visible = true,
})
Gui:CreateLabel({
    Name = "McClock", Text = "09:00", ClassName = "TextLabel",
    AnchorX = 0.88, AnchorY = 0.08, SizeW = 80, SizeH = 24,
    BackgroundColor = {0.05, 0.08, 0.15}, BackgroundTransparency = 0.35,
    TextColor = {0.85, 0.9, 1}, FontScale = 1.0, Visible = true,
})

local function mkButton(name, text, ax, ay, w, h, r, g, b)
    Gui:CreateLabel({
        Name = name, Text = text, ClassName = "TextButton",
        AnchorX = ax, AnchorY = ay, SizeW = w, SizeH = h,
        BackgroundColor = {r, g, b}, BackgroundTransparency = 0.15,
        TextColor = {1, 1, 1}, FontScale = 1.15, Visible = true,
    })
end
mkButton("McBreak", "ROMPI", 0.90, 0.55, 96, 50, 0.70, 0.25, 0.20)
mkButton("McPlace", "METTI", 0.90, 0.64, 96, 50, 0.20, 0.55, 0.30)
mkButton("McCraft", "CRAFT", 0.90, 0.73, 96, 44, 0.55, 0.40, 0.15)
mkButton("McEat",   "MANGIA",0.90, 0.81, 96, 40, 0.35, 0.55, 0.25)
mkButton("McPrev",  "<",     0.84, 0.88, 40, 40, 0.20, 0.30, 0.50)
mkButton("McNext",  ">",     0.96, 0.88, 40, 40, 0.20, 0.30, 0.50)

local function refreshHotbar()
    local parts = {}
    for i, mat in ipairs(HOTBAR) do
        local n = inventory[mat] or 0
        local s = string.format("%d:%s x%d", i, labelOf(mat), n)
        if i == selected then s = "[" .. s .. "]" end
        parts[#parts + 1] = s
    end
    setLabel("McHotbar", table.concat(parts, "  "))
end

local function refreshFood()
    setLabel("McFood", string.format("Cibo %d", math.floor(food)))
end

local function give(mat, n)
    if not mat then return end
    inventory[mat] = (inventory[mat] or 0) + (n or 1)
    -- Se e' un minerale nuovo, mettilo in uno slot libero (dopo i primi 4)
    local known = false
    for _, m in ipairs(HOTBAR) do if m == mat then known = true break end end
    if not known then
        for i = 5, #HOTBAR do
            if (inventory[HOTBAR[i]] or 0) == 0 or HOTBAR[i] == mat then
                HOTBAR[i] = mat
                break
            end
        end
    end
    refreshHotbar()
end

--------------------------------------------------------------- craft --------

-- Ricette: consuma A, produce B
local RECIPES = {
    { need = M.Wood,         nNeed = 1, give = M.WoodPlanks, nGive = 4, name = "Assi" },
    { need = M.Cobblestone,  nNeed = 4, give = M.Brick,      nGive = 4, name = "Mattoni" },
    { need = M.Sand,         nNeed = 2, give = M.Glass,      nGive = 2, name = "Vetro", also = "Coal", alsoN = 1 },
    { need = M.WoodPlanks,   nNeed = 1, give = "Torch",      nGive = 4, name = "Torcia", also = "Coal", alsoN = 1 },
    { need = "Iron",         nNeed = 3, give = M.Cobblestone, nGive = 0, name = "Piccone (buff)", buff = true },
}

local craftMsg = ""
local digBuff = 1.0  -- >1 = scavi piu' veloce dopo "piccone"

local function tryCraft()
    for _, r in ipairs(RECIPES) do
        local have = inventory[r.need] or 0
        local alsoOk = true
        if r.also then alsoOk = (inventory[r.also] or 0) >= (r.alsoN or 1) end
        if have >= r.nNeed and alsoOk then
            inventory[r.need] = have - r.nNeed
            if r.also then inventory[r.also] = inventory[r.also] - (r.alsoN or 1) end
            if r.buff then
                digBuff = math.min(2.2, digBuff + 0.35)
                craftMsg = "Piccone migliorato! scavo x" .. string.format("%.1f", digBuff)
            else
                give(r.give, r.nGive)
                craftMsg = "Craft: " .. r.name
            end
            refreshHotbar()
            setLabel("McTarget", craftMsg)
            return
        end
    end
    setLabel("McTarget", "Craft: serve legno / pietra / sabbia+carbone / ferro")
end

local function tryEat()
    local ate = false
    if (inventory["Mushroom"] or 0) > 0 then
        inventory["Mushroom"] = inventory["Mushroom"] - 1
        food = math.min(20, food + 6)
        ate = true
    elseif (inventory["Flower"] or 0) > 0 then
        inventory["Flower"] = inventory["Flower"] - 1
        food = math.min(20, food + 2)
        ate = true
    end
    if ate then
        local hp = Humanoid.GetHealth()
        if hp < 100 and food > 8 then
            -- cura leggera mangiando
        end
        refreshFood()
        refreshHotbar()
        setLabel("McTarget", "Buon appetito!")
    else
        setLabel("McTarget", "Niente da mangiare (funghi/fiori)")
    end
end

--------------------------------------------------------------- azioni --------

local function aimHit()
    local o, d = Camera.ScreenRay()
    if not o or not d then return nil end
    local hit = workspace:Raycast(o, d, 6 * B)
    if hit and hit.Instance == nil and hit.Material then return hit end
    return nil
end

local function cellsFromHit(hit)
    local p, n = hit.Position, hit.Normal
    local ix, iy, iz = Terrain:WorldToCell(
        Vector3.new(p.X - n.X * B * 0.5, p.Y - n.Y * B * 0.5, p.Z - n.Z * B * 0.5))
    local ox, oy, oz = Terrain:WorldToCell(
        Vector3.new(p.X + n.X * B * 0.5, p.Y + n.Y * B * 0.5, p.Z + n.Z * B * 0.5))
    return ix, iy, iz, ox, oy, oz
end

local function setCell(x, y, z, mat)
    local wx, wy, wz = Terrain:CellCenterToWorld(x, y, z)
    Terrain:FillBlock(CFrame.new(wx, wy, wz), Vector3.new(B, B, B), mat)
    return wx, wy, wz
end

local digKey, digProgress = nil, 0

local function breakAt(hit, instant)
    local ix, iy, iz = cellsFromHit(hit)
    local mat = hit.Material
    local t = HARDNESS[mat]
    if not t then
        setLabel("McTarget", labelOf(mat) .. " — indistruttibile")
        return false
    end
    local k = key(ix, iz) .. "," .. iy
    if digKey ~= k then digKey, digProgress = k, 0 end
    if instant then digProgress = 1 end
    return mat, ix, iy, iz, t / digBuff
end

local function finishBreak(mat, ix, iy, iz)
    local wx, wy, wz = setCell(ix, iy, iz, M.Air)
    Fx:Smoke(Vector3.new(wx, wy, wz), 4)
    local drop = DROP[mat] or mat
    if NO_DROP[mat] then drop = nil end
    if mat == M.LeafyGrass and hash(ix, iz, iy) < 0.12 then drop = M.LeafyGrass end
    if mat == "Flower" then drop = "Flower" end
    if mat == "Mushroom" then drop = "Mushroom" end
    give(drop, 1)
    digKey, digProgress = nil, 0
end

local function playerOverlaps(x, y, z)
    local p = Humanoid.GetPosition()
    local wx, wy, wz = Terrain:CellCenterToWorld(x, y, z)
    return math.abs(wx - p.X) < B * 0.5 + 1.2
       and math.abs(wz - p.Z) < B * 0.5 + 1.2
       and wy + B * 0.5 > p.Y - 3.2 and wy - B * 0.5 < p.Y + 2.4
end

local function placeBlock()
    local hit = aimHit()
    if not hit then return end
    local mat = HOTBAR[selected]
    if (inventory[mat] or 0) <= 0 then
        setLabel("McTarget", "Non hai " .. labelOf(mat))
        return
    end
    local _, _, _, ox, oy, oz = cellsFromHit(hit)
    if playerOverlaps(ox, oy, oz) then return end
    setCell(ox, oy, oz, mat)
    inventory[mat] = inventory[mat] - 1
    refreshHotbar()
end

local function selectSlot(i)
    if i < 1 then i = #HOTBAR end
    if i > #HOTBAR then i = 1 end
    selected = i
    refreshHotbar()
end

Gui:ConnectButton("McPrev",  function() selectSlot(selected - 1) end)
Gui:ConnectButton("McNext",  function() selectSlot(selected + 1) end)
Gui:ConnectButton("McPlace", placeBlock)
Gui:ConnectButton("McCraft", tryCraft)
Gui:ConnectButton("McEat",   tryEat)
Gui:ConnectButton("McBreak", function()
    local hit = aimHit()
    if not hit then return end
    local mat, ix, iy, iz = breakAt(hit, true)
    if mat then finishBreak(mat, ix, iy, iz) end
end)

------------------------------------------------------------------ loop -------

local rmbWas, lastHealth = false, Humanoid.GetHealth()
local keyWas = {}
local foodAcc, dayAcc = 0, 0
local lastClockLabel = ""

local function applyDayNight()
    Light:SetClockTime(clockTime)
    local night = (clockTime < 6 or clockTime > 19)
    if night then
        Light:SetBrightness(0.55)
        Light:SetFogStart(20)
        Light:SetFogEnd(90)
        Light:SetFogColor(0.08, 0.09, 0.16)
    else
        Light:SetBrightness(2.0)
        Light:SetFogStart(40)
        Light:SetFogEnd(140)
        Light:SetFogColor(0.55, 0.65, 0.78)
    end
    local h = math.floor(clockTime) % 24
    local m = math.floor((clockTime % 1) * 60)
    local lab = string.format("%02d:%02d", h, m)
    if lab ~= lastClockLabel then
        lastClockLabel = lab
        Gui:SetLabelText("McClock", lab)
    end
end

RS.Heartbeat:Connect(function(dt)
    if type(dt) ~= "number" or dt ~= dt or dt <= 0 then dt = 1 / 60 end
    if dt > 0.1 then dt = 0.1 end
    if not worldReady then return end

    -- Giorno/notte (~12 min / giorno di gioco)
    dayAcc = dayAcc + dt
    if dayAcc >= 0.2 then
        clockTime = (clockTime + dayAcc * 0.04) % 24
        dayAcc = 0
        applyDayNight()
    end

    -- Fame
    foodAcc = foodAcc + dt
    if foodAcc >= 8 then
        foodAcc = 0
        food = food - 1
        if food < 0 then food = 0 end
        if food <= 0 then
            Humanoid.TakeDamage(4)
            setLabel("McTarget", "Hai fame!")
        end
        refreshFood()
    end

    local hp = Humanoid.GetHealth()
    if hp > 0 and lastHealth <= 0 then spawnPlayer() end
    lastHealth = hp
    local pos = Humanoid.GetPosition()
    if pos.Y < -4 then spawnPlayer() end

    for i = 1, #HOTBAR do
        local down = Input:IsKeyDown(tostring(i))
        if down and not keyWas[i] then selectSlot(i) end
        keyWas[i] = down
    end
    if Input:IsKeyDown("c") and not keyWas.c then tryCraft() end
    keyWas.c = Input:IsKeyDown("c")
    if Input:IsKeyDown("e") and not keyWas.e then tryEat() end
    keyWas.e = Input:IsKeyDown("e")

    local hit = aimHit()
    if Input:IsMouseButtonPressed(0) and hit then
        local mat, ix, iy, iz, t = breakAt(hit, false)
        if mat then
            digProgress = digProgress + dt / t
            setLabel("McTarget", string.format("%s %d%%", labelOf(mat),
                math.floor(math.min(digProgress, 1) * 100)))
            if digProgress >= 1 then finishBreak(mat, ix, iy, iz) end
        end
    else
        digKey, digProgress = nil, 0
        if hit then
            setLabel("McTarget", labelOf(hit.Material))
        else
            setLabel("McTarget", craftMsg ~= "" and craftMsg or "")
        end
    end

    local rmb = Input:IsMouseButtonPressed(1)
    if rmb and not rmbWas then placeBlock() end
    rmbWas = rmb
end)

----------------------------------------------------------------- avvio -------

do
    local stray = workspace:FindFirstChild("Part")
    if stray then stray:Destroy() end
end

Light:SetClockTime(9)
Light:SetFogStart(40)
Light:SetFogEnd(140)
Light:SetFogColor(0.55, 0.65, 0.78)
Humanoid.SetWalkSpeed(16)
Humanoid.SetJumpPower(48)
refreshHotbar()
refreshFood()
applyDayNight()
task.spawn(generateWorld)
