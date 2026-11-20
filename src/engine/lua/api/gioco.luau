-- Chiamate che un gioco usa e che prima mancavano.
-- Chat e TextChatService scrivono e fanno scattare l'evento: non c'e' la chat di Roblox.
-- VRService resta spento. Il corpo R15 non sta qui.

local Internal = ...

Internal.modulo("gioco", function(I)
	local function fuoco(self, nome, ...)
		local ok, sig = pcall(function() return self[nome] end)
		if ok and type(sig) == "table" and type(sig.Fire) == "function" then
			sig:Fire(...)
		end
	end

	I.realFuncs.Chat = {
		Chat = function(self, part, message)
			local testo = tostring(message or "")
			print("[Chat] " .. testo)
			fuoco(self, "Chatted", part, testo)
			return testo
		end,
		CanUserChatAsync = function() return true end,
		CanUsersChatAsync = function() return true end,
		FilterStringForBroadcast = function(_, testo) return tostring(testo or "") end,
		FilterStringAsync = function(_, testo) return tostring(testo or "") end,
	}

	local function canale(nome)
		local ch = { Name = nome, ClassName = "TextChannel" }
		ch.MessageReceived = I.Signal.new()
		function ch:SendAsync(message)
			local testo = tostring(message or "")
			print("[TextChat] " .. testo)
			ch.MessageReceived:Fire({ Text = testo, TextSource = { Name = "Player" } })
			return testo
		end
		return ch
	end
	local canali = { RBXGeneral = canale("RBXGeneral") }

	I.realFuncs.TextChatService = {
		DisplayBubble = function(self, part, message)
			local testo = tostring(message or "")
			fuoco(self, "BubbleDisplayed", part, testo)
			return testo
		end,
		CanUserChatAsync = function() return true end,
		CanUsersChatAsync = function() return true end,
	}
	I.realProps.TextChatService = I.realProps.TextChatService or {}
	I.realProps.TextChatService.TextChannels = function() return canali end
end)
