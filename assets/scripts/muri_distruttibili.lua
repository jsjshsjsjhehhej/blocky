--[[ muri_distruttibili.lua — tutte le Part si possono rompere col fucile ]]

local kids = workspace:GetChildren()
for i = 1, #kids do
    local p = kids[i]
    if p and p.CanDestroy ~= nil then
        p.CanDestroy = true
        if not p.Health or p.Health <= 0 then
            p.Health = 100
        end
    end
end
print("[Muri] Part rese distruttibili")
