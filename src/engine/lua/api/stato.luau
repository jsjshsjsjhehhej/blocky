-- Macchine a stati, cooldown, pool, timer e un bus di eventi.
-- Tutto su game:GetService("Dev"), senza toccare il mondo.

local Internal = ...

Internal.modulo("stato", function(I)
	local function dev()
		return I.Dev or I.extraServices.Dev
	end

	local function segnale()
		return I.Signal.new()
	end

	local Macchina = {}
	Macchina.__index = Macchina

	function Macchina:Add(nome, def)
		def = def or {}
		self.stati[nome] = {
			Enter = def.Enter,
			Leave = def.Leave,
			Update = def.Update,
			Can = def.Can,
		}
		return self
	end

	function Macchina:Can(nome)
		local s = self.stati[nome]
		if not s then return false end
		if s.Can and not s.Can(self, self.corrente) then return false end
		return true
	end

	function Macchina:Go(nome, ...)
		if nome == self.corrente then return false end
		if not self:Can(nome) then return false end
		local prima = self.corrente
		local vecchio = prima and self.stati[prima]
		if vecchio and vecchio.Leave then vecchio.Leave(self, nome) end
		self.precedente = prima
		self.corrente = nome
		self.tempo = 0
		if prima then
			table.insert(self.storia, prima)
			if #self.storia > 16 then table.remove(self.storia, 1) end
		end
		local nuovo = self.stati[nome]
		if nuovo and nuovo.Enter then nuovo.Enter(self, prima, ...) end
		self.Changed:Fire(nome, prima)
		return true
	end

	function Macchina:Update(dt)
		dt = dt or 0
		self.tempo += dt
		local s = self.stati[self.corrente]
		if s and s.Update then s.Update(self, dt) end
	end

	function Macchina:Is(nome)
		return self.corrente == nome
	end

	function Macchina:Current()
		return self.corrente
	end

	function Macchina:Previous()
		return self.precedente
	end

	function Macchina:Time()
		return self.tempo
	end

	function Macchina:Reset(nome)
		self.corrente = nil
		self.precedente = nil
		self.tempo = 0
		table.clear(self.storia)
		if nome then self:Go(nome) end
	end

	local Cooldown = {}
	Cooldown.__index = Cooldown

	function Cooldown:Set(nome, secondi)
		self.durate[nome] = secondi
		if self.pronti[nome] == nil then
			self.pronti[nome] = self.orologio - secondi
		end
		return self
	end

	function Cooldown:Ready(nome)
		return (self.orologio - (self.pronti[nome] or 0)) >= (self.durate[nome] or 0)
	end

	function Cooldown:Remaining(nome)
		local manca = (self.durate[nome] or 0) - (self.orologio - (self.pronti[nome] or 0))
		if manca < 0 then return 0 end
		return manca
	end

	function Cooldown:Use(nome)
		if not self:Ready(nome) then return false end
		self.pronti[nome] = self.orologio
		return true
	end

	function Cooldown:Tick(dt)
		self.orologio += dt or 0
	end

	function Cooldown:Reset(nome)
		if nome then
			self.pronti[nome] = self.orologio - (self.durate[nome] or 0)
		else
			for k in pairs(self.durate) do
				self.pronti[k] = self.orologio - self.durate[k]
			end
		end
	end

	function Cooldown:Names()
		local out = {}
		for k in pairs(self.durate) do table.insert(out, k) end
		table.sort(out)
		return out
	end

	local Pool = {}
	Pool.__index = Pool

	function Pool:Get()
		local o = table.remove(self.liberi)
		if not o then
			o = self.crea()
			self.creati += 1
		end
		self.inUso += 1
		if self.prepara then self.prepara(o) end
		return o
	end

	function Pool:Release(o)
		if o == nil then return end
		if self.pulisci then self.pulisci(o) end
		self.inUso = math.max(0, self.inUso - 1)
		table.insert(self.liberi, o)
	end

	function Pool:Prewarm(n)
		for _ = 1, n do
			table.insert(self.liberi, self.crea())
			self.creati += 1
		end
	end

	function Pool:Free()
		return #self.liberi
	end

	function Pool:Busy()
		return self.inUso
	end

	function Pool:Clear()
		table.clear(self.liberi)
		self.inUso = 0
	end

	local Timer = {}
	Timer.__index = Timer

	function Timer:Tick(dt)
		if not self.acceso then return false end
		self.accum += dt or 0
		local sparato = false
		while self.accum >= self.ogni do
			self.accum -= self.ogni
			self.volte += 1
			sparato = true
			if self.fn then self.fn(self.volte) end
			self.Fired:Fire(self.volte)
			if not self.ripete then
				self.acceso = false
				break
			end
		end
		return sparato
	end

	function Timer:Start()
		self.acceso = true
		return self
	end

	function Timer:Stop()
		self.acceso = false
		return self
	end

	function Timer:Restart()
		self.accum = 0
		self.acceso = true
		return self
	end

	local Cronometro = {}
	Cronometro.__index = Cronometro

	function Cronometro:Start()
		if self.acceso then return self end
		self.t0 = os.clock()
		self.acceso = true
		return self
	end

	function Cronometro:Stop()
		if self.acceso then
			self.accum += os.clock() - self.t0
			self.acceso = false
		end
		return self.accum
	end

	function Cronometro:Elapsed()
		local extra = 0
		if self.acceso then extra = os.clock() - self.t0 end
		return self.accum + extra
	end

	function Cronometro:Lap()
		local e = self:Elapsed()
		local giro = e - self.ultimoGiro
		self.ultimoGiro = e
		table.insert(self.giri, giro)
		return giro
	end

	function Cronometro:Laps()
		return self.giri
	end

	function Cronometro:Reset()
		self.accum = 0
		self.acceso = false
		self.ultimoGiro = 0
		table.clear(self.giri)
		return self
	end

	local Bus = {}
	Bus.__index = Bus

	function Bus:On(nome, fn)
		local lista = self.map[nome]
		if not lista then
			lista = {}
			self.map[nome] = lista
		end
		table.insert(lista, fn)
		return function()
			local i = table.find(lista, fn)
			if i then table.remove(lista, i) end
		end
	end

	function Bus:Once(nome, fn)
		local stacca
		stacca = self:On(nome, function(...)
			stacca()
			fn(...)
		end)
		return stacca
	end

	function Bus:Emit(nome, ...)
		local lista = self.map[nome]
		if not lista then return 0 end
		local copia = table.clone(lista)
		for _, fn in ipairs(copia) do fn(...) end
		return #copia
	end

	function Bus:Clear(nome)
		if nome then self.map[nome] = nil else table.clear(self.map) end
	end

	local Sequenza = {}
	Sequenza.__index = Sequenza

	function Sequenza:Add(fn)
		table.insert(self.passi, fn)
		return self
	end

	function Sequenza:Tick()
		if self.i > #self.passi then
			self.finita = true
			return true
		end
		local fn = self.passi[self.i]
		local fatto = true
		if fn then fatto = fn(self) ~= false end
		if fatto then self.i += 1 end
		if self.i > #self.passi then self.finita = true end
		return self.finita
	end

	function Sequenza:Done()
		return self.finita
	end

	function Sequenza:Restart()
		self.i = 1
		self.finita = false
		return self
	end

	local function attendiDev()
		local D = dev()
		if not D then error("Dev non pronto", 2) end
		return D
	end

	local D = attendiDev()

	function D:Machine(iniziale)
		local m = setmetatable({
			stati = {},
			corrente = nil,
			precedente = nil,
			tempo = 0,
			storia = {},
			Changed = segnale(),
		}, Macchina)
		if iniziale then m:Add(iniziale, {}) m:Go(iniziale) end
		return m
	end

	function D:Cooldown()
		return setmetatable({ durate = {}, pronti = {}, orologio = 0 }, Cooldown)
	end

	function D:Pool(crea, prepara, pulisci)
		return setmetatable({
			crea = crea,
			prepara = prepara,
			pulisci = pulisci,
			liberi = {},
			inUso = 0,
			creati = 0,
		}, Pool)
	end

	function D:Timer(ogni, fn, ripete)
		if ripete == nil then ripete = false end
		return setmetatable({
			ogni = ogni,
			fn = fn,
			ripete = ripete,
			accum = 0,
			acceso = false,
			volte = 0,
			Fired = segnale(),
		}, Timer)
	end

	function D:Stopwatch()
		return setmetatable({
			accum = 0,
			acceso = false,
			t0 = 0,
			ultimoGiro = 0,
			giri = {},
		}, Cronometro)
	end

	function D:Bus()
		return setmetatable({ map = {} }, Bus)
	end

	function D:Sequence()
		return setmetatable({ passi = {}, i = 1, finita = false }, Sequenza)
	end
end)
