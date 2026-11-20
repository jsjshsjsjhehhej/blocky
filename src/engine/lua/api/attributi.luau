-- Attributi su tutti gli oggetti del motore che non li hanno gia'
-- (workspace, Lighting, i servizi...): SetAttribute, GetAttribute,
-- GetAttributes, GetAttributeChangedSignal, AttributeChanged.
-- Le part e le Instance Luau hanno i loro e non passano di qui.
-- I valori stanno in tabelle "deboli" a parte: funziona anche sulle tabelle
-- del motore in sola lettura (workspace, game).
-- Trovato con il progetto dead-rails: workspace:SetAttribute("Night", ...).

local Internal = ...

Internal.modulo("attributi", function(I)
	local deboli = { __mode = "k" }
	local valori = setmetatable({}, deboli)     -- oggetto -> { nome = valore }
	local segnali = setmetatable({}, deboli)    -- oggetto -> { nome = Signal }
	local cambiati = setmetatable({}, deboli)   -- oggetto -> Signal (AttributeChanged)

	local function cambiatoDi(obj)
		local s = cambiati[obj]
		if not s then
			s = I.Signal.new()
			cambiati[obj] = s
		end
		return s
	end

	local f = I.realFuncs.Instance or {}
	I.realFuncs.Instance = f
	f.SetAttribute = function(self, nome, v)
		if type(nome) ~= "string" then error("SetAttribute: il nome deve essere un testo", 2) end
		local t = valori[self]
		if not t then
			t = {}
			valori[self] = t
		end
		if t[nome] == v then return end
		t[nome] = v
		local s = segnali[self] and segnali[self][nome]
		if s then s:Fire() end
		local c = cambiati[self]
		if c then c:Fire(nome) end
	end
	f.GetAttribute = function(self, nome)
		local t = valori[self]
		return t and t[nome]
	end
	f.GetAttributes = function(self)
		local out = {}
		for k, v in pairs(valori[self] or {}) do out[k] = v end
		return out
	end
	f.GetAttributeChangedSignal = function(self, nome)
		local t = segnali[self]
		if not t then
			t = {}
			segnali[self] = t
		end
		if not t[nome] then t[nome] = I.Signal.new() end
		return t[nome]
	end

	local p = I.realProps.Instance or {}
	I.realProps.Instance = p
	p.AttributeChanged = function(self) return cambiatoDi(self) end
end)
