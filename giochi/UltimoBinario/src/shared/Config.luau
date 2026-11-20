-- Ultimo Binario: tutti i numeri del gioco in un posto solo.
-- Il treno va verso +Z. La meta (il Faro) e' a LUNGHEZZA_VIAGGIO stud.

local Config = {}

-- Prova automatica (per chi sviluppa): il treno parte da solo con la caldaia piena.
-- Lasciarlo false nel gioco vero.
Config.PROVA_AUTOMATICA = false

-- Viaggio
Config.LUNGHEZZA_VIAGGIO = 10000
Config.VELOCITA_MAX = 26          -- stud al secondo con la leva avanti e la caldaia piena
Config.ACCELERAZIONE = 3.5
Config.FRENATA = 6
Config.PARTENZA_Z = 0

-- Caldaia
Config.CARBONE_MAX = 100
Config.CARBONE_INIZIO = 55
Config.CONSUMO = 1.0              -- per secondo, a velocita' massima
Config.CARBONE_PER_PEZZO = 24
Config.PEZZI_IN_MANO_MAX = 3

-- Mondo
Config.PEZZO_LUNGO = 160          -- lunghezza di un pezzo di mondo
Config.PEZZI_AVANTI = 3           -- quanti pezzi si preparano davanti al treno
Config.PEZZI_DIETRO = 2           -- quanti se ne tengono dietro (chi resta indietro non cade nel vuoto)
Config.SEME = 20261114
-- Cosa c'e' lungo i binari (posizioni Z): un avamposto, una citta', un forte...
-- Si ripete la sequenza ogni TAPPA stud.
Config.TAPPA = 900
Config.SEQUENZA = { "citta", "avamposto", "accampamento", "citta", "forte", "avamposto", "citta", "accampamento", "forte", "citta" }
Config.LATO_INSEDIAMENTI = -1     -- -1 = a sinistra dei binari (x negativa)

-- Giorno e notte (un giorno intero dura DURATA_GIORNO secondi)
Config.DURATA_GIORNO = 420
Config.ORA_INIZIO = 8
Config.NOTTE_DA = 19.5
Config.NOTTE_A = 5.5

-- Zombie
Config.ZOMBIE_MAX = 18            -- vivi tutti insieme (per i telefoni)
Config.ZOMBIE_SVEGLIA = 60        -- a questa distanza ti vedono (di notte di piu')
Config.ZOMBIE_SVEGLIA_NOTTE = 95
Config.ZOMBIE_ORDA_NOTTE = 0.55   -- quanti ne nascono al secondo, di notte, vicino al treno
Config.ZOMBIE_TIPI = {
	--            velocita' vita danno ogni  gettoni pelle            maglia           pantaloni
	Zombie    = { v = 12, vita = 100, danno = 10, ogni = 1.1, premio = 2, pelle = { 108, 128, 92 }, maglia = { 96, 74, 58 }, pantaloni = { 52, 62, 88 } },
	Corridore = { v = 20, vita = 60, danno = 7, ogni = 0.8, premio = 3, pelle = { 150, 160, 130 }, maglia = { 110, 110, 116 }, pantaloni = { 70, 60, 50 } },
	Bruto     = { v = 8, vita = 320, danno = 26, ogni = 1.6, premio = 10, pelle = { 70, 96, 62 }, maglia = { 40, 38, 36 }, pantaloni = { 40, 38, 36 } },
	Soldato   = { v = 14, vita = 170, danno = 15, ogni = 1.0, premio = 6, pelle = { 104, 122, 96 }, maglia = { 120, 112, 74 }, pantaloni = { 92, 86, 58 } },
	-- i Banditi non sono zombie: stanno a distanza e sparano (vedi Zombie.luau, "distanza")
	Bandito   = { v = 15, vita = 130, danno = 11, ogni = 1.9, premio = 8, pelle = { 198, 160, 124 }, maglia = { 92, 60, 38 }, pantaloni = { 44, 40, 38 },
		distanza = 24, mira = 0.45, cappello = { 30, 26, 22 }, oro = 0.35 },
}

-- Treno: vita e officina (sul tender)
Config.TRENO_VITA = 300
Config.TRENO_DANNO_ZOMBIE = 6       -- ogni colpo di uno zombie a bordo quando non trova nessuno
Config.POTENZIAMENTI = {
	--             prezzo  cosa fa
	Corazza    = { prezzo = 40,  testo = "Corazza (+200 vita treno)" },
	Caldaia    = { prezzo = 60,  testo = "Caldaia grande (150%)" },
	Pistoni    = { prezzo = 80,  testo = "Pistoni (+25% velocita')" },
	Torretta   = { prezzo = 120, testo = "Torretta automatica" },
	Riparazione = { prezzo = 15, testo = "Ripara (+120 vita)" },
}
Config.TORRETTA = { gittata = 70, ogni = 0.7, danno = 30 }

-- Armi (i Tool li crea Arma.client; il server controlla ricarica e munizioni)
Config.ARMI = {
	Revolver = { danno = 30, ricarica = 0.32, gittata = 90, pallini = 1, apertura = 0, colpo = 1.4, slot = 1 },
	Fucile   = { danno = 55, ricarica = 0.55, gittata = 160, pallini = 1, apertura = 0, colpo = 1.2, slot = 2 },
	Pompa    = { danno = 20, ricarica = 0.9, gittata = 45, pallini = 7, apertura = 0.09, colpo = 1.8, slot = 3 },
}
Config.DINAMITE = { raggio = 15, danno = 170, miccia = 1.6, lancio = 38 }
Config.PALA = { danno = 48, ricarica = 0.45, raggio = 7 }
Config.PREZZI_ARMI = { Pompa = 55, Dinamite = 20 }      -- dal mercante (Dinamite: 2 candelotti)

-- Missioni (bacheca dello sceriffo nelle citta')
Config.MISSIONI = {
	{ tipo = "uccidi", quanti = 8, premio = 35, testo = "Abbatti %d zombie" },
	{ tipo = "uccidi", quanti = 15, premio = 70, testo = "Abbatti %d zombie" },
	{ tipo = "oro", quanti = 3, premio = 40, testo = "Vendi %d lingotti alla cassaforte" },
	{ tipo = "banditi", quanti = 3, premio = 60, testo = "Abbatti %d banditi" },
	{ tipo = "strada", quanti = 1200, premio = 45, testo = "Fai altri %d stud di binario" },
}

-- Tempeste di sabbia (solo di giorno)
Config.TEMPESTA = { ogni = { 150, 300 }, dura = { 35, 70 } }

-- Paesaggio
Config.FIUME_OGNI = 2600            -- un fiume con il ponte
Config.MESA_PROB = 0.35             -- probabilita' di una mesa all'orizzonte in un pezzo

-- Fucile
Config.MUNIZIONI_INIZIO = 16
Config.DANNO_FUCILE = 55
Config.DANNO_TESTA = 2            -- moltiplicatore se colpisci in alto
Config.GITTATA = 160
Config.RICARICA = 0.55            -- secondi tra un colpo e l'altro

-- Economia
Config.ORO_VALORE = 15            -- gettoni per lingotto, alla cassaforte del treno
Config.ORO_MAX = 12               -- lingotti in mano
Config.PREZZI = { Munizioni = 10, Carbone = 8, Bende = 12 }
Config.CONFEZIONE = { Munizioni = 8, Carbone = 1, Bende = 1 }
Config.BENDE_VITA = 40

-- Colori (Color3.fromRGB)
Config.COLORI = {
	sabbia = { 196, 164, 112 },
	sabbiaScura = { 170, 138, 92 },
	pietrisco = { 120, 112, 104 },
	traversina = { 92, 62, 38 },
	rotaia = { 150, 150, 156 },
	locomotiva = { 46, 52, 60 },
	rifiniture = { 168, 128, 52 },
	caldaia = { 64, 70, 78 },
	vagone = { 120, 74, 40 },
	ruota = { 30, 30, 32 },
	fuoco = { 255, 140, 40 },
	cassa = { 140, 96, 52 },
	legnoVecchio = { 110, 84, 60 },
	legnoChiaro = { 160, 124, 84 },
	legnoScuro = { 70, 50, 36 },
	roccia = { 128, 116, 104 },
	cactus = { 74, 112, 62 },
	tetto = { 90, 60, 44 },
	faro = { 236, 232, 220 },
	oro = { 240, 190, 50 },
	mattone = { 150, 82, 60 },
	intonaco = { 204, 186, 156 },
	pietraForte = { 140, 130, 116 },
	strada = { 160, 130, 92 },
	vetro = { 60, 70, 80 },
	bandiera = { 160, 40, 36 },
}

function Config.colore(nome: string): Color3
	local c = Config.COLORI[nome]
	return Color3.fromRGB(c[1], c[2], c[3])
end

function Config.rgb(t: { number }): Color3
	return Color3.fromRGB(t[1], t[2], t[3])
end

return Config
