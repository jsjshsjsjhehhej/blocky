-- Classe Signal fatta in casa con BindableEvent (molti giochi la usano).
local Signal = {}
Signal.__index = Signal
function Signal.new()
	local self = setmetatable({}, Signal)
	self._event = Instance.new("BindableEvent")
	return self
end
function Signal:Connect(fn) return self._event.Event:Connect(fn) end
function Signal:Fire(...) self._event:Fire(...) end
function Signal:Wait() return self._event.Event:Wait() end
function Signal:Destroy() self._event:Destroy() end
local s = Signal.new()
local total = 0
local c = s:Connect(function(n) total += n end)
s:Fire(5)
task.spawn(function()
	local v = s:Wait()
	assert(v == 7, "Wait: " .. tostring(v))
	c:Disconnect()
	s:Fire(100)
	task.wait(0.05)
	assert(total == 12, "totale: " .. total)
	s:Destroy()
	COMPAT_OK = true
end)
task.delay(0.05, function() s:Fire(7) end)
