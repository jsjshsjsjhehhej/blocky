-- Tanti ProximityPrompt (come le casse di un gioco): ognuno e' suo.
local function cassa(x)
	local p = Instance.new("Part")
	p.Anchored = true
	p.Position = Vector3.new(x, 1, 0)
	p.Parent = workspace
	local pp = Instance.new("ProximityPrompt")   -- tutti con lo stesso Name
	pp.ActionText = "Apri"
	pp.ObjectText = "Cassa " .. x
	pp.HoldDuration = 0.4
	pp.Parent = p
	return p, pp
end
local _, a = cassa(0)
local partB, b = cassa(10)
assert(a.ObjectText == "Cassa 0" and b.ObjectText == "Cassa 10", "ogni prompt tiene le sue proprieta'")
assert(math.abs(a.HoldDuration - 0.4) < 1e-5 and a.Enabled == true, "valori letti dal prompt")   -- float, come su Roblox

-- scrivere due volte la stessa proprieta' arriva tutte e due le volte
a.ActionText = "Avvia"
a.ActionText = "Ferma"
assert(a.ActionText == "Ferma", "ActionText riscritto: " .. tostring(a.ActionText))
a.Enabled = false
a.Enabled = true
a.Enabled = false
assert(a.Enabled == false, "Enabled riscritto")
a.KeyboardKeyCode = Enum.KeyCode.F
assert(a.KeyboardKeyCode == Enum.KeyCode.F, "KeyboardKeyCode: " .. tostring(a.KeyboardKeyCode))

-- Parent dopo Instance.new("ProximityPrompt", part) e cambio di Parent
local leva = Instance.new("Part")
leva.Anchored = true
leva.Parent = workspace
local c = Instance.new("ProximityPrompt", leva)
assert(c.Parent == leva, "Instance.new con il Parent")
c.Parent = partB
assert(c.Parent == partB, "cambio di Parent")

-- Destroy del prompt e della sua part
b:Destroy()
assert(b.Parent == nil, "prompt:Destroy")
partB:Destroy()   -- porta via anche c
c.ActionText = "dopo"   -- su un prompt sparito non va in errore
local fired = 0
a.Triggered:Connect(function(player) fired += 1 end)

COMPAT_OK = true
