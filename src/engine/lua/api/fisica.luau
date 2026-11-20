-- Gruppi di collisione con il nome (come Roblox) e ricerche nel mondo.
-- Il numero 0..31 resta nel motore: qui si traduce il nome.
-- Le box ignorano la rotazione della Part (AABB).

local Internal = ...

Internal.modulo("fisica", function(I)
	local MAX = 32
	local nomi = { [0] = "Default" }
	local idDi = { Default = 0 }
	local vivo = { Default = true }
	local ordine = { "Default" }
	local matrice = {}

	local function vx(v) return type(v) == "table" and (v.X or v.x or 0) or 0 end
	local function vy(v) return type(v) == "table" and (v.Y or v.y or 0) or 0 end
	local function vz(v) return type(v) == "table" and (v.Z or v.z or 0) or 0 end
	local function v3(x, y, z) return Vector3.new(x, y, z) end

	local ps = game:GetService("PhysicsService")
	local setNativo = ps.CollisionGroupSetCollidable
	local creaNativo = ps.CreateCollisionGroup

	local function coppia(a, b, on)
		if on == nil then
			local r = matrice[a]
			if not r or r[b] == nil then return true end
			return r[b]
		end
		matrice[a] = matrice[a] or {}
		matrice[b] = matrice[b] or {}
		matrice[a][b] = on
		matrice[b][a] = on
		if setNativo then pcall(setNativo, ps, a, b, on) end
	end

	local function id(v, crea)
		if type(v) == "number" then return v end
		local nome = tostring(v)
		if vivo[nome] and idDi[nome] ~= nil then return idDi[nome] end
		if not crea then error("PhysicsService: gruppo non registrato '" .. nome .. "'", 3) end
		if nome == "Default" then return 0 end
		local n
		if creaNativo then
			local ok, got = pcall(creaNativo, ps)
			if ok and type(got) == "number" then n = got end
		end
		if n == nil or nomi[n] ~= nil then
			n = 1
			while n < MAX and nomi[n] ~= nil do n += 1 end
		end
		if n >= MAX then error("PhysicsService: al massimo 32 gruppi", 3) end
		nomi[n] = nome
		idDi[nome] = n
		vivo[nome] = true
		table.insert(ordine, nome)
		return n
	end

	local function idSeCera(v)
		if type(v) == "number" then return v end
		local nome = tostring(v)
		if vivo[nome] and idDi[nome] ~= nil then return idDi[nome] end
		return nil
	end

	function I.collisionGroupId(nome)
		return id(nome, true)
	end
	function I.collisionGroupName(n)
		local nome = nomi[n]
		if nome and vivo[nome] and idDi[nome] == n then return nome end
		return nil
	end

	local function registra(nome)
		nome = tostring(nome)
		if vivo[nome] then return idDi[nome] end
		return id(nome, true)
	end

	local function registraMetodo(self, nome)
		if type(nome) ~= "string" or nome == "" then error("RegisterCollisionGroup(nome)", 2) end
		registra(nome)
	end
	local function togliMetodo(self, nome)
		nome = tostring(nome)
		if nome == "Default" then error("non si puo' togliere il gruppo Default", 2) end
		if not vivo[nome] then error("PhysicsService: gruppo non registrato '" .. nome .. "'", 2) end
		vivo[nome] = false
	end
	local function registratoMetodo(self, nome)
		return vivo[tostring(nome)] == true
	end
	local function rinominaMetodo(self, da, a)
		da, a = tostring(da), tostring(a)
		if da == "Default" then error("non si puo' rinominare Default", 2) end
		if not vivo[da] then error("PhysicsService: gruppo non registrato '" .. da .. "'", 2) end
		if vivo[a] then error("PhysicsService: '" .. a .. "' esiste gia'", 2) end
		local n = idDi[da]
		vivo[da] = false
		idDi[da] = nil
		idDi[a] = n
		nomi[n] = a
		vivo[a] = true
		for i, v in ipairs(ordine) do if v == da then ordine[i] = a end end
	end
	local function listaMetodo()
		local out = {}
		for _, nome in ipairs(ordine) do
			if vivo[nome] then table.insert(out, { name = nome }) end
		end
		return out
	end
	local function maxMetodo() return MAX end
	local function setCollide(self, a, b, on)
		coppia(id(a, false), id(b, false), on ~= false)
	end
	local function areCollide(self, a, b)
		return coppia(id(a, false), id(b, false))
	end
	local function idMetodo(self, nome) return id(nome, false) end
	local function nomeMetodo(self, n)
		return I.collisionGroupName(n)
	end
	local function setPart(self, part, nome)
		part.CollisionGroup = nome
	end
	local function contiene(self, nome, part)
		return part.CollisionGroup == tostring(nome)
	end
	local function creaMetodo(self, nome)
		if type(nome) ~= "string" then
			if creaNativo then return creaNativo(self) end
			return 0
		end
		return registra(nome)
	end

	ps.CollisionGroupSetCollidable = setCollide
	ps.CollisionGroupsAreCollidable = areCollide
	ps.CreateCollisionGroup = creaMetodo
	rawset(ps, "__blkGruppi", true)

	local gruppi = {
		RegisterCollisionGroup = registraMetodo,
		UnregisterCollisionGroup = togliMetodo,
		RemoveCollisionGroup = togliMetodo,
		IsCollisionGroupRegistered = registratoMetodo,
		RenameCollisionGroup = rinominaMetodo,
		GetRegisteredCollisionGroups = listaMetodo,
		GetMaxCollisionGroups = maxMetodo,
		CollisionGroupSetCollidable = setCollide,
		CollisionGroupsAreCollidable = areCollide,
		GetCollisionGroupId = idMetodo,
		GetCollisionGroupName = nomeMetodo,
		SetPartCollisionGroup = setPart,
		CollisionGroupContainsPart = contiene,
		CreateCollisionGroup = creaMetodo,
	}
	I.realFuncs.PhysicsService = gruppi
	I.realFuncs.WorldRoot = I.realFuncs.WorldRoot or {}
	for k, f in pairs(gruppi) do I.realFuncs.WorldRoot[k] = f end

	local cget = game.GetService
	game.GetService = function(self, nome)
		if nome == "PhysicsService" then return ps end
		return cget(self, nome)
	end
	local cfind = game.FindService
	if type(cfind) == "function" then
		game.FindService = function(self, nome)
			if nome == "PhysicsService" then return ps end
			return cfind(self, nome)
		end
	end

	local function stesso(a, b)
		if a == b then return true end
		if type(a) ~= "table" or type(b) ~= "table" then return false end
		local ia, ib = rawget(a, "__partIndex"), rawget(b, "__partIndex")
		return ia ~= nil and ia == ib
	end

	local function aabb(part)
		local p, s = part.Position, part.Size
		local hx, hy, hz = vx(s) * 0.5, vy(s) * 0.5, vz(s) * 0.5
		local x, y, z = vx(p), vy(p), vz(p)
		return x - hx, y - hy, z - hz, x + hx, y + hy, z + hz
	end

	local function ePart(p)
		return type(p) == "table" and type(rawget(p, "__partIndex")) == "number"
	end

	local function partiMondo()
		local out = {}
		local ok, lista = pcall(function() return workspace:GetDescendants() end)
		if ok and type(lista) == "table" then
			for _, p in ipairs(lista) do
				if ePart(p) then table.insert(out, p) end
			end
		end
		return out
	end

	local function nelFiltro(part, lista)
		for _, x in ipairs(lista) do
			if stesso(part, x) then return true end
			local ok, d = pcall(function() return part:IsDescendantOf(x) end)
			if ok and d then return true end
		end
		return false
	end

	local function passa(part, params)
		if type(params) ~= "table" then return true end
		if params.RespectCanCollide and part.CanCollide == false then return false end
		local cg = params.CollisionGroup
		if type(cg) == "string" and cg ~= "" then
			local mio = part.CollisionGroup
			if type(mio) ~= "string" then mio = "Default" end
			local ia, ib = idSeCera(mio), idSeCera(cg)
			if ia and ib and not coppia(ia, ib) then return false end
		end
		local lista = params.FilterDescendantsInstances
		if type(lista) == "table" and #lista > 0 then
			local ft = params.FilterType
			if type(ft) == "table" then ft = ft.Name end
			local dentro = nelFiltro(part, lista)
			local include = ft == "Include" or ft == "Whitelist"
			if include then return dentro end
			if dentro then return false end
		end
		return true
	end

	local function filtra(lista, params)
		local out, tetto = {}, nil
		if type(params) == "table" and type(params.MaxParts) == "number" and params.MaxParts > 0 then
			tetto = params.MaxParts
		end
		for _, p in ipairs(lista) do
			if passa(p, params) then
				table.insert(out, p)
				if tetto and #out >= tetto then break end
			end
		end
		return out
	end

	local function distPuntoBox(px, py, pz, x0, y0, z0, x1, y1, z1)
		local cx = math.clamp(px, x0, x1)
		local cy = math.clamp(py, y0, y1)
		local cz = math.clamp(pz, z0, z1)
		local dx, dy, dz = px - cx, py - cy, pz - cz
		return dx * dx + dy * dy + dz * dz
	end

	-- Raggio (origine + direzione, t in 0..1) contro una AABB. Ritorna t oppure nil.
	local function raggioBox(ox, oy, oz, dx, dy, dz, x0, y0, z0, x1, y1, z1)
		local tmin, tmax = 0, 1
		local function asse(o, d, a, b)
			if math.abs(d) < 1e-9 then return o >= a and o <= b end
			local t1, t2 = (a - o) / d, (b - o) / d
			if t1 > t2 then t1, t2 = t2, t1 end
			if t1 > tmin then tmin = t1 end
			if t2 < tmax then tmax = t2 end
			return tmin <= tmax
		end
		if not asse(ox, dx, x0, x1) or not asse(oy, dy, y0, y1) or not asse(oz, dz, z0, z1) then
			return nil
		end
		if tmin < 0 then tmin = 0 end
		if tmin > 1 then return nil end
		return tmin
	end

	local function normale(px, py, pz, x0, y0, z0, x1, y1, z1)
		local lati = {
			{ math.abs(px - x0), -1, 0, 0 },
			{ math.abs(px - x1), 1, 0, 0 },
			{ math.abs(py - y0), 0, -1, 0 },
			{ math.abs(py - y1), 0, 1, 0 },
			{ math.abs(pz - z0), 0, 0, -1 },
			{ math.abs(pz - z1), 0, 0, 1 },
		}
		local best = lati[1]
		for i = 2, #lati do if lati[i][1] < best[1] then best = lati[i] end end
		return best[2], best[3], best[4]
	end

	local function risultato(part, x, y, z, nx, ny, nz, dist)
		local mat = part.Material
		return {
			Instance = part,
			Position = v3(x, y, z),
			Normal = v3(nx, ny, nz),
			Distance = dist,
			Material = mat,
		}
	end

	local function inRaggio(self, pos, raggio, params)
		raggio = tonumber(raggio) or 0
		local px, py, pz = vx(pos), vy(pos), vz(pos)
		local r2 = raggio * raggio
		local trovati = {}
		for _, p in ipairs(partiMondo()) do
			local x0, y0, z0, x1, y1, z1 = aabb(p)
			if distPuntoBox(px, py, pz, x0, y0, z0, x1, y1, z1) <= r2 then
				table.insert(trovati, p)
			end
		end
		return filtra(trovati, params)
	end

	local function spherecast(self, origine, raggio, direzione, params)
		raggio = tonumber(raggio) or 0
		local ox, oy, oz = vx(origine), vy(origine), vz(origine)
		local dx, dy, dz = vx(direzione), vy(direzione), vz(direzione)
		local lung = math.sqrt(dx * dx + dy * dy + dz * dz)
		if lung < 1e-6 then return nil end
		local best, bestT = nil, 2
		for _, p in ipairs(partiMondo()) do
			if passa(p, params) then
				local x0, y0, z0, x1, y1, z1 = aabb(p)
				local t = raggioBox(ox, oy, oz, dx, dy, dz, x0 - raggio, y0 - raggio, z0 - raggio, x1 + raggio, y1 + raggio, z1 + raggio)
				if t and t < bestT then best, bestT = p, t end
			end
		end
		if not best then return nil end
		local x, y, z = ox + dx * bestT, oy + dy * bestT, oz + dz * bestT
		local x0, y0, z0, x1, y1, z1 = aabb(best)
		local nx, ny, nz = normale(x, y, z, x0 - raggio, y0 - raggio, z0 - raggio, x1 + raggio, y1 + raggio, z1 + raggio)
		return risultato(best, x, y, z, nx, ny, nz, lung * bestT)
	end

	local function blockcast(self, cf, size, direzione, params)
		local pos = type(cf) == "table" and (cf.Position or cf) or cf
		local ox, oy, oz = vx(pos), vy(pos), vz(pos)
		local hx, hy, hz = vx(size) * 0.5, vy(size) * 0.5, vz(size) * 0.5
		local dx, dy, dz = vx(direzione), vy(direzione), vz(direzione)
		local lung = math.sqrt(dx * dx + dy * dy + dz * dz)
		if lung < 1e-6 then return nil end
		local best, bestT = nil, 2
		for _, p in ipairs(partiMondo()) do
			if passa(p, params) then
				local x0, y0, z0, x1, y1, z1 = aabb(p)
				local t = raggioBox(ox, oy, oz, dx, dy, dz, x0 - hx, y0 - hy, z0 - hz, x1 + hx, y1 + hy, z1 + hz)
				if t and t < bestT then best, bestT = p, t end
			end
		end
		if not best then return nil end
		local x, y, z = ox + dx * bestT, oy + dy * bestT, oz + dz * bestT
		local x0, y0, z0, x1, y1, z1 = aabb(best)
		local nx, ny, nz = normale(x, y, z, x0 - hx, y0 - hy, z0 - hz, x1 + hx, y1 + hy, z1 + hz)
		return risultato(best, x, y, z, nx, ny, nz, lung * bestT)
	end

	local function bulk(self, parti, cframe)
		if type(parti) ~= "table" or type(cframe) ~= "table" then return end
		for i, p in ipairs(parti) do
			local cf = cframe[i]
			if cf ~= nil and type(p) == "table" then p.CFrame = cf end
		end
	end

	local function siToccano(self, lista)
		if type(lista) ~= "table" then return false end
		local dentro = {}
		for _, p in ipairs(lista) do dentro[p] = true end
		for _, p in ipairs(lista) do
			if ePart(p) then
				local x0, y0, z0, x1, y1, z1 = aabb(p)
				for _, o in ipairs(partiMondo()) do
					if not dentro[o] and not stesso(o, p) then
						local a0, b0, c0, a1, b1, c1 = aabb(o)
						local tocca = a0 <= x1 and a1 >= x0 and b0 <= y1 and b1 >= y0 and c0 <= z1 and c1 >= z0
						if tocca then return true end
					end
				end
			end
		end
		return false
	end

	I.realFuncs.WorldRoot.GetPartBoundsInRadius = inRaggio
	I.realFuncs.WorldRoot.Spherecast = spherecast
	I.realFuncs.WorldRoot.Blockcast = blockcast
	I.realFuncs.WorldRoot.BulkMoveTo = bulk
	I.realFuncs.WorldRoot.ArePartsTouchingOthers = siToccano

	if type(workspace) == "table" then
		local boxVecchia = workspace.GetPartBoundsInBox
		if type(boxVecchia) == "function" then
			workspace.GetPartBoundsInBox = function(self, cf, size, params)
				return filtra(boxVecchia(self, cf, size) or {}, params)
			end
		end
		local inPart = workspace.GetPartsInPart
		if type(inPart) == "function" then
			workspace.GetPartsInPart = function(self, part, params)
				return filtra(inPart(self, part) or {}, params)
			end
		end
	end

	local GIUNTO = { Weld = true, WeldConstraint = true, Motor6D = true, ManualWeld = true, Snap = true }
	I.realFuncs.BasePart = I.realFuncs.BasePart or {}
	I.realFuncs.BasePart.GetMass = function(self)
		return self.Mass
	end
	I.realFuncs.BasePart.CanCollideWith = function(self, altro)
		if type(altro) ~= "table" then return false end
		local a, b = self.CollisionGroup, altro.CollisionGroup
		if type(a) ~= "string" then a = "Default" end
		if type(b) ~= "string" then b = "Default" end
		local ia, ib = idSeCera(a), idSeCera(b)
		if not ia or not ib then return true end
		return coppia(ia, ib)
	end
	I.realFuncs.BasePart.GetConnectedParts = function(self)
		local out = {}
		local ok, lista = pcall(function() return workspace:GetDescendants() end)
		if not ok or type(lista) ~= "table" then return out end
		for _, g in ipairs(lista) do
			if type(g) == "table" and GIUNTO[g.ClassName] then
				local a, b = g.Part0, g.Part1
				local altro = (stesso(a, self) and b) or (stesso(b, self) and a)
				if type(altro) == "table" then table.insert(out, altro) end
			end
		end
		return out
	end

	local Region3 = {}
	local RMT = { __metatable = "Region3", __tostring = function() return "Region3" end }
	function Region3.new(min, max)
		local x0, y0, z0 = math.min(vx(min), vx(max)), math.min(vy(min), vy(max)), math.min(vz(min), vz(max))
		local x1, y1, z1 = math.max(vx(min), vx(max)), math.max(vy(min), vy(max)), math.max(vz(min), vz(max))
		return setmetatable({
			Min = v3(x0, y0, z0),
			Max = v3(x1, y1, z1),
			CFrame = CFrame.new((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5),
			Size = v3(x1 - x0, y1 - y0, z1 - z0),
		}, RMT)
	end
	function RMT.__index(self, k)
		if k == "ExpandToGrid" then
			return function(reg, res)
				res = tonumber(res) or 4
				if res <= 0 then res = 4 end
				local function giu(v) return math.floor(v / res) * res end
				local function su(v) return math.ceil(v / res) * res end
				return Region3.new(
					v3(giu(vx(reg.Min)), giu(vy(reg.Min)), giu(vz(reg.Min))),
					v3(su(vx(reg.Max)), su(vy(reg.Max)), su(vz(reg.Max)))
				)
			end
		end
		return rawget(Region3, k)
	end
	_G.Region3 = Region3
end)
