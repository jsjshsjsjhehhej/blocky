-- ChildAdded / DescendantAdded sugli oggetti del motore (workspace, i servizi).
-- Il motore non li ha: prima workspace.DescendantAdded era nil e :Connect
-- fermava lo script. I figli Luau e le Part li fanno scattare quando
-- cambia il Parent.

local Internal = ...

Internal.modulo("discendenti", function(I)
	local deboli = { __mode = "k" }
	local segnali = setmetatable({}, deboli)

	local function segnale(obj, nome)
		local t = segnali[obj]
		if not t then
			t = {}
			segnali[obj] = t
		end
		if not t[nome] then t[nome] = I.Signal.new() end
		return t[nome]
	end

	local function spara(obj, nome, ...)
		local t = type(obj) == "table" and segnali[obj]
		local s = t and t[nome]
		if s then s:Fire(...) end
	end

	local function padreDi(obj)
		if I.isLuaInstance(obj) then
			local s = I.store[obj]
			return s and s.data.Parent
		end
		if type(obj) ~= "table" then return nil end
		local ok, p = pcall(function() return obj.Parent end)
		if ok then return p end
		return nil
	end

	local function aggiunto(child, parent)
		if type(parent) ~= "table" then return end
		if not I.isLuaInstance(parent) then spara(parent, "ChildAdded", child) end
		local p = parent
		local visto = {}
		for _ = 1, 32 do
			if type(p) ~= "table" or visto[p] then break end
			visto[p] = true
			if not I.isLuaInstance(p) then spara(p, "DescendantAdded", child) end
			p = padreDi(p)
		end
	end

	local prima = Internal.apiEngineIndex
	function Internal.apiEngineIndex(obj, k, cls)
		local v = prima(obj, k, cls)
		if v ~= nil then return v end
		if k == "ChildAdded" or k == "ChildRemoved" or k == "DescendantAdded"
			or k == "DescendantRemoving" or k == "AncestryChanged" then
			return segnale(obj, k)
		end
		return nil
	end

	local primaPart = Internal.partParented
	function Internal.partParented(part, parent)
		if primaPart then primaPart(part, parent) end
		if parent ~= nil then aggiunto(part, parent) end
	end

	local primaParent = Internal.onParentChanged
	Internal.onParentChanged = function(self)
		if primaParent then primaParent(self) end
		local s = I.store[self]
		local parent = s and s.data.Parent
		if parent ~= nil then aggiunto(self, parent) end
	end
end)
