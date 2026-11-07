-- Modulo con stato privato e funzioni pubbliche (il pattern dei ModuleScript).
local Inventory = {}
Inventory.__index = Inventory
local MAX = 5

function Inventory.new() return setmetatable({ items = {} }, Inventory) end
function Inventory:Add(item)
	if #self.items >= MAX then return false end
	table.insert(self.items, item)
	return true
end
function Inventory:Has(item) return table.find(self.items, item) ~= nil end
function Inventory:Sorted()
	local copy = table.clone(self.items)
	table.sort(copy)
	return copy
end

local inv = Inventory.new()
for _, it in ipairs({ "spada", "arco", "pozione" }) do inv:Add(it) end
assert(inv:Has("arco") and not inv:Has("scudo"))
assert(table.concat(inv:Sorted(), ",") == "arco,pozione,spada")
local parts = string.split("a,b,c", ",")
assert(#parts == 3 and parts[2] == "b", "string.split")
COMPAT_OK = true
