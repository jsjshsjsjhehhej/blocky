-- Geometria e curve per gli script: box, griglia, colori, easing, molla.
-- I metodi stanno su game:GetService("Dev").

local Internal = ...

Internal.modulo("spazio", function(I)
	local function dev()
		return I.Dev or I.extraServices.Dev
	end

	local function vx(v) return v.X or v.x or 0 end
	local function vy(v) return v.Y or v.y or 0 end
	local function vz(v) return v.Z or v.z or 0 end

	local function v3(x, y, z)
		return Vector3.new(x, y, z)
	end

	local function meta(part)
		local p = part.Position
		local s = part.Size
		return vx(p), vy(p), vz(p), vx(s) * 0.5, vy(s) * 0.5, vz(s) * 0.5
	end

	local function dentro(px, py, pz, cx, cy, cz, hx, hy, hz)
		return math.abs(px - cx) <= hx and math.abs(py - cy) <= hy and math.abs(pz - cz) <= hz
	end

	local function dist2(ax, ay, az, bx, by, bz)
		local dx, dy, dz = ax - bx, ay - by, az - bz
		return dx * dx + dy * dy + dz * dz
	end

	local Easing = {}

	local function clamp01(t)
		if t < 0 then return 0 end
		if t > 1 then return 1 end
		return t
	end

	function Easing.Linear(t) return t end
	function Easing.Quad(t) return t * t end
	function Easing.Cubic(t) return t * t * t end
	function Easing.Quart(t) return t * t * t * t end
	function Easing.Quint(t) return t * t * t * t * t end
	function Easing.Sine(t) return 1 - math.cos(t * math.pi * 0.5) end
	function Easing.Expo(t)
		if t <= 0 then return 0 end
		return 2 ^ (10 * (t - 1))
	end
	function Easing.Circ(t) return 1 - math.sqrt(math.max(0, 1 - t * t)) end
	function Easing.Back(t)
		local c1 = 1.70158
		local c3 = c1 + 1
		return c3 * t * t * t - c1 * t * t
	end
	function Easing.Elastic(t)
		if t <= 0 then return 0 end
		if t >= 1 then return 1 end
		local c4 = (2 * math.pi) / 3
		return -(2 ^ (10 * t - 10)) * math.sin((t * 10 - 10.75) * c4)
	end
	function Easing.Bounce(t)
		local n1, d1 = 7.5625, 2.75
		if t < 1 / d1 then return n1 * t * t end
		if t < 2 / d1 then
			t -= 1.5 / d1
			return n1 * t * t + 0.75
		end
		if t < 2.5 / d1 then
			t -= 2.25 / d1
			return n1 * t * t + 0.9375
		end
		t -= 2.625 / d1
		return n1 * t * t + 0.984375
	end

	local function entra(stile, t)
		local f = Easing[stile] or Easing.Linear
		return f(t)
	end

	local function curva(stile, dir, t)
		t = clamp01(t)
		dir = dir or "In"
		if stile == "Linear" or dir == "In" then
			if stile == "Linear" then return t end
			return entra(stile, t)
		end
		if dir == "Out" then
			if stile == "Bounce" then return Easing.Bounce(t) end
			return 1 - entra(stile, 1 - t)
		end
		if t < 0.5 then
			return entra(stile, t * 2) * 0.5
		end
		return 1 - entra(stile, (1 - t) * 2) * 0.5
	end

	local function lerp(a, b, t)
		return a + (b - a) * t
	end

	local function hsvInRgb(h, s, v)
		h = (h % 1) * 6
		local c = v * s
		local x = c * (1 - math.abs(h % 2 - 1))
		local m = v - c
		local r, g, b = 0, 0, 0
		if h < 1 then r, g = c, x
		elseif h < 2 then r, g = x, c
		elseif h < 3 then g, b = c, x
		elseif h < 4 then g, b = x, c
		elseif h < 5 then r, b = x, c
		else r, b = c, x
		end
		return r + m, g + m, b + m
	end

	local function rgbInHsv(r, g, b)
		local mx = math.max(r, g, b)
		local mn = math.min(r, g, b)
		local d = mx - mn
		local h = 0
		if d > 1e-8 then
			if mx == r then h = ((g - b) / d) % 6
			elseif mx == g then h = (b - r) / d + 2
			else h = (r - g) / d + 4
			end
			h /= 6
			if h < 0 then h += 1 end
		end
		local s = mx <= 1e-8 and 0 or d / mx
		return h, s, mx
	end

	local Griglia = {}
	Griglia.__index = Griglia

	local function chiave(self, x, z)
		local cx = math.floor(x / self.cella)
		local cz = math.floor(z / self.cella)
		return cx .. ":" .. cz, cx, cz
	end

	function Griglia:Add(pos, dato)
		local k = chiave(self, vx(pos), vz(pos))
		local cella = self.celle[k]
		if not cella then
			cella = {}
			self.celle[k] = cella
		end
		local voce = { pos = pos, dato = dato }
		table.insert(cella, voce)
		self.n += 1
		return voce
	end

	function Griglia:Query(pos, raggio)
		local out = {}
		local r2 = raggio * raggio
		local px, py, pz = vx(pos), vy(pos), vz(pos)
		local n = math.ceil(raggio / self.cella)
		local _, cx, cz = chiave(self, px, pz)
		for dz = -n, n do
			for dx = -n, n do
				local cella = self.celle[(cx + dx) .. ":" .. (cz + dz)]
				if cella then
					for _, voce in ipairs(cella) do
						local d2 = dist2(px, py, pz, vx(voce.pos), vy(voce.pos), vz(voce.pos))
						if d2 <= r2 then
							table.insert(out, voce.dato)
						end
					end
				end
			end
		end
		return out
	end

	function Griglia:Count()
		return self.n
	end

	function Griglia:Clear()
		table.clear(self.celle)
		self.n = 0
	end

	local Molla = {}
	Molla.__index = Molla

	function Molla:Step(dt, bersaglio)
		-- integrazione semi-implicita, stabile anche con dt grandi
		local f = 1 + self.smorz * dt + self.k * dt * dt
		local acc = (bersaglio - self.x) * self.k
		self.v = (self.v + acc * dt) / f
		self.x += self.v * dt
		return self.x
	end

	function Molla:Snap(x)
		self.x = x
		self.v = 0
		return self
	end

	local D = dev()
	if not D then
		print("[Lua] modulo API spazio: Dev non pronto")
		return
	end

	function D:Ease(stile, dir, t)
		if type(dir) == "number" then
			t = dir
			dir = "In"
		end
		return curva(stile or "Linear", dir, t or 0)
	end

	function D:Lerp(a, b, t)
		return lerp(a, b, t)
	end

	function D:Map(v, a0, a1, b0, b1)
		if a1 == a0 then return b0 end
		return b0 + (v - a0) / (a1 - a0) * (b1 - b0)
	end

	function D:Contains(part, punto)
		local cx, cy, cz, hx, hy, hz = meta(part)
		return dentro(vx(punto), vy(punto), vz(punto), cx, cy, cz, hx, hy, hz)
	end

	function D:Bounds(lista)
		if #lista == 0 then return nil end
		local minx, miny, minz = math.huge, math.huge, math.huge
		local maxx, maxy, maxz = -math.huge, -math.huge, -math.huge
		for _, part in ipairs(lista) do
			local cx, cy, cz, hx, hy, hz = meta(part)
			if cx - hx < minx then minx = cx - hx end
			if cy - hy < miny then miny = cy - hy end
			if cz - hz < minz then minz = cz - hz end
			if cx + hx > maxx then maxx = cx + hx end
			if cy + hy > maxy then maxy = cy + hy end
			if cz + hz > maxz then maxz = cz + hz end
		end
		local cx, cy, cz = (minx + maxx) * 0.5, (miny + maxy) * 0.5, (minz + maxz) * 0.5
		return {
			min = v3(minx, miny, minz),
			max = v3(maxx, maxy, maxz),
			center = v3(cx, cy, cz),
			size = v3(maxx - minx, maxy - miny, maxz - minz),
		}
	end

	function D:Closest(origine, lista)
		local best, bestD = nil, math.huge
		local ox, oy, oz = vx(origine), vy(origine), vz(origine)
		for _, part in ipairs(lista) do
			local p = part.Position
			local d = dist2(ox, oy, oz, vx(p), vy(p), vz(p))
			if d < bestD then
				best, bestD = part, d
			end
		end
		return best, math.sqrt(bestD)
	end

	function D:InRadius(origine, raggio, lista)
		local out = {}
		local r2 = raggio * raggio
		local ox, oy, oz = vx(origine), vy(origine), vz(origine)
		for _, part in ipairs(lista) do
			local p = part.Position
			if dist2(ox, oy, oz, vx(p), vy(p), vz(p)) <= r2 then
				table.insert(out, part)
			end
		end
		return out
	end

	function D:CastBox(origine, direzione, distanza, lista)
		local ox, oy, oz = vx(origine), vy(origine), vz(origine)
		local dx, dy, dz = vx(direzione), vy(direzione), vz(direzione)
		local len = math.sqrt(dx * dx + dy * dy + dz * dz)
		if len < 1e-8 then return nil end
		dx, dy, dz = dx / len, dy / len, dz / len
		local best, bestT = nil, distanza
		for _, part in ipairs(lista) do
			local cx, cy, cz, hx, hy, hz = meta(part)
			local tmin, tmax = 0, distanza
			local ok = true
			local orig = { ox, oy, oz }
			local dir = { dx, dy, dz }
			local cen = { cx, cy, cz }
			local half = { hx, hy, hz }
			for a = 1, 3 do
				if math.abs(dir[a]) < 1e-8 then
					if orig[a] < cen[a] - half[a] or orig[a] > cen[a] + half[a] then ok = false break end
				else
					local t1 = (cen[a] - half[a] - orig[a]) / dir[a]
					local t2 = (cen[a] + half[a] - orig[a]) / dir[a]
					if t1 > t2 then t1, t2 = t2, t1 end
					if t1 > tmin then tmin = t1 end
					if t2 < tmax then tmax = t2 end
					if tmin > tmax then ok = false break end
				end
			end
			if ok and tmin >= 0 and tmin < bestT then
				best, bestT = part, tmin
			end
		end
		if not best then return nil end
		return best, bestT, v3(ox + dx * bestT, oy + dy * bestT, oz + dz * bestT)
	end

	function D:Grid(cella)
		return setmetatable({ cella = cella or 8, celle = {}, n = 0 }, Griglia)
	end

	function D:Spring(k, smorz, x0)
		return setmetatable({ k = k or 8, smorz = smorz or 4, x = x0 or 0, v = 0 }, Molla)
	end

	function D:LerpColor(a, b, t)
		return Color3.new(lerp(a.R, b.R, t), lerp(a.G, b.G, t), lerp(a.B, b.B, t))
	end

	function D:ToHSV(c)
		return rgbInHsv(c.R, c.G, c.B)
	end

	function D:FromHSV(h, s, v)
		local r, g, b = hsvInRgb(h, s, v)
		return Color3.new(r, g, b)
	end

	function D:Bezier(p0, p1, p2, p3, t)
		local u = 1 - t
		local x = u^3 * vx(p0) + 3 * u^2 * t * vx(p1) + 3 * u * t^2 * vx(p2) + t^3 * vx(p3)
		local y = u^3 * vy(p0) + 3 * u^2 * t * vy(p1) + 3 * u * t^2 * vy(p2) + t^3 * vy(p3)
		local z = u^3 * vz(p0) + 3 * u^2 * t * vz(p1) + 3 * u * t^2 * vz(p2) + t^3 * vz(p3)
		return v3(x, y, z)
	end

	function D:LookFlat(da, a)
		local dx, dz = vx(a) - vx(da), vz(a) - vz(da)
		local yaw = math.deg(math.atan2(dx, dz))
		return yaw
	end

	I.Easing = Easing
end)
