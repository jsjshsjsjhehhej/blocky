-- HttpService: JSON annidato, GUID, UrlEncode.
local Http = game:GetService("HttpService")
local data = { nome = "Blocky", livelli = { 1, 2, 3 }, attivo = true, info = { x = 1.5 } }
local json = Http:JSONEncode(data)
assert(type(json) == "string" and json:find("Blocky"), "JSONEncode")
local back = Http:JSONDecode(json)
assert(back.nome == "Blocky" and back.livelli[3] == 3 and back.attivo == true and back.info.x == 1.5, "JSONDecode")
local guid = Http:GenerateGUID(false)
assert(type(guid) == "string" and #guid >= 32, "GenerateGUID: " .. tostring(guid))
assert(Http:UrlEncode("a b&c") == "a%20b%26c", "UrlEncode: " .. tostring(Http:UrlEncode("a b&c")))
COMPAT_OK = true
