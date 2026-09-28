-- Monete che restano anche dopo aver chiuso il gioco.
-- Esempio del DataStore: tocca la parte gialla per guadagnare, chiudi
-- Blocky, riapri — le monete sono ancora li'.

local store = game:GetService("DataStoreService"):GetDataStore("Monete")
local giocatore = "walter"   -- in un gioco vero: il nome del giocatore

-- Legge il salvataggio (nil la primissima volta)
local monete = store:GetAsync(giocatore) or 0
print("Monete caricate:", monete)

-- Cartello che mostra il totale
local cartello = workspace:Add(Instance.new("Part"))
cartello.Name = "Cartello"
cartello.Position = Vector3.new(0, 6, -8)
cartello.Size = Vector3.new(6, 2, 0.5)
cartello.Color = Color3.new(0.1, 0.1, 0.15)
cartello.Anchored = true

-- La moneta da toccare
local moneta = workspace:Add(Instance.new("Part"))
moneta.Name = "Moneta"
moneta.Position = Vector3.new(0, 2, -4)
moneta.Size = Vector3.new(1.5, 1.5, 1.5)
moneta.Color = Color3.new(1, 0.82, 0.2)
moneta.Material = "Neon"
moneta.Anchored = true

local function aggiorna()
    print("Monete:", monete)
end

moneta.Touched:Connect(function()
    -- IncrementAsync salva e ritorna il nuovo totale in una sola chiamata
    monete = store:IncrementAsync(giocatore, 1)
    aggiorna()

    -- piccolo cooldown perche' il tocco si ripete ogni frame
    moneta.Position = Vector3.new(0, -50, 0)
    task.wait(1)
    moneta.Position = Vector3.new(0, 2, -4)
end)

aggiorna()
print("Chiudi e riapri il gioco: le monete restano.")
