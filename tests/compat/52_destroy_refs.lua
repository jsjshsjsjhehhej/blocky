-- Dopo un Destroy le variabili delle ALTRE part puntano ancora alla part
-- giusta (prima, distrutta una part, quelle dopo scalavano di indice e lo
-- script finiva su un'altra), con nomi e attributi. E Destroy toglie anche
-- le figlie, come Roblox.
local function mk(name, x)
    local p = Instance.new("Part")
    p.Parent = workspace
    p.Name = name
    p.Position = Vector3.new(x, 5, 0)
    p.Anchored = true
    return p
end
local a = mk("Prima", 0)
local b = mk("Seconda", 10)
local c = mk("Terza", 20)
b:SetAttribute("vita", 42)
local padre = mk("Padre", 30)
local figlia = mk("Figlia", 31)
figlia.Parent = padre

a:Destroy()
padre:Destroy()
task.wait()   -- le distruzioni avvengono a fine frame

assert(b.Name == "Seconda", "b punta a: " .. tostring(b.Name))
assert(c.Name == "Terza", "c punta a: " .. tostring(c.Name))
assert(math.abs(c.Position.X - 20) < 0.01, "posizione di c: " .. tostring(c.Position.X))
assert(b:GetAttribute("vita") == 42, "attributo di b: " .. tostring(b:GetAttribute("vita")))
c.Position = Vector3.new(25, 5, 0)
assert(math.abs(workspace:FindFirstChild("Terza").Position.X - 25) < 0.01, "scrivere su c cambia c")
assert(math.abs(b.Position.X - 10) < 0.01, "e non b")
assert(workspace:FindFirstChild("Prima") == nil, "la distrutta non c'e' piu'")
assert(workspace:FindFirstChild("Figlia", true) == nil, "la figlia se ne va col padre")
assert(a.Parent == nil, "Parent nil dopo Destroy")
COMPAT_OK = true
