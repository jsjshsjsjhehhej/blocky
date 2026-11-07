-- UserInputService con Enum.KeyCode.
local UIS = game:GetService("UserInputService")
UIS.InputBegan:Connect(function(input, gameProcessed)
	if gameProcessed then return end
	if input.KeyCode == Enum.KeyCode.E then INTERACT = true end
	if input.UserInputType == Enum.UserInputType.MouseButton1 then CLICK = true end
end)
UIS.InputEnded:Connect(function(input) end)
assert(Enum.KeyCode.E ~= nil and Enum.KeyCode.LeftShift ~= nil, "Enum.KeyCode")
assert(typeof(UIS:GetMouseLocation()) == "Vector2", "GetMouseLocation")
COMPAT_OK = true
