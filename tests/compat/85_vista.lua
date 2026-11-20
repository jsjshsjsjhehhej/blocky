-- Materiali simili, luce attaccata dopo, particelle, immagine, lista, suono.
local p = Instance.new("Part")
p.Anchored = true
p.Parent = workspace

p.Material = Enum.Material.WoodPlanks
assert(p.Material.Name == "Wood", "woodplanks")
p.Material = Enum.Material.Sandstone
assert(p.Material.Name == "Sand", "sandstone")
p.Material = Enum.Material.Pebble
assert(p.Material.Name == "Slate", "pebble")
p.Material = Enum.Material.Plastic
assert(p.Material.Name == "Plastic", "plastic")

local luce = Instance.new("PointLight")
luce.Range = 24
luce.Brightness = 2
luce.Color = Color3.new(1, 0.4, 0.1)
luce.Parent = p
assert(p.PointLight == true, "luce accesa")
assert(math.abs(p.LightRange - 24) < 0.01, "raggio")
assert(math.abs(p.LightBrightness - 2) < 0.01, "luminosita")
luce.Enabled = false
assert(p.PointLight == false, "luce spenta")

local pe = Instance.new("ParticleEmitter")
pe.Parent = p
pe.Speed = NumberRange.new(2, 6)
pe.Size = 0.4
pe.SpreadAngle = Vector2.new(0, 40)
pe.Drag = 2
pe.Acceleration = Vector3.new(0, -20, 0)
pe:Emit(4)
assert(pe.Speed.Min == 2 and pe.Speed.Max == 6, "velocita")
assert(pe.Size == 0.4, "size")

local img = Instance.new("ImageButton")
assert(img.ClassName == "ImageButton", "classe bottone")
img.Image = "textures/ui.png"
img.ImageColor3 = Color3.new(1, 0, 0)
img.ImageTransparency = 0.25
assert(img.Image == "textures/ui.png", "image")
assert(math.abs(img.ImageColor3.R - 1) < 0.01, "tinta")
assert(math.abs(img.ImageTransparency - 0.25) < 0.01, "trasparenza")

local lista = Instance.new("UIListLayout")
lista.FillDirection = Enum.FillDirection.Horizontal
assert(lista.FillDirection.Name == "Horizontal", "lista")
lista.FillDirection = Enum.FillDirection.Vertical
assert(lista.FillDirection.Name == "Vertical", "lista verticale")

local suono = Instance.new("Sound")
suono.PlaybackSpeed = 1.5
assert(suono.PlaybackSpeed == 1.5, "velocita suono")
assert(type(suono.Pause) == "function" and type(suono.Resume) == "function", "pausa")
suono:Pause()
assert(suono.Playing == false, "fermo")

COMPAT_OK = true
