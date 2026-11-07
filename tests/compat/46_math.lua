-- math e bit32 che si usano nei giochi.
assert(math.noise(0.5, 0.5) ~= nil)
assert(math.abs(math.noise(1.1, 2.2, 3.3) - math.noise(1.1, 2.2, 3.3)) == 0)
assert(math.floor(-1.5) == -2 and math.ceil(1.2) == 2)
assert(math.max(1, 5, 3) == 5 and math.min(4, 2) == 2)
assert(math.huge > 1e308 and -math.huge < -1e308)
assert(math.fmod(7, 3) == 1 and math.modf(3.7) == 3)
assert(bit32.bor(1, 2) == 3 and bit32.lshift(1, 4) == 16 and bit32.extract(0xF0, 4, 4) == 15)
assert(math.pi > 3.14159 and math.sqrt(16) == 4 and math.log(8, 2) == 3)
local r = math.random(1, 10)
assert(r >= 1 and r <= 10)
math.randomseed(42)
COMPAT_OK = true
