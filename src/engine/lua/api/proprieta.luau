-- Instance:GetPropertyChangedSignal sugli oggetti del motore
-- (workspace, Camera...): prima il metodo non c'era e lo script si fermava.
-- Le Part e le Instance Luau hanno gia' il loro. Qui, se la proprieta'
-- letta cambia, il segnale scatta (al massimo una volta a frame).

local Internal = ...

Internal.modulo("proprieta", function(I)
	local deboli = { __mode = "k" }
	local segnali = setmetatable({}, deboli)
	local osservati = {}

	local function firma(v)
		local tv = typeof(v)
		if tv == "Vector3" or tv == "Vector2" then
			return tv .. ":" .. tostring(v.X) .. ":" .. tostring(v.Y) .. ":" .. tostring(v.Z or 0)
		end
		if tv == "Color3" then
			return "c:" .. tostring(v.R) .. ":" .. tostring(v.G) .. ":" .. tostring(v.B)
		end
		if tv == "CFrame" then
			local p = v.Position
			return "cf:" .. tostring(p.X) .. ":" .. tostring(p.Y) .. ":" .. tostring(p.Z)
		end
		if tv == "nil" or tv == "number" or tv == "string" or tv == "boolean" then return v end
		-- tabelle (la Camera, un'istanza): l'identita' basta
		return v
	end

	local function segnale(obj, nome)
		local t = segnali[obj]
		if not t then
			t = {}
			segnali[obj] = t
		end
		local s = t[nome]
		if not s then
			s = I.Signal.new()
			t[nome] = s
			local ok, ora = pcall(function() return obj[nome] end)
			table.insert(osservati, { obj = obj, nome = nome, sig = s, ultimo = ok and firma(ora) or nil, letto = ok })
		end
		return s
	end

	game:GetService("RunService").Heartbeat:Connect(function()
		for i = #osservati, 1, -1 do
			local o = osservati[i]
			if o.obj == nil then
				table.remove(osservati, i)
			else
				local ok, ora = pcall(function() return o.obj[o.nome] end)
				local f = ok and firma(ora) or nil
				if ok and o.letto and f ~= o.ultimo then
					o.ultimo = f
					o.sig:Fire()
				end
				o.letto = ok
				o.ultimo = f
			end
		end
	end)

	local f = I.realFuncs.Instance or {}
	I.realFuncs.Instance = f
	f.GetPropertyChangedSignal = function(self, nome)
		if type(nome) ~= "string" then error("GetPropertyChangedSignal: il nome deve essere un testo", 2) end
		return segnale(self, nome)
	end
end)
