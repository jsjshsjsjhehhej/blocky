-- ModuleScript con Source e require.
local RS = game:GetService("ReplicatedStorage")
local mod = Instance.new("ModuleScript")
mod.Name = "Config"
mod.Source = "local M = {} M.MaxPlayers = 12 function M.double(x) return x * 2 end return M"
mod.Parent = RS
local Config = require(mod)
assert(Config.MaxPlayers == 12 and Config.double(21) == 42, "require ModuleScript")
local again = require(RS:FindFirstChild("Config"))
assert(again == Config, "require deve restituire la stessa tabella")
COMPAT_OK = true
