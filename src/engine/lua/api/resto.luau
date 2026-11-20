-- Il resto delle chiamate che uno script Roblox fa e che qui erano solo un nome.
-- Niente VR, niente corpo R15, niente acquisti veri, niente rete esterna.
-- I pass di simulazione (PreSimulation, PostSimulation, PreRender, PreAnimation)
-- scattano insieme a Stepped / Heartbeat / RenderStepped: un task.wait dentro
-- è legale, come sugli altri eventi.

local Internal = ...

Internal.modulo("resto", function(I)
	local function fuoco(self, nome, ...)
		local ok, sig = pcall(function() return self[nome] end)
		if ok and type(sig) == "table" and type(sig.Fire) == "function" then
			sig:Fire(...)
		end
	end

	local RS = game:GetService("RunService")
	local function aggancia(nome, sorgente)
		local ok, sig = pcall(function() return RS[nome] end)
		local srcOk, src = pcall(function() return RS[sorgente] end)
		if ok and srcOk and type(sig) == "table" and type(sig.Fire) == "function"
			and type(src) == "table" and type(src.Connect) == "function" then
			src:Connect(function(...) sig:Fire(...) end)
		end
	end
	aggancia("PreSimulation", "Stepped")
	aggancia("PostSimulation", "Heartbeat")
	aggancia("PreAnimation", "Heartbeat")
	aggancia("PreRender", "RenderStepped")

	local chiusure = {}
	rawset(_G, "__blockyBindToClose", chiusure)
	I.realFuncs.DataModel = I.realFuncs.DataModel or {}
	I.realFuncs.DataModel.BindToClose = function(_, fn)
		if type(fn) ~= "function" then error("BindToClose(function)", 2) end
		table.insert(chiusure, fn)
	end

	I.realFuncs.Players = I.realFuncs.Players or {}
	function I.realFuncs.Players:GetUserIdFromNameAsync(name)
		local n = tostring(name or "")
		local h = 0
		for i = 1, #n do h = (h * 33 + string.byte(n, i)) % 1000000000 end
		if h == 0 then h = 1 end
		return h
	end
	function I.realFuncs.Players:GetNameFromUserIdAsync(id)
		local lp = self.LocalPlayer
		if type(lp) == "table" and lp.UserId == id then return lp.Name end
		return "Utente" .. tostring(id)
	end

	I.realFuncs.LocalizationService = {
		GetTranslatorForPlayerAsync = function()
			return {
				Translate = function(_, source) return tostring(source or "") end,
				FormatByKey = function(_, key) return tostring(key or "") end,
			}
		end,
	}

	I.realFuncs.GuiService = {
		IsTenFootInterface = function() return false end,
	}

	I.realFuncs.TextChatService = I.realFuncs.TextChatService or {}
	I.realFuncs.TextChatService.DisplaySystemMessage = function(_, message)
		local testo = tostring(message or "")
		print("[TextChat] " .. testo)
		return testo
	end

	I.realFuncs.BasePart = I.realFuncs.BasePart or {}
	I.realFuncs.BasePart.GetTouchingParts = function(self)
		local ok, list = pcall(function() return workspace:GetPartsInPart(self) end)
		if not ok or type(list) ~= "table" then return {} end
		local out = {}
		for _, p in ipairs(list) do
			if p ~= self then out[#out + 1] = p end
		end
		return out
	end

	-- Explosion: il Parent (anche dopo Instance.new) fa partire raggi e danno.
	local scoppi = setmetatable({}, { __mode = "k" })
	local primaParent = I.onParentChanged
	I.onParentChanged = function(self)
		if primaParent then primaParent(self) end
		local fn = scoppi[self]
		if fn and self.Parent ~= nil then fn() end
	end
	local nuovo = Instance.new
	function Instance.new(cls, parent)
		local inst = nuovo(cls, parent)
		if cls == "Explosion" and type(inst) == "table" then
			local fatto = false
			local function scoppia()
				if fatto then return end
				fatto = true
				local pos = inst.Position
				if typeof(pos) ~= "Vector3" then pos = Vector3.new(0, 5, 0) end
				local raggio = tonumber(inst.BlastRadius) or 8
				pcall(function()
					local n = math.clamp(math.floor(raggio), 4, 24)
					game:GetService("Particles"):Explode(pos, n)
				end)
				fuoco(inst, "Hit", nil, raggio)
			end
			scoppi[inst] = scoppia
			if parent ~= nil then task.defer(scoppia) end
		end
		return inst
	end
end)
