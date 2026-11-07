-- Squadre con Teams e player.Team.
local Teams = game:GetService("Teams")
local red = Instance.new("Team")
red.Name = "Rossi"
red.TeamColor = BrickColor.new("Bright red")
red.AutoAssignable = true
red.Parent = Teams
local blue = Instance.new("Team")
blue.Name = "Blu"
blue.TeamColor = BrickColor.new("Bright blue")
blue.Parent = Teams
assert(#Teams:GetTeams() >= 2 or #Teams:GetChildren() >= 2, "GetTeams")
local player = game:GetService("Players").LocalPlayer
player.Team = red
assert(player.Team == red, "player.Team")
assert(Teams:FindFirstChild("Blu") == blue)
COMPAT_OK = true
