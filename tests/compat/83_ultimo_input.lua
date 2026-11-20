-- GetLastInputType è un EnumItem: i menu leggono .Name, non una stringa.
local uis = game:GetService("UserInputService")
local last = uis:GetLastInputType()
assert(type(last) == "table", "EnumItem")
assert(last.Name == "Keyboard", last.Name)
assert(Enum.UserInputType.Keyboard.Name == "Keyboard")
assert(Enum.UserInputType.Keyboard.Value == 8, "kb=" .. tostring(Enum.UserInputType.Keyboard.Value))
assert(Enum.UserInputType.MouseButton1.Value == 0, "mb=" .. tostring(Enum.UserInputType.MouseButton1.Value))
assert(Enum.UserInputType.Gamepad1.Name == "Gamepad1")
COMPAT_OK = true
