-- Stringhe: gmatch, gsub con funzione, format, utf8, pack.
local words = {}
for w in ("ciao mondo blocky"):gmatch("%a+") do table.insert(words, w) end
assert(#words == 3 and words[3] == "blocky")
local up = ("abc"):gsub("%a", function(c) return c:upper() end)
assert(up == "ABC")
assert(string.format("%5.2f|%-4s|%x", 3.14159, "ok", 255) == " 3.14|ok  |ff")
assert(utf8.len("perche'") == 7 and #"è" == 2 and utf8.len("è") == 1)
assert(("a,b,,c"):split(",")[3] == "")
assert(string.rep("ab", 3) == "ababab")
local packed = string.pack("<I4", 258)
assert(#packed == 4 and string.unpack("<I4", packed) == 258)
assert(("Hello"):find("ll", 1, true) == 3)
COMPAT_OK = true
