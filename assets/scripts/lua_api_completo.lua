--[[============================================================================
  Blocky / RobloxEngine — LINGUA SCRIPT LUA COMPLETA (demo + riferimento)
  ==============================================================================
  Avvio: Studio → View → Script → "API completa" → Test (F5)
  Oppure place script = assets/scripts/lua_api_completo.lua

  Tocca i PAD colorati (riga z=12) per vedere ogni effetto.

  ---------------------------------------------------------------------------
  TIPI / COSTRUTTORI
    Vector3.new(x, y, z)          → {x,y,z}  (anche .X .Y .Z in lettura)
    Color3.new(r, g, b)           → {r,g,b}  0..1
    Instance.new("Part")
    Instance.new("Sound")
    Instance.new("BindableEvent", "Nome")
    Instance.new("RemoteEvent", "Nome")

  WORKSPACE
    workspace / Workspace
    workspace:Add(part)           → Part live
    workspace:FindFirstChild(name)→ Part live | nil
    workspace:GetChildren()       → lista snapshot
    workspace:SetPartPosition(name, Vector3|x,y,z)
    workspace:Raycast(origin, dir, maxDist?) → {Instance, Position, Distance} | nil

  PART (live = script.Parent oppure dopo Add/Find)
    .Name  .Position  .Size  .CanCollide  .ClassName
    :SetPosition(v)  :GetPosition()  :Destroy()
    .Touched:Connect(function(other, partName) ... end)  -- player entra AABB

  GAME SERVICES
    game.RunService.Heartbeat:Connect(function(dt) end)
    game:GetService("RunService"|"SoundService"|"Particles"|"Players"|"Workspace"|"AntiLag")
    -- AntiLag:SetMode("Auto"|"Off"|"Strong")  SetTargetFps(n)  GetLevel()  GetStats()
    game.SoundService:Play(path [, looped])  |  :Stop()
    game.Animator:Play("Wave"|"Walk"|"Dance"|"Idle"|id)
    game.Animator:Stop()  :SetWalkAnim(id)  :SetJumpAnim(id)  :List()
    -- File: %APPDATA%/Blocky/anims/<id>.r6anim.json  (BlockyPlayerAnim v2)
    game.Particles:Emit(pos [, count] [, color])   -- burst
    game.Particles:Sparks / Smoke / Coins(pos [, count] [, color])
    game.Players.LocalPlayer.Character.Humanoid  (= Humanoid globale)
    game.Events  (= Events globale)

  HUMANOID / INPUT
    Humanoid.SetJumpEnabled(bool)  SetWalkEnabled  SetWalkSpeed(n)  SetJumpPower(n)
    Humanoid.Get() → {JumpEnabled, WalkEnabled, WalkSpeed, JumpPower}
    UserInputService.DisableJump() / EnableJump()

  EVENTI
    Events.Create(name)  Events.Connect(name, fn)  Events.Fire(name, ...)
    Events.DisconnectAll([name])
    BindableEvent:Fire(...)  .Event:Connect(fn)
    RemoteEvent:FireServer(payloadString)
    RemoteEvent:FireAllClients(payloadString)
    .OnServerEvent:Connect(fn)  .OnClientEvent:Connect(fn)
    (payload = string; LAN Host/Client)

  ALTRO
    print(...)
    AnimateFrame(frame, Vector3)  — keyframe Model
    script.Parent  — solo negli script attaccati a una Part
    task.wait(n) / task.spawn(fn) / task.delay(n, fn) / wait(n)

  SANDBOX (disabilitati / protetti): dofile loadfile load require collectgarbage
    rawset setmetatable getmetatable string.dump io os debug
  LIMITI: WalkSpeed≤28 JumpPower≤70 Particles≤128 Heartbeat≤64
  task.wait / task.spawn / task.delay — wait solo dentro spawn
============================================================================]]

print("========== Blocky Lua API completa ==========")

-----------------------------------------------------------------------
-- Helper: crea un pad tocabile
-----------------------------------------------------------------------
local function makePad(name, x, color, onTouch)
    local p = Instance.new("Part")
    p.Name = name
    p.Position = Vector3.new(x, 1.5, 12)
    p.Size = Vector3.new(3.2, 1.2, 3.2)
    p.BrickColor = color
    p.CanCollide = true
    local live = workspace:Add(p)
    if live and onTouch then
        local once = false
        live.Touched:Connect(function(other)
            print("[PAD]", name, "toccato da", other and other.Name or "?")
            onTouch(live, once)
            once = true
        end)
    end
    return live
end

-----------------------------------------------------------------------
-- Events + BindableEvent
-----------------------------------------------------------------------
Events.Create("OnBoom")
Events.Connect("OnBoom", function(msg)
    print("[Events] OnBoom →", msg)
end)

local bind = Instance.new("BindableEvent", "Reload")
bind.Event:Connect(function()
    print("[Bindable] Reload!")
    game.Particles:Coins(Vector3.new(0, 4, 12), 16, Color3.new(1, 0.85, 0.2))
end)

-----------------------------------------------------------------------
-- RemoteEvent (string payload; ok anche offline su Host locale)
-----------------------------------------------------------------------
local remote = Instance.new("RemoteEvent", "ApiPing")
remote.OnServerEvent:Connect(function(fromName, data)
    print("[Remote Server]", fromName, data)
    pcall(function()
        remote:FireAllClients("echo:" .. tostring(data))
    end)
end)
remote.OnClientEvent:Connect(function(fromName, data)
    print("[Remote Client]", fromName, data)
end)

-----------------------------------------------------------------------
-- PAD 1 — ESPLOSIONE (sparks + burst + smoke + Destroy)
-----------------------------------------------------------------------
makePad("PadEsplodi", -14, Color3.new(0.9, 0.15, 0.1), function(part, done)
    if done then return end
    local pos = part.Position
    game.Particles:Sparks(pos, 45)
    game.Particles:Emit(pos, 55, Color3.new(1, 0.35, 0.05))
    game.Particles:Smoke(pos, 28)
    Events.Fire("OnBoom", "esplosione!")
    part:Destroy()
end)

-----------------------------------------------------------------------
-- PAD 2 — SCINTILLE
-----------------------------------------------------------------------
makePad("PadSparks", -10, Color3.new(1, 0.85, 0.2), function(part)
    game.Particles:Sparks(part.Position, 36)
end)

-----------------------------------------------------------------------
-- PAD 3 — FUMO
-----------------------------------------------------------------------
makePad("PadFumo", -6, Color3.new(0.55, 0.55, 0.6), function(part)
    game.Particles:Smoke(part.Position, 30)
end)

-----------------------------------------------------------------------
-- PAD 4 — MONETE
-----------------------------------------------------------------------
makePad("PadMonete", -2, Color3.new(1, 0.75, 0.1), function(part)
    game.Particles:Coins(part.Position, 24, Color3.new(1, 0.9, 0.2))
    bind:Fire()
end)

-----------------------------------------------------------------------
-- PAD 5 — BURST COLORATO
-----------------------------------------------------------------------
makePad("PadBurst", 2, Color3.new(0.3, 0.7, 1), function(part)
    game.Particles:Emit(part.Position, 40, Color3.new(0.3, 0.8, 1))
end)

-----------------------------------------------------------------------
-- PAD 6 — SPEED BOOST
-----------------------------------------------------------------------
makePad("PadSpeed", 6, Color3.new(0.2, 0.9, 0.35), function()
    Humanoid.SetWalkSpeed(28)
    print("WalkSpeed = 28 max (clamp anti-cheat)")
end)

-----------------------------------------------------------------------
-- PAD 7 — SUPER JUMP
-----------------------------------------------------------------------
makePad("PadJump", 10, Color3.new(0.55, 0.3, 1), function()
    Humanoid.SetJumpEnabled(true)
    Humanoid.SetJumpPower(70)
    print("JumpPower = 70 max")
end)

-----------------------------------------------------------------------
-- PAD 8 — NO JUMP
-----------------------------------------------------------------------
makePad("PadNoJump", 14, Color3.new(0.35, 0.35, 0.4), function()
    UserInputService.DisableJump()
    print("Salto disattivato — tocca Reset")
end)

-----------------------------------------------------------------------
-- PAD 9 — RESET PLAYER + RAYCAST + REMOTE
-----------------------------------------------------------------------
makePad("PadReset", 18, Color3.new(0.9, 0.9, 0.9), function(part)
    Humanoid.SetWalkSpeed(16)
    Humanoid.SetJumpPower(50)
    UserInputService.EnableJump()
    Humanoid.SetJumpEnabled(true)
    Humanoid.SetWalkEnabled(true)
    local st = Humanoid.Get()
    print("Reset Humanoid", st.WalkSpeed, st.JumpPower, st.JumpEnabled)

    local hit = workspace:Raycast(Vector3.new(0, 40, 12), Vector3.new(0, -1, 0), 80)
    if hit then
        print("Raycast →", hit.Instance.Name, "dist", hit.Distance)
        game.Particles:Sparks(hit.Position, 12)
    else
        print("Raycast: nessun hit")
    end

    pcall(function()
        remote:FireServer("ping-from-pad")
    end)

    pcall(function()
        game.Animator:Play("Wave")
    end)

    -- Muovi un po' il pad (demo Position live)
    local p = part.Position
    part.Position = Vector3.new(p.x, p.y + 0.5, p.z)
end)

-----------------------------------------------------------------------
-- Heartbeat: timer leggero (demo RunService)
-----------------------------------------------------------------------
local t = 0
local pulse = 0
game.RunService.Heartbeat:Connect(function(dt)
    t = t + dt
    pulse = pulse + dt
    if pulse >= 8 then
        pulse = 0
        -- ogni 8s: piccola scintilla al centro demo
        game.Particles:Sparks(Vector3.new(0, 3, 12), 8)
    end
end)

-----------------------------------------------------------------------
-- GetService + Players + lista figli
-----------------------------------------------------------------------
local RS = game:GetService("RunService")
local PS = game:GetService("Particles")
local LP = game.Players.LocalPlayer
print("GetService RunService ok:", RS ~= nil)
print("LocalPlayer.Character.Humanoid =", LP and LP.Character and LP.Character.Humanoid)

local kids = workspace:GetChildren()
print("Workspace parti:", #kids)

-----------------------------------------------------------------------
-- Neon / Materiali / PointLight / Explode
-----------------------------------------------------------------------
-- Material: Plastic, SmoothPlastic, Neon, Metal, Wood, Concrete,
--           Grass, Glass, Ice, Brick, ForceField
-- part.CanDestroy = true/false
-- part:Destroy()  part:Explode([count])
local neon = Instance.new("Part")
neon.Name = "GlowPad"
neon.Size = Vector3.new(4, 1, 4)
neon.Position = Vector3.new(0, 1, 20)
neon.BrickColor = Color3.new(0.2, 1, 0.9)
neon.Material = "Neon"
neon.PointLight = true
neon.LightRange = 26
neon.LightBrightness = 1.6
workspace:Add(neon)

local bomb = Instance.new("Part")
bomb.Name = "TNT"
bomb.Size = Vector3.new(3, 3, 3)
bomb.Position = Vector3.new(8, 2, 20)
bomb.BrickColor = Color3.new(0.9, 0.2, 0.1)
bomb.Material = "Metal"
bomb.CanDestroy = true
local live = workspace:Add(bomb)
live.Touched:Connect(function()
    live:Explode(60)
end)

-----------------------------------------------------------------------
-- Sound (se manca il file non crasha)
-----------------------------------------------------------------------
pcall(function()
    game.SoundService:Play("assets/sounds/click.wav", false)
end)

-----------------------------------------------------------------------
-- AnimateFrame (utile se c'è un Model; innocuo altrimenti)
-----------------------------------------------------------------------
pcall(function()
    AnimateFrame(0, Vector3.new(0, 2, 0))
end)

print("========== Tocca i pad colorati (z=12) ==========")
print("Rosso=esplodi  Giallo=sparks  Grigio=fumo  Oro=monete")
print("Azzurro=burst  Verde=speed  Viola=jump  Scuro=nojump  Bianco=reset")
