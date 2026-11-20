-- Il tempo: tempeste di sabbia che arrivano di giorno, salgono, durano un po'
-- e se ne vanno. Main legge Meteo.intensita() per la nebbia e il colore del cielo,
-- il client per la polvere davanti allo schermo (Stato.Tempesta).

local RS = game:GetService("ReplicatedStorage")
local Shared = RS:WaitForChild("Shared")
local Config = require(Shared:WaitForChild("Config"))

local Meteo = {}

local rng = Random.new(Config.SEME + 77)
local prossima = 0
local fino = 0
local tempo = 0
local intensita = 0       -- 0 = sereno, 1 = tempesta piena
local avvisa: (Player?, string) -> () = function() end

function Meteo.quandoAvviso(f: (Player?, string) -> ()) avvisa = f end

local function pianifica()
	prossima = tempo + rng:NextNumber(Config.TEMPESTA.ogni[1], Config.TEMPESTA.ogni[2])
end

function Meteo.nuovaPartita()
	tempo = 0
	intensita = 0
	fino = 0
	pianifica()
end

--- notte: di notte le tempeste non partono (ce n'e' gia' abbastanza).
function Meteo.passo(dt: number, notte: boolean)
	tempo += dt
	local attiva = tempo < fino
	if not attiva and tempo >= prossima and not notte then
		fino = tempo + rng:NextNumber(Config.TEMPESTA.dura[1], Config.TEMPESTA.dura[2])
		pianifica()
		prossima = math.max(prossima, fino + 60)
		avvisa(nil, "Arriva una tempesta di sabbia!")
		attiva = true
	end
	-- sale e scende piano
	local obiettivo = if attiva then 1 else 0
	local v = if attiva then 0.25 else 0.15
	if intensita < obiettivo then
		intensita = math.min(obiettivo, intensita + dt * v)
	else
		intensita = math.max(obiettivo, intensita - dt * v)
	end
end

function Meteo.intensita(): number return intensita end

return Meteo
