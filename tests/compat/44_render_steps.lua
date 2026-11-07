-- RunService: BindToRenderStep, RenderStepped, Stepped(tempo, dt).
local RS = game:GetService("RunService")
local frames, stepped = 0, 0
RS:BindToRenderStep("Camera", Enum.RenderPriority and Enum.RenderPriority.Camera.Value or 200, function(dt) frames += 1 end)
RS.RenderStepped:Connect(function(dt) end)
RS.Stepped:Connect(function(t, dt) if type(dt) == "number" then stepped += 1 end end)
task.delay(0.3, function()
	RS:UnbindFromRenderStep("Camera")
	local f = frames
	task.wait(0.1)
	assert(f > 3, "BindToRenderStep: " .. f)
	assert(frames == f, "UnbindFromRenderStep")
	assert(stepped > 3, "Stepped(t, dt): " .. stepped)
	assert(RS:IsClient() ~= nil and RS:IsServer() ~= nil and RS:IsStudio() ~= nil)
	COMPAT_OK = true
end)
