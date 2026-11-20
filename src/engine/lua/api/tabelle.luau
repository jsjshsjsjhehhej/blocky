-- Tabelle, caso, anelli, slot e una pila di comandi annullabili.
-- Metodi su game:GetService("Dev").

local Internal = ...

Internal.modulo("dati", function(I)
	local D = I.Dev or I.extraServices.Dev
	if not D then
		print("[Lua] modulo API dati: Dev non pronto")
		return
	end

	function D:Filter(lista, fn)
		local out = {}
		for i, v in ipairs(lista) do
			if fn(v, i) then table.insert(out, v) end
		end
		return out
	end

	function D:Map(lista, fn)
		local out = {}
		for i, v in ipairs(lista) do
			out[i] = fn(v, i)
		end
		return out
	end

	function D:Reduce(lista, fn, acc)
		local i0 = 1
		if acc == nil then
			acc = lista[1]
			i0 = 2
		end
		for i = i0, #lista do
			acc = fn(acc, lista[i], i)
		end
		return acc
	end

	function D:Group(lista, fn)
		local out = {}
		for i, v in ipairs(lista) do
			local k = fn(v, i)
			local g = out[k]
			if not g then
				g = {}
				out[k] = g
			end
			table.insert(g, v)
		end
		return out
	end

	function D:Unique(lista)
		local visto, out = {}, {}
		for _, v in ipairs(lista) do
			if not visto[v] then
				visto[v] = true
				table.insert(out, v)
			end
		end
		return out
	end

	function D:Zip(a, b)
		local n = math.min(#a, #b)
		local out = table.create(n)
		for i = 1, n do
			out[i] = { a[i], b[i] }
		end
		return out
	end

	function D:Shuffle(lista, rng)
		rng = rng or math.random
		for i = #lista, 2, -1 do
			local j = rng(i)
			lista[i], lista[j] = lista[j], lista[i]
		end
		return lista
	end

	function D:Pick(lista, rng)
		if #lista == 0 then return nil end
		rng = rng or math.random
		return lista[rng(#lista)]
	end

	function D:Weighted(voci, rng)
		rng = rng or math.random
		local tot = 0
		for _, v in ipairs(voci) do
			tot += (v.peso or v.Weight or 1)
		end
		if tot <= 0 then return nil end
		local tiro = rng() * tot
		local acc = 0
		for _, v in ipairs(voci) do
			acc += (v.peso or v.Weight or 1)
			if tiro <= acc then return v.valore or v.Value or v end
		end
		local ultimo = voci[#voci]
		return ultimo.valore or ultimo.Value or ultimo
	end

	function D:Bisect(lista, valore, fn)
		fn = fn or function(x) return x end
		local lo, hi = 1, #lista
		while lo <= hi do
			local mid = math.floor((lo + hi) / 2)
			if fn(lista[mid]) < valore then
				lo = mid + 1
			else
				hi = mid - 1
			end
		end
		return lo
	end

	function D:Merge(base, sopra)
		local out = D:Clone(base)
		local function fondi(a, b)
			for k, v in pairs(b) do
				if type(v) == "table" and type(a[k]) == "table" then
					fondi(a[k], v)
				else
					a[k] = D:Clone(v)
				end
			end
		end
		fondi(out, sopra or {})
		return out
	end

	function D:Keys(t)
		local out = {}
		for k in pairs(t) do table.insert(out, k) end
		table.sort(out, function(a, b) return tostring(a) < tostring(b) end)
		return out
	end

	function D:SortBy(lista, fn)
		local decorata = {}
		for i, v in ipairs(lista) do
			decorata[i] = { k = fn(v, i), i = i, v = v }
		end
		table.sort(decorata, function(a, b)
			if a.k == b.k then return a.i < b.i end
			return a.k < b.k
		end)
		local out = {}
		for i, d in ipairs(decorata) do out[i] = d.v end
		return out
	end

	local Anello = {}
	Anello.__index = Anello

	function Anello:Push(v)
		self.buf[self.coda] = v
		self.coda = (self.coda % self.cap) + 1
		if self.n < self.cap then
			self.n += 1
		else
			self.testa = (self.testa % self.cap) + 1
		end
		return self
	end

	function Anello:Get(i)
		if i < 1 or i > self.n then return nil end
		local idx = ((self.testa - 1 + (i - 1)) % self.cap) + 1
		return self.buf[idx]
	end

	function Anello:Last()
		if self.n == 0 then return nil end
		return self:Get(self.n)
	end

	function Anello:Items()
		local out = table.create(self.n)
		for i = 1, self.n do out[i] = self:Get(i) end
		return out
	end

	function Anello:Clear()
		table.clear(self.buf)
		self.testa, self.coda, self.n = 1, 1, 0
	end

	local Slot = {}
	Slot.__index = Slot

	function Slot:Put(i, item)
		if i < 1 or i > self.n then return false end
		if self.celle[i] ~= nil then return false end
		self.celle[i] = item
		self.occupati += 1
		return true
	end

	function Slot:Take(i)
		local item = self.celle[i]
		if item == nil then return nil end
		self.celle[i] = nil
		self.occupati -= 1
		return item
	end

	function Slot:Swap(a, b)
		if a < 1 or b < 1 or a > self.n or b > self.n then return false end
		self.celle[a], self.celle[b] = self.celle[b], self.celle[a]
		return true
	end

	function Slot:Find(pred)
		for i = 1, self.n do
			local it = self.celle[i]
			if it ~= nil and pred(it, i) then return i, it end
		end
		return nil
	end

	function Slot:Free()
		return self.n - self.occupati
	end

	local Comandi = {}
	Comandi.__index = Comandi

	function Comandi:Do(cmd)
		if cmd.Do then cmd:Do() elseif cmd.esegui then cmd.esegui() end
		table.insert(self.fatti, cmd)
		table.clear(self.disfatti)
		if #self.fatti > self.cap then table.remove(self.fatti, 1) end
		return self
	end

	function Comandi:Undo()
		local cmd = table.remove(self.fatti)
		if not cmd then return false end
		if cmd.Undo then cmd:Undo() elseif cmd.annulla then cmd.annulla() end
		table.insert(self.disfatti, cmd)
		return true
	end

	function Comandi:Redo()
		local cmd = table.remove(self.disfatti)
		if not cmd then return false end
		if cmd.Do then cmd:Do() elseif cmd.esegui then cmd.esegui() end
		table.insert(self.fatti, cmd)
		return true
	end

	function Comandi:Depth()
		return #self.fatti, #self.disfatti
	end

	local Curva = {}
	Curva.__index = Curva

	function Curva:Add(t, v)
		table.insert(self.chiavi, { t = t, v = v })
		table.sort(self.chiavi, function(a, b) return a.t < b.t end)
		return self
	end

	function Curva:Eval(t)
		local k = self.chiavi
		if #k == 0 then return 0 end
		if t <= k[1].t then return k[1].v end
		if t >= k[#k].t then return k[#k].v end
		for i = 1, #k - 1 do
			local a, b = k[i], k[i + 1]
			if t >= a.t and t <= b.t then
				local u = (b.t == a.t) and 0 or (t - a.t) / (b.t - a.t)
				return a.v + (b.v - a.v) * u
			end
		end
		return k[#k].v
	end

	function D:Ring(cap)
		return setmetatable({ buf = {}, cap = math.max(1, cap or 8), testa = 1, coda = 1, n = 0 }, Anello)
	end

	function D:Slots(n)
		return setmetatable({ n = n, celle = {}, occupati = 0 }, Slot)
	end

	function D:Commands(cap)
		return setmetatable({ fatti = {}, disfatti = {}, cap = cap or 32 }, Comandi)
	end

	function D:Curve()
		return setmetatable({ chiavi = {} }, Curva)
	end

	function D:Chance(p, rng)
		rng = rng or math.random
		return rng() < p
	end

	function D:Clamp(v, lo, hi)
		if v < lo then return lo end
		if v > hi then return hi end
		return v
	end

	function D:Wrap(v, lo, hi)
		local span = hi - lo
		if span == 0 then return lo end
		local x = (v - lo) % span
		if x < 0 then x += span end
		return lo + x
	end

	function D:Round(v, passo)
		passo = passo or 1
		return math.floor(v / passo + 0.5) * passo
	end

	function D:Sign(v)
		if v > 0 then return 1 end
		if v < 0 then return -1 end
		return 0
	end

	function D:Approach(cur, bersaglio, passo)
		local d = bersaglio - cur
		if math.abs(d) <= passo then return bersaglio end
		return cur + D:Sign(d) * passo
	end
end)
