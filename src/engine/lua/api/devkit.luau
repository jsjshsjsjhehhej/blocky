-- Kit da sviluppo: controlli di tipo, albero delle Instance, log, benchmark.
-- game:GetService("Dev") — non e' un servizio di Roblox, e' di Blocky.

local Internal = ...

Internal.modulo("devkit", function(I)
	local Dev = {}
	Dev.ClassName = "Dev"
	Dev.Name = "Dev"

	local tracce = {}
	local tracciaMax = 80
	local avvisi = {}
	local profili = {}

	local function tipoDi(v)
		local t = typeof(v)
		if t == "Instance" or (type(v) == "table" and type(rawget(v, "ClassName")) == "string") then
			return "Instance"
		end
		return t
	end

	function Dev:Tipo(v)
		return tipoDi(v)
	end

	function Dev:Assert(cond, msg)
		if not cond then error(msg or "Dev:Assert fallita", 2) end
		return cond
	end

	function Dev:Expect(v, atteso, msg)
		local ha = tipoDi(v)
		if ha ~= atteso then
			error((msg or "Dev:Expect") .. ": atteso " .. tostring(atteso) .. ", arrivato " .. ha, 2)
		end
		return v
	end

	local function conforme(v, spec)
		if spec == "any" then return true end
		if type(spec) == "string" then
			local opt = string.sub(spec, -1) == "?"
			local base = opt and string.sub(spec, 1, -2) or spec
			if v == nil then return opt end
			if base == "Instance" then return tipoDi(v) == "Instance" end
			if base == "integer" then return type(v) == "number" and v % 1 == 0 end
			return tipoDi(v) == base or type(v) == base
		end
		if type(spec) ~= "table" then return false end
		if type(v) ~= "table" then return false, "attesa una tabella" end
		for k, s in pairs(spec) do
			local ok, err = conforme(v[k], s)
			if not ok then return false, tostring(k) .. ": " .. tostring(err or "tipo") end
		end
		return true
	end

	function Dev:Check(v, spec)
		local ok, err = conforme(v, spec)
		if ok then return true end
		return false, err or "schema"
	end

	function Dev:Clone(v, visto)
		visto = visto or {}
		if type(v) ~= "table" then return v end
		if visto[v] then return visto[v] end
		local out = {}
		visto[v] = out
		for k, x in pairs(v) do
			out[Dev:Clone(k, visto)] = Dev:Clone(x, visto)
		end
		return out
	end

	function Dev:Uguali(a, b, visto)
		if a == b then return true end
		if type(a) ~= "table" or type(b) ~= "table" then return false end
		visto = visto or {}
		if visto[a] then return visto[a] == b end
		visto[a] = b
		for k, x in pairs(a) do
			if not Dev:Uguali(x, b[k], visto) then return false end
		end
		for k in pairs(b) do
			if a[k] == nil then return false end
		end
		return true
	end

	function Dev:Diff(prima, dopo)
		local aggiunti, tolti, cambiati = {}, {}, {}
		prima, dopo = prima or {}, dopo or {}
		for k, v in pairs(dopo) do
			if prima[k] == nil then
				aggiunti[k] = v
			elseif not Dev:Uguali(prima[k], v) then
				cambiati[k] = { prima = prima[k], dopo = v }
			end
		end
		for k, v in pairs(prima) do
			if dopo[k] == nil then tolti[k] = v end
		end
		return { aggiunti = aggiunti, tolti = tolti, cambiati = cambiati }
	end

	local function figliDi(obj)
		if type(obj) ~= "table" then return {} end
		local ok, list = pcall(function()
			if type(obj.GetChildren) == "function" then return obj:GetChildren() end
			return nil
		end)
		if ok and type(list) == "table" then return list end
		return {}
	end

	local function nomeDi(obj)
		if type(obj) ~= "table" then return tostring(obj) end
		local ok, n = pcall(function() return obj.Name end)
		if ok and n ~= nil then return tostring(n) end
		return "?"
	end

	local function classeDi(obj)
		if type(obj) ~= "table" then return typeof(obj) end
		local ok, c = pcall(function() return obj.ClassName end)
		if ok and c ~= nil then return tostring(c) end
		return "table"
	end

	function Dev:Dump(obj, profondita)
		profondita = profondita or 2
		local righe = {}
		local function visita(o, liv, visto)
			if liv > profondita then return end
			if type(o) == "table" then
				if visto[o] then
					table.insert(righe, string.rep("  ", liv) .. nomeDi(o) .. " (gia' visto)")
					return
				end
				visto[o] = true
			end
			table.insert(righe, string.rep("  ", liv) .. nomeDi(o) .. " [" .. classeDi(o) .. "]")
			for _, c in ipairs(figliDi(o)) do
				visita(c, liv + 1, visto)
			end
		end
		visita(obj, 0, {})
		return table.concat(righe, "\n")
	end

	function Dev:Path(radice, percorso)
		if type(percorso) ~= "string" or percorso == "" then return radice end
		local cur = radice
		for pezzo in string.gmatch(percorso, "[^/%.]+") do
			if type(cur) ~= "table" or type(cur.FindFirstChild) ~= "function" then return nil end
			local ok, nxt = pcall(function() return cur:FindFirstChild(pezzo) end)
			if not ok then return nil end
			cur = nxt
			if cur == nil then return nil end
		end
		return cur
	end

	function Dev:Find(radice, pred, limite)
		local out = {}
		limite = limite or 200
		local function visita(o, visto)
			if #out >= limite then return end
			if type(o) == "table" then
				if visto[o] then return end
				visto[o] = true
			end
			local ok, si = pcall(pred, o)
			if ok and si then table.insert(out, o) end
			for _, c in ipairs(figliDi(o)) do visita(c, visto) end
		end
		visita(radice, {})
		return out
	end

	function Dev:Count(radice, classe)
		local n = 0
		Dev:Find(radice, function(o)
			if classeDi(o) == classe then n += 1 end
			return false
		end, 5000)
		return n
	end

	function Dev:Benchmark(nome, fn, volte)
		volte = volte or 1
		if volte < 1 then volte = 1 end
		local t0 = os.clock()
		for _ = 1, volte do fn() end
		local ms = (os.clock() - t0) * 1000
		Dev:Trace(string.format("bench %s x%d = %.3f ms", tostring(nome), volte, ms))
		return ms
	end

	function Dev:Trace(...)
		local parti = {}
		for i = 1, select("#", ...) do
			parti[i] = tostring(select(i, ...))
		end
		local riga = table.concat(parti, " ")
		table.insert(tracce, riga)
		if #tracce > tracciaMax then table.remove(tracce, 1) end
		return riga
	end

	function Dev:Lines()
		local copia = {}
		for i, r in ipairs(tracce) do copia[i] = r end
		return copia
	end

	function Dev:ClearLines()
		table.clear(tracce)
	end

	function Dev:WarnOnce(chiave, msg)
		if avvisi[chiave] then return false end
		avvisi[chiave] = true
		warn("[Dev] " .. tostring(msg or chiave))
		Dev:Trace("warn", chiave, msg)
		return true
	end

	function Dev:Profile(nome)
		local p = { nome = nome, t0 = os.clock(), spent = 0, aperto = true }
		profili[nome] = p
		return p
	end

	function Dev:ProfileEnd(nome)
		local p = profili[nome]
		if not p or not p.aperto then return 0 end
		p.spent += os.clock() - p.t0
		p.aperto = false
		return p.spent * 1000
	end

	function Dev:ProfileRead(nome)
		local p = profili[nome]
		if not p then return 0 end
		local extra = p.aperto and (os.clock() - p.t0) or 0
		return (p.spent + extra) * 1000
	end

	function Dev:Media(lista)
		if type(lista) ~= "table" or #lista == 0 then return 0, 0, 0 end
		local s, lo, hi = 0, lista[1], lista[1]
		for _, n in ipairs(lista) do
			s += n
			if n < lo then lo = n end
			if n > hi then hi = n end
		end
		return s / #lista, lo, hi
	end

	function Dev:FormattaNumero(n, decimali)
		decimali = decimali or 0
		local mul = 10 ^ decimali
		local v = math.floor(n * mul + 0.5) / mul
		return string.format("%." .. decimali .. "f", v)
	end

	function Dev:FormattaTempo(secondi)
		secondi = math.max(0, secondi or 0)
		local h = math.floor(secondi / 3600)
		local m = math.floor(secondi / 60) % 60
		local s = math.floor(secondi) % 60
		if h > 0 then return string.format("%d:%02d:%02d", h, m, s) end
		return string.format("%d:%02d", m, s)
	end

	function Dev:FormattaByte(n)
		n = n or 0
		if n < 1024 then return tostring(math.floor(n)) .. " B" end
		if n < 1024 * 1024 then return Dev:FormattaNumero(n / 1024, 1) .. " KB" end
		return Dev:FormattaNumero(n / (1024 * 1024), 2) .. " MB"
	end

	function Dev:Retry(volte, fn)
		local ultimo
		for i = 1, math.max(1, volte) do
			local ok, a, b, c = pcall(fn, i)
			if ok then return true, a, b, c end
			ultimo = a
		end
		return false, ultimo
	end

	function Dev:Watch(obj, prop)
		local sig = I.Signal.new()
		local ok0, last = pcall(function() return obj[prop] end)
		if not ok0 then last = nil end
		local conn
		pcall(function()
			if type(obj.GetPropertyChangedSignal) == "function" then
				local ev = obj:GetPropertyChangedSignal(prop)
				if ev and type(ev.Connect) == "function" then
					conn = ev:Connect(function()
						local ok, v = pcall(function() return obj[prop] end)
						if not ok then return end
						local prima = last
						last = v
						sig:Fire(v, prima)
					end)
				end
			end
		end)
		local w = {}
		function w:Poll()
			local ok, v = pcall(function() return obj[prop] end)
			if not ok then return last end
			if v ~= last then
				local prima = last
				last = v
				sig:Fire(v, prima)
			end
			return v
		end
		function w:Disconnect()
			if conn and conn.Disconnect then conn:Disconnect() end
			conn = nil
		end
		w.Signal = sig
		return w
	end

	local function nuovaMedia()
		local o = { n = 0, s = 0, finestra = {}, cap = 16 }
		function o:Push(x)
			self.n += 1
			self.s += x
			table.insert(self.finestra, x)
			if #self.finestra > self.cap then
				self.s -= table.remove(self.finestra, 1)
			end
		end
		function o:Media()
			if #self.finestra == 0 then return 0 end
			local t = 0
			for _, v in ipairs(self.finestra) do t += v end
			return t / #self.finestra
		end
		function o:Totale()
			return self.n
		end
		return o
	end

	function Dev:MediaMobile(cap)
		local m = nuovaMedia()
		m.cap = cap or 16
		return m
	end

	function Dev:Istogramma(lista, secchi, lo, hi)
		secchi = secchi or 4
		if secchi < 1 then secchi = 1 end
		if lo == nil or hi == nil then
			lo, hi = lista[1] or 0, lista[1] or 0
			for _, n in ipairs(lista) do
				if n < lo then lo = n end
				if n > hi then hi = n end
			end
		end
		if hi <= lo then hi = lo + 1 end
		local conti = table.create(secchi, 0)
		local passo = (hi - lo) / secchi
		for _, n in ipairs(lista) do
			local i = math.floor((n - lo) / passo) + 1
			if i < 1 then i = 1 end
			if i > secchi then i = secchi end
			conti[i] += 1
		end
		return conti, lo, hi
	end

	I.extraServices.Dev = Dev
	I.Dev = Dev
end)
