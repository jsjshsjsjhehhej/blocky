-- part.Changed e GetPropertyChangedSignal su una part vera.
local p = Instance.new("Part")
p.Parent = workspace
local changedProps = {}
p.Changed:Connect(function(prop) changedProps[prop] = true end)
local transp = 0
p:GetPropertyChangedSignal("Transparency"):Connect(function() transp += 1 end)
p.Transparency = 0.5
p.Name = "Vetro"
task.delay(0.1, function()
	assert(transp == 1, "GetPropertyChangedSignal: " .. transp)
	assert(changedProps.Transparency and changedProps.Name, "Changed")
	COMPAT_OK = true
end)
