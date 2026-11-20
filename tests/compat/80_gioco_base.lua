-- Camera sullo schermo, terreno, stati del corpo, TextBox, cono della luce.
local cam = workspace.CurrentCamera
cam.CFrame = CFrame.lookAt(Vector3.new(0, 5, 20), Vector3.new(0, 5, 0))
local v, on = cam:WorldToScreenPoint(Vector3.new(0, 5, 0))
assert(type(v.X) == "number" and type(v.Y) == "number", "WorldToScreenPoint")
assert(on == true, "il punto davanti alla camera e' sullo schermo")
assert(math.abs(v.X - 640) < 80 and math.abs(v.Y - 360) < 80, "centro schermo")
local ray = cam:ViewportPointToRay(v.X, v.Y)
assert(type(ray.Origin.X) == "number" and type(ray.Direction.Z) == "number", "ViewportPointToRay")
assert(ray.Direction.Z < -0.5, "il raggio al centro guarda avanti")
local same = cam:ScreenPointToRay(100, 100)
assert(type(same.Direction.X) == "number", "ScreenPointToRay usa i pixel")

local h = Humanoid
h:ChangeState("Flying")
assert(h:GetState() == "Flying", "volo")
h:ChangeState(Enum.HumanoidStateType.Ragdoll)
assert(h:GetState() == "Ragdoll", "ragdoll")
h:ChangeState("Swimming")
assert(h:GetState() == "Swimming", "nuoto")
h:SetStateEnabled("Swimming", false)
h:ChangeState("Swimming")
assert(h:GetState() ~= "Swimming", "nuoto spento")
h:SetStateEnabled("Swimming", true)
h:ChangeState("Running")
assert(h:GetState() == "Running", "torna a correre")

local box = Instance.new("TextBox")
assert(box.ClassName == "TextBox", "TextBox resta TextBox")
local hits = 0
local enter = nil
box.FocusLost:Connect(function(e)
    hits = hits + 1
    enter = e
end)
box:CaptureFocus()
assert(box.IsFocused == true, "CaptureFocus")
box:ReleaseFocus()
assert(hits == 1 and enter == false, "FocusLost")
assert(box.IsFocused == false, "ReleaseFocus")

local part = Instance.new("Part")
local spot = Instance.new("SpotLight", part)
assert(spot.ClassName == "SpotLight", "SpotLight")
assert(part.LightKind == 1, "cono")
part.LightAngle = 40
assert(part.LightAngle == 40, "angolo")
local surf = Instance.new("SurfaceLight", part)
assert(surf.ClassName == "SurfaceLight" and part.LightKind == 2, "SurfaceLight")

local terrain = game:GetService("Terrain")
assert(terrain == workspace.Terrain, "GetService Terrain")
local n = terrain:FillBlock(CFrame.new(0, 12, 0), Vector3.new(6, 6, 6), "Grass")
assert(type(n) == "number" and n > 0, "FillBlock")
local read = terrain:ReadVoxels({ Min = Vector3.new(-4, 8, -4), Max = Vector3.new(4, 16, 4) }, 4)
assert(type(read.materials) == "table" and #read.materials > 0, "ReadVoxels")

COMPAT_OK = true
