-- Camera: CFrame, FieldOfView, ViewportSize, WorldToScreenPoint.
local cam = workspace.CurrentCamera
assert(cam, "CurrentCamera")
cam.FieldOfView = 80
assert(cam.FieldOfView == 80)
assert(typeof(cam.ViewportSize) == "Vector2", "ViewportSize")
local cf = cam.CFrame
assert(typeof(cf) == "CFrame", "camera.CFrame")
local screen, onScreen = cam:WorldToScreenPoint(cf.Position + cf.LookVector * 10)
assert(typeof(screen) == "Vector3" and type(onScreen) == "boolean", "WorldToScreenPoint")
local ray = cam:ScreenPointToRay(100, 100)
assert(ray and ray.Direction, "ScreenPointToRay")
COMPAT_OK = true
