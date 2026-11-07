-- Salvataggio con tentativi e UpdateAsync.
local DSS = game:GetService("DataStoreService")
local store = DSS:GetDataStore("CompatRetry")
local function retry(fn, tries)
	for i = 1, tries do
		local ok, res = pcall(fn)
		if ok then return true, res end
		task.wait(0.05)
	end
	return false
end
store:SetAsync("visite", 0)
for i = 1, 3 do
	local ok = retry(function() return store:UpdateAsync("visite", function(old) return (old or 0) + 1 end) end, 3)
	assert(ok, "UpdateAsync")
end
assert(store:GetAsync("visite") == 3, "visite: " .. tostring(store:GetAsync("visite")))
assert(store:IncrementAsync("visite", 2) == 5, "IncrementAsync")
COMPAT_OK = true
