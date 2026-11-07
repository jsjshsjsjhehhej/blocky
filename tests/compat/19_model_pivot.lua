-- Model con PivotTo / GetPivot.
local model = Instance.new("Model")
model.Name = "Car"
local body = Instance.new("Part")
body.Name = "Body"
body.Size = Vector3.new(4, 1, 8)
body.Position = Vector3.new(0, 1, 0)
body.Parent = model
model.PrimaryPart = body
model.Parent = workspace
model:PivotTo(CFrame.new(20, 1, 20))
local pivot = model:GetPivot()
assert((pivot.Position - Vector3.new(20, 1, 20)).Magnitude < 0.1, "PivotTo: " .. tostring(pivot.Position))
COMPAT_OK = true
