-- ============================================================
--  ZOMBIE SURVIVAL - Blocky Game Engine
--  Architettura a POOL: le parti si creano una volta e non si
--  distruggono MAI (in questo motore Destroy fa scalare gli
--  indici e corrompe le altre parti). Gli zombie morti vengono
--  nascosti e riusati.
-- ============================================================

print("[Zombie Survival] Avvio...")

-- ---- parametri ----
local POOL_SIZE     = 8       -- zombie massimi contemporanei (parti fisse)
local ARENA_HALF    = 26
local ARENA_LIMIT   = 27
local ZOMBIE_SPEED  = 5
local ATTACK_RANGE  = 4.0
local ATTACK_DPS    = 15
local SPAWN_EVERY   = 3.0
local SPEEDUP_EVERY = 22
local SHOOT_RANGE   = 34
local SHOOT_COOL    = 0.35
local SHOOT_CONE    = 0.5
local HIDE          = Vector3.new(9999, -80, 9999)   -- posizione "nascosto"

local GREEN     = Color3.new(0.20, 0.55, 0.20)
local DARKGREEN = Color3.new(0.14, 0.40, 0.14)
local RED       = Color3.new(0.85, 0.10, 0.10)
local GUNMETAL  = Color3.new(0.15, 0.16, 0.18)

local Sound = game:GetService("SoundService")
local function playSound(n) pcall(function() Sound:Play("assets/sounds/"..n..".wav", false) end) end

local function atan2(y, x)
    if x > 0 then return math.atan(y/x)
    elseif x < 0 then if y >= 0 then return math.atan(y/x)+math.pi else return math.atan(y/x)-math.pi end
    else if y > 0 then return math.pi/2 elseif y < 0 then return -math.pi/2 else return 0 end end
end

local function newPart(sx, sy, sz, col)
    local p = Instance.new("Part")
    p.Size = Vector3.new(sx, sy, sz); p.BrickColor = col
    p.CanCollide = false
    return workspace:Add(p)
end

-- ---- 1. arena (creata una volta) ----
local function makePart(name, x, y, z, sx, sy, sz, col)
    local p = Instance.new("Part")
    p.Name = name; p.Position = Vector3.new(x,y,z); p.Size = Vector3.new(sx,sy,sz); p.BrickColor = col
    workspace:Add(p)
end
local h = 30
makePart("Floor",0,0,0, 62,1,62, Color3.new(0.25,0.27,0.30))
makePart("WallN",0,4,-h, 60,8,2, Color3.new(0.35,0.20,0.20))
makePart("WallS",0,4,h, 60,8,2, Color3.new(0.35,0.20,0.20))
makePart("WallE",h,4,0, 2,8,60, Color3.new(0.35,0.20,0.20))
makePart("WallW",-h,4,0, 2,8,60, Color3.new(0.35,0.20,0.20))
makePart("Cover1",-12,1.5,10, 5,3,5, Color3.new(0.30,0.45,0.55))
makePart("Cover2",14,1.5,-8, 6,3,4, Color3.new(0.30,0.45,0.55))
makePart("Cover3",0,1.5,18, 8,3,3, Color3.new(0.30,0.45,0.55))
makePart("Cover4",-16,1.5,-14, 4,3,8, Color3.new(0.30,0.45,0.55))
print("[Zombie Survival] Arena pronta.")

-- pistola nell'inventario (equip col tasto 1)
Backpack.Give("Pistola")

-- ---- 2. POOL di zombie (8 parti ciascuno, create ORA e mai distrutte) ----
local pool = {}
for i = 1, POOL_SIZE do
    pool[i] = {
        active = false, x = 0, z = 0, yaw = 0, phase = 0, groanT = 0,
        torso = newPart(2.0,2.0,1.0, DARKGREEN),
        head  = newPart(1.3,1.3,1.3, GREEN),
        eyeL  = newPart(0.30,0.30,0.15, RED),
        eyeR  = newPart(0.30,0.30,0.15, RED),
        armL  = newPart(1.0,2.0,1.0, GREEN),
        armR  = newPart(1.0,2.0,1.0, GREEN),
        legL  = newPart(1.0,2.0,1.0, DARKGREEN),
        legR  = newPart(1.0,2.0,1.0, DARKGREEN),
    }
end
local ZPARTS = {"torso","head","eyeL","eyeR","armL","armR","legL","legR"}

local function hideZombie(z)
    z.active = false
    for _,k in ipairs(ZPARTS) do z[k]:SetPosition(HIDE) end
end
for i = 1, POOL_SIZE do hideZombie(pool[i]) end

local function spawnZombie()
    for i = 1, POOL_SIZE do
        local z = pool[i]
        if not z.active then
            local edge = math.random(1,4); local m = ARENA_HALF
            if edge==1 then z.x,z.z=-m,math.random(-m,m) elseif edge==2 then z.x,z.z=m,math.random(-m,m)
            elseif edge==3 then z.x,z.z=math.random(-m,m),-m else z.x,z.z=math.random(-m,m),m end
            z.active = true; z.phase = math.random()*6.28; z.groanT = math.random()*3
            return
        end
    end
end

local function poseZombie(z, dt)
    local ry = math.rad(z.yaw)
    local fx, fz = math.sin(ry), math.cos(ry)
    local rx, rz = math.cos(ry), -math.sin(ry)
    z.phase = z.phase + dt*8
    local sw = math.sin(z.phase)*25
    local function place(part, ox, oy, oz, rotX)
        part:SetPosition(Vector3.new(z.x + rx*ox + fx*oz, oy, z.z + rz*ox + fz*oz))
        part.Orientation = Vector3.new(rotX, z.yaw, 0)
    end
    place(z.torso, 0.0, 3.0, 0.0, 0)
    place(z.head,  0.0, 4.65, 0.0, 0)
    place(z.eyeL, -0.32, 4.85, 0.66, 0)
    place(z.eyeR,  0.32, 4.85, 0.66, 0)
    place(z.armL, -1.5, 3.0, 0.6, -80)
    place(z.armR,  1.5, 3.0, 0.6, -80)
    place(z.legL, -0.5, 1.0, 0.0,  sw)
    place(z.legR,  0.5, 1.0, 0.0, -sw)
    z.groanT = z.groanT - dt
    if z.groanT <= 0 then z.groanT = 5 + math.random()*5; playSound("groan") end
end

-- ---- 3. pistola visual (2 parti + flash), create una volta ----
local gunBarrel = newPart(0.35,0.35,1.6, GUNMETAL)
local gunGrip   = newPart(0.35,0.7,0.35, GUNMETAL)
local flash     = newPart(0.4,0.4,0.4, Color3.new(1.0,0.85,0.2)); flash.Material = "Neon"
local flashT = 0
local function hideGun()
    gunBarrel:SetPosition(HIDE); gunGrip:SetPosition(HIDE); flash:SetPosition(HIDE)
end
local function poseGun(pp, look)
    local fx, fz = look.X, look.Z
    local rx, rz = fz, -fx
    local hx, hz = pp.X + fx*1.2 + rx*0.5, pp.Z + fz*1.2 + rz*0.5
    local hy = pp.Y + 2.6
    local yaw = math.deg(atan2(fx, fz))
    gunBarrel:SetPosition(Vector3.new(hx + fx*0.5, hy, hz + fz*0.5))
    gunBarrel.Orientation = Vector3.new(0, yaw, 0)
    gunGrip:SetPosition(Vector3.new(hx, hy-0.4, hz))
    gunGrip.Orientation = Vector3.new(0, yaw, 0)
    if flashT > 0 then flash:SetPosition(Vector3.new(hx + fx*1.5, hy, hz + fz*1.5))
    else flash:SetPosition(HIDE) end
end
hideGun()

-- ---- 4. loop ----
local spawnT, speedT, survived, speedMul, shootCd, kills, gameOver = 0,0,0,1.0,0,0,false

local function clampPlayer()
    local p = Humanoid.GetPosition()
    local cx = math.max(-ARENA_LIMIT, math.min(ARENA_LIMIT, p.X))
    local cz = math.max(-ARENA_LIMIT, math.min(ARENA_LIMIT, p.Z))
    if cx ~= p.X or cz ~= p.Z then Humanoid.SetPosition(Vector3.new(cx, p.Y, cz)) end
end

game:GetService("RunService").Heartbeat:Connect(function(dt)
    if gameOver then return end
    survived = survived + dt; spawnT = spawnT + dt; speedT = speedT + dt
    shootCd = math.max(0, shootCd - dt)
    if flashT > 0 then flashT = flashT - dt end

    if spawnT >= SPAWN_EVERY then spawnT = 0; spawnZombie() end
    if speedT >= SPEEDUP_EVERY then speedT = 0; speedMul = speedMul + 0.12 end

    clampPlayer()
    local pp = Humanoid.GetPosition()
    local look = Humanoid.GetLook()
    local armed = (Backpack.Equipped() == "Pistola")
    if armed then poseGun(pp, look) else hideGun() end

    -- sparo su F (solo se armato)
    if armed and shootCd <= 0 and UserInputService.IsKeyDown("f") then
        shootCd = SHOOT_COOL; flashT = 0.06; playSound("shoot")
        local best, bestD = nil, SHOOT_RANGE
        for i = 1, POOL_SIZE do
            local z = pool[i]
            if z.active then
                local dx, dz = z.x - pp.X, z.z - pp.Z
                local d = math.sqrt(dx*dx + dz*dz)
                if d > 0.001 and d < bestD then
                    local dot = (dx/d)*look.X + (dz/d)*look.Z
                    if dot > SHOOT_CONE then best = z; bestD = d end
                end
            end
        end
        if best then hideZombie(best); kills = kills + 1; playSound("hit")
            print("[Zombie Survival] Uccisi: " .. kills) end
    end

    -- muovi zombie attivi
    for i = 1, POOL_SIZE do
        local z = pool[i]
        if z.active then
            local dx, dz = pp.X - z.x, pp.Z - z.z
            local d = math.sqrt(dx*dx + dz*dz)
            if d > 0.001 then
                z.yaw = math.deg(atan2(dx, dz))
                local s = ZOMBIE_SPEED * speedMul * dt
                z.x = z.x + dx/d*s; z.z = z.z + dz/d*s
            end
            poseZombie(z, dt)
            if d < ATTACK_RANGE then Humanoid.TakeDamage(ATTACK_DPS * dt) end
        end
    end

    if Humanoid.GetHealth() <= 0 then
        gameOver = true
        print(string.format("[Zombie Survival] SEI MORTO. %d uccisi, %.0f secondi.", kills, survived))
    end
end)

print("[Zombie Survival] Pronto! Tasto 1 = pistola, F = spara.")
