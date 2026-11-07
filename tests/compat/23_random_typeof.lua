-- Random, typeof, math e stringhe.
local rng = Random.new(1234)
local roll = rng:NextInteger(1, 6)
assert(roll >= 1 and roll <= 6)
assert(typeof(Vector3.new()) == "Vector3" and typeof(CFrame.new()) == "CFrame" and typeof(Color3.new()) == "Color3")
assert(math.clamp(15, 0, 10) == 10 and math.round(2.5) == 3 and math.sign(-3) == -1)
local name = ("  Blocky  "):gsub("^%s+", ""):gsub("%s+$", "")
assert(name == "Blocky")
assert(("%03d"):format(7) == "007")
assert(#tostring(os.time()) >= 10)
COMPAT_OK = true
