-- Funzioni vecchie che tanti script usano ancora: wait, spawn, delay, tick, time.
local t0 = tick()
assert(type(t0) == "number" and t0 > 1e9, "tick")
assert(type(time()) == "number", "time")
local spawned, delayed = false, false
spawn(function() spawned = true end)
delay(0.05, function() delayed = true end)
task.spawn(function()
	local waited = wait(0.1)
	assert(waited >= 0.09, "wait restituisce il tempo")
	assert(spawned and delayed, "spawn/delay")
	assert(os.date("%Y") == tostring(os.date("*t").year))
	COMPAT_OK = true
end)
