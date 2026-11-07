-- coroutine create/resume/yield/wrap/status/close.
local co = coroutine.create(function(a)
	local b = coroutine.yield(a + 1)
	return a + b
end)
local ok1, v1 = coroutine.resume(co, 10)
assert(ok1 and v1 == 11)
assert(coroutine.status(co) == "suspended")
local ok2, v2 = coroutine.resume(co, 5)
assert(ok2 and v2 == 15 and coroutine.status(co) == "dead")
local gen = coroutine.wrap(function() for i = 1, 3 do coroutine.yield(i) end end)
assert(gen() == 1 and gen() == 2 and gen() == 3)
local c2 = coroutine.create(function() coroutine.yield() end)
coroutine.resume(c2)
assert(coroutine.close(c2) == true and coroutine.status(c2) == "dead")
-- Come Roblox: il corpo dello script gira in una coroutine, quindi si puo' aspettare.
assert(coroutine.isyieldable() == true)
COMPAT_OK = true
