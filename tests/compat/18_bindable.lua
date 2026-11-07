-- BindableEvent e BindableFunction per comunicare tra script.
local event = Instance.new("BindableEvent")
local got
event.Event:Connect(function(a, b) got = a + b end)
event:Fire(2, 3)
local fn = Instance.new("BindableFunction")
fn.OnInvoke = function(x) return x * 2 end
assert(fn:Invoke(21) == 42, "BindableFunction")
task.delay(0.1, function()
	assert(got == 5, "BindableEvent: " .. tostring(got))
	COMPAT_OK = true
end)
