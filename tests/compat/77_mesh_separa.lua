-- Mesh sulla Part, SpecialMesh, SeparateAsync, punto sulla faccia ruotata.
local GS = game:GetService("GeometryService")

local mesh = Instance.new("MeshPart")
mesh.Name = "MeshProva"
mesh.Anchored = true
mesh.Size = Vector3.new(2, 2, 2)
mesh.Position = Vector3.new(80, 20, 80)
mesh.MeshId = "models/cassa.obj"
mesh.Parent = workspace
assert(mesh.ClassName == "MeshPart", "MeshPart")
assert(type(mesh.MeshId) == "string" and #mesh.MeshId > 0, "MeshId")

local base = Instance.new("Part")
base.Name = "BaseMesh"
base.Anchored = true
base.Size = Vector3.new(2, 2, 2)
base.Position = Vector3.new(90, 20, 80)
base.Parent = workspace
local sm = Instance.new("SpecialMesh")
sm.MeshId = "models/cassa.obj"
sm.Parent = base
assert(type(base.MeshId) == "string" and #base.MeshId > 0, "SpecialMesh.MeshId")
sm.MeshType = "Sphere"
local forma = base.Shape
local formaNome = type(forma) == "string" and forma or (type(forma) == "table" and forma.Name) or tostring(forma)
assert(formaNome == "Ball" or forma == Enum.PartType.Ball, "MeshType Sphere " .. tostring(formaNome))

local a = Instance.new("Part")
a.Name = "SepA"
a.Anchored = true
a.Size = Vector3.new(4, 2, 2)
a.Position = Vector3.new(120, 20, 80)
a.Parent = workspace
local b = Instance.new("Part")
b.Name = "SepB"
b.Anchored = true
b.Size = Vector3.new(2, 2, 2)
b.Position = Vector3.new(122, 20, 80)
b.Parent = workspace
local u = GS:UnionAsync(a, { b })
local pezzi = GS:SeparateAsync(u)
assert(type(pezzi) == "table" and #pezzi >= 2, "SeparateAsync")
local nomi = {}
for _, p in ipairs(pezzi) do nomi[p.Name] = true end
assert(nomi.SepA and nomi.SepB, "i pezzi originali")

local box = Instance.new("Part")
box.Anchored = true
box.Size = Vector3.new(2, 2, 2)
box.CFrame = CFrame.new(0, 50, 0) * CFrame.Angles(0, math.rad(90), 0)
box.Parent = workspace
local cf = box.CFrame
local fuori = cf.Position + cf.LookVector * 10
local hit = box:GetClosestPointOnSurface(fuori)
local atteso = cf.Position + cf.LookVector * 1
assert((hit - atteso).Magnitude < 0.2, "faccia ruotata")

local lunga = Instance.new("Part")
lunga.Name = "RotLunga"
lunga.Anchored = true
lunga.Size = Vector3.new(10, 1, 1)
lunga.CFrame = CFrame.new(0, 60, 0) * CFrame.Angles(0, math.rad(90), 0)
lunga.Parent = workspace
local function contiene(lista, nome)
	for _, p in ipairs(lista) do
		if p.Name == nome then return true end
	end
	return false
end
local zona = workspace:GetPartBoundsInBox(CFrame.new(0, 60, 4), Vector3.new(2, 2, 2))
assert(contiene(zona, "RotLunga"), "la part ruotata entra nella box")
local fuoriX = workspace:GetPartBoundsInBox(CFrame.new(4, 60, 0), Vector3.new(2, 2, 2))
assert(not contiene(fuoriX, lunga.Name), "l'asse lungo non e' piu' su X")

for _, p in ipairs(pezzi) do p:Destroy() end
mesh:Destroy()
base:Destroy()
box:Destroy()
lunga:Destroy()

COMPAT_OK = true
