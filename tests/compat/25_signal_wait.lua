-- :Wait() su eventi e task.cancel.
local RunService = game:GetService("RunService")
task.spawn(function()
	local dt = RunService.Heartbeat:Wait()
	assert(type(dt) == "number", "Heartbeat:Wait() restituisce dt")
	local th = task.delay(5, function() error("non doveva partire") end)
	task.cancel(th)
	task.wait(0.1)
	COMPAT_OK = true
end)
