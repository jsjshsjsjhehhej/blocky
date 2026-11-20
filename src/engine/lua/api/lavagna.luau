-- Lavagna, lavori periodici e una coda di richiami.
-- Servono a tenere lo stato di un personaggio senza spargere variabili.

local Internal = ...

Internal.modulo("bacheca", function(I)
	local D = I.Dev or I.extraServices.Dev
	if not D then
		print("[Lua] modulo API bacheca: Dev non pronto")
		return
	end

	local Lavagna = {}
	Lavagna.__index = Lavagna

	function Lavagna:Set(chiave, valore)
		local prima = self.dati[chiave]
		if prima == valore then return false end
		self.dati[chiave] = valore
		self.Changed:Fire(chiave, valore, prima)
		return true
	end

	function Lavagna:Get(chiave, seManca)
		local v = self.dati[chiave]
		if v == nil then return seManca end
		return v
	end

	function Lavagna:Has(chiave)
		return self.dati[chiave] ~= nil
	end

	function Lavagna:Clear(chiave)
		if chiave == nil then
			table.clear(self.dati)
			return
		end
		local prima = self.dati[chiave]
		self.dati[chiave] = nil
		if prima ~= nil then self.Changed:Fire(chiave, nil, prima) end
	end

	function Lavagna:Keys()
		return D:Keys(self.dati)
	end

	local Lavori = {}
	Lavori.__index = Lavori

	function Lavori:Every(nome, secondi, fn)
		self.lista[nome] = {
			ogni = math.max(secondi, 1e-4),
			fn = fn,
			acc = 0,
			acceso = true,
			volte = 0,
		}
		return self
	end

	function Lavori:Tick(dt)
		dt = dt or 0
		for _, job in pairs(self.lista) do
			if job.acceso then
				job.acc += dt
				while job.acc >= job.ogni do
					job.acc -= job.ogni
					job.volte += 1
					if job.fn then job.fn(dt, job.volte) end
				end
			end
		end
	end

	function Lavori:Pause(nome)
		local job = self.lista[nome]
		if job then job.acceso = false end
	end

	function Lavori:Resume(nome)
		local job = self.lista[nome]
		if job then job.acceso = true end
	end

	function Lavori:Remove(nome)
		self.lista[nome] = nil
	end

	function Lavori:Times(nome)
		local job = self.lista[nome]
		return job and job.volte or 0
	end

	local Chiavistello = {}
	Chiavistello.__index = Chiavistello

	function Chiavistello:Arm(chiave)
		if self.fatti[chiave] then return false end
		self.fatti[chiave] = true
		return true
	end

	function Chiavistello:Has(chiave)
		return self.fatti[chiave] == true
	end

	function Chiavistello:Reset(chiave)
		if chiave then self.fatti[chiave] = nil else table.clear(self.fatti) end
	end

	local Metro = {}
	Metro.__index = Metro

	function Metro:Mark(n)
		self.cont += n or 1
	end

	function Metro:Tick(dt)
		self.tempo += dt or 0
		if self.tempo >= self.finestra then
			self.ultimo = self.cont / self.tempo
			self.cont = 0
			self.tempo = 0
		end
		return self.ultimo
	end

	function Metro:Rate()
		return self.ultimo
	end

	local Coda = {}
	Coda.__index = Coda

	function Coda:After(secondi, fn)
		table.insert(self.voci, { t = secondi, fn = fn })
		return self
	end

	function Coda:Tick(dt)
		dt = dt or 0
		for i = #self.voci, 1, -1 do
			local v = self.voci[i]
			v.t -= dt
			if v.t <= 0 then
				table.remove(self.voci, i)
				if v.fn then v.fn() end
			end
		end
	end

	function Coda:Pending()
		return #self.voci
	end

	function Coda:Clear()
		table.clear(self.voci)
	end

	function D:Board()
		return setmetatable({ dati = {}, Changed = I.Signal.new() }, Lavagna)
	end

	function D:Jobs()
		return setmetatable({ lista = {} }, Lavori)
	end

	function D:Latch()
		return setmetatable({ fatti = {} }, Chiavistello)
	end

	function D:Meter(finestra)
		return setmetatable({
			finestra = finestra or 1,
			cont = 0,
			tempo = 0,
			ultimo = 0,
		}, Metro)
	end

	function D:Queue()
		return setmetatable({ voci = {} }, Coda)
	end
end)
