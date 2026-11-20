-- La rete delle part (SetNetworkOwner & co.).
-- In Blocky la fisica e' gia' tutta del server: questi metodi non devono fare
-- niente, ma prima davano errore e fermavano gli script dei veicoli.
-- Test: tests/compat/60_seat_tool_attrs.lua

local Internal = ...

Internal.modulo("rete", function(I)
	local f = I.realFuncs.BasePart or {}
	I.realFuncs.BasePart = f
	f.SetNetworkOwner = function() end
	f.SetNetworkOwnershipAuto = function() end
	f.GetNetworkOwner = function() return nil end
	f.GetNetworkOwnershipAuto = function() return false end
	f.CanSetNetworkOwnership = function() return true, nil end
end)
