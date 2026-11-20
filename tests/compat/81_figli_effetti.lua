-- Il + e Instance.new mettono lo stesso figlio: lo script lo trova per nome.
local p = Instance.new("Part")
p.Name = "PortaEffetti"
p.Parent = workspace

local fire = Instance.new("Fire", p)
assert(fire and fire.ClassName == "Fire", "Fire creato")
local found = p:FindFirstChild("Fire")
assert(found and found.ClassName == "Fire", "FindFirstChild Fire")
assert(p:FindFirstChildOfClass("Fire").ClassName == "Fire", "OfClass Fire")

local smoke = Instance.new("Smoke", p)
assert(p:FindFirstChild("Smoke").ClassName == "Smoke", "FindFirstChild Smoke")

local att = Instance.new("Attachment", p)
assert(p:FindFirstChild("Attachment").ClassName == "Attachment", "FindFirstChild Attachment")

local trail = Instance.new("Trail", p)
assert(trail and p:FindFirstChild("Trail").ClassName == "Trail", "FindFirstChild Trail")

local click = Instance.new("ClickDetector", p)
assert(click and p:FindFirstChild("ClickDetector").ClassName == "ClickDetector", "FindFirstChild ClickDetector")
assert(p:FindFirstChildOfClass("ClickDetector").MouseClick ~= nil, "MouseClick")

local saw = { Fire = false, Smoke = false, Attachment = false, Trail = false, ClickDetector = false }
for _, c in ipairs(p:GetChildren()) do
    if saw[c.ClassName] ~= nil then saw[c.ClassName] = true end
end
assert(saw.Fire and saw.Smoke and saw.Attachment and saw.Trail and saw.ClickDetector, "GetChildren")

local desc = p:GetDescendants()
local nFire = 0
for _, c in ipairs(desc) do
    if c.ClassName == "Fire" then nFire = nFire + 1 end
end
assert(nFire >= 1, "GetDescendants")

COMPAT_OK = true
