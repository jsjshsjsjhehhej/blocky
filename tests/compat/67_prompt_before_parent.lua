-- Come i raccoglibili: il prompt nasce sulla part PRIMA che lei sia nel mondo.
local p = Instance.new("Part")
p.Anchored = true
p.Position = Vector3.new(12, 1, -4)
p.Size = Vector3.new(2, 2, 2)

local pp = Instance.new("ProximityPrompt")
pp.ActionText = "Pick up"
pp.ObjectText = "Coal"
pp.HoldDuration = 0
pp.MaxActivationDistance = 10
pp.Parent = p

p.Parent = workspace

assert(pp.Parent == p, "il prompt resta sulla part")
assert(pp.ActionText == "Pick up", "ActionText: " .. tostring(pp.ActionText))
assert(pp.ObjectText == "Coal", "ObjectText")
assert(math.abs(pp.MaxActivationDistance - 10) < 1e-4, "distanza")
assert(pp.Enabled == true, "acceso quando la part e' nel mondo")

-- spostarla dopo non deve staccare il prompt
p.Position = Vector3.new(14, 1, -4)
assert(pp.Parent == p, "ancora attaccato dopo lo spostamento")

COMPAT_OK = true
