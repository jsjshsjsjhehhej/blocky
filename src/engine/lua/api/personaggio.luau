-- Player:LoadCharacter, come su Roblox.
-- Il personaggio c'e' gia' (il motore lo crea sempre): lo rimette in vita,
-- lo sposta sul RespawnLocation e fa scattare CharacterAdded.
-- Il segnale del giocatore locale e' del motore e non ha Fire: ci si
-- aggancia a Connect e si richiamano da qui.

local Internal = ...

Internal.modulo("personaggio", function(I)
	local ascolti = setmetatable({}, { __mode = "k" })

	local function aggancia(player)
		if type(player) ~= "table" or ascolti[player] then return end
		local sig = rawget(player, "CharacterAdded")
		if type(sig) ~= "table" or type(sig.Connect) ~= "function" or sig.Fire then return end
		local lista = {}
		ascolti[player] = lista
		local orig = sig.Connect
		sig.Connect = function(self, fn)
			if type(fn) == "function" then table.insert(lista, fn) end
			return orig(self, fn)
		end
	end

	pcall(function() aggancia(game:GetService("Players").LocalPlayer) end)

	local function piediSu(part)
		if type(part) ~= "table" then return nil end
		local pos = part.Position
		if type(pos) ~= "table" or pos.X == nil then
			local ok, cf = pcall(function() return part.CFrame end)
			pos = ok and type(cf) == "table" and cf.Position or nil
		end
		if type(pos) ~= "table" or pos.X == nil then return nil end
		local y = pos.Y
		local ok, size = pcall(function() return part.Size end)
		if ok and type(size) == "table" and type(size.Y) == "number" then
			y = pos.Y + size.Y * 0.5 + 0.15
		end
		return Vector3.new(pos.X, y, pos.Z)
	end

	local function sposta(char, dove)
		if type(char) ~= "table" or not dove then return end
		local root = char:FindFirstChild("HumanoidRootPart") or char.HumanoidRootPart
		if type(root) == "table" then
			pcall(function() root.CFrame = CFrame.new(dove) end)
		end
		local hum = char:FindFirstChild("Humanoid") or char.Humanoid
		if type(hum) ~= "table" then return end
		pcall(function() hum:SetPosition(dove) end)
		pcall(function()
			local maxh = hum.MaxHealth
			if type(maxh) ~= "number" or maxh <= 0 then maxh = 100 end
			hum.Health = maxh
		end)
	end

	local function avvisa(player, char)
		local sig = player.CharacterAdded
		if type(sig) == "table" and type(sig.Fire) == "function" then
			sig:Fire(char)
			return
		end
		local lista = ascolti[player]
		if not lista then return end
		for _, fn in ipairs(lista) do fn(char) end
	end

	local f = I.realFuncs.Player or {}
	I.realFuncs.Player = f
	f.LoadCharacter = function(player)
		aggancia(player)
		local char = player.Character
		if char == nil and I.isLuaInstance(player) then
			local s = I.store[player]
			char = s and s.data.Character
		end
		if char == nil then error("LoadCharacter: il giocatore non ha un Character", 2) end
		sposta(char, piediSu(player.RespawnLocation))
		avvisa(player, char)
		return char
	end

	f.DistanceFromCharacter = function(self, pos)
		if type(pos) ~= "table" then return 0 end
		local char = self.Character
		local root = char and (char:FindFirstChild("HumanoidRootPart") or char.HumanoidRootPart)
		local p = root and root.Position
		if type(p) ~= "table" then return 0 end
		local dx, dy, dz = (p.X or 0) - (pos.X or 0), (p.Y or 0) - (pos.Y or 0), (p.Z or 0) - (pos.Z or 0)
		return math.sqrt(dx * dx + dy * dy + dz * dz)
	end

	-- In un gioco da solo non chiude l'app: lo script non si ferma.
	f.Kick = function(self, motivo)
		print("[Kick] " .. tostring(self.Name or "?") .. ": " .. tostring(motivo or ""))
		return true
	end
end)
