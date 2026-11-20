-- Memoria di partita, catalogo locale, voce.
-- La voce non apre il microfono: dice solo che qui e' spenta.
-- Il catalogo e' quello del dispositivo, non il sito di Roblox.

local Internal = ...

Internal.modulo("servizi", function(I)
	local extra = I.extraServices
	local mappe = {}
	local code = {}

	local function scaduto(e)
		return e and e.exp and e.exp < os.clock()
	end

	local function hashmap(nome)
		mappe[nome] = mappe[nome] or {}
		local box = mappe[nome]
		local api = {}
		function api:SetAsync(key, value, ttl)
			box[tostring(key)] = { value = value, exp = os.clock() + (tonumber(ttl) or 60) }
			return true
		end
		function api:GetAsync(key)
			local e = box[tostring(key)]
			if not e or scaduto(e) then
				box[tostring(key)] = nil
				return nil
			end
			return e.value
		end
		function api:RemoveAsync(key)
			box[tostring(key)] = nil
			return true
		end
		function api:UpdateAsync(key, fn, ttl)
			local now = api:GetAsync(key)
			local nextv = fn(now)
			api:SetAsync(key, nextv, ttl)
			return nextv
		end
		function api:GetRangeAsync(minK, maxK, limite)
			local out = {}
			minK, maxK = tostring(minK or ""), tostring(maxK or "\255")
			for k, e in pairs(box) do
				if not scaduto(e) and k >= minK and k <= maxK then
					out[#out + 1] = { key = k, value = e.value }
				end
			end
			table.sort(out, function(a, b) return a.key < b.key end)
			limite = tonumber(limite) or #out
			while #out > limite do out[#out] = nil end
			return out
		end
		return api
	end

	local function coda(nome)
		code[nome] = code[nome] or {}
		local q = code[nome]
		local api = {}
		function api:AddAsync(value, ttl)
			q[#q + 1] = { value = value, exp = os.clock() + (tonumber(ttl) or 60), id = tostring(#q + 1) .. ":" .. tostring(os.clock()) }
			return true
		end
		function api:ReadAsync(count)
			count = tonumber(count) or 1
			local out = {}
			local i = 1
			while i <= #q do
				if scaduto(q[i]) then
					table.remove(q, i)
				else
					if #out < count then out[#out + 1] = q[i].value end
					i = i + 1
				end
			end
			return out
		end
		function api:RemoveAsync(value)
			for i = 1, #q do
				if q[i].value == value then
					table.remove(q, i)
					return true
				end
			end
			return false
		end
		return api
	end

	extra.MemoryStoreService = {
		Name = "MemoryStoreService",
		ClassName = "MemoryStoreService",
		GetHashMap = function(_, nome) return hashmap(tostring(nome)) end,
		GetSortedMap = function(_, nome) return hashmap("sort:" .. tostring(nome)) end,
		GetQueue = function(_, nome) return coda(tostring(nome)) end,
	}

	-- Nessun microfono: un gioco puo' chiederlo senza errore.
	extra.VoiceChatService = {
		Name = "VoiceChatService",
		ClassName = "VoiceChatService",
		IsVoiceEnabledForUserIdAsync = function() return false end,
		GetChatGroupsAsync = function() return {} end,
	}

	local catalogo = {
		{ Name = "Blocky", Id = 1, AssetType = "Avatar", Price = 0 },
	}
	extra.AvatarEditorService = {
		Name = "AvatarEditorService",
		ClassName = "AvatarEditorService",
		SearchCatalog = function()
			return { GetCurrentPage = function() return catalogo end, IsFinished = true }
		end,
		SearchCatalogAsync = function()
			return { GetCurrentPage = function() return catalogo end, IsFinished = true }
		end,
		GetItemDetailsAsync = function(_, id)
			for _, it in ipairs(catalogo) do
				if it.Id == id or it.Name == id then return it end
			end
			return nil
		end,
	}
end)
