-- Piccoli aiuti per costruire il mondo con le Part.
-- Ogni funzione imposta le proprieta' PRIMA del Parent (come si fa su Roblox):
-- cosi' la part entra nel mondo gia' completa.

local Config = require(script.Parent.Config)

local Costruisci = {}

export type Opzioni = {
	nome: string?,
	dimensione: Vector3,
	posizione: Vector3,
	colore: string | Color3,
	materiale: Enum.Material?,
	forma: Enum.PartType?,
	collisione: boolean?,
	rotazione: Vector3?,     -- gradi (Orientation)
	neon: boolean?,
	trasparenza: number?,
}

-- Costruire tante part in un colpo fa scattare il gioco (e oltre 50 ms Blocky
-- ferma lo script): si lavora al massimo MS_PER_FRAME millisecondi di fila, poi
-- si lascia passare un frame. A tempo, non a numero: 25 part di una citta'
-- (casse, prompt, oro) costavano fino a 90 ms, 25 traversine quasi niente.
local MS_PER_FRAME = 3
local inizio = os.clock()

function Costruisci.part(o: Opzioni, padre: Instance): Part
	local adesso = os.clock()
	if adesso - inizio > 0.05 then
		inizio = adesso   -- pausa lunga dall'ultima part: e' un altro frame, si riparte da zero
	elseif (adesso - inizio) * 1000 > MS_PER_FRAME and coroutine.isyieldable() then
		task.wait()
		inizio = os.clock()
	end
	local p = Instance.new("Part")
	p.Name = o.nome or "Part"
	p.Anchored = true
	p.Size = o.dimensione
	p.Position = o.posizione
	if o.rotazione then p.Orientation = o.rotazione end
	p.Color = if typeof(o.colore) == "Color3" then o.colore else Config.colore(o.colore :: string)
	if o.materiale then p.Material = o.materiale end
	if o.forma then p.Shape = o.forma end
	if o.neon then p.Material = Enum.Material.Neon end
	if o.trasparenza then p.Transparency = o.trasparenza end
	p.CanCollide = if o.collisione == nil then true else o.collisione
	p.Parent = padre
	return p
end

function Costruisci.modello(nome: string, padre: Instance): Model
	local m = Instance.new("Model")
	m.Name = nome
	m.Parent = padre
	return m
end

return Costruisci
