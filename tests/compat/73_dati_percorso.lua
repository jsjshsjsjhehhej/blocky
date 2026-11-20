local Dev = game:GetService("Dev")

local pari = Dev:Filter({ 1, 2, 3, 4 }, function(n) return n % 2 == 0 end)
if #pari ~= 2 or pari[1] ~= 2 then error("filter") end

local scelto = Dev:Weighted({
	{ peso = 1, valore = "a" },
	{ peso = 3, valore = "b" },
}, function() return 0.9 end)
if scelto ~= "b" then error("weighted") end

local dove = Dev:Bisect({ 1, 3, 5, 7 }, 4)
if dove ~= 3 then error("bisect " .. tostring(dove)) end

local anello = Dev:Ring(2)
anello:Push(1):Push(2):Push(3)
if anello:Get(1) ~= 2 or anello:Last() ~= 3 then error("anello") end

local slot = Dev:Slots(2)
if not slot:Put(1, "chiave") or slot:Take(1) ~= "chiave" then error("slot") end

local x = 0
local comandi = Dev:Commands()
comandi:Do({
	esegui = function() x = 1 end,
	annulla = function() x = 0 end,
})
comandi:Undo()
comandi:Redo()
if x ~= 1 then error("comandi") end

local curva = Dev:Curve()
curva:Add(0, 0):Add(1, 10)
if math.abs(curva:Eval(0.5) - 5) > 1e-4 then error("curva") end

local mappa = Dev:PathGrid(5, 5)
mappa:Block(2, 2)
mappa:Block(2, 3)
mappa:Block(2, 4)
local path = mappa:Find(1, 3, 4, 3)
if not path or path[1][1] ~= 1 or path[#path][1] ~= 4 then error("percorso") end
if mappa:Find(1, 1, 1, 1)[1][1] ~= 1 then error("percorso fermo") end

local na = Dev:Noise(1.2, 3.4, 7)
local nb = Dev:Noise(1.2, 3.4, 7)
if na ~= nb or na < 0 or na > 1 then error("rumore") end

local traccia = Dev:Track()
traccia:Key(0, 0, "Linear"):Key(1, 10, "Linear")
if math.abs(traccia:Eval(0.5) - 5) > 1e-4 then error("traccia") end

Dev:Seed(3)
local r1 = Dev:Rand()
Dev:Seed(3)
if Dev:Rand() ~= r1 then error("seme") end

COMPAT_OK = true
