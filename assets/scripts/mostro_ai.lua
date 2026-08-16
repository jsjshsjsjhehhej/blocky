-- ============================================================
--  MOSTRO — modello importato, animato da script
--
--  Mostra come comandare un modello con animazioni invece di
--  lasciarlo in loop: si caricano le tracce una volta, poi si
--  alternano quando cambia lo stato.
--
--  Nel place serve un Model importato con almeno due clip.
--  Cambia MODEL_NAME col nome che vedi nell'Explorer.
-- ============================================================

local MODEL_NAME  = "Model1"

local CLIP_IDLE   = "idle"      -- cercate per sottostringa: "Armature|IdleAction"
local CLIP_WALK   = "walk"      -- viene trovata lo stesso
local CLIP_CHASE  = "chase"

local VIEW_RANGE  = 30          -- entro cui ti nota
local GIVE_UP     = 42          -- oltre cui smette di inseguire
local WALK_SPEED  = 4
local CHASE_SPEED = 9
local WAYPOINTS   = {
    Vector3.new(-14, 0, -14),
    Vector3.new( 14, 0, -14),
    Vector3.new( 14, 0,  14),
    Vector3.new(-14, 0,  14),
}

-- ---- aggancio ----
local mostro = Workspace:FindFirstChild(MODEL_NAME)
if not mostro then
    print("[Mostro] '" .. MODEL_NAME .. "' non trovato: controlla il nome nell'Explorer")
    return
end

mostro:SetCanCollide(true)   -- blocca il player invece di farsi attraversare

local animator = mostro.Animator
local idle  = animator:LoadAnimation(CLIP_IDLE)
local walk  = animator:LoadAnimation(CLIP_WALK)
local chase = animator:LoadAnimation(CLIP_CHASE)

for _, track in ipairs({idle, walk, chase}) do
    track:SetLooped(true)
end

-- ---- stato ----
local state    = ""
local target   = 1
local lastSeen = 0

-- Play() ferma le altre tracce, quindi il cambio di stato e' una riga sola.
local function setState(next)
    if state == next then return end
    state = next

    if next == "chase" then
        chase:AdjustSpeed(1.2)
        chase:Play()
    elseif next == "patrol" then
        walk:Play()
    else
        idle:Play()
    end
    print("[Mostro] " .. next)
end

local function flatDistance(a, b)
    local dx, dz = a.X - b.X, a.Z - b.Z
    return math.sqrt(dx * dx + dz * dz)
end

-- Avanza verso `to` e gira il modello nella direzione di marcia.
local function moveToward(from, to, speed, dt)
    local dx, dz = to.X - from.X, to.Z - from.Z
    local len = math.sqrt(dx * dx + dz * dz)
    if len < 0.001 then return from, false end

    local step = math.min(speed * dt, len)
    local nx = from.X + dx / len * step
    local nz = from.Z + dz / len * step

    -- atan2 in gradi: il modello guarda dove sta andando.
    mostro:SetYaw(math.deg(math.atan(dx / (dz == 0 and 0.0001 or dz))) + (dz < 0 and 180 or 0))

    return Vector3.new(nx, from.Y, nz), (len - step) < 0.5
end

setState("idle")

game:GetService("RunService").Heartbeat:Connect(function(dt)
    local here   = mostro:GetPosition()
    local player = Humanoid.GetPosition()
    local d      = flatDistance(here, player)

    -- --- inseguimento ---
    if d < VIEW_RANGE then
        lastSeen = 0
        setState("chase")
        local moved = moveToward(here, player, CHASE_SPEED, dt)
        mostro:SetPosition(moved)
        return
    end

    -- Non molla subito: continua qualche secondo verso l'ultima direzione.
    if state == "chase" then
        lastSeen = lastSeen + dt
        if d < GIVE_UP and lastSeen < 3 then
            local moved = moveToward(here, player, CHASE_SPEED * 0.7, dt)
            mostro:SetPosition(moved)
            return
        end
    end

    -- --- ronda ---
    setState("patrol")
    local moved, arrived = moveToward(here, WAYPOINTS[target], WALK_SPEED, dt)
    mostro:SetPosition(moved)
    if arrived then
        target = target % #WAYPOINTS + 1
    end
end)

print("[Mostro] pronto — " .. #WAYPOINTS .. " punti di ronda")
