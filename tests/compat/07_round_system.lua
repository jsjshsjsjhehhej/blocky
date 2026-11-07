-- Sistema a round con task.wait dentro task.spawn.
local status = Instance.new("StringValue")
status.Name = "Status"
status.Parent = game:GetService("ReplicatedStorage")
local changes = 0
status.Changed:Connect(function() changes += 1 end)

task.spawn(function()
	for round = 1, 2 do
		status.Value = "Intermezzo " .. round
		task.wait(0.2)
		status.Value = "Round " .. round
		task.wait(0.2)
	end
	assert(changes >= 4, "Changed doveva scattare 4 volte, scattato " .. changes)
	COMPAT_OK = true
end)
