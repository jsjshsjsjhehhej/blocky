-- Demo RemoteEvent (Host + Client in LAN)
-- Host: riceve FireServer e risponde con FireAllClients
-- Client: invia FireServer

local ev = Instance.new("RemoteEvent", "ScorePing")

if game:GetService("RunService") then
    -- sempre ok
end

ev.OnServerEvent:Connect(function(fromName, data)
    print("[Server] da", fromName, "→", data)
    ev:FireAllClients("pong:" .. tostring(data))
end)

ev.OnClientEvent:Connect(function(fromName, data)
    print("[Client] da", fromName, "→", data)
end)

local t = 0
game.RunService.Heartbeat:Connect(function(dt)
    t = t + dt
    if t < 3.0 then return end
    t = 0
    -- Prova invio (su Host resta locale OnServerEvent)
    ev:FireServer("tick")
end)

print("RemoteEvent ScorePing pronto")
