local Dev = game:GetService("Dev")

local ok = Dev:Check({ hp = 3, nome = "a" }, { hp = "number", nome = "string" })
if not ok then error("schema valido rifiutato") end
ok = Dev:Check({ hp = "no" }, { hp = "number" })
if ok then error("schema invalido accettato") end

local diff = Dev:Diff({ a = 1, b = 2 }, { b = 3, c = 4 })
if diff.aggiunti.c ~= 4 or diff.tolti.a ~= 1 or diff.cambiati.b.dopo ~= 3 then
	error("diff sbagliata")
end

local f = Instance.new("Folder")
f.Name = "RadiceDev"
f.Parent = workspace
local g = Instance.new("Folder")
g.Name = "Dentro"
g.Parent = f
if Dev:Path(workspace, "RadiceDev/Dentro") ~= g then error("Path non trova Dentro") end
local testo = Dev:Dump(f, 2)
if not string.find(testo, "Dentro", 1, true) then error("Dump senza il figlio") end
f:Destroy()

local ms = Dev:Benchmark("vuoto", function() end, 4)
if type(ms) ~= "number" or ms < 0 then error("benchmark") end

local tentativi = 0
local bene, val = Dev:Retry(3, function()
	tentativi += 1
	if tentativi < 3 then error("no") end
	return 7
end)
if not bene or val ~= 7 or tentativi ~= 3 then error("retry") end

local o = { Hp = 1 }
local visto
local w = Dev:Watch(o, "Hp")
w.Signal:Connect(function(v) visto = v end)
o.Hp = 5
w:Poll()
if visto ~= 5 then error("watch") end
w:Disconnect()

if Dev:FormattaTempo(75) ~= "1:15" then error("tempo") end
if Dev:FormattaByte(512) ~= "512 B" then error("byte") end

COMPAT_OK = true
