-- Classe a oggetti con metatable ed ereditarieta.
local Weapon = {}
Weapon.__index = Weapon

function Weapon.new(name, damage)
	local self = setmetatable({}, Weapon)
	self.Name = name
	self.Damage = damage
	return self
end

function Weapon:Describe()
	return string.format("%s (%d danni)", self.Name, self.Damage)
end

local Sword = setmetatable({}, { __index = Weapon })
Sword.__index = Sword
function Sword.new(name)
	local self = Weapon.new(name, 25)
	return setmetatable(self, Sword)
end
function Sword:Slash() return self.Damage * 2 end

local s = Sword.new("Katana")
assert(s:Describe() == "Katana (25 danni)", s:Describe())
assert(s:Slash() == 50)
assert(getmetatable(s) == Sword)
COMPAT_OK = true
