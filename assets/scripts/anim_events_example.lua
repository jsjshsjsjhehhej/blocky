-- Esempio API senza limiti + AnimationId
-- Disattiva jump, eventi custom, anima FancyWalk / Shoot

print("[Blocky] Script esempio — Humanoid / Events / Animator")

-- Disattiva pulsante salto (Space / pad A)
UserInputService.DisableJump()
-- oppure: Humanoid.SetJumpEnabled(false)

Humanoid.SetWalkSpeed(22)
-- Humanoid.SetJumpPower(0)

-- Eventi illimitati
Events.Create("OnShoot")
Events.Connect("OnShoot", function()
    print("Sparo!")
    game.Animator:Play("Shoot") -- AnimationId rilasciato dal Catalog
end)

-- Walk custom (se hai rilasciato FancyWalk dal Catalog)
-- game.Animator:SetWalkAnim("FancyWalk")
-- game.Animator:SetJumpAnim("classic_jump")

-- BindableEvent
local ev = Instance.new("BindableEvent", "Reload")
ev.Event:Connect(function()
    print("Ricarica")
end)

game.RunService.Heartbeat:Connect(function(dt)
    -- ogni frame: qui puoi fare qualsiasi logica
end)

print("Prova: Events.Fire('OnShoot') dalla console / altro script")
Events.Fire("OnShoot")
