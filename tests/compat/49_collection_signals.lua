-- CollectionService: segnali di tag aggiunto/tolto.
local CS = game:GetService("CollectionService")
local added, removed = 0, 0
CS:GetInstanceAddedSignal("Moneta"):Connect(function(inst) added += 1 end)
CS:GetInstanceRemovedSignal("Moneta"):Connect(function(inst) removed += 1 end)
local p = Instance.new("Part")
p.Parent = workspace
CS:AddTag(p, "Moneta")
assert(#CS:GetTags(p) == 1 and CS:GetTags(p)[1] == "Moneta", "GetTags")
CS:RemoveTag(p, "Moneta")
task.delay(0.05, function()
	assert(added == 1 and removed == 1, "segnali tag: " .. added .. "/" .. removed)
	COMPAT_OK = true
end)
