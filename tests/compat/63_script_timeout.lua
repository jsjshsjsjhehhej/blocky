-- Uno script che lavora piu' di 50 ms deve finire, come su Roblox.
-- Il ciclo infinito resta fermato, ma solo dopo 10 secondi.
local t0 = os.clock()
local n = 0
while os.clock() - t0 < 0.2 do
	n += 1
end
assert(n > 1000, "lo script e' stato fermato prima di 200 ms")
assert(os.clock() - t0 >= 0.2, "il tempo non e' passato")
COMPAT_OK = true
