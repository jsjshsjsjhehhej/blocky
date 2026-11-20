local Dev = game:GetService("Dev")

local entrato = false
local m = Dev:Machine("Idle")
m:Add("Run", { Enter = function() entrato = true end })
if not m:Is("Idle") then error("parte da Idle") end
if not m:Go("Run") or not entrato or not m:Is("Run") then error("transizione") end
m:Update(0.5)
if math.abs(m:Time() - 0.5) > 1e-6 then error("tempo macchina") end

local c = Dev:Cooldown()
c:Set("shot", 0.4)
if not c:Ready("shot") or not c:Use("shot") or c:Ready("shot") then error("cooldown uso") end
c:Tick(0.4)
if not c:Ready("shot") then error("cooldown scaduto") end

local creati = 0
local pool = Dev:Pool(function()
	creati += 1
	return { id = creati }
end)
local a = pool:Get()
pool:Release(a)
local b = pool:Get()
if a ~= b or creati ~= 1 or pool:Busy() ~= 1 then error("pool") end

local colpi = 0
local timer = Dev:Timer(1, function() colpi += 1 end, true)
timer:Start()
timer:Tick(2.5)
if colpi ~= 2 then error("timer " .. colpi) end

local bus = Dev:Bus()
local n = 0
bus:On("x", function(v) n += v end)
bus:Emit("x", 3)
if n ~= 3 then error("bus") end

local passi = 0
local seq = Dev:Sequence()
seq:Add(function() passi += 1 end)
seq:Add(function() passi += 1 end)
seq:Tick()
seq:Tick()
if passi ~= 2 or not seq:Done() then error("sequenza") end

local board = Dev:Board()
local cambiata
board.Changed:Connect(function(k, v) cambiata = k .. "=" .. tostring(v) end)
board:Set("bersaglio", "porta")
if board:Get("bersaglio") ~= "porta" or cambiata ~= "bersaglio=porta" then error("lavagna") end

local jobs = Dev:Jobs()
local volte = 0
jobs:Every("tick", 0.5, function() volte += 1 end)
jobs:Tick(1.1)
if volte ~= 2 then error("lavori " .. volte) end

local latch = Dev:Latch()
if not latch:Arm("visto") or latch:Arm("visto") then error("chiavistello") end

local coda = Dev:Queue()
local sparato = false
coda:After(0.2, function() sparato = true end)
coda:Tick(0.3)
if not sparato or coda:Pending() ~= 0 then error("coda") end

COMPAT_OK = true
