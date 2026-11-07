-- Laboratorio di Fisica Blocky
-- 12 prove di fisica avanzata in fila, ognuna con un cartello che misura
-- quello che succede. Cammina sulla piastra verde davanti a una prova per
-- farla ripartire; ripartono anche da sole ogni tanto.
--
-- Regole di questo script:
--   * niente parentesi graffe: il caricatore dei place di Blocky chiude una
--     part alla prima graffa, anche se sta dentro lo script. Le tabelle si
--     creano con table.create(0).
--   * nessun Destroy: le prove si rimettono a posto spostando le part,
--     cosi' gli indici delle part non cambiano mai durante il gioco.

local RunService = game:GetService("RunService")

local function T() return table.create(0) end

local stations = T()
-- Blocchi di costruzione: eseguiti uno per volta (vedi in fondo), cosi' lo
-- script non supera il tempo massimo che Blocky concede a ogni avvio.
local builders = T()
local built = 0
local function pace()
	built += 1
	if built % 25 == 0 and coroutine.isyieldable() then task.wait() end
end
local V3 = Vector3.new
local rad = math.rad

------------------------------------------------------------------------
-- Aiuti
------------------------------------------------------------------------

local function rgb(r, g, b) return Color3.fromRGB(r, g, b) end

local GREY = rgb(120, 124, 130)
local DARK = rgb(55, 58, 66)
local GREEN = rgb(60, 220, 110)
local WOOD = rgb(176, 128, 78)
local RED = rgb(225, 70, 60)
local BLUE = rgb(70, 140, 235)
local YELLOW = rgb(245, 200, 60)
local WHITE = rgb(235, 235, 240)

-- Crea una part nel mondo. lcf e' in coordinate della prova.
local function part(st, name, size, lcf, color, anchored)
	pace()
	local p = Instance.new("Part")
	p.Parent = workspace
	p.Name = name
	p.Size = size
	p.CFrame = st.cf * lcf
	p.Color = color
	p.Anchored = anchored
	return p
end

-- Part che si muove: ricorda dove sta, per il reset.
local function dyn(st, name, size, lcf, color)
	-- nasce ancorata: si sgancia solo quando la prova parte (vicino al giocatore)
	local p = part(st, name, size, lcf, color, true)
	local d = T()
	d.part = p
	d.home = st.cf * lcf
	table.insert(st.dyn, d)
	return p
end

local function ball(st, name, diameter, lcf, color)
	local p = dyn(st, name, V3(diameter, diameter, diameter), lcf, color)
	p.Shape = "Ball"
	return p
end

local function setText(st, text)
	if st.label then pcall(function() st.label.Text = text end) end
end

local function pos(p) return p.Position end
local function vel(p)
	local ok, v = pcall(function() return p.AssemblyLinearVelocity end)
	if ok and v then return v end
	return V3(0, 0, 0)
end

-- Direzione locale della prova -> mondo.
local function dir(st, x, y, z) return st.cf:VectorToWorldSpace(V3(x, y, z)) end
local function lpos(st, p) return st.cf:PointToObjectSpace(p.Position) end

-- Spinta: prova ApplyImpulse (massa vera), altrimenti velocita' diretta.
local function kick(p, dv)
	local ok, m = pcall(function() return p:GetMass() end)
	if ok and type(m) == "number" and m > 0 and p.ApplyImpulse then
		p:ApplyImpulse(dv * m)
	else
		p.AssemblyLinearVelocity = dv
	end
end

------------------------------------------------------------------------
-- Prove: posto, cartello, piastra, reset
------------------------------------------------------------------------

local SLOT_X = T()
for i = 1, 6 do SLOT_X[i] = -100 + (i - 1) * 40 end

-- Vetrina: pareti di vetro, montanti e cornice di metallo, cima aperta.
-- base = CFrame al centro del pavimento. door > 0 = porta in quella larghezza
-- sul lato -Z (verso lo spawn).
local GLASS = rgb(170, 215, 255)
local FRAME = rgb(70, 76, 88)
local function glassBox(base, w, d, h, door)
	local t = 0.4
	local function pane(size, lcf)
		pace()
		local p = Instance.new("Part")
		p.Parent = workspace
		p.Name = "Vetro"
		p.Size = size
		p.CFrame = base * lcf
		p.Anchored = true
		p.Color = GLASS
		p.Material = "Glass"
		p.Transparency = 0.78
		p.Reflectance = 0.25
		pcall(function() p.CastShadow = false end)
		return p
	end
	local function bar(size, lcf)
		pace()
		local p = Instance.new("Part")
		p.Parent = workspace
		p.Name = "Cornice"
		p.Size = size
		p.CFrame = base * lcf
		p.Anchored = true
		p.Color = FRAME
		p.Material = "Metal"
		return p
	end
	-- Leggera: solo 5 lastre (3 pareti + 2 ai lati della porta), con una
	-- sottile cornice scura in cima che le fa sembrare una vetrina.
	local hx, hz = w / 2, d / 2
	pane(V3(w, h, t), CFrame.new(0, h / 2, hz))
	pane(V3(t, h, d), CFrame.new(-hx, h / 2, 0))
	pane(V3(t, h, d), CFrame.new(hx, h / 2, 0))
	if door and door > 0 then
		local side = (w - door) / 2
		pane(V3(side, h, t), CFrame.new(-hx + side / 2, h / 2, -hz))
		pane(V3(side, h, t), CFrame.new(hx - side / 2, h / 2, -hz))
	else
		pane(V3(w, h, t), CFrame.new(0, h / 2, -hz))
	end
	bar(V3(w + 0.6, 0.4, 0.6), CFrame.new(0, h, hz))
end

-- Cose che si possono "cliccare": inquadrale al centro dello schermo (o col
-- mouse) e premi il tasto sinistro. Vedi il ciclo Heartbeat in fondo.
local clickables = T()
local function onClick(p, fn)
	local e = T()
	e.part = p
	e.fn = fn
	table.insert(clickables, e)
end
local function samePart(a, b)
	if type(a) ~= "table" or type(b) ~= "table" then return false end
	local ia, ib = rawget(a, "__partIndex"), rawget(b, "__partIndex")
	return ia ~= nil and ia == ib
end


local function newStation(index, title, how)
	local st = T()
	st.index = index
	st.title = title
	st.how = how
	st.dyn = T()
	st.state = T()
	st.period = 22
	st.clock = 0
	st.busy = false
	st.ready = false -- diventa true quando tutto il laboratorio e' costruito
	-- File: 1-6 a z=45, 7-12 a z=-45, 13-16 a z=85, 17-20 a z=-85.
	local x, z
	if index <= 12 then
		local slot = index <= 6 and index or index - 6
		x = SLOT_X[slot]
		z = index <= 6 and 45 or -45
	else
		local slot = index <= 16 and index - 12 or index - 16
		x = -60 + (slot - 1) * 40
		z = index <= 16 and 85 or -85
	end
	-- +Z locale = lontano dallo spawn, per tutte le file.
	st.cf = CFrame.new(x, 0, z) * CFrame.Angles(0, z > 0 and 0 or math.pi, 0)

	-- pavimento scuro della prova, dentro una vetrina con la porta verso lo spawn
	part(st, "Pedana", V3(34, 0.2, 32), CFrame.new(0, 0.1, 0), DARK, true)
	glassBox(st.cf, 34.4, 33, 14, 9)
	-- piastra verde: riparti
	local pad = part(st, "Riparti", V3(6, 0.3, 3), CFrame.new(0, 0.25, -17.5), GREEN, true)
	pad.Material = "Neon"
	st.pad = pad
	-- palo del cartello
	local pole = part(st, "Cartello", V3(0.8, 7, 0.8), CFrame.new(-9, 3.5, -17), WHITE, true)
	local ok, bb = pcall(function() return Instance.new("BillboardGui", pole) end)
	if ok and bb then
		pcall(function() bb.StudsOffset = V3(0, 5, 0) end)
		st.label = bb
	end
	setText(st, title)
	table.insert(stations, st)
	return st
end

local function restart(st)
	if st.busy then return end
	st.busy = true
	st.active = true
	st.clock = 0
	task.spawn(function()
		-- Ancorate e rimesse a casa: il motore ricrea il corpo fisico, quindi
		-- spariscono anche velocita' e rotazione rimaste.
		for _, d in ipairs(st.dyn) do
			d.part.Anchored = true
			d.part.CFrame = d.home
		end
		if st.prepare then pcall(st.prepare, st) end
		task.wait(0.2)
		for _, d in ipairs(st.dyn) do d.part.Anchored = false end
		task.wait(0.15)
		st.state = T()
		st.t0 = os.clock()
		if st.start then
			local ok, err = pcall(st.start, st)
			if not ok then print("[Laboratorio] " .. st.title .. ": " .. tostring(err)) end
		end
		st.busy = false
	end)
end

-- Prova ferma: part ancorate al loro posto, zero lavoro per la fisica.
local function freeze(st)
	st.active = false
	for _, d in ipairs(st.dyn) do
		d.part.Anchored = true
		d.part.CFrame = d.home
	end
	setText(st, st.title .. "\navvicinati per farla partire")
end

-- Posizione del giocatore (nil se il motore non la da'): serve per far
-- girare solo le prove vicine. Lontane = ferme = niente lag.
local NEAR = 42
local function playerPos()
	local ok, p = pcall(function() return Humanoid:GetPosition() end)
	if ok and typeof(p) == "Vector3" then return p end
	return nil
end

local function hookPad(st)
	local last = 0
	st.pad.Touched:Connect(function(hit)
		if not hit or hit.Name ~= "Player" then return end
		if os.clock() - last < 2 then return end
		last = os.clock()
		restart(st)
	end)
	onClick(st.pad, function() restart(st) end)
end

local hasConstraints = type(Constraints) == "table"

-- Gravita' vera della fisica di Blocky (80 stud/s^2; workspace.Gravity non la
-- cambia). La prova 13 la misura e aggiorna questo valore per le altre.
local G = 80

------------------------------------------------------------------------
-- 1. Domino
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(1, "1. DOMINO", "una palla spinge la prima tessera")
	st.tiles = T()
	local n = 20
	for i = 1, n do
		local t = (i - 1) / (n - 1)
		local x = math.sin(t * math.pi * 1.5) * 6
		local z = -10 + t * 22
		local dx = math.cos(t * math.pi * 1.5) * 6 * math.pi * 1.5 / 22
		local yaw = math.atan2(dx, 1)
		local tile = dyn(st, "Tessera", V3(2, 4, 0.5), CFrame.new(x, 2.2, z) * CFrame.Angles(0, yaw, 0), i % 2 == 0 and WHITE or RED)
		tile.Density = 1
		table.insert(st.tiles, tile)
	end
	st.starter = ball(st, "PalladAvvio", 1.4, CFrame.new(0, 3.6, -14), YELLOW)
	st.starter.Density = 3
	st.period = 18
	st.start = function(s) s.starter.AssemblyLinearVelocity = dir(s, 0, 1, 14) end
	st.update = function(s)
		local fallen = 0
		for _, t in ipairs(s.tiles) do
			local up = t.CFrame.UpVector
			if up.Y < 0.7 then fallen += 1 end
		end
		if fallen == #s.tiles and not s.state.done then s.state.done = os.clock() - (s.t0 or os.clock()) end
		local extra = s.state.done and string.format("\ntutte giu' in %.1f s", s.state.done) or ""
		setText(s, string.format("%s\ncadute %d / %d%s", s.title, fallen, #s.tiles, extra))
	end
end)

------------------------------------------------------------------------
-- 2. Piramide e palla di cannone
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(2, "2. PALLA DI CANNONE", "massa e quantita' di moto")
	st.crates = T()
	local rows = 4
	for r = 0, rows - 1 do
		local count = rows - r
		for c = 0, count - 1 do
			local x = (c - (count - 1) / 2) * 2.05
			local cr = dyn(st, "Cassa", V3(2, 2, 2), CFrame.new(x, 1.2 + r * 2.02, 6), WOOD)
			cr.Material = "Wood"
			table.insert(st.crates, cr)
		end
	end
	st.cannon = ball(st, "PallaDiCannone", 2.4, CFrame.new(0, 2.5, -12), DARK)
	st.cannon.Density = 12
	st.cannon.Material = "Metal"
	st.period = 16
	st.start = function(s)
		task.wait(0.6)
		s.state.v0 = 42
		s.cannon.AssemblyLinearVelocity = dir(s, 0, 2, 42)
	end
	st.update = function(s)
		local moved = 0
		for _, d in ipairs(s.dyn) do
			if d.part ~= s.cannon and (pos(d.part) - d.home.Position).Magnitude > 1 then moved += 1 end
		end
		local v = vel(s.cannon).Magnitude
		setText(s, string.format("%s\npalla %.1f stud/s\ncasse spostate %d / %d", s.title, v, moved, #s.crates))
	end
end)

------------------------------------------------------------------------
-- 3. Rimbalzo (Elasticity)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(3, "3. RIMBALZO", "Elasticity 0 / 0.35 / 0.7 / 0.95")
	st.balls = T()
	local e = T()
	e[1] = 0; e[2] = 0.35; e[3] = 0.7; e[4] = 0.95
	local colors = T()
	colors[1] = GREY; colors[2] = BLUE; colors[3] = YELLOW; colors[4] = RED
	st.dropH = 20
	for i = 1, 4 do
		local b = ball(st, "Palla", 2, CFrame.new(-7.5 + (i - 1) * 5, st.dropH, 4), colors[i])
		b.Elasticity = e[i]
		b.Friction = 0.3
		b.Material = "SmoothPlastic"
		table.insert(st.balls, b)
	end
	st.period = 14
	st.update = function(s)
		local out = s.title
		for i, b in ipairs(s.balls) do
			local key = "b" .. i
			local info = s.state[key]
			if not info then info = T() info.lastVy = 0 info.best = 0 info.bounces = 0 s.state[key] = info end
			local vy = vel(b).Y
			local y = pos(b).Y
			if info.lastVy < -1 and vy >= 0 then
				info.bounces += 1
				info.peak = y
			end
			if info.peak and info.bounces == 1 then info.peak = math.max(info.peak, y) end
			info.lastVy = vy
			local pct = 0
			if info.peak then pct = math.clamp((info.peak - 1) / (s.dropH - 1) * 100, 0, 100) end
			out = out .. string.format("\ne=%.2f  1o rimbalzo %d%%", b.Elasticity, math.floor(pct + 0.5))
		end
		setText(s, out)
	end
end)

------------------------------------------------------------------------
-- 4. Attrito su una rampa (Friction)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(4, "4. ATTRITO", "rampa di 22 gradi")
	local rampCf = CFrame.new(0, 5, 3) * CFrame.Angles(rad(-22), 0, 0)
	local ramp = part(st, "Rampa", V3(16, 1, 24), rampCf, GREY, true)
	ramp.Friction = 1
	st.blocks = T()
	local f = T()
	f[1] = 0; f[2] = 0.05; f[3] = 0.12; f[4] = 0.6
	for i = 1, 4 do
		local b = dyn(st, "Blocco", V3(2.4, 1.2, 2.4), rampCf * CFrame.new(-5.4 + (i - 1) * 3.6, 1.15, 9), i == 4 and RED or BLUE)
		b.Friction = f[i]
		table.insert(st.blocks, b)
	end
	st.period = 16
	st.update = function(s)
		local out = s.title
		for i, b in ipairs(s.blocks) do
			local home = s.dyn[i].home.Position
			local d = (pos(b) - home).Magnitude
			out = out .. string.format("\nattrito %.2f  scesa %.1f stud", b.Friction, d)
		end
		setText(s, out)
	end
end)

------------------------------------------------------------------------
-- 5. Massa (Density): urti pesante -> leggero e leggero -> pesante
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(5, "5. MASSA", "Density 20 contro 0.3, sul ghiaccio")
	local function lane(x, firstDensity, secondDensity, c1, c2)
		local a = dyn(st, "Proiettile", V3(2, 2, 2), CFrame.new(x, 1.3, -10), c1)
		local b = dyn(st, "Bersaglio", V3(2, 2, 2), CFrame.new(x, 1.3, 2), c2)
		a.Density = firstDensity
		b.Density = secondDensity
		a.Friction = 0
		b.Friction = 0
		return a, b
	end
	st.a1, st.b1 = lane(-5, 20, 0.3, RED, BLUE)
	st.a2, st.b2 = lane(5, 0.3, 20, BLUE, RED)
	part(st, "Muro", V3(34, 3, 1), CFrame.new(0, 1.6, 15.5), GREY, true)
	st.period = 14
	st.start = function(s)
		s.a1.AssemblyLinearVelocity = dir(s, 0, 0, 16)
		s.a2.AssemblyLinearVelocity = dir(s, 0, 0, 16)
	end
	st.update = function(s)
		local function lv(p) return st.cf:VectorToObjectSpace(vel(p)).Z end
		setText(s, string.format("%s\npesante->leggero: %.1f / %.1f\nleggero->pesante: %.1f / %.1f",
			s.title, lv(s.a1), lv(s.b1), lv(s.a2), lv(s.b2)))
	end
end)

------------------------------------------------------------------------
-- 6. Esplosione (impulsi che calano con la distanza)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(6, "6. ESPLOSIONE", "impulso radiale, cala con la distanza")
	st.crates = T()
	for layer = 0, 2 do
		for k = 0, 3 do
			local a = k * math.pi / 2 + layer * math.pi / 4
			local x, z = math.cos(a) * 3, 4 + math.sin(a) * 3
			local c = dyn(st, "Cassa", V3(2, 2, 2), CFrame.new(x, 1.2 + layer * 2.05, z) * CFrame.Angles(0, -a, 0), WOOD)
			c.Material = "Wood"
			table.insert(st.crates, c)
		end
	end
	local flash = part(st, "Lampo", V3(1, 1, 1), CFrame.new(0, 3, 4), YELLOW, true)
	flash.Shape = "Ball"
	flash.Material = "Neon"
	flash.CanCollide = false
	flash.Transparency = 1
	st.flash = flash
	st.center = st.cf * CFrame.new(0, 2, 4)
	st.period = 15
	st.start = function(s)
		task.wait(1.2)
		local c = s.center.Position
		local launched, vmax = 0, 0
		for _, cr in ipairs(s.crates) do
			local d = pos(cr) - c
			local dist = d.Magnitude
			local R = 16
			if dist < R then
				local power = 48 * (1 - dist / R)
				local push = d.Unit * power + V3(0, power * 0.6, 0)
				kick(cr, push)
				launched += 1
				vmax = math.max(vmax, push.Magnitude)
			end
		end
		s.state.launched = launched
		s.state.vmax = vmax
		-- lampo: cresce e sfuma
		task.spawn(function()
			for i = 1, 12 do
				local k = i / 12
				s.flash.Size = V3(1, 1, 1) * (2 + k * 14)
				s.flash.Transparency = 0.2 + k * 0.8
				task.wait(0.03)
			end
			s.flash.Transparency = 1
		end)
	end
	st.update = function(s)
		local high = 0
		for _, cr in ipairs(s.crates) do high = math.max(high, pos(cr).Y) end
		setText(s, string.format("%s\ncasse lanciate %d\nspinta max %.0f stud/s\npiu' in alto %.1f stud",
			s.title, s.state.launched or 0, s.state.vmax or 0, high))
	end
end)

------------------------------------------------------------------------
-- 7. Pendolo doppio (Hinge): due quasi uguali, poi il caos
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(7, "7. PENDOLO DOPPIO", "due pendoli quasi identici")
	st.p = T()
	local function pendulum(x, color)
		local top = part(st, "Perno", V3(0.6, 0.6, 0.6), CFrame.new(x, 21, 0), WHITE, true)
		top.CanCollide = false
		-- Hinge mette il perno a meta' strada tra i centri: perno a 21, braccio
		-- centrato a 15 = cerniera a 18 (cima del braccio).
		local a1 = dyn(st, "Braccio1", V3(0.6, 6, 0.6), CFrame.new(x, 15, 0), color)
		local a2 = dyn(st, "Braccio2", V3(0.6, 6, 0.6), CFrame.new(x, 9, 0), color)
		a1.CanCollide = false
		a2.CanCollide = false
		a1.AngularDamping = 0
		a2.AngularDamping = 0
		a1.LinearDamping = 0
		a2.LinearDamping = 0
		if hasConstraints then
			local axis = dir(st, 0, 0, 1)
			Constraints.Hinge(top, a1, axis)
			Constraints.Hinge(a1, a2, axis)
		end
		local e = T()
		e.a1 = a1
		e.a2 = a2
		table.insert(st.p, e)
	end
	pendulum(-4, BLUE)
	pendulum(4, RED)
	part(st, "Trave", V3(14, 0.6, 0.6), CFrame.new(0, 21.6, 0), GREY, true)
	st.period = 30
	st.start = function(s)
		kick(s.p[1].a2, dir(s, 26, 0, 0))
		kick(s.p[2].a2, dir(s, 26.05, 0, 0))
	end
	st.update = function(s)
		local function angle(e)
			local up = e.a2.CFrame.UpVector
			return math.deg(math.atan2(up.X, up.Y))
		end
		local d = (pos(s.p[1].a2) - pos(s.p[2].a2) - (st.cf:VectorToWorldSpace(V3(-8, 0, 0)))).Magnitude
		setText(s, string.format("%s\nangolo blu %.0f  rosso %.0f\ndifferenza %.1f stud", s.title, angle(s.p[1]), angle(s.p[2]), d))
	end
end)

------------------------------------------------------------------------
-- 8. Molle (Spring) di durezza diversa
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(8, "8. MOLLE", "1 / 2.5 / 6 Hz, riposo 4 stud")
	part(st, "Trave", V3(16, 0.8, 0.8), CFrame.new(0, 16, 2), GREY, true)
	st.weights = T()
	st.tops = T()
	local hz = T()
	hz[1] = 1; hz[2] = 2.5; hz[3] = 6
	for i = 1, 3 do
		local x = (i - 2) * 5
		local top = part(st, "Gancio", V3(0.6, 0.6, 0.6), CFrame.new(x, 15.3, 2), WHITE, true)
		top.CanCollide = false
		local w = dyn(st, "Peso", V3(2, 2, 2), CFrame.new(x, 8.3, 2), i == 1 and BLUE or (i == 2 and YELLOW or RED))
		w.LinearDamping = 0
		if hasConstraints then Constraints.Spring(top, w, hz[i], 0.05, 4) end
		table.insert(st.weights, w)
		table.insert(st.tops, top)
	end
	st.period = 25
	st.update = function(s)
		local out = s.title
		for i, w in ipairs(s.weights) do
			out = out .. string.format("\nmolla %d: lunga %.1f stud", i, (pos(w) - pos(s.tops[i])).Magnitude)
		end
		setText(s, out)
	end
end)

------------------------------------------------------------------------
-- 9. Ponte di corda (catena di cerniere)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(9, "9. PONTE SOSPESO", "11 cerniere, sali sopra!")
	local n = 10
	local span = 12
	local sag = 1.6
	local y0 = 6
	local prev = part(st, "Pilone", V3(6, 1, 1.6), CFrame.new(0, y0, -span), GREY, true)
	part(st, "Base", V3(6, y0 - 0.5, 1.6), CFrame.new(0, (y0 - 0.5) / 2, -span), GREY, true)
	part(st, "Base", V3(6, y0 - 0.5, 1.6), CFrame.new(0, (y0 - 0.5) / 2, span), GREY, true)
	local last = part(st, "Pilone", V3(6, 1, 1.6), CFrame.new(0, y0, span), GREY, true)
	st.planks = T()
	local axis = dir(st, 1, 0, 0)
	for i = 1, n do
		local z = -span + 2 * span * i / (n + 1)
		local k = z / span
		local y = y0 - sag * (1 - k * k)
		local pl = dyn(st, "Asse", V3(5, 0.4, 1.7), CFrame.new(0, y, z), WOOD)
		pl.Material = "Wood"
		if hasConstraints then Constraints.Hinge(prev, pl, axis) end
		prev = pl
		table.insert(st.planks, pl)
	end
	if hasConstraints then Constraints.Hinge(prev, last, axis) end
	st.weight = ball(st, "Masso", 2.6, CFrame.new(0, 20, 0), DARK)
	st.weight.Density = 6
	st.period = 24
	st.update = function(s)
		local low = math.huge
		for _, pl in ipairs(s.planks) do low = math.min(low, pos(pl).Y) end
		setText(s, string.format("%s\npunto piu' basso %.2f stud\nabbassamento %.2f", s.title, low, y0 - low))
	end
end)

------------------------------------------------------------------------
-- 10. Pendolo di Newton (Rod)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(10, "10. PENDOLO DI NEWTON", "5 sfere, aste rigide, elasticita' 1")
	part(st, "Trave", V3(14, 0.6, 0.6), CFrame.new(0, 12.3, 2), GREY, true)
	st.balls = T()
	local L = 8
	for i = 1, 5 do
		local x = (i - 3) * 2.02
		local top = part(st, "Gancio", V3(0.4, 0.4, 0.4), CFrame.new(x, 12, 2), WHITE, true)
		top.CanCollide = false
		local b = ball(st, "Sfera", 2, CFrame.new(x, 12 - L, 2), rgb(200, 205, 215))
		b.Material = "Metal"
		b.Elasticity = 1
		b.Friction = 0
		b.LinearDamping = 0
		b.AngularDamping = 0
		if hasConstraints then Constraints.Rod(top, b, L) end
		table.insert(st.balls, b)
	end
	-- La prima parte sollevata di 50 gradi (stessa distanza dal gancio).
	local a = rad(50)
	st.dyn[1].home = st.cf * CFrame.new(-2 * 2.02 - math.sin(a) * L, 12 - math.cos(a) * L, 2)
	st.period = 20
	st.update = function(s)
		local v5 = vel(s.balls[5]).Magnitude
		local v1 = vel(s.balls[1]).Magnitude
		s.state.best = math.max(s.state.best or 0, v5)
		setText(s, string.format("%s\nprima %.1f  ultima %.1f stud/s\nultima al massimo %.1f", s.title, v1, v5, s.state.best))
	end
end)

------------------------------------------------------------------------
-- 11. Saldature (WeldConstraint)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(11, "11. SALDATURE", "sinistra saldato, destra no")
	local function tableAt(x, welded, color)
		local cf = CFrame.new(x, 14, 2) * CFrame.Angles(rad(20), rad(30), rad(12))
		local top = dyn(st, "Piano", V3(6, 0.6, 4), cf, color)
		local legs = T()
		for i = 0, 3 do
			local lx = (i % 2 == 0) and -2.6 or 2.6
			local lz = (i < 2) and -1.6 or 1.6
			local leg = dyn(st, "Gamba", V3(0.6, 3, 0.6), cf * CFrame.new(lx, -1.8, lz), color)
			if welded and hasConstraints then Constraints.Weld(top, leg) end
			table.insert(legs, leg)
		end
		return top, legs
	end
	st.topA, st.legsA = tableAt(-6, true, BLUE)
	st.topB, st.legsB = tableAt(6, false, RED)
	st.period = 14
	st.update = function(s)
		local function spread(top, legs)
			local sum = 0
			for _, l in ipairs(legs) do sum += (pos(l) - pos(top)).Magnitude end
			return sum / #legs
		end
		setText(s, string.format("%s\nsaldato: gambe a %.1f stud\nlibero: gambe a %.1f stud",
			s.title, spread(s.topA, s.legsA), spread(s.topB, s.legsB)))
	end
end)

------------------------------------------------------------------------
-- 12. Smorzamento (LinearDamping) sul ghiaccio
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(12, "12. SMORZAMENTO", "LinearDamping 0 / 0.4 / 1.5")
	st.blocks = T()
	local damp = T()
	damp[1] = 0; damp[2] = 0.4; damp[3] = 1.5
	for i = 1, 3 do
		local b = dyn(st, "Slitta", V3(2.4, 1, 2.4), CFrame.new((i - 2) * 6, 0.8, -12), i == 1 and BLUE or (i == 2 and YELLOW or RED))
		b.Friction = 0
		b.LinearDamping = damp[i]
		table.insert(st.blocks, b)
	end
	part(st, "Muro", V3(34, 3, 1), CFrame.new(0, 1.6, 15.5), GREY, true)
	st.period = 14
	st.start = function(s)
		for _, b in ipairs(s.blocks) do b.AssemblyLinearVelocity = dir(s, 0, 0, 14) end
	end
	st.update = function(s)
		local out = s.title
		for i, b in ipairs(s.blocks) do
			local d = (pos(b) - s.dyn[i].home.Position).Magnitude
			out = out .. string.format("\nsmorz. %.1f  %.1f stud/s  %.1f stud", b.LinearDamping, vel(b).Magnitude, d)
		end
		setText(s, out)
	end
end)

------------------------------------------------------------------------
-- 13. Gravita' misurata (caduta libera) e resistenza dell'aria
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(13, "13. GRAVITA'", "caduta libera da 60 stud")
	st.rock = ball(st, "Sasso", 2, CFrame.new(-4, 60, 4), DARK)
	st.feather = ball(st, "Piuma", 2, CFrame.new(4, 60, 4), WHITE)
	st.feather.LinearDamping = 2
	st.feather.Density = 0.1
	st.period = 12
	-- Ogni frame: velocita' tra due istanti della caduta -> g = dv / dt.
	st.tick = function(s, dt)
		local f = s.state
		f.t = (f.t or 0) + dt
		local v = vel(s.rock).Y
		local y = pos(s.rock).Y
		if y > 4 and v < -2 then
			if not f.t1 then f.t1 = f.t f.v1 = v end
			f.t2 = f.t
			f.v2 = v
		end
		if f.t1 and f.t2 and f.t2 - f.t1 > 0.3 then
			f.g = (f.v1 - f.v2) / (f.t2 - f.t1)
			G = f.g
		end
		f.featherMax = math.max(f.featherMax or 0, -vel(s.feather).Y)
	end
	st.update = function(s)
		local f = s.state
		local wg = 0
		pcall(function() wg = workspace.Gravity end)
		setText(s, string.format("%s\ng misurata %s stud/s^2\nworkspace.Gravity dice %.1f\npiuma: max %.1f stud/s (attrito dell'aria)",
			s.title, f.g and string.format("%.1f", f.g) or "...", wg, f.featherMax or 0))
	end
end)

------------------------------------------------------------------------
-- 14. Periodo del pendolo: misura contro 2*pi*sqrt(L/g)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(14, "14. PERIODO DEL PENDOLO", "aste da 4, 8 e 14 stud")
	part(st, "Trave", V3(18, 0.6, 0.6), CFrame.new(0, 22.3, 2), GREY, true)
	st.pend = T()
	local lengths = T()
	lengths[1] = 4; lengths[2] = 8; lengths[3] = 14
	local a = rad(20)
	for i = 1, 3 do
		local x = (i - 2) * 6
		local L = lengths[i]
		local top = part(st, "Gancio", V3(0.4, 0.4, 0.4), CFrame.new(x, 22, 2), WHITE, true)
		top.CanCollide = false
		local b = ball(st, "Peso", 1.4, CFrame.new(x, 22 - L, 2), i == 1 and RED or (i == 2 and YELLOW or BLUE))
		b.CanCollide = false
		b.LinearDamping = 0
		b.AngularDamping = 0
		if hasConstraints then Constraints.Rod(top, b, L) end
		-- parte spostata di 20 gradi
		st.dyn[#st.dyn].home = st.cf * CFrame.new(x + math.sin(a) * L, 22 - math.cos(a) * L, 2)
		local e = T()
		e.ball = b
		e.top = top
		e.L = L
		table.insert(st.pend, e)
	end
	st.period = 30
	st.tick = function(s, dt)
		s.state.t = (s.state.t or 0) + dt
		for i, e in ipairs(s.pend) do
			local key = "p" .. i
			local info = s.state[key]
			if not info then info = T() info.crossings = 0 s.state[key] = info end
			local side = lpos(s, e.ball).X - lpos(s, e.top).X
			local sign = side >= 0 and 1 or -1
			if info.sign and sign ~= info.sign then
				info.crossings += 1
				if info.crossings == 1 then info.first = s.state.t end
				if info.crossings >= 3 then
					-- due passaggi per lo zero = mezzo periodo
					info.T = (s.state.t - info.first) / ((info.crossings - 1) / 2)
				end
			end
			info.sign = sign
		end
	end
	st.update = function(s)
		local out = s.title
		for i, e in ipairs(s.pend) do
			local info = s.state["p" .. i]
			local theory = 2 * math.pi * math.sqrt(e.L / G)
			out = out .. string.format("\nL=%d: misurato %s s, teoria %.2f s", e.L,
				info and info.T and string.format("%.2f", info.T) or "...", theory)
		end
		setText(s, out)
	end
end)

------------------------------------------------------------------------
-- 15. Gittata del proiettile: 30 / 45 / 60 gradi
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(15, "15. GITTATA", "stessa velocita', tre angoli")
	st.shots = T()
	local angles = T()
	angles[1] = 30; angles[2] = 45; angles[3] = 60
	for i = 1, 3 do
		local b = ball(st, "Proiettile", 1, CFrame.new((i - 2) * 6, 0.75, -12), i == 1 and BLUE or (i == 2 and YELLOW or RED))
		b.Friction = 1
		b.Elasticity = 0
		local e = T()
		e.ball = b
		e.deg = angles[i]
		table.insert(st.shots, e)
	end
	-- arrivo: righe ogni 5 stud
	for k = 0, 5 do
		part(st, "Riga", V3(18, 0.05, 0.25), CFrame.new(0, 0.23, -12 + k * 5), WHITE, true).CanCollide = false
	end
	st.period = 10
	st.start = function(s)
		-- velocita' scelta perche' a 45 gradi arrivi a 24 stud
		s.state.v = math.sqrt(24 * G)
		for _, e in ipairs(s.shots) do
			local a = rad(e.deg)
			e.ball.AssemblyLinearVelocity = dir(s, 0, s.state.v * math.sin(a), s.state.v * math.cos(a))
		end
	end
	st.update = function(s)
		local out = s.title
		for i, e in ipairs(s.shots) do
			local key = "s" .. i
			local info = s.state[key]
			if not info then info = T() s.state[key] = info end
			local y = pos(e.ball).Y
			if y > 2 then info.flying = true end
			if info.flying and not info.range and y < 1 then
				info.range = lpos(s, e.ball).Z + 12
			end
			local v = s.state.v or math.sqrt(24 * G)
			local theory = v * v * math.sin(2 * rad(e.deg)) / G
			out = out .. string.format("\n%d gradi: %s stud (teoria %.1f)", e.deg,
				info.range and string.format("%.1f", info.range) or "...", theory)
		end
		setText(s, out)
	end
end)

------------------------------------------------------------------------
-- 16. Catapulta (leva su una cerniera)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(16, "16. CATAPULTA", "un peso cade sulla leva")
	part(st, "Fulcro", V3(1.2, 2.8, 1.2), CFrame.new(0, 1.6, 2), GREY, true)
	local leverCf = CFrame.new(0, 3.25, 2)
	-- perno invisibile nello stesso punto della leva: la cerniera sta al centro
	local pin = part(st, "Perno", V3(0.3, 0.3, 0.3), leverCf, WHITE, true)
	pin.CanCollide = false
	pin.Transparency = 1
	local lever = dyn(st, "Leva", V3(2, 0.4, 16), leverCf, WOOD)
	lever.Density = 0.3
	lever.Material = "Wood"
	if hasConstraints then Constraints.Hinge(pin, lever, dir(st, 1, 0, 0)) end
	st.load = ball(st, "Sasso", 1.2, CFrame.new(0, 4.1, 9.2), YELLOW)
	st.load.Density = 0.4
	st.weight = dyn(st, "Peso", V3(2, 2, 2), CFrame.new(0, 22, -6.8), DARK)
	st.weight.Density = 12
	st.weight.Material = "Metal"
	st.period = 14
	st.update = function(s)
		local y = pos(s.load).Y
		s.state.best = math.max(s.state.best or 0, y)
		setText(s, string.format("%s\nsasso a %.1f stud\npiu' in alto: %.1f stud", s.title, y, s.state.best))
	end
end)

------------------------------------------------------------------------
-- 17. Quinconce di Galton: il caso fa una campana
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(17, "17. QUINCONCE DI GALTON", "24 palline, 6 file di pioli")
	local depth = 1.5
	-- vetri davanti e dietro
	local glassF = part(st, "Vetro", V3(16, 22, 0.3), CFrame.new(0, 11, 2 - depth / 2 - 0.15), rgb(180, 220, 255), true)
	local glassB = part(st, "Vetro", V3(16, 22, 0.3), CFrame.new(0, 11, 2 + depth / 2 + 0.15), rgb(180, 220, 255), true)
	glassF.Transparency = 0.75
	glassB.Transparency = 0.6
	part(st, "Lato", V3(0.4, 22, depth), CFrame.new(-8, 11, 2), GREY, true)
	part(st, "Lato", V3(0.4, 22, depth), CFrame.new(8, 11, 2), GREY, true)
	-- imbuto
	part(st, "Imbuto", V3(6, 0.4, depth), CFrame.new(-4.2, 19.5, 2) * CFrame.Angles(0, 0, rad(-25)), GREY, true)
	part(st, "Imbuto", V3(6, 0.4, depth), CFrame.new(4.2, 19.5, 2) * CFrame.Angles(0, 0, rad(25)), GREY, true)
	-- pioli a rombo, file sfalsate
	for r = 0, 5 do
		local count = r + 2
		for c = 0, count - 1 do
			local x = (c - (count - 1) / 2) * 2
			part(st, "Piolo", V3(0.5, 0.5, depth), CFrame.new(x, 16.5 - r * 1.8, 2) * CFrame.Angles(0, 0, rad(45)), WHITE, true)
		end
	end
	-- 7 scomparti
	st.binW = 2
	for k = 0, 7 do
		part(st, "Divisorio", V3(0.2, 4, depth), CFrame.new(-7 + k * 2, 2.2, 2), GREY, true)
	end
	st.balls = T()
	for i = 1, 24 do
		local jitter = ((i * 37) % 11 - 5) * 0.04
		local b = ball(st, "Pallina", 0.9, CFrame.new(jitter, 22 + i * 1.1, 2), i % 2 == 0 and RED or BLUE)
		b.Elasticity = 0.25
		b.Friction = 0.2
		table.insert(st.balls, b)
	end
	st.period = 30
	st.update = function(s)
		local bins = T()
		for k = 1, 7 do bins[k] = 0 end
		for _, b in ipairs(s.balls) do
			local p = lpos(s, b)
			if p.Y < 4.5 then
				local k = math.clamp(math.floor((p.X + 7) / 2) + 1, 1, 7)
				bins[k] += 1
			end
		end
		local out = s.title
		for k = 1, 7 do out = out .. "\n" .. k .. " " .. string.rep("#", bins[k]) end
		setText(s, out)
	end
end)

------------------------------------------------------------------------
-- 18. Gruppi di collisione (PhysicsService)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(18, "18. GRUPPI DI COLLISIONE", "il muro ferma i blu, i rossi passano")
	local WALL, BLUEG, REDG = 5, 6, 7
	local wall = part(st, "MuroSelettivo", V3(16, 6, 1), CFrame.new(0, 3.2, 4), rgb(150, 200, 255), true)
	wall.Transparency = 0.4
	wall.CollisionGroup = WALL
	pcall(function()
		local ps = game:GetService("PhysicsService")
		ps:CollisionGroupSetCollidable(WALL, REDG, false)
		ps:CollisionGroupSetCollidable(WALL, BLUEG, true)
	end)
	st.balls = T()
	for i = 1, 6 do
		local red = i % 2 == 0
		local b = ball(st, red and "Fantasma" or "Solida", 1.6, CFrame.new(-6.25 + (i - 1) * 2.5, 1.1, -10), red and RED or BLUE)
		b.CollisionGroup = red and REDG or BLUEG
		local e = T()
		e.ball = b
		e.red = red
		table.insert(st.balls, e)
	end
	part(st, "Muro", V3(34, 3, 1), CFrame.new(0, 1.6, 15.5), GREY, true)
	st.period = 10
	st.start = function(s)
		for _, e in ipairs(s.balls) do e.ball.AssemblyLinearVelocity = dir(s, 0, 2, 22) end
	end
	st.update = function(s)
		local redPast, bluePast = 0, 0
		for _, e in ipairs(s.balls) do
			if lpos(s, e.ball).Z > 5 then
				if e.red then redPast += 1 else bluePast += 1 end
			end
		end
		setText(s, string.format("%s\nrossi passati %d / 3\nblu passati %d / 3", s.title, redPast, bluePast))
	end
end)

------------------------------------------------------------------------
-- 19. Bilancia: la legge della leva (massa x braccio)
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(19, "19. BILANCIA", "3 x 2 = 1 x 6 ?")
	st.beams = T()
	local function scale(x, rightDensity, label)
		local cf = CFrame.new(x, 6, 2)
		part(st, "Colonna", V3(0.8, 5.4, 0.8), CFrame.new(x, 2.9, 2.9), GREY, true)
		local pin = part(st, "Perno", V3(0.3, 0.3, 0.3), cf, WHITE, true)
		pin.CanCollide = false
		pin.Transparency = 1
		local beam = dyn(st, "Braccio", V3(16, 0.4, 1.2), cf, WOOD)
		beam.Density = 0.2
		if hasConstraints then Constraints.Hinge(pin, beam, dir(st, 0, 0, 1)) end
		-- sinistra: densita' 3 a 2 stud; destra: rightDensity a 6 stud
		local left = dyn(st, "Massa", V3(1.6, 1.6, 1.6), cf * CFrame.new(-2, 1, 0), RED)
		left.Density = 3
		local right = dyn(st, "Massa", V3(1.6, 1.6, 1.6), cf * CFrame.new(6, 1, 0), BLUE)
		right.Density = rightDensity
		if hasConstraints then
			Constraints.Weld(beam, left)
			Constraints.Weld(beam, right)
		end
		local e = T()
		e.beam = beam
		e.label = label
		table.insert(st.beams, e)
	end
	scale(-8, 1, "1 x 6")
	scale(8, 1.5, "1.5 x 6")
	st.period = 20
	st.update = function(s)
		local out = s.title
		for _, e in ipairs(s.beams) do
			local r = e.beam.CFrame.RightVector
			local tilt = math.deg(math.asin(math.clamp(r.Y, -1, 1)))
			out = out .. string.format("\n3 x 2 contro %s: %.1f gradi", e.label, tilt)
		end
		setText(s, out)
	end
end)

------------------------------------------------------------------------
-- 20. Mulino: palline che fanno girare una ruota
------------------------------------------------------------------------
table.insert(builders, function()
	local st = newStation(20, "20. MULINO", "ruota su cerniera + saldature")
	local c = CFrame.new(0, 10, 2)
	local pin = part(st, "Asse", V3(0.3, 0.3, 0.3), c, WHITE, true)
	pin.CanCollide = false
	pin.Transparency = 1
	part(st, "Supporto", V3(0.8, 9.4, 0.8), CFrame.new(0, 4.9, 3.6), GREY, true)
	st.hub = dyn(st, "Mozzo", V3(1, 1, 1.6), c, DARK)
	if hasConstraints then Constraints.Hinge(pin, st.hub, dir(st, 0, 0, 1)) end
	st.paddles = T()
	for k = 0, 3 do
		local a = k * math.pi / 2
		local pcf = c * CFrame.Angles(0, 0, a) * CFrame.new(3.5, 0, 0)
		local pad = dyn(st, "Pala", V3(5, 0.5, 1.6), pcf, k % 2 == 0 and YELLOW or WOOD)
		pad.Density = 0.4
		if hasConstraints then Constraints.Weld(st.hub, pad) end
		table.insert(st.paddles, pad)
	end
	for i = 1, 10 do
		local b = ball(st, "Pallina", 1.2, CFrame.new(3.6 + (i % 2) * 0.3, 15 + i * 1.5, 2), BLUE)
		b.Density = 2
	end
	st.period = 18
	st.tick = function(s, dt)
		local p = lpos(s, s.paddles[1]) - lpos(s, s.hub)
		local ang = math.atan2(p.Y, p.X)
		local f = s.state
		if f.last then
			local d = ang - f.last
			if d > math.pi then d -= 2 * math.pi elseif d < -math.pi then d += 2 * math.pi end
			f.total = (f.total or 0) + d
			f.rpm = (f.rpm or 0) * 0.9 + (d / dt) / (2 * math.pi) * 60 * 0.1
		end
		f.last = ang
	end
	st.update = function(s)
		local f = s.state
		setText(s, string.format("%s\n%.0f giri al minuto\ngiri fatti %.2f", s.title, math.abs(f.rpm or 0), math.abs(f.total or 0) / (2 * math.pi)))
	end
end)

------------------------------------------------------------------------
-- Cartello centrale e avvio
------------------------------------------------------------------------
table.insert(builders, function()
	local board = Instance.new("Part")
	board.Parent = workspace
	board.Name = "Benvenuto"
	board.Size = V3(1, 6, 1)
	board.CFrame = CFrame.new(0, 3, 8)
	board.Anchored = true
	board.Color = WHITE
	local ok, bb = pcall(function() return Instance.new("BillboardGui", board) end)
	if ok and bb then
		pcall(function()
			bb.StudsOffset = V3(0, 5, 0)
			bb.Text = "LABORATORIO DI FISICA\n20 prove: 1-6 e 13-16 da un lato,\n7-12 e 17-20 dall'altro\npiastra verde = riparti"
		end)
	end
end)

-- Costruzione a pezzi, poi partenza scaglionata (non tutte nello stesso frame).
task.spawn(function()
	for _, build in ipairs(builders) do
		local ok, err = pcall(build)
		if not ok then print("[Laboratorio] costruzione: " .. tostring(err)) end
		task.wait()
	end
	for _, st in ipairs(stations) do hookPad(st) end
	print("Laboratorio di Fisica pronto: " .. #stations .. " prove, " .. built .. " part")
	for _, st in ipairs(stations) do
		st.ready = true
		freeze(st)
	end
end)

-- Clic: il motore non manda InputBegan, quindi si guarda il tasto sinistro
-- ogni frame. Raggio dal centro dello schermo e, se c'e', dal mouse.
local UIS = UserInputService
pcall(function() UIS = game:GetService("UserInputService") or UserInputService end)
local mouseWas = false
local function aimedParts()
	local hits = T()
	local cam = workspace.CurrentCamera
	if not cam then return hits end
	pcall(function()
		local cf = cam.CFrame
		local r = workspace:Raycast(cf.Position, cf.LookVector * 400)
		if r and r.Instance then table.insert(hits, r.Instance) end
	end)
	pcall(function()
		local m = UIS:GetMouseLocation()
		local ray = cam:ScreenPointToRay(m.X, m.Y)
		local r = workspace:Raycast(ray.Origin, ray.Direction * 400)
		if r and r.Instance then table.insert(hits, r.Instance) end
	end)
	return hits
end
local function checkClick()
	local down = false
	pcall(function() down = UIS:IsMouseButtonPressed(0) end)
	if down and not mouseWas then
		for _, hit in ipairs(aimedParts()) do
			for _, c in ipairs(clickables) do
				if samePart(hit, c.part) then
					pcall(c.fn)
					mouseWas = down
					return
				end
			end
		end
	end
	mouseWas = down
end

local acc = 0
local nearAcc = 0
RunService.Heartbeat:Connect(function(dt)
	acc += dt
	checkClick()
	nearAcc += dt
	if nearAcc > 0.5 then
		nearAcc = 0
		local p = playerPos()
		for _, st in ipairs(stations) do
			if st.ready and not st.busy then
				local c = st.cf.Position
				local near = p == nil or math.sqrt((c.X - p.X) ^ 2 + (c.Z - p.Z) ^ 2) < NEAR
				if near and not st.active then restart(st)
				elseif not near and st.active then freeze(st) end
			end
		end
	end
	for _, st in ipairs(stations) do
		if not st.ready or not st.active then continue end
		st.clock += dt
		if st.tick and not st.busy then
			local ok, err = pcall(st.tick, st, dt)
			if not ok and not st.warned then
				st.warned = true
				print("[Laboratorio] " .. st.title .. ": " .. tostring(err))
			end
		end
		if not st.busy and st.clock > st.period then restart(st) end
	end
	if acc < 0.25 then return end
	acc = 0
	for _, st in ipairs(stations) do
		if st.ready and st.active and st.update and not st.busy then
			local ok, err = pcall(st.update, st)
			if not ok and not st.warned then
				st.warned = true
				print("[Laboratorio] " .. st.title .. ": " .. tostring(err))
			end
		end
	end
end)

