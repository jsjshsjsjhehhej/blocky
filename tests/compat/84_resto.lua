-- :Wait() su un evento del motore, i pass di simulazione, e i servizi locali.
local Players = game:GetService("Players")
local RS = game:GetService("RunService")

local atteso = false
RS.PreSimulation:Connect(function()
	task.wait(0.05)
	atteso = true
end)

task.spawn(function()
	local p = Players.PlayerAdded:Wait()
	assert(type(p.Name) == "string" and #p.Name > 0, "PlayerAdded:Wait")
	local id = Players:GetUserIdFromNameAsync(p.Name)
	assert(type(id) == "number" and id > 0, "GetUserIdFromNameAsync")
	assert(Players:GetNameFromUserIdAsync(p.UserId) == p.Name, "GetNameFromUserIdAsync")
	assert(game:GetService("MarketplaceService"):UserOwnsGamePassAsync(p.UserId, 1) == false, "nessun pass")
	local info = game:GetService("MarketplaceService"):GetProductInfo(5)
	assert(info.IsForSale == false, "prodotto locale")
	local tr = game:GetService("LocalizationService"):GetTranslatorForPlayerAsync(p)
	assert(tr:Translate("ciao") == "ciao", "traduttore identita")
	local inset = game:GetService("GuiService"):GetGuiInset()
	assert(type(inset) == "table" and inset.Y == 36, "inset")
	game:BindToClose(function() end)
	local boom = Instance.new("Explosion")
	local colpito = false
	boom.Hit:Connect(function() colpito = true end)
	boom.Position = Vector3.new(0, 8, 0)
	boom.BlastRadius = 6
	boom.Parent = workspace
	assert(colpito, "Explosion")
	game:GetService("TextChatService"):DisplaySystemMessage("sistema")
	local visto = false
	task.wait(0.12)
	assert(atteso, "PreSimulation ha aspettato")
	local a = Instance.new("Part")
	a.Size = Vector3.new(4, 4, 4)
	a.Position = Vector3.new(0, 3, 0)
	a.Anchored = true
	a.Parent = workspace
	local b = Instance.new("Part")
	b.Size = Vector3.new(4, 4, 4)
	b.Position = Vector3.new(1, 3, 0)
	b.Anchored = true
	b.Parent = workspace
	local toccati = a:GetTouchingParts()
	assert(type(toccati) == "table", "GetTouchingParts")
	visto = true
	COMPAT_OK = visto
end)
