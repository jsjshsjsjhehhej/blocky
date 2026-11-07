-- Negozio: il client chiede il prezzo con InvokeServer.
local RS = game:GetService("ReplicatedStorage")
local getPrice = Instance.new("RemoteFunction")
getPrice.Name = "GetPrice"
getPrice.Parent = RS
local prices = { Spada = 100, Scudo = 80 }
getPrice.OnServerInvoke = function(player, item) return prices[item] end
local ok, price = pcall(function() return getPrice:InvokeServer("Spada") end)
assert(ok, tostring(price))
assert(price == 100, "InvokeServer: " .. tostring(price))
COMPAT_OK = true
