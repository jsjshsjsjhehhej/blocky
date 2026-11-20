-- CSG dagli script, punto sulla superficie, distanza dal personaggio, chat.
local GS = game:GetService("GeometryService")

local a = Instance.new("Part")
a.Name = "CsgA"
a.Anchored = true
a.Size = Vector3.new(4, 4, 4)
a.Position = Vector3.new(200, 40, 200)
a.Parent = workspace
local b = Instance.new("Part")
b.Name = "CsgB"
b.Anchored = true
b.Size = Vector3.new(4, 4, 4)
b.Position = Vector3.new(202, 40, 200)
b.Parent = workspace

local u = GS:UnionAsync(a, { b })
assert(u.ClassName == "UnionOperation", "classe unione")
assert(u.Name == "Union", "nome unione")
assert(math.abs(u.Position.X - 201) < 0.6, "centro unione " .. tostring(u.Position.X))
assert(u.Size.X > 5, "la unione e' piu' larga di una sola Part")
assert(a.Parent == nil and b.Parent == nil, "le part unite non restano")

local c = Instance.new("Part")
c.Anchored = true
c.Size = Vector3.new(4, 2, 2)
c.Position = Vector3.new(260, 40, 200)
c.Parent = workspace
local d = Instance.new("Part")
d.Anchored = true
d.Size = Vector3.new(2, 2, 2)
d.Position = Vector3.new(261, 40, 200)
d.Parent = workspace
local sub = c:SubtractAsync({ d })
assert(sub.ClassName == "UnionOperation", "sottrazione")
assert(sub.Size.X <= 4.1, "la sottrazione non e' piu' larga del pezzo")

local n = Instance.new("Part")
n.Anchored = true
n.Parent = workspace
GS:NegateAsync(n)
assert(n.IsNegative == true, "NegateAsync")
n:Destroy()

local box = Instance.new("Part")
box.Anchored = true
box.Size = Vector3.new(2, 2, 2)
box.Position = Vector3.new(0, 10, 0)
box.Parent = workspace
local vicino = box:GetClosestPointOnSurface(Vector3.new(10, 10, 0))
assert(math.abs(vicino.X - 1) < 0.05 and math.abs(vicino.Y - 10) < 0.05, "punto sulla faccia")
box:Destroy()

local plr = game:GetService("Players").LocalPlayer
local dist = plr:DistanceFromCharacter(Vector3.new(0, 0, 0))
assert(type(dist) == "number" and dist >= 0, "DistanceFromCharacter")
plr:Kick("prova")

local sent
game:GetService("Chat").Chatted:Connect(function(_, msg) sent = msg end)
game:GetService("Chat"):Chat(nil, "ciao")
assert(sent == "ciao", "Chat")
local bolla = game:GetService("TextChatService"):DisplayBubble(nil, "hey")
assert(bolla == "hey", "DisplayBubble")

u:Destroy()
sub:Destroy()

COMPAT_OK = true
