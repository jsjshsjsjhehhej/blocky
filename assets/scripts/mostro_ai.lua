-- ============================================================
--  MOSTRO — modello importato, animato e guidato da script
--
--  Ronda su percorso fisso, ti nota a vista, ti insegue aggirando
--  gli ostacoli. L'animazione cambia con lo stato invece di restare
--  in loop.
--
--  Serve un Model importato con almeno due clip animate.
--  Cambia MODEL_NAME col nome che leggi nell'Explorer.
-- ============================================================

local MODEL_NAME  = "Model1"

local VIEW_RANGE  = 30      -- entro cui ti nota
local GIVE_UP     = 45      -- oltre cui rinuncia
local WALK_SPEED  = 4
local CHASE_SPEED = 9
local REPATH_TIME = 0.4     -- ogni quanto ricalcola la strada mentre insegue
local ARRIVE      = 1.2     -- quanto vicino per considerare preso un waypoint

local WAYPOINTS = {
    Vector3.new(-14, 0, -14),
    Vector3.new( 14, 0, -14),
    Vector3.new( 14, 0,  14),
    Vector3.new(-14, 0,  14),
}

-- ---- aggancio ----
local mostro = Workspace:FindFirstChild(MODEL_NAME)
if not mostro then
    print("[Mostro] '" .. MODEL_NAME .. "' non trovato — controlla il nome nell'Explorer")
    return
end

mostro:SetCanCollide(true)

-- Se il tuo file usa nomi diversi, mappali qui una volta sola: da qui in poi
-- lo script parla di "idle"/"walk"/"chase" e non gli importa come li ha
-- chiamati Blender.
-- mostro:SetClipAlias("chase", "Armature|ChasingAction")

local animator = mostro.Animator
local idle  = animator:LoadAnimation("idle")
local walk  = animator:LoadAnimation("walk")
local chase = animator:LoadAnimation("chase")

for _, t in ipairs({idle, walk, chase}) do t:SetLooped(true) end

Workspace:RebuildNav()   -- la scena è costruita: calcola gli ostacoli

-- ---- stato ----
local state     = ""
local wpIndex   = 1
local path      = nil
local pathStep  = 1
local repathIn  = 0
local lostFor   = 0

-- atan2 non c'è nel sandbox: la ricostruisco dai casi di math.atan.
local function atan2(y, x)
    if x > 0 then return math.atan(y / x) end
    if x < 0 then
        if y >= 0 then return math.atan(y / x) + math.pi end
        return math.atan(y / x) - math.pi
    end
    if y > 0 then return math.pi / 2 end
    if y < 0 then return -math.pi / 2 end
    return 0
end

-- Play() ferma le altre tracce, quindi il cambio di stato è una riga sola.
local function setState(next)
    if state == next then return end
    state = next
    if next == "chase" then
        chase:AdjustSpeed(1.25)
        chase:Play()
    elseif next == "patrol" then
        walk:AdjustSpeed(1.0)
        walk:Play()
    else
        idle:Play()
    end
    print("[Mostro] " .. next)
end

local function flatDist(a, b)
    local dx, dz = a.X - b.X, a.Z - b.Z
    return math.sqrt(dx * dx + dz * dz)
end

-- Avanza verso `to` e gira il modello nella direzione di marcia.
local function step(from, to, speed, dt)
    local dx, dz = to.X - from.X, to.Z - from.Z
    local len = math.sqrt(dx * dx + dz * dz)
    if len < 0.0001 then return from, true end

    local d = math.min(speed * dt, len)
    mostro:SetYaw(math.deg(atan2(dx, dz)))

    return Vector3.new(from.X + dx / len * d, from.Y, from.Z + dz / len * d),
           (len - d) <= ARRIVE
end

-- Segue il percorso calcolato; ritorna la nuova posizione.
local function followPath(here, speed, dt)
    if not path or pathStep > #path then return here end
    local moved, arrived = step(here, path[pathStep], speed, dt)
    if arrived then pathStep = pathStep + 1 end
    return moved
end

setState("idle")

game:GetService("RunService").Heartbeat:Connect(function(dt)
    local here   = mostro:GetPosition()
    local player = Humanoid.GetPosition()
    local d      = flatDist(here, player)

    -- --- inseguimento ---
    if d < VIEW_RANGE then
        lostFor = 0
        setState("chase")

        repathIn = repathIn - dt
        if repathIn <= 0 or not path then
            -- Il percorso aggira i muri; se non ne esiste uno, si va dritti.
            path = Workspace:FindPath(here, player)
            pathStep = 1
            repathIn = REPATH_TIME
        end

        if path then
            mostro:SetPosition(followPath(here, CHASE_SPEED, dt))
        else
            mostro:SetPosition(step(here, player, CHASE_SPEED, dt))
        end
        return
    end

    -- Non molla subito: insiste qualche secondo sull'ultima strada nota.
    if state == "chase" then
        lostFor = lostFor + dt
        if d < GIVE_UP and lostFor < 3 then
            mostro:SetPosition(followPath(here, CHASE_SPEED * 0.7, dt))
            return
        end
        path = nil
    end

    -- --- ronda ---
    setState("patrol")
    local moved, arrived = step(here, WAYPOINTS[wpIndex], WALK_SPEED, dt)
    mostro:SetPosition(moved)
    if arrived then
        wpIndex = wpIndex % #WAYPOINTS + 1
    end
end)

print("[Mostro] pronto — " .. #WAYPOINTS .. " punti di ronda, pathfinding attivo")
