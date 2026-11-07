-- GetChildren / GetDescendants contano ogni oggetto una volta sola.
local before = #workspace:GetChildren()
local beforeD = #workspace:GetDescendants()
local p = Instance.new("Part")
p.Name = "Unica"
p.Parent = workspace
local f = Instance.new("Folder")
f.Parent = workspace
assert(#workspace:GetChildren() == before + 2, "GetChildren: " .. before .. " -> " .. #workspace:GetChildren())
local n = 0
for _, c in ipairs(workspace:GetChildren()) do if c.Name == "Unica" then n += 1 end end
assert(n == 1, "la part compare " .. n .. " volte")
assert(#workspace:GetDescendants() == beforeD + 2, "GetDescendants: " .. beforeD .. " -> " .. #workspace:GetDescendants())
-- Model Luau con una part dentro: Model + part, una volta ciascuno.
local m = Instance.new("Model")
local q = Instance.new("Part")
q.Name = "DentroAlModel"
q.Parent = m
m.Parent = workspace
local seen = 0
for _, d in ipairs(workspace:GetDescendants()) do if d.Name == "DentroAlModel" then seen += 1 end end
assert(seen == 1, "la part nel Model compare " .. seen .. " volte")
assert(#workspace:GetDescendants() == beforeD + 4, "GetDescendants col Model: " .. #workspace:GetDescendants())
COMPAT_OK = true
