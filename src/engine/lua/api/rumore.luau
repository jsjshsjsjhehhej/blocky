-- Rumore, palette e una traccia di chiavi per animare a mano un numero.
-- Non sostituisce TweenService: Eval la chiami tu ogni frame.

local Internal = ...

Internal.modulo("rumore", function(I)
	local D = I.Dev or I.extraServices.Dev
	if not D then
		print("[Lua] modulo API rumore: Dev non pronto")
		return
	end

	local function hash(ix, iy, seme)
		local n = bit32.bxor(bit32.bxor(ix * 374761393, iy * 668265263), seme * 1274126177)
		n = bit32.bxor(n, bit32.rshift(n, 13))
		n = bit32.bxor(n, bit32.lshift(n, 5))
		return n / 4294967295
	end

	local function fade(t)
		return t * t * t * (t * (t * 6 - 15) + 10)
	end

	local function valore(x, y, seme)
		local x0, y0 = math.floor(x), math.floor(y)
		local tx, ty = x - x0, y - y0
		local u, v = fade(tx), fade(ty)
		local a = hash(x0, y0, seme)
		local b = hash(x0 + 1, y0, seme)
		local c = hash(x0, y0 + 1, seme)
		local d = hash(x0 + 1, y0 + 1, seme)
		local ab = a + (b - a) * u
		local cd = c + (d - c) * u
		return ab + (cd - ab) * v
	end

	function D:Noise(x, y, seme)
		seme = seme or 1
		return valore(x, y or 0, seme)
	end

	function D:NoiseOctave(x, y, ottave, seme)
		ottave = ottave or 3
		seme = seme or 1
		local amp, freq, tot, norm = 1, 1, 0, 0
		for i = 1, ottave do
			tot += valore(x * freq, (y or 0) * freq, seme + i * 17) * amp
			norm += amp
			amp *= 0.5
			freq *= 2
		end
		if norm == 0 then return 0 end
		return tot / norm
	end

	local Palette = {}
	Palette.__index = Palette

	function Palette:Add(nome, colore)
		self.voci[nome] = colore
		table.insert(self.ordine, nome)
		return self
	end

	function Palette:Get(nome)
		return self.voci[nome]
	end

	function Palette:Names()
		local out = {}
		for i, n in ipairs(self.ordine) do out[i] = n end
		return out
	end

	function Palette:Lerp(a, b, t)
		local ca, cb = self.voci[a], self.voci[b]
		if not ca or not cb then return nil end
		return D:LerpColor(ca, cb, t)
	end

	function D:Palette()
		return setmetatable({ voci = {}, ordine = {} }, Palette)
	end

	local Traccia = {}
	Traccia.__index = Traccia

	function Traccia:Key(t, valore, stile)
		table.insert(self.chiavi, { t = t, v = valore, stile = stile or "Linear" })
		table.sort(self.chiavi, function(a, b) return a.t < b.t end)
		return self
	end

	function Traccia:Eval(t)
		local k = self.chiavi
		if #k == 0 then return 0 end
		if t <= k[1].t then return k[1].v end
		if t >= k[#k].t then return k[#k].v end
		for i = 1, #k - 1 do
			local a, b = k[i], k[i + 1]
			if t >= a.t and t <= b.t then
				local u = (b.t == a.t) and 0 or (t - a.t) / (b.t - a.t)
				local e = D:Ease(a.stile or "Linear", "In", u)
				if type(a.v) == "number" and type(b.v) == "number" then
					return a.v + (b.v - a.v) * e
				end
				if typeof(a.v) == "Vector3" and typeof(b.v) == "Vector3" then
					return Vector3.new(
						a.v.X + (b.v.X - a.v.X) * e,
						a.v.Y + (b.v.Y - a.v.Y) * e,
						a.v.Z + (b.v.Z - a.v.Z) * e
					)
				end
				return e < 1 and a.v or b.v
			end
		end
		return k[#k].v
	end

	function Traccia:Duration()
		if #self.chiavi == 0 then return 0 end
		return self.chiavi[#self.chiavi].t - self.chiavi[1].t
	end

	function D:Track()
		return setmetatable({ chiavi = {} }, Traccia)
	end

	function D:Seed(n)
		self._seme = n or 1
		return self
	end

	function D:Rand()
		local s = (self._seme or 1) % 2147483647
		if s <= 0 then s += 2147483646 end
		s = (s * 48271) % 2147483647
		self._seme = s
		return (s - 1) / 2147483646
	end

	function D:RandInt(lo, hi)
		return lo + math.floor(D:Rand() * (hi - lo + 1))
	end

	function D:Range(a, b, passo)
		passo = passo or 1
		local out = {}
		if passo == 0 then return out end
		if passo > 0 then
			for x = a, b, passo do table.insert(out, x) end
		else
			for x = a, b, passo do table.insert(out, x) end
		end
		return out
	end

	function D:Sum(lista)
		local s = 0
		for _, n in ipairs(lista) do s += n end
		return s
	end
end)
