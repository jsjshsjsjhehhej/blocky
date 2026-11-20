local Dev = game:GetService("Dev")

if math.abs(Dev:Ease("Linear", 0.5) - 0.5) > 1e-6 then error("linear") end
if Dev:Ease("Quad", "In", 0) ~= 0 or Dev:Ease("Quad", "In", 1) ~= 1 then error("quad") end

local p = Instance.new("Part")
p.Name = "ScatolaDev"
p.Size = Vector3.new(2, 2, 2)
p.Position = Vector3.new(0, 10, 0)
p.Anchored = true
p.Parent = workspace
if not Dev:Contains(p, Vector3.new(0, 10, 0)) then error("contains") end
if Dev:Contains(p, Vector3.new(0, 14, 0)) then error("fuori") end

local q = Instance.new("Part")
q.Name = "LontanoDev"
q.Size = Vector3.new(2, 2, 2)
q.Position = Vector3.new(30, 10, 0)
q.Anchored = true
q.Parent = workspace

local vicino = Dev:Closest(Vector3.new(0, 10, 1), { p, q })
if vicino ~= p then error("closest") end
local box = Dev:Bounds({ p, q })
if not box or box.size.X < 30 then error("bounds") end

local colpito, dist = Dev:CastBox(Vector3.new(0, 10, -20), Vector3.new(0, 0, 1), 40, { p, q })
if colpito ~= p or not dist or dist > 21 then error("cast") end

local griglia = Dev:Grid(4)
griglia:Add(Vector3.new(0, 0, 0), "a")
griglia:Add(Vector3.new(100, 0, 0), "b")
local trovati = griglia:Query(Vector3.new(1, 0, 0), 3)
if #trovati ~= 1 or trovati[1] ~= "a" then error("griglia") end

local molla = Dev:Spring(30, 8, 0)
local x = molla:Step(0.05, 1)
if x <= 0 or x >= 1 then error("molla") end

local rosso = Color3.new(1, 0, 0)
local h, s, v = Dev:ToHSV(rosso)
if h > 0.01 or math.abs(s - 1) > 0.01 or math.abs(v - 1) > 0.01 then error("hsv") end
local diNuovo = Dev:FromHSV(0, 1, 1)
if math.abs(diNuovo.R - 1) > 0.02 or diNuovo.G > 0.02 then error("da hsv") end

local mezzo = Dev:Bezier(
	Vector3.new(0, 0, 0),
	Vector3.new(0, 2, 0),
	Vector3.new(2, 2, 0),
	Vector3.new(2, 0, 0),
	0.5
)
if math.abs(mezzo.Y - 1.5) > 0.05 then error("bezier") end

p:Destroy()
q:Destroy()
COMPAT_OK = true
