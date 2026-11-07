-- DataStore con pcall, come si fa per non perdere i salvataggi.
local DataStoreService = game:GetService("DataStoreService")
local store = DataStoreService:GetDataStore("CompatTest")
local key = "Player_" .. tostring(game.Players.LocalPlayer.UserId)
local ok, err = pcall(function() store:SetAsync(key, { Coins = 50, Level = 3 }) end)
assert(ok, tostring(err))
local ok2, data = pcall(function() return store:GetAsync(key) end)
assert(ok2 and type(data) == "table" and data.Coins == 50, "GetAsync: " .. tostring(data))
local ok3, newValue = pcall(function()
	return store:UpdateAsync(key, function(old) old = old or {} old.Coins = (old.Coins or 0) + 1 return old end)
end)
assert(ok3, tostring(newValue))
COMPAT_OK = true
