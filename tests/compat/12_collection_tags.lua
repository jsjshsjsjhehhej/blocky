-- CollectionService: tutte le part con il tag Lava.
local CollectionService = game:GetService("CollectionService")
for i = 1, 3 do
	local p = Instance.new("Part")
	p.Name = "Lava" .. i
	p.Anchored = true
	p.Position = Vector3.new(i * 5, 0.5, 40)
	p.Parent = workspace
	CollectionService:AddTag(p, "Lava")
end
local tagged = CollectionService:GetTagged("Lava")
assert(#tagged == 3, "GetTagged: " .. #tagged)
assert(CollectionService:HasTag(tagged[1], "Lava"))
for _, p in ipairs(tagged) do
	p.Touched:Connect(function() end)
end
COMPAT_OK = true
