-- Path con i parametri dell'agente, testo che va a capo, memoria, voce, catalogo.
local path = game:GetService("PathfindingService"):CreatePath({
	AgentRadius = 2,
	AgentHeight = 5,
	AgentCanJump = true,
	AgentJumpHeight = 7,
	WaypointSpacing = 4,
})
assert(type(path.ComputeAsync) == "function", "path")
path:ComputeAsync(Vector3.new(0, 3, 0), Vector3.new(8, 3, 0))
local wps = path:GetWaypoints()
assert(type(wps) == "table", "waypoints")

local TS = game:GetService("TextService")
local una = TS:GetTextSize("ciao", 20, Enum.Font.Gotham, Vector2.new(1000, 200))
assert(una.X > 0 and una.Y == 20, "riga " .. tostring(una.Y))
local lungo = string.rep("a", 80)
local piega = TS:GetTextSize(lungo, 10, Enum.Font.Gotham, Vector2.new(40, 500))
assert(piega.Y > 10 and piega.X <= 40, "a capo " .. tostring(piega.X) .. " " .. tostring(piega.Y))

local mapa = game:GetService("MemoryStoreService"):GetHashMap("test")
assert(mapa:SetAsync("k", 7, 30) == true, "set")
assert(mapa:GetAsync("k") == 7, "get")
assert(mapa:UpdateAsync("k", function(v) return v + 1 end, 30) == 8, "upd")
mapa:RemoveAsync("k")
assert(mapa:GetAsync("k") == nil, "del")
local coda = game:GetService("MemoryStoreService"):GetQueue("q")
coda:AddAsync("go", 30)
local letti = coda:ReadAsync(5)
assert(letti[1] == "go", "coda")
assert(coda:RemoveAsync("go") == true, "togli")

assert(game:GetService("VoiceChatService"):IsVoiceEnabledForUserIdAsync(1) == false, "voce")
local pag = game:GetService("AvatarEditorService"):SearchCatalogAsync()
local lista = pag:GetCurrentPage()
assert(lista[1].Name == "Blocky", "catalogo")
local niente = game:GetService("InsertService"):LoadAsset(999999)
assert(niente == nil, "insert vuoto")

local visto = nil
game:GetService("MessagingService"):SubscribeAsync("giro", function(msg) visto = msg end)
game:GetService("MessagingService"):PublishAsync("giro", "ok")
assert(visto == "ok", "msg")

COMPAT_OK = true
