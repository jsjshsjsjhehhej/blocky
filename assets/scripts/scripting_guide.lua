--[[
  Blocky Scripting — guida ordinata (facile + sicura)
  ====================================================
  Studio: View → Guida Lua Blocky (F2)
  oppure Properties → preset "Blocco esplosivo"

  IMPORTANTE
    Lua 5.4 da solo NON sa far esplodere nulla.
    Explode / Particles / Touched sono API Blocky (stile Roblox).

  ESPLOSIONE (consigliata)
    part:Explode()
    part:Explode(60)
    game.Particles:Explode(Vector3.new(0, 4, 0))

  SICUREZZA (anti-executor)
    - niente load / require / io / os / debug / rawset / setmetatable
    - API game/workspace congelata
    - WalkSpeed max 28 · JumpPower max 70 · Particles max 128
    - task.wait solo dentro task.spawn

  PATTERN BASE
]]

print("[Blocky] scripting_guide pronto — apri Guida Lua in Studio")

-- 1) Aspetta e fai qualcosa
task.spawn(function()
    task.wait(1.0)
    print("Dopo 1 secondo")
    game.Particles:Sparks(Vector3.new(0, 4, 0), 20)
end)

-- 2) Se lo script è su una Part: tocco → esplosione
local part = script and script.Parent
if part and part.Touched then
    part.Touched:Connect(function()
        part:Explode()
    end)
end

-- 3) Servizi (GetService ordinato)
local RS = game:GetService("RunService")
local Particles = game:GetService("Particles")
print("Servizi ok", RS ~= nil, Particles ~= nil)

--[[
  API RAPIDA
    Vector3.new  Color3.new  Instance.new("Part"|"RemoteEvent"|"BindableEvent"|"Sound")
    workspace:Add / FindFirstChild / GetChildren / Raycast
    part.Position Size Orientation CanCollide
    part:Destroy()  part:Explode([n])  part.Touched:Connect
    task.wait  task.spawn  task.delay  wait()
    game.Particles:Sparks/Smoke/Coins/Emit/Explode
    Humanoid.SetWalkSpeed / SetJumpPower / SetJumpEnabled
    Events.*  RemoteEvent:FireServer / FireAllClients
    game.RunService.Heartbeat:Connect(function(dt) end)
]]
