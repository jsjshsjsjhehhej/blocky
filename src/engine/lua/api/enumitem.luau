-- Enum.Material.Wood e Enum.KeyCode.F erano un testo e un numero.
-- Diventano EnumItem: .Name, .Value, typeof "EnumItem", e == resta vero.
-- Gli Enum gia' costruiti dal dump (EasingStyle...) non si toccano.

local Internal = ...

Internal.modulo("enumitem", function()
	local itemMT = {
		__metatable = "EnumItem",
		__tostring = function(it)
			return "Enum." .. tostring(it.EnumType) .. "." .. tostring(it.Name)
		end,
	}

	local function enumMT(e, nome)
		return {
			__metatable = "Enum",
			__tostring = function() return nome end,
			__index = function(_, k)
				if k == "GetEnumItems" then
					return function()
						local lista = {}
						for chiave, it in pairs(e) do
							if type(chiave) == "string" and type(it) == "table" and it.Name then
								table.insert(lista, it)
							end
						end
						table.sort(lista, function(a, b)
							return (tonumber(a.Value) or 0) < (tonumber(b.Value) or 0)
						end)
						return lista
					end
				end
				if k == "FromName" then
					return function(_, n) return rawget(e, n) end
				end
				if k == "FromValue" then
					return function(_, v)
						for _, it in pairs(e) do
							if type(it) == "table" and it.Value == v then return it end
						end
						return nil
					end
				end
				return nil
			end,
		}
	end

	local function promuovi(e, nome)
		if type(e) ~= "table" or getmetatable(e) == "Enum" then return end
		local chiavi, n = {}, 0
		for k in pairs(e) do
			if type(k) == "string" then table.insert(chiavi, k) end
		end
		for _, k in ipairs(chiavi) do
			local v = rawget(e, k)
			if type(v) == "number" then
				rawset(e, k, setmetatable({ Name = k, Value = v, EnumType = nome }, itemMT))
			elseif type(v) == "string" then
				rawset(e, k, setmetatable({ Name = k, Value = n, EnumType = nome }, itemMT))
				n += 1
			elseif type(v) == "table" and type(rawget(v, "Name")) == "string" and getmetatable(v) ~= "EnumItem" then
				if rawget(v, "EnumType") == nil then rawset(v, "EnumType", nome) end
				setmetatable(v, itemMT)
			end
		end
		pcall(setmetatable, e, enumMT(e, nome))
	end

	if type(Enum) ~= "table" then return end
	local nomi = {}
	for nome, e in pairs(Enum) do
		if type(nome) == "string" then table.insert(nomi, nome) end
	end
	for _, nome in ipairs(nomi) do
		promuovi(rawget(Enum, nome), nome)
	end

	if rawget(Enum, "AnimationPriority") == nil then
		local e = {}
		local voci = { "Core", "Idle", "Movement", "Action", "Action2", "Action3", "Action4" }
		for i, nome in ipairs(voci) do
			e[nome] = setmetatable({ Name = nome, Value = i - 1, EnumType = "AnimationPriority" }, itemMT)
		end
		setmetatable(e, enumMT(e, "AnimationPriority"))
		pcall(function() Enum.AnimationPriority = e end)
	end
end)
