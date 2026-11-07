-- Metatabelle: __index funzione, __newindex, __call, operatori.
local log = {}
local proxy = setmetatable({}, {
	__index = function(_, k) return "def_" .. k end,
	__newindex = function(t, k, v) table.insert(log, k) rawset(t, k, v) end,
	__call = function(self, x) return x * 3 end,
})
assert(proxy.foo == "def_foo")
proxy.bar = 1
assert(proxy.bar == 1 and log[1] == "bar")
assert(proxy(4) == 12)
local Money = {}
Money.__index = Money
Money.__add = function(a, b) return setmetatable({ v = a.v + b.v }, Money) end
Money.__eq = function(a, b) return a.v == b.v end
Money.__lt = function(a, b) return a.v < b.v end
Money.__le = function(a, b) return a.v <= b.v end
Money.__tostring = function(a) return "$" .. a.v end
Money.__concat = function(a, b) return tostring(a) .. tostring(b) end
Money.__len = function(a) return a.v end
local function m(v) return setmetatable({ v = v }, Money) end
assert((m(2) + m(3)) == m(5) and m(1) < m(2) and m(2) <= m(2))
assert(tostring(m(7)) == "$7" and #m(9) == 9)
assert(rawequal(proxy, proxy) and rawlen({ 1, 2 }) == 2 and rawget(proxy, "zzz") == nil)
COMPAT_OK = true
