-- Record e classifica: la leaderstats di ogni giocatore (Distanza, Abbattuti,
-- Record) e il record personale salvato con DataStoreService.

local DataStoreService = game:GetService("DataStoreService")

local Record = {}

local archivio = nil
pcall(function() archivio = DataStoreService:GetDataStore("UltimoBinario_v1") end)

local salvati: { [Player]: { record: number, oro: number, partite: number } } = {}

local function valore(cartella: Instance, nome: string, iniziale: number): IntValue
	local v = cartella:FindFirstChild(nome)
	if not v then
		v = Instance.new("IntValue")
		v.Name = nome
		v.Value = iniziale
		v.Parent = cartella
	end
	return v :: IntValue
end

-- Blocky: il giocatore ha gia' una "leaderstats" sua che non e' un Folder vero
-- (FindFirstChild non e' una funzione, vedi docs/limiti_trovati.md): qui se ne
-- crea sempre una nuova e la si ricorda, senza cercarla.
local cartelle: { [Player]: Instance } = {}

local function stats(g: Player): Instance
	local ls = cartelle[g]
	if not ls then
		ls = Instance.new("Folder")
		ls.Name = "leaderstats"
		ls.Parent = g
		cartelle[g] = ls
	end
	return ls
end

--- Carica il record di g (se il DataStore risponde) e prepara la leaderstats.
function Record.carica(g: Player)
	local dati = { record = 0, oro = 0, partite = 0 }
	if archivio then
		local ok, letto = pcall(function() return archivio:GetAsync("g_" .. tostring(g.UserId)) end)
		if ok and type(letto) == "table" then
			dati.record = tonumber(letto.record) or 0
			dati.oro = tonumber(letto.oro) or 0
			dati.partite = tonumber(letto.partite) or 0
		end
	end
	salvati[g] = dati
	local ls = stats(g)
	valore(ls, "Distanza", 0)
	valore(ls, "Abbattuti", 0)
	valore(ls, "Record", dati.record).Value = dati.record
	g:SetAttribute("Record", dati.record)
end

--- Aggiorna la leaderstats durante la partita.
function Record.aggiorna(g: Player, distanza: number)
	local ls = stats(g)
	valore(ls, "Distanza", 0).Value = math.floor(distanza)
	valore(ls, "Abbattuti", 0).Value = g:GetAttribute("Uccisi") or 0
end

--- Fine partita: se e' un record, si salva. Restituisce true se e' un record nuovo.
function Record.fine(g: Player, distanza: number): boolean
	local dati = salvati[g]
	if not dati then return false end
	dati.partite += 1
	dati.oro += g:GetAttribute("OroVenduto") or 0
	local nuovo = distanza > dati.record
	if nuovo then dati.record = math.floor(distanza) end
	valore(stats(g), "Record", 0).Value = dati.record
	g:SetAttribute("Record", dati.record)
	if archivio then
		task.spawn(function()
			pcall(function()
				archivio:SetAsync("g_" .. tostring(g.UserId), { record = dati.record, oro = dati.oro, partite = dati.partite })
			end)
		end)
	end
	return nuovo
end

function Record.dimentica(g: Player)
	salvati[g] = nil
	cartelle[g] = nil
end

return Record
