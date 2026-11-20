-- Griglia di percorso: celle chiuse, costo, A* a 4 o 8 vicini.
-- Dev:PathGrid(larghezza, altezza) poi :Find(x0, y0, x1, y1).
-- Le coordinate sono intere, da 1. Non usa il mondo: lo script passa i muri.

local Internal = ...

Internal.modulo("percorso", function(I)
	local D = I.Dev or I.extraServices.Dev
	if not D then
		print("[Lua] modulo API percorso: Dev non pronto")
		return
	end

	local function heapNuovo()
		return { n = 0, a = {} }
	end

	local function heapSu(h, i)
		local a = h.a
		while i > 1 do
			local p = math.floor(i / 2)
			if a[p].f <= a[i].f then break end
			a[p], a[i] = a[i], a[p]
			i = p
		end
	end

	local function heapGiu(h, i)
		local a, n = h.a, h.n
		while true do
			local l, r = i * 2, i * 2 + 1
			local m = i
			if l <= n and a[l].f < a[m].f then m = l end
			if r <= n and a[r].f < a[m].f then m = r end
			if m == i then break end
			a[i], a[m] = a[m], a[i]
			i = m
		end
	end

	local function heapPush(h, nodo)
		h.n += 1
		h.a[h.n] = nodo
		heapSu(h, h.n)
	end

	local function heapPop(h)
		if h.n == 0 then return nil end
		local top = h.a[1]
		h.a[1] = h.a[h.n]
		h.a[h.n] = nil
		h.n -= 1
		if h.n > 0 then heapGiu(h, 1) end
		return top
	end

	local Griglia = {}
	Griglia.__index = Griglia

	local function id(x, y)
		return x .. ":" .. y
	end

	function Griglia:Inside(x, y)
		return x >= 1 and y >= 1 and x <= self.w and y <= self.h
	end

	function Griglia:Block(x, y, chiusa)
		if not self:Inside(x, y) then return false end
		self.muri[id(x, y)] = chiusa ~= false
		return true
	end

	function Griglia:Blocked(x, y)
		return self.muri[id(x, y)] == true
	end

	function Griglia:Cost(x, y, c)
		if c == nil then return self.costi[id(x, y)] or 1 end
		self.costi[id(x, y)] = c
		return c
	end

	local function euristica(self, x, y, gx, gy)
		local dx, dy = math.abs(x - gx), math.abs(y - gy)
		if self.otto then
			local m = math.min(dx, dy)
			return (dx + dy) + (math.sqrt(2) - 2) * m
		end
		return dx + dy
	end

	local function vicini(self, x, y)
		local out = {}
		local dritti = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }
		for _, d in ipairs(dritti) do
			local nx, ny = x + d[1], y + d[2]
			if self:Inside(nx, ny) and not self:Blocked(nx, ny) then
				table.insert(out, { nx, ny, 1 })
			end
		end
		if self.otto then
			local diag = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } }
			for _, d in ipairs(diag) do
				local nx, ny = x + d[1], y + d[2]
				if self:Inside(nx, ny) and not self:Blocked(nx, ny) then
					if not self:Blocked(x, y + d[2]) and not self:Blocked(x + d[1], y) then
						table.insert(out, { nx, ny, math.sqrt(2) })
					end
				end
			end
		end
		return out
	end

	function Griglia:Find(x0, y0, x1, y1)
		if not self:Inside(x0, y0) or not self:Inside(x1, y1) then return nil end
		if self:Blocked(x0, y0) or self:Blocked(x1, y1) then return nil end
		if x0 == x1 and y0 == y1 then return { { x0, y0 } } end
		local aperti = heapNuovo()
		local best = {}
		local da = {}
		local partenza = id(x0, y0)
		best[partenza] = 0
		heapPush(aperti, { x = x0, y = y0, f = euristica(self, x0, y0, x1, y1), g = 0 })
		local visti = {}
		local passi = 0
		local tetto = self.w * self.h
		while aperti.n > 0 and passi < tetto do
			passi += 1
			local cur = heapPop(aperti)
			local cid = id(cur.x, cur.y)
			if not visti[cid] then
				visti[cid] = true
				if cur.x == x1 and cur.y == y1 then
					local path = {}
					local k = cid
					while k do
						local sx, sy = string.match(k, "^(%-?%d+):(%-?%d+)$")
						table.insert(path, 1, { tonumber(sx), tonumber(sy) })
						k = da[k]
					end
					return path
				end
				for _, nb in ipairs(vicini(self, cur.x, cur.y)) do
					local nid = id(nb[1], nb[2])
					if not visti[nid] then
						local g = cur.g + nb[3] * self:Cost(nb[1], nb[2])
						if best[nid] == nil or g < best[nid] then
							best[nid] = g
							da[nid] = cid
							local f = g + euristica(self, nb[1], nb[2], x1, y1)
							heapPush(aperti, { x = nb[1], y = nb[2], f = f, g = g })
						end
					end
				end
			end
		end
		return nil
	end

	function Griglia:Length(path)
		if not path or #path < 2 then return 0 end
		local s = 0
		for i = 2, #path do
			local a, b = path[i - 1], path[i]
			local dx, dy = a[1] - b[1], a[2] - b[2]
			s += math.sqrt(dx * dx + dy * dy)
		end
		return s
	end

	local function vede(self, ax, ay, bx, by)
		local dx, dy = bx - ax, by - ay
		local passi = math.max(math.abs(dx), math.abs(dy))
		if passi == 0 then return true end
		for i = 1, passi do
			local t = i / passi
			local x = math.floor(ax + dx * t + 0.5)
			local y = math.floor(ay + dy * t + 0.5)
			if self:Blocked(x, y) then return false end
		end
		return true
	end

	function Griglia:Smooth(path)
		if not path or #path < 3 then return path end
		local out = { path[1] }
		local i = 1
		while i < #path do
			local j = #path
			while j > i + 1 do
				if vede(self, path[i][1], path[i][2], path[j][1], path[j][2]) then break end
				j -= 1
			end
			table.insert(out, path[j])
			i = j
		end
		return out
	end

	function D:PathGrid(w, h, otto)
		return setmetatable({
			w = w,
			h = h,
			otto = otto == true,
			muri = {},
			costi = {},
		}, Griglia)
	end
end)
