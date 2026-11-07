-- Menu: Frame con UIListLayout e un tween di comparsa.
local TweenService = game:GetService("TweenService")
local gui = Instance.new("ScreenGui")
gui.Parent = game:GetService("Players").LocalPlayer.PlayerGui
local frame = Instance.new("Frame")
frame.Size = UDim2.new(0, 300, 0, 200)
frame.Position = UDim2.new(0.5, -150, 1, 0)
frame.BackgroundTransparency = 1
frame.Parent = gui
local layout = Instance.new("UIListLayout")
layout.SortOrder = Enum.SortOrder.LayoutOrder
layout.Padding = UDim.new(0, 6)
layout.Parent = frame
for i = 1, 3 do
	local b = Instance.new("TextButton")
	b.Text = "Voce " .. i
	b.LayoutOrder = i
	b.Size = UDim2.new(1, 0, 0, 40)
	b.Parent = frame
end
local tw = TweenService:Create(frame, TweenInfo.new(0.3), { BackgroundTransparency = 0 })
tw:Play()
tw.Completed:Connect(function() TW_DONE = true end)
task.delay(0.6, function()
	assert(TW_DONE, "il tween della GUI doveva finire")
	assert(math.abs(frame.BackgroundTransparency) < 1e-3, "BackgroundTransparency: " .. tostring(frame.BackgroundTransparency))
	COMPAT_OK = true
end)
