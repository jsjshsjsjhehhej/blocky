-- table: create, move, freeze, unpack, select.
local t = table.create(3, 0)
assert(#t == 3 and t[2] == 0)
local dst = table.move({ 1, 2, 3 }, 1, 3, 2, { 9 })
assert(table.concat(dst, ",") == "9,1,2,3")
local frozen = table.freeze({ a = 1 })
assert(table.isfrozen(frozen) and not pcall(function() frozen.a = 2 end))
assert(select("#", table.unpack({ 1, 2, nil, 4 }, 1, 4)) == 4)
assert(select(2, "a", "b", "c") == "b")
local sorted = { 5, 2, 9, 1 }
table.sort(sorted, function(a, b) return a > b end)
assert(sorted[1] == 9 and sorted[4] == 1)
assert(table.find({ "x", "y" }, "y") == 2)
table.clear(sorted)
assert(next(sorted) == nil)
COMPAT_OK = true
