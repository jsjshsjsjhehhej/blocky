-- Unione, sottrazione e intersezione delle Part (CSG), come GeometryService.
-- Il risultato e' una mesh nel mondo. SeparateAsync rimette i pezzi originali.
-- GetClosestPointOnSurface segue il CFrame della Part.

local Internal = ...

Internal.modulo("geometria", function(I)
	local function lista(base, altri)
		local t = {}
		if type(base) == "table" then table.insert(t, base) end
		if type(altri) == "table" then
			for _, p in ipairs(altri) do
				if type(p) == "table" then table.insert(t, p) end
			end
		end
		return t
	end

	local function esegui(mode, base, altri)
		if type(__blkCsg) ~= "function" then error("CSG non disponibile", 2) end
		local parti = lista(base, altri)
		if #parti < 2 then error("CSG: servono almeno 2 Part", 2) end
		local info = __blkCsg(mode, parti)
		if type(info) ~= "table" then error("CSG fallita", 2) end
		for _, p in ipairs(parti) do
			pcall(function() p:Destroy() end)
		end
		local u = Instance.new("UnionOperation")
		u.Name = info.Name or "Union"
		u.Position = Vector3.new(info.X or 0, info.Y or 0, info.Z or 0)
		u.Size = Vector3.new(info.SX or 1, info.SY or 1, info.SZ or 1)
		if Color3 then
			pcall(function() u.Color = Color3.new(info.R or 0.6, info.G or 0.6, info.B or 0.6) end)
		end
		u.MeshId = info.Path or ""
		u.Parent = workspace
		return u
	end

	local geo = {
		UnionAsync = function(_, base, altri) return esegui(0, base, altri) end,
		SubtractAsync = function(_, base, altri) return esegui(1, base, altri) end,
		IntersectAsync = function(_, base, altri) return esegui(2, base, altri) end,
		NegateAsync = function(_, part)
			if type(part) ~= "table" then error("NegateAsync(part)", 2) end
			part.IsNegative = true
			return part
		end,
		SeparateAsync = function(_, union)
			if type(union) ~= "table" then error("SeparateAsync(unione)", 2) end
			local path = union.MeshId
			if type(path) ~= "string" or path == "" then
				error("SeparateAsync: l'unione non ha la mesh", 2)
			end
			if type(__blkCsgSeparate) ~= "function" then error("SeparateAsync non disponibile", 2) end
			local pezzi = __blkCsgSeparate(path)
			pcall(function() union:Destroy() end)
			return pezzi
		end,
	}
	I.realFuncs.GeometryService = geo

	local b = I.realFuncs.BasePart or {}
	I.realFuncs.BasePart = b
	b.UnionAsync = function(self, altri) return geo.UnionAsync(nil, self, altri) end
	b.SubtractAsync = function(self, altri) return geo.SubtractAsync(nil, self, altri) end
	b.IntersectAsync = function(self, altri) return geo.IntersectAsync(nil, self, altri) end
	b.GetClosestPointOnSurface = function(self, point)
		local cf, s = self.CFrame, self.Size
		if type(s) ~= "table" or type(point) ~= "table" then
			return Vector3.new(0, 0, 0)
		end
		local hx, hy, hz = (s.X or 0) * 0.5, (s.Y or 0) * 0.5, (s.Z or 0) * 0.5
		local px, py, pz = point.X or 0, point.Y or 0, point.Z or 0
		local function taglia(v, a, b0) return math.max(a, math.min(b0, v)) end
		local function fuori(lx, ly, lz)
			local ix, iy, iz = taglia(lx, -hx, hx), taglia(ly, -hy, hy), taglia(lz, -hz, hz)
			local dentro = ix == lx and iy == ly and iz == lz
			if not dentro then return ix, iy, iz end
			local best, asse = math.abs(lx + hx), 1
			local function considera(dist, quale)
				if dist < best then best, asse = dist, quale end
			end
			considera(math.abs(hx - lx), 2)
			considera(math.abs(ly + hy), 3)
			considera(math.abs(hy - ly), 4)
			considera(math.abs(lz + hz), 5)
			considera(math.abs(hz - lz), 6)
			if asse == 1 then return -hx, ly, lz end
			if asse == 2 then return hx, ly, lz end
			if asse == 3 then return lx, -hy, lz end
			if asse == 4 then return lx, hy, lz end
			if asse == 5 then return lx, ly, -hz end
			return lx, ly, hz
		end
		if type(cf) == "table" and type(cf.R00) == "number" then
			local dx, dy, dz = px - (cf.X or 0), py - (cf.Y or 0), pz - (cf.Z or 0)
			local lx = cf.R00 * dx + cf.R10 * dy + cf.R20 * dz
			local ly = cf.R01 * dx + cf.R11 * dy + cf.R21 * dz
			local lz = cf.R02 * dx + cf.R12 * dy + cf.R22 * dz
			local ox, oy, oz = fuori(lx, ly, lz)
			return Vector3.new(
				cf.R00 * ox + cf.R01 * oy + cf.R02 * oz + (cf.X or 0),
				cf.R10 * ox + cf.R11 * oy + cf.R12 * oz + (cf.Y or 0),
				cf.R20 * ox + cf.R21 * oy + cf.R22 * oz + (cf.Z or 0))
		end
		local p = self.Position
		local x = type(p) == "table" and (p.X or 0) or 0
		local y = type(p) == "table" and (p.Y or 0) or 0
		local z = type(p) == "table" and (p.Z or 0) or 0
		local ox, oy, oz = fuori(px - x, py - y, pz - z)
		return Vector3.new(x + ox, y + oy, z + oz)
	end
end)
