-- RemoteEvent e RemoteFunction in ReplicatedStorage.
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local remote = Instance.new("RemoteEvent")
remote.Name = "BuyItem"
remote.Parent = ReplicatedStorage

local fn = Instance.new("RemoteFunction")
fn.Name = "GetPrice"
fn.Parent = ReplicatedStorage

remote.OnServerEvent:Connect(function(player, item) LAST_ITEM = item end)
fn.OnServerInvoke = function(player, item) return 100 end

assert(ReplicatedStorage:FindFirstChild("BuyItem"), "RemoteEvent in ReplicatedStorage")
assert(ReplicatedStorage:FindFirstChild("GetPrice"), "RemoteFunction")
COMPAT_OK = true
