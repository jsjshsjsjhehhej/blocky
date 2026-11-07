-- Attributi sulle part (al posto dei Value).
local p = Instance.new("Part")
p.Parent = workspace
p:SetAttribute("Health", 100)
p:SetAttribute("Team", "Red")
local fired = 0
p:GetAttributeChangedSignal("Health"):Connect(function() fired += 1 end)
p:SetAttribute("Health", 80)
assert(p:GetAttribute("Health") == 80 and p:GetAttribute("Team") == "Red")
task.delay(0.1, function()
	assert(fired == 1, "GetAttributeChangedSignal: " .. fired)
	COMPAT_OK = true
end)
