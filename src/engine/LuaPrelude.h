#pragma once

/**
 * Prelude Luau di Blocky: la parte delle API Roblox scritta in Luau invece
 * che in C++, caricata prima degli script del gioco.
 *
 * Restituisce una tabella "interna" che il motore tiene nel registry (gli
 * script non la vedono e non possono sostituirla):
 *   makeConnect(orig, once)   avvolge una Connect del C++: restituisce un
 *                             oggetto connessione con Disconnect/Connected
 *                             e, con once, si stacca da solo al primo evento
 *   Signal                    eventi fatti in Luau (AnimationTrack, ...)
 *   CFrame                    CFrame completo: posizione + rotazione
 *   cframe(x,y,z, rx,ry,rz)   CFrame da posizione e gradi YXZ (le Part)
 *   cframeToEulerYXZ(cf)      il contrario, per assegnare part.CFrame
 *   vec3Extra / vec3Index     metodi di Vector3 che mancavano al C++
 *   Random                    Random.new(seed) come Roblox
 *
 * Regola: niente qui dentro deve poter rompere l'avvio. Ogni pezzo usa solo
 * le librerie standard di Luau e Vector3.new del motore.
 */

namespace blk {
namespace luaprelude {

inline const char* source() {
    return R"LUAU(
-- rawset arriva dal motore come argomento: gli script non ce l'hanno (sandbox).
local rawset = ...
local Internal = {}

------------------------------------------------------------------------------
-- Connection / Signal
------------------------------------------------------------------------------
local Connection = {}
Connection.__index = Connection
Connection.__metatable = "RBXScriptConnection"

function Connection:Disconnect()
    if not self.Connected then return end
    self.Connected = false
    if self._signal then self._signal:_remove(self) end
    if self._drop then
        local d = self._drop
        self._drop = nil
        d()
    end
end
Connection.disconnect = Connection.Disconnect

-- Una Connect del C++ diventa una Connect "vera": la funzione dello script
-- passa da una guardia che controlla Connected, e la chiamata restituisce la
-- connessione. Se il C++ restituisce una funzione, e' quella che stacca
-- davvero il listener (RunService): Disconnect la chiama.
function Internal.makeConnect(orig, mode)
    -- mode: false/0 = Connect, true/1 = Once, 2 = Wait
    local once = mode == true or mode == 1
    if mode == 2 then
        local connect = Internal.makeConnect(orig, false)
        return function(self)
            if not coroutine.isyieldable() then
                error(":Wait() va usato dentro task.spawn(function() ... end)", 2)
            end
            local co = coroutine.running()
            local conn
            conn = connect(self, function(...)
                conn:Disconnect()
                task.spawn(co, ...)
            end)
            return coroutine.yield()
        end
    end
    return function(...)
        local args = table.pack(...)
        local fidx = nil
        for i = 1, args.n do
            if type(args[i]) == "function" then fidx = i break end
        end
        if not fidx then return orig(...) end
        local f = args[fidx]
        local conn = setmetatable({ Connected = true }, Connection)
        args[fidx] = function(...)
            if not conn.Connected then return end
            if once then conn:Disconnect() end
            return f(...)
        end
        local drop = orig(table.unpack(args, 1, args.n))
        if type(drop) == "function" then conn._drop = drop end
        return conn
    end
end

local Signal = {}
Signal.__index = Signal
Signal.__metatable = "RBXScriptSignal"

function Signal.new()
    return setmetatable({ _handlers = {} }, Signal)
end

function Signal:Connect(f)
    if type(f) ~= "function" then error("Connect richiede una funzione", 2) end
    local conn = setmetatable({ Connected = true, _signal = self, _fn = f }, Connection)
    table.insert(self._handlers, conn)
    return conn
end

function Signal:Once(f)
    local c
    c = self:Connect(function(...)
        c:Disconnect()
        return f(...)
    end)
    return c
end

function Signal:Wait()
    if not coroutine.isyieldable() then
        error(":Wait() va usato dentro task.spawn(function() ... end)", 2)
    end
    local co = coroutine.running()
    local c
    c = self:Connect(function(...)
        c:Disconnect()
        task.spawn(co, ...)
    end)
    return coroutine.yield()
end

function Signal:Fire(...)
    local list = table.clone(self._handlers)
    for _, c in ipairs(list) do
        if c.Connected then
            local ok, err = pcall(task.spawn, c._fn, ...)
            if not ok then print("Errore in un evento: " .. tostring(err)) end
        end
    end
end

function Signal:_remove(conn)
    for i, c in ipairs(self._handlers) do
        if c == conn then table.remove(self._handlers, i) return end
    end
end

function Signal:DisconnectAll()
    for _, c in ipairs(self._handlers) do c.Connected = false end
    table.clear(self._handlers)
end

Internal.Signal = Signal

------------------------------------------------------------------------------
-- Vector3: i metodi che mancavano
------------------------------------------------------------------------------
-- Vector3 esiste solo dopo registerApi: si chiama al momento, non al caricamento.
local function V(x, y, z) return Vector3.new(x, y, z) end
local function vx(v) return v.X or v.x or 0 end
local function vy(v) return v.Y or v.y or 0 end
local function vz(v) return v.Z or v.z or 0 end

local vec3Extra = {}
function vec3Extra.Dot(a, b) return vx(a) * vx(b) + vy(a) * vy(b) + vz(a) * vz(b) end
function vec3Extra.Cross(a, b)
    return V(vy(a) * vz(b) - vz(a) * vy(b), vz(a) * vx(b) - vx(a) * vz(b), vx(a) * vy(b) - vy(a) * vx(b))
end
function vec3Extra.Lerp(a, b, t) return V(vx(a) + (vx(b) - vx(a)) * t, vy(a) + (vy(b) - vy(a)) * t, vz(a) + (vz(b) - vz(a)) * t) end
function vec3Extra.FuzzyEq(a, b, eps)
    eps = eps or 1e-5
    return math.abs(vx(a) - vx(b)) <= eps and math.abs(vy(a) - vy(b)) <= eps and math.abs(vz(a) - vz(b)) <= eps
end
function vec3Extra.Abs(a) return V(math.abs(vx(a)), math.abs(vy(a)), math.abs(vz(a))) end
function vec3Extra.Floor(a) return V(math.floor(vx(a)), math.floor(vy(a)), math.floor(vz(a))) end
function vec3Extra.Ceil(a) return V(math.ceil(vx(a)), math.ceil(vy(a)), math.ceil(vz(a))) end
function vec3Extra.Sign(a) return V(math.sign(vx(a)), math.sign(vy(a)), math.sign(vz(a))) end
function vec3Extra.Min(a, b) return V(math.min(vx(a), vx(b)), math.min(vy(a), vy(b)), math.min(vz(a), vz(b))) end
function vec3Extra.Max(a, b) return V(math.max(vx(a), vx(b)), math.max(vy(a), vy(b)), math.max(vz(a), vz(b))) end
function vec3Extra.Angle(a, b, axis)
    local la = math.sqrt(vec3Extra.Dot(a, a))
    local lb = math.sqrt(vec3Extra.Dot(b, b))
    if la < 1e-9 or lb < 1e-9 then return 0 end
    local ang = math.acos(math.clamp(vec3Extra.Dot(a, b) / (la * lb), -1, 1))
    if axis and vec3Extra.Dot(vec3Extra.Cross(a, b), axis) < 0 then ang = -ang end
    return ang
end
Internal.vec3Extra = vec3Extra

-- __index del motore (Magnitude, Unit, ...) con prima i metodi nuovi.
function Internal.vec3Index(orig)
    return function(t, k)
        local m = vec3Extra[k]
        if m ~= nil then return m end
        if orig then return orig(t, k) end
        return nil
    end
end

function Internal.vec3Div(a, b)
    if type(b) == "number" then return V(vx(a) / b, vy(a) / b, vz(a) / b) end
    if type(a) == "number" then return V(a / vx(b), a / vy(b), a / vz(b)) end
    return V(vx(a) / vx(b), vy(a) / vy(b), vz(a) / vz(b))
end
function Internal.vec3Eq(a, b) return vx(a) == vx(b) and vy(a) == vy(b) and vz(a) == vz(b) end
function Internal.vec3ToString(a) return string.format("%g, %g, %g", vx(a), vy(a), vz(a)) end


------------------------------------------------------------------------------
-- CFrame: posizione + matrice di rotazione 3x3 (colonne = Right, Up, -Look)
------------------------------------------------------------------------------
local CFrame = {}
local CF = {}

local function make(x, y, z, r00, r01, r02, r10, r11, r12, r20, r21, r22)
    local c = {
        X = x, Y = y, Z = z, x = x, y = y, z = z,
        R00 = r00, R01 = r01, R02 = r02,
        R10 = r10, R11 = r11, R12 = r12,
        R20 = r20, R21 = r21, R22 = r22,
    }
    c.Position = V(x, y, z)
    c.p = c.Position
    return setmetatable(c, CF)
end

local function rotMul(a, b) -- a.R * b.R
    return a.R00 * b.R00 + a.R01 * b.R10 + a.R02 * b.R20, a.R00 * b.R01 + a.R01 * b.R11 + a.R02 * b.R21, a.R00 * b.R02 + a.R01 * b.R12 + a.R02 * b.R22,
           a.R10 * b.R00 + a.R11 * b.R10 + a.R12 * b.R20, a.R10 * b.R01 + a.R11 * b.R11 + a.R12 * b.R21, a.R10 * b.R02 + a.R11 * b.R12 + a.R12 * b.R22,
           a.R20 * b.R00 + a.R21 * b.R10 + a.R22 * b.R20, a.R20 * b.R01 + a.R21 * b.R11 + a.R22 * b.R21, a.R20 * b.R02 + a.R21 * b.R12 + a.R22 * b.R22
end

local function rotX(a) local c, s = math.cos(a), math.sin(a) return 1, 0, 0, 0, c, -s, 0, s, c end
local function rotY(a) local c, s = math.cos(a), math.sin(a) return c, 0, s, 0, 1, 0, -s, 0, c end
local function rotZ(a) local c, s = math.cos(a), math.sin(a) return c, -s, 0, s, c, 0, 0, 0, 1 end

local function fromRot(x, y, z, ...) return make(x, y, z, ...) end
local function rotOnly(...) return make(0, 0, 0, ...) end

-- getmetatable degli script restituisce __metatable ("CFrame"), non la tabella.
local function isCF(v) return type(v) == "table" and rawget(v, "R00") ~= nil and getmetatable(v) == "CFrame" end
local function isV3(v) return type(v) == "table" and (v.X ~= nil or v.x ~= nil) and v.R00 == nil end

function CFrame.new(a, b, c, ...)
    local extra = select("#", ...)
    if a == nil then return make(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1) end
    if isV3(a) then
        if isV3(b) then return CFrame.lookAt(a, b) end
        return make(vx(a), vy(a), vz(a), 1, 0, 0, 0, 1, 0, 0, 0, 1)
    end
    if extra == 9 then return make(a, b, c, ...) end
    if extra == 4 then
        local qx, qy, qz, qw = ...
        local n = math.sqrt(qx * qx + qy * qy + qz * qz + qw * qw)
        if n < 1e-9 then n = 1 end
        qx, qy, qz, qw = qx / n, qy / n, qz / n, qw / n
        return make(a, b, c,
            1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy - qz * qw), 2 * (qx * qz + qy * qw),
            2 * (qx * qy + qz * qw), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz - qx * qw),
            2 * (qx * qz - qy * qw), 2 * (qy * qz + qx * qw), 1 - 2 * (qx * qx + qy * qy))
    end
    return make(a or 0, b or 0, c or 0, 1, 0, 0, 0, 1, 0, 0, 0, 1)
end

function CFrame.Angles(rx, ry, rz) -- Rx * Ry * Rz, come Roblox
    local A = rotOnly(rotX(rx or 0))
    local B = rotOnly(rotY(ry or 0))
    local C = rotOnly(rotZ(rz or 0))
    return rotOnly(rotMul(rotOnly(rotMul(A, B)), C))
end
CFrame.fromEulerAnglesXYZ = CFrame.Angles

function CFrame.fromEulerAnglesYXZ(rx, ry, rz) -- Ry * Rx * Rz
    local Y = rotOnly(rotY(ry or 0))
    local X = rotOnly(rotX(rx or 0))
    local Z = rotOnly(rotZ(rz or 0))
    return rotOnly(rotMul(rotOnly(rotMul(Y, X)), Z))
end
CFrame.fromOrientation = CFrame.fromEulerAnglesYXZ

function CFrame.fromAxisAngle(axis, angle)
    local x, y, z = vx(axis), vy(axis), vz(axis)
    local n = math.sqrt(x * x + y * y + z * z)
    if n < 1e-9 then return CFrame.new() end
    x, y, z = x / n, y / n, z / n
    local c, s = math.cos(angle), math.sin(angle)
    local t = 1 - c
    return make(0, 0, 0,
        t * x * x + c, t * x * y - s * z, t * x * z + s * y,
        t * x * y + s * z, t * y * y + c, t * y * z - s * x,
        t * x * z - s * y, t * y * z + s * x, t * z * z + c)
end

function CFrame.fromMatrix(pos, right, up, back)
    if back == nil then back = vec3Extra.Cross(right, up) end
    return make(vx(pos), vy(pos), vz(pos),
        vx(right), vx(up), vx(back),
        vy(right), vy(up), vy(back),
        vz(right), vz(up), vz(back))
end

function CFrame.lookAt(at, target, up)
    up = up or Vector3.yAxis
    local fx, fy, fz = vx(target) - vx(at), vy(target) - vy(at), vz(target) - vz(at)
    local n = math.sqrt(fx * fx + fy * fy + fz * fz)
    if n < 1e-9 then return CFrame.new(at) end
    fx, fy, fz = fx / n, fy / n, fz / n
    local look = V(fx, fy, fz)
    local right = vec3Extra.Cross(look, up)
    local rl = right.Magnitude or math.sqrt(vec3Extra.Dot(right, right))
    if rl < 1e-6 then right = vec3Extra.Cross(look, Vector3.zAxis) rl = math.sqrt(vec3Extra.Dot(right, right)) end
    right = V(vx(right) / rl, vy(right) / rl, vz(right) / rl)
    local up2 = vec3Extra.Cross(right, look)
    return CFrame.fromMatrix(at, right, up2, V(-fx, -fy, -fz))
end

-- Metodi e proprieta' calcolate
local methods = {}
CF.__index = function(t, k)
    local m = methods[k]
    if m then return m end
    if k == "LookVector" or k == "lookVector" then return V(-t.R02, -t.R12, -t.R22) end
    if k == "RightVector" or k == "rightVector" then return V(t.R00, t.R10, t.R20) end
    if k == "UpVector" or k == "upVector" then return V(t.R01, t.R11, t.R21) end
    if k == "XVector" then return V(t.R00, t.R10, t.R20) end
    if k == "YVector" then return V(t.R01, t.R11, t.R21) end
    if k == "ZVector" then return V(t.R02, t.R12, t.R22) end
    if k == "Rotation" then return rotOnly(t.R00, t.R01, t.R02, t.R10, t.R11, t.R12, t.R20, t.R21, t.R22) end
    return nil
end
CF.__metatable = "CFrame"

local function pointTo(cf, v) -- cf * v
    local x, y, z = vx(v), vy(v), vz(v)
    return V(cf.R00 * x + cf.R01 * y + cf.R02 * z + cf.X,
             cf.R10 * x + cf.R11 * y + cf.R12 * z + cf.Y,
             cf.R20 * x + cf.R21 * y + cf.R22 * z + cf.Z)
end

function methods.Inverse(cf)
    -- rotazione trasposta, posizione -R^T * p
    local x, y, z = cf.X, cf.Y, cf.Z
    return make(-(cf.R00 * x + cf.R10 * y + cf.R20 * z), -(cf.R01 * x + cf.R11 * y + cf.R21 * z), -(cf.R02 * x + cf.R12 * y + cf.R22 * z),
        cf.R00, cf.R10, cf.R20, cf.R01, cf.R11, cf.R21, cf.R02, cf.R12, cf.R22)
end
methods.inverse = methods.Inverse

local function mulCF(a, b)
    local p = pointTo(a, b.Position)
    return fromRot(vx(p), vy(p), vz(p), rotMul(a, b))
end

function methods.ToWorldSpace(a, b) return mulCF(a, b or CFrame.identity) end
function methods.ToObjectSpace(a, b) return mulCF(methods.Inverse(a), b or CFrame.identity) end
function methods.PointToWorldSpace(a, v) return pointTo(a, v) end
function methods.PointToObjectSpace(a, v) return pointTo(methods.Inverse(a), v) end
function methods.VectorToWorldSpace(a, v)
    local x, y, z = vx(v), vy(v), vz(v)
    return V(a.R00 * x + a.R01 * y + a.R02 * z, a.R10 * x + a.R11 * y + a.R12 * z, a.R20 * x + a.R21 * y + a.R22 * z)
end
function methods.VectorToObjectSpace(a, v)
    local x, y, z = vx(v), vy(v), vz(v)
    return V(a.R00 * x + a.R10 * y + a.R20 * z, a.R01 * x + a.R11 * y + a.R21 * z, a.R02 * x + a.R12 * y + a.R22 * z)
end
function methods.GetComponents(a)
    return a.X, a.Y, a.Z, a.R00, a.R01, a.R02, a.R10, a.R11, a.R12, a.R20, a.R21, a.R22
end
methods.components = methods.GetComponents

-- Quaternione dalla matrice (per Lerp e ToAxisAngle).
local function toQuat(a)
    local tr = a.R00 + a.R11 + a.R22
    local qw, qx, qy, qz
    if tr > 0 then
        local s = math.sqrt(tr + 1) * 2
        qw, qx, qy, qz = 0.25 * s, (a.R21 - a.R12) / s, (a.R02 - a.R20) / s, (a.R10 - a.R01) / s
    elseif a.R00 > a.R11 and a.R00 > a.R22 then
        local s = math.sqrt(1 + a.R00 - a.R11 - a.R22) * 2
        qw, qx, qy, qz = (a.R21 - a.R12) / s, 0.25 * s, (a.R01 + a.R10) / s, (a.R02 + a.R20) / s
    elseif a.R11 > a.R22 then
        local s = math.sqrt(1 + a.R11 - a.R00 - a.R22) * 2
        qw, qx, qy, qz = (a.R02 - a.R20) / s, (a.R01 + a.R10) / s, 0.25 * s, (a.R12 + a.R21) / s
    else
        local s = math.sqrt(1 + a.R22 - a.R00 - a.R11) * 2
        qw, qx, qy, qz = (a.R10 - a.R01) / s, (a.R02 + a.R20) / s, (a.R12 + a.R21) / s, 0.25 * s
    end
    return qx, qy, qz, qw
end

function methods.Lerp(a, b, t)
    local ax, ay, az, aw = toQuat(a)
    local bx, by, bz, bw = toQuat(b)
    local d = ax * bx + ay * by + az * bz + aw * bw
    if d < 0 then bx, by, bz, bw, d = -bx, -by, -bz, -bw, -d end
    local k0, k1
    if d > 0.9995 then
        k0, k1 = 1 - t, t
    else
        local th = math.acos(d)
        local s = math.sin(th)
        k0, k1 = math.sin((1 - t) * th) / s, math.sin(t * th) / s
    end
    return CFrame.new(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t,
        ax * k0 + bx * k1, ay * k0 + by * k1, az * k0 + bz * k1, aw * k0 + bw * k1)
end
methods.lerp = methods.Lerp

function methods.ToAxisAngle(a)
    local qx, qy, qz, qw = toQuat(a)
    qw = math.clamp(qw, -1, 1)
    local angle = 2 * math.acos(qw)
    local s = math.sqrt(1 - qw * qw)
    if s < 1e-6 then return Vector3.xAxis, 0 end
    return V(qx / s, qy / s, qz / s), angle
end

function methods.ToEulerAnglesXYZ(a) -- inversa di CFrame.Angles
    local ry = math.asin(math.clamp(a.R02, -1, 1))
    local rx, rz
    if math.abs(a.R02) < 0.99999 then
        rx = math.atan2(-a.R12, a.R22)
        rz = math.atan2(-a.R01, a.R00)
    else
        rx = math.atan2(a.R21, a.R11)
        rz = 0
    end
    return rx, ry, rz
end
methods.toEulerAnglesXYZ = methods.ToEulerAnglesXYZ

function methods.ToEulerAnglesYXZ(a) -- inversa di fromEulerAnglesYXZ
    local rx = math.asin(math.clamp(-a.R12, -1, 1))
    local ry, rz
    if math.abs(a.R12) < 0.99999 then
        ry = math.atan2(a.R02, a.R22)
        rz = math.atan2(a.R10, a.R11)
    else
        ry = math.atan2(-a.R20, a.R00)
        rz = 0
    end
    return rx, ry, rz
end
methods.ToOrientation = methods.ToEulerAnglesYXZ

function methods.FuzzyEq(a, b, eps)
    eps = eps or 1e-5
    local ca = { a:GetComponents() }
    local cb = { b:GetComponents() }
    for i = 1, 12 do if math.abs(ca[i] - cb[i]) > eps then return false end end
    return true
end

function methods.Orthonormalize(a)
    local r = V(a.R00, a.R10, a.R20)
    local u = V(a.R01, a.R11, a.R21)
    local rl = math.sqrt(vec3Extra.Dot(r, r))
    r = V(vx(r) / rl, vy(r) / rl, vz(r) / rl)
    local d = vec3Extra.Dot(u, r)
    u = V(vx(u) - vx(r) * d, vy(u) - vy(r) * d, vz(u) - vz(r) * d)
    local ul = math.sqrt(vec3Extra.Dot(u, u))
    u = V(vx(u) / ul, vy(u) / ul, vz(u) / ul)
    return CFrame.fromMatrix(a.Position, r, u)
end

CF.__mul = function(a, b)
    if isCF(b) then return mulCF(a, b) end
    if isV3(b) then return pointTo(a, b) end
    error("CFrame * " .. typeof(b) .. " non e' valido", 2)
end
CF.__add = function(a, b)
    if not isV3(b) then error("CFrame + Vector3", 2) end
    return fromRot(a.X + vx(b), a.Y + vy(b), a.Z + vz(b), a.R00, a.R01, a.R02, a.R10, a.R11, a.R12, a.R20, a.R21, a.R22)
end
CF.__sub = function(a, b)
    if not isV3(b) then error("CFrame - Vector3", 2) end
    return fromRot(a.X - vx(b), a.Y - vy(b), a.Z - vz(b), a.R00, a.R01, a.R02, a.R10, a.R11, a.R12, a.R20, a.R21, a.R22)
end
CF.__eq = function(a, b) return isCF(b) and methods.FuzzyEq(a, b, 0) end
CF.__tostring = function(a)
    return string.format("%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g", a:GetComponents())
end

Internal.CFrame = CFrame

-- Le Part di Blocky ruotano in gradi YXZ (Ry * Rx * Rz).
function Internal.cframe(x, y, z, rxDeg, ryDeg, rzDeg)
    local r = CFrame.fromEulerAnglesYXZ(math.rad(rxDeg or 0), math.rad(ryDeg or 0), math.rad(rzDeg or 0))
    return fromRot(x, y, z, r.R00, r.R01, r.R02, r.R10, r.R11, r.R12, r.R20, r.R21, r.R22)
end

function Internal.cframeToEulerYXZ(cf)
    if not isCF(cf) then return nil end
    local rx, ry, rz = methods.ToEulerAnglesYXZ(cf)
    return math.deg(rx), math.deg(ry), math.deg(rz)
end

------------------------------------------------------------------------------
-- Random
------------------------------------------------------------------------------
local Random = {}
Random.__index = Random
Random.__metatable = "Random"

local function nextU32(self) -- xorshift32
    local x = self._s
    x = bit32.bxor(x, bit32.lshift(x, 13))
    x = bit32.bxor(x, bit32.rshift(x, 17))
    x = bit32.bxor(x, bit32.lshift(x, 5))
    self._s = x
    return x
end

local seedCounter = 0
function Random.new(seed)
    local s
    if seed ~= nil then
        s = bit32.band(math.floor(seed), 0xFFFFFFFF)
    else
        seedCounter += 1
        s = bit32.band(math.floor(os.clock() * 1e6) + seedCounter * 2654435761, 0xFFFFFFFF)
    end
    if s == 0 then s = 0x9E3779B9 end
    local r = setmetatable({ _s = s }, Random)
    for _ = 1, 4 do nextU32(r) end
    return r
end

function Random:NextNumber(a, b)
    local u = nextU32(self) / 4294967296
    if a == nil then return u end
    return a + (b - a) * u
end

function Random:NextInteger(a, b)
    a, b = math.floor(a), math.floor(b)
    if a > b then a, b = b, a end
    return a + nextU32(self) % (b - a + 1)
end

function Random:NextUnitVector()
    local z = self:NextNumber(-1, 1)
    local t = self:NextNumber(0, 2 * math.pi)
    local r = math.sqrt(math.max(0, 1 - z * z))
    return V(r * math.cos(t), r * math.sin(t), z)
end

function Random:Shuffle(t)
    for i = #t, 2, -1 do
        local j = self:NextInteger(1, i)
        t[i], t[j] = t[j], t[i]
    end
end

function Random:Clone()
    return setmetatable({ _s = self._s }, Random)
end

Internal.Random = Random

------------------------------------------------------------------------------
-- typeof come Roblox: "Vector3", "CFrame", "RBXScriptSignal"...
------------------------------------------------------------------------------
local kNamed = { Vector3 = true, CFrame = true, Random = true, RBXScriptSignal = true, RBXScriptConnection = true,
                 Color3 = true, AnimationTrack = true, Ray = true, ColorSequence = true, ColorSequenceKeypoint = true,
                 PhysicalProperties = true, DateTime = true, Axes = true, Faces = true, Instance = true, Vector2 = true }
local function installTypeof()
    -- Avvolge il typeof del motore (StudioProApis), che conosce Instance, UDim2...
    local baseTypeof = typeof
    typeof = function(v)
    if type(v) == "table" then
        local m = getmetatable(v)
        if type(m) == "string" and kNamed[m] then return m end
        if rawget(v, "ClassName") ~= nil then return "Instance" end
    end
    return baseTypeof(v)
    end
end

------------------------------------------------------------------------------
-- Instance generici (classi che il motore non conosce: Attachment, Highlight,
-- Configuration, BillboardGui...): albero vero tra di loro, eventi, attributi.
------------------------------------------------------------------------------
local store = setmetatable({}, { __mode = "k" })
local INST = { __metatable = "Instance" }
local instMethods = {}
-- Riempite piu' sotto (classi con metodi/segnali propri, figli di oggetti del motore).
local classMethods, classSignals, initClass, foreignList, adoptContainer
local kSignals = { Changed = true, ChildAdded = true, ChildRemoved = true, DescendantAdded = true,
                   DescendantRemoving = true, AncestryChanged = true, AttributeChanged = true, Destroying = true }

-- Un po' di ereditarieta' per IsA, come la gerarchia di classi di Roblox.
local kBase = {
    IntValue = "ValueBase", NumberValue = "ValueBase", StringValue = "ValueBase", BoolValue = "ValueBase",
    ObjectValue = "ValueBase", Vector3Value = "ValueBase", CFrameValue = "ValueBase", Color3Value = "ValueBase",
    BillboardGui = "LayerCollector", SurfaceGui = "LayerCollector", ScreenGui = "LayerCollector",
    Attachment = "Instance", Highlight = "Instance", Configuration = "Instance",
    Beam = "Instance", Trail = "Instance", Decal = "FaceInstance", Texture = "Decal",
    Model = "PVInstance", PVInstance = "Instance",
    Part = "BasePart", BasePart = "PVInstance",
}

local function sigOf(s, name)
    local sg = s.signals[name]
    if not sg then sg = Signal.new() s.signals[name] = sg end
    return sg
end

local function fireIf(s, name, ...)
    local sg = s.signals[name]
    if sg then sg:Fire(...) end
end

local function isLuaInstance(v) return type(v) == "table" and store[v] ~= nil end

local function forAncestors(inst, fn)
    local s = store[inst]
    local p = s and s.data.Parent
    while isLuaInstance(p) do
        fn(p)
        p = store[p].data.Parent
    end
end

local function setParent(self, parent)
    local s = store[self]
    if s.destroyed and parent ~= nil then error("l'oggetto e' stato distrutto (Destroy)", 3) end
    if parent == self then error("un oggetto non puo' essere figlio di se stesso", 3) end
    if isLuaInstance(parent) then
        local p = parent
        while isLuaInstance(p) do
            if p == self then error("Parent creerebbe un cerchio", 3) end
            p = store[p].data.Parent
        end
    end
    local old = s.data.Parent
    if old == parent then return end
    if isLuaInstance(old) then
        local os_ = store[old]
        forAncestors(self, function(a) fireIf(store[a], "DescendantRemoving", self) end)
        for i, c in ipairs(os_.children) do if c == self then table.remove(os_.children, i) break end end
        fireIf(os_, "ChildRemoved", self)
    end
    if old ~= nil and not isLuaInstance(old) then
        local l = foreignList(old)
        for i, c in ipairs(l) do if c == self then table.remove(l, i) break end end
        if type(old) == "table" and rawget(old, "__blkAdopted") and rawget(old, s.data.Name) == self then rawset(old, s.data.Name, nil) end
    end
    s.data.Parent = parent
    if isLuaInstance(parent) then
        table.insert(store[parent].children, self)
        fireIf(store[parent], "ChildAdded", self)
        forAncestors(self, function(a) fireIf(store[a], "DescendantAdded", self) end)
    end
    if parent ~= nil and not isLuaInstance(parent) then
        table.insert(foreignList(parent), self)
        -- player.leaderstats: sui contenitori adottati il figlio si legge anche per nome
        if type(parent) == "table" and rawget(parent, "__blkAdopted") and rawget(parent, s.data.Name) == nil then
            rawset(parent, s.data.Name, self)
        end
    end
    fireIf(s, "AncestryChanged", self, parent)
    fireIf(s, "Changed", "Parent")
    local ps = s.propSignals.Parent
    if ps then ps:Fire() end
end

INST.__index = function(self, k)
    local m = instMethods[k]
    if m then return m end
    local s = store[self]
    if not s then return nil end
    local cm = classMethods[s.data.ClassName]
    if cm and cm[k] then return cm[k] end
    if kSignals[k] then return sigOf(s, k) end
    local cs = classSignals[s.data.ClassName]
    if cs and cs[k] then return sigOf(s, k) end
    local v = s.data[k]
    if v ~= nil then return v end
    for _, c in ipairs(s.children) do
        if store[c] and store[c].data.Name == k then return c end
    end
    return nil
end

INST.__newindex = function(self, k, v)
    local s = store[self]
    if not s then return end
    if k == "Parent" then setParent(self, v) return end
    if k == "ClassName" then error("ClassName non si puo' cambiare", 2) end
    if instMethods[k] or kSignals[k] then error(tostring(k) .. " non si puo' cambiare", 2) end
    local old = s.data[k]
    s.data[k] = v
    if old ~= v then
        -- Le classi Value passano il valore nuovo a Changed (come Roblox), le altre il nome.
        if s.isValue and k == "Value" then fireIf(s, "Changed", v) else fireIf(s, "Changed", k) end
        local ps = s.propSignals[k]
        if ps then ps:Fire() end
        if s.data.ClassName == "Humanoid" and k == "Health" then
            if type(v) == "number" and v > (s.data.MaxHealth or 100) then s.data.Health = s.data.MaxHealth v = s.data.MaxHealth end
            fireIf(s, "HealthChanged", v)
            if type(v) == "number" and v <= 0 and not s.dead then s.dead = true fireIf(s, "Died") end
        end
    end
end

INST.__tostring = function(self)
    local s = store[self]
    return s and tostring(s.data.Name) or "Instance"
end

local function newInstance(className, parent)
    local self = {}
    store[self] = { data = { ClassName = className, Name = className, Archivable = true },
                    children = {}, attrs = {}, signals = {}, propSignals = {}, attrSignals = {} }
    setmetatable(self, INST)
    if initClass then initClass(self, className) end
    if parent ~= nil then setParent(self, parent) end
    return self
end
Internal.newInstance = newInstance

-- Figli misti: Instance Luau e oggetti del motore (part messe in un Folder Luau...).
local function childName(c)
    if store[c] then return store[c].data.Name end
    local ok, n = pcall(function() return c.Name end)
    return ok and n or nil
end
local function childClass(c)
    if store[c] then return store[c].data.ClassName end
    local ok, n = pcall(function() return c.ClassName end)
    return ok and n or nil
end

function instMethods.GetChildren(self) return table.clone(store[self].children) end

function instMethods.GetDescendants(self)
    local out = {}
    local function walk(i)
        for _, c in ipairs(store[i].children) do
            table.insert(out, c)
            if store[c] then walk(c) end
        end
    end
    walk(self)
    return out
end

function instMethods.FindFirstChild(self, name, recursive)
    for _, c in ipairs(store[self].children) do
        if childName(c) == name then return c end
    end
    if recursive then
        for _, c in ipairs(store[self].children) do
            if store[c] then
                local f = instMethods.FindFirstChild(c, name, true)
                if f then return f end
            end
        end
    end
    return nil
end

------------------------------------------------------------------------------
-- Part vere (motore): segnali per indice e ingresso in parent Luau.
------------------------------------------------------------------------------
local partSig = {}
local partLuaParent = {} -- indice part -> Instance Luau che la contiene
function Internal.luaParentOfPart(idx) return partLuaParent[idx] end
-- FindFirstAncestor / FindFirstAncestorOfClass oltre i parent del motore.
function Internal.luaAncestorOfPart(idx, wanted, byClass)
    local p = partLuaParent[idx]
    while isLuaInstance(p) do
        local d = store[p].data
        if byClass then
            if d.ClassName == wanted or instMethods.IsA(p, wanted) then return p end
        elseif d.Name == wanted then return p end
        p = d.Parent
    end
    return nil
end
local function partEntry(idx)
    local e = partSig[idx]
    if not e then e = { props = {} } partSig[idx] = e end
    return e
end
function Internal.partSignal(idx, kind)
    local e = partEntry(idx)
    if kind == nil then
        e.changed = e.changed or Signal.new()
        return e.changed
    elseif kind == "\1destroying" then
        e.destroying = e.destroying or Signal.new()
        return e.destroying
    end
    e.props[kind] = e.props[kind] or Signal.new()
    return e.props[kind]
end
function Internal.firePartChanged(idx, prop)
    local e = partSig[idx]
    if not e then return end
    if e.changed then e.changed:Fire(prop) end
    local p = e.props[prop]
    if p then p:Fire() end
end
function Internal.partDestroying(idx)
    local e = partSig[idx]
    if e and e.destroying then e.destroying:Fire() end
    local lp = partLuaParent[idx]
    partLuaParent[idx] = nil
    if isLuaInstance(lp) then
        local s = store[lp]
        for i = #s.children, 1, -1 do
            local c = s.children[i]
            if not store[c] and type(c) == "table" and rawget(c, "__partIndex") == idx then
                table.remove(s.children, i)
                fireIf(s, "ChildRemoved", c)
            end
        end
    end
end
-- Il motore ha tolto la part idx: gli indici dopo scalano di uno.
function Internal.partRemoved(idx)
    local moved = {}
    for k, v in pairs(partSig) do
        if k < idx then moved[k] = v elseif k > idx then moved[k - 1] = v end
    end
    partSig = moved
    local lp = {}
    for k, v in pairs(partLuaParent) do
        if k < idx then lp[k] = v elseif k > idx then lp[k - 1] = v end
    end
    partLuaParent = lp
    for _, s in pairs(store) do
        for i = #s.children, 1, -1 do
            local c = s.children[i]
            if not store[c] and type(c) == "table" then
                local pi = rawget(c, "__partIndex")
                if pi == idx or (type(pi) == "number" and pi < 0) then
                    table.remove(s.children, i)
                    fireIf(s, "ChildRemoved", c)
                end
                -- gli indici delle part dopo li aggiorna il motore (remapPartTables)
            end
        end
    end
end
-- Una part del motore ha ricevuto un Parent.
function Internal.partParented(part, parent)
    if type(part) ~= "table" then return end
    local pi = rawget(part, "__partIndex")
    -- toglierla da un eventuale parent Luau di prima
    for _, s in pairs(store) do
        for i = #s.children, 1, -1 do
            local c = s.children[i]
            if c == part or (not store[c] and type(c) == "table" and pi ~= nil and rawget(c, "__partIndex") == pi) then
                table.remove(s.children, i)
                fireIf(s, "ChildRemoved", c)
            end
        end
    end
    if type(pi) == "number" then partLuaParent[pi] = isLuaInstance(parent) and parent or nil end
    if isLuaInstance(parent) then
        table.insert(store[parent].children, part)
        rawset(part, "Parent", parent)
        fireIf(store[parent], "ChildAdded", part)
        local p = parent
        while isLuaInstance(p) do
            fireIf(store[p], "DescendantAdded", part)
            p = store[p].data.Parent
        end
    elseif parent ~= nil and type(parent) == "table" and rawget(parent, "__partIndex") == nil then
        table.insert(foreignList(parent), part)
    end
end

function instMethods.IsA(self, cls)
    local c = store[self].data.ClassName
    if cls == "Instance" then return true end
    while c ~= nil do
        if c == cls then return true end
        c = kBase[c]
    end
    return false
end

function instMethods.FindFirstChildOfClass(self, cls)
    for _, c in ipairs(store[self].children) do if childClass(c) == cls then return c end end
    return nil
end

function instMethods.FindFirstChildWhichIsA(self, cls, recursive)
    for _, c in ipairs(store[self].children) do
        if store[c] then
            if instMethods.IsA(c, cls) then return c end
        else
            local ok, yes = pcall(function() return c:IsA(cls) end)
            if ok and yes then return c end
        end
    end
    if recursive then
        for _, c in ipairs(store[self].children) do
            local f = store[c] and instMethods.FindFirstChildWhichIsA(c, cls, true)
            if f then return f end
        end
    end
    return nil
end

local function ancestorWhere(self, pred)
    local p = store[self].data.Parent
    while p ~= nil do
        if pred(p) then return p end
        if not isLuaInstance(p) then return nil end
        p = store[p].data.Parent
    end
    return nil
end
function instMethods.FindFirstAncestor(self, name) return ancestorWhere(self, function(p) return p.Name == name end) end
function instMethods.FindFirstAncestorOfClass(self, cls) return ancestorWhere(self, function(p) return p.ClassName == cls end) end
function instMethods.FindFirstAncestorWhichIsA(self, cls)
    return ancestorWhere(self, function(p) return (isLuaInstance(p) and instMethods.IsA(p, cls)) or p.ClassName == cls end)
end

function instMethods.IsDescendantOf(self, anc)
    return ancestorWhere(self, function(p) return p == anc end) ~= nil
end
function instMethods.IsAncestorOf(self, desc)
    return isLuaInstance(desc) and instMethods.IsDescendantOf(desc, self)
end

function instMethods.GetFullName(self)
    local parts = { tostring(store[self].data.Name) }
    local p = store[self].data.Parent
    while p ~= nil and p ~= game do
        table.insert(parts, 1, tostring(p.Name))
        if not isLuaInstance(p) then break end
        p = store[p].data.Parent
    end
    return table.concat(parts, ".")
end

function instMethods.WaitForChild(self, name, timeout)
    local found = instMethods.FindFirstChild(self, name)
    if found then return found end
    if not coroutine.isyieldable() then
        error(":WaitForChild() va usato dentro task.spawn quando il figlio non c'e' ancora", 2)
    end
    local co = coroutine.running()
    local done = false
    local conn
    conn = sigOf(store[self], "ChildAdded"):Connect(function(c)
        if not done and childName(c) == name then
            done = true
            conn:Disconnect()
            task.spawn(co, c)
        end
    end)
    if timeout then
        task.delay(timeout, function()
            if not done then done = true conn:Disconnect() task.spawn(co, nil) end
        end)
    end
    return coroutine.yield()
end

function instMethods.ClearAllChildren(self)
    for _, c in ipairs(table.clone(store[self].children)) do
        if store[c] then instMethods.Destroy(c) else pcall(function() c:Destroy() end) end
    end
    -- i figli del motore escono dalla lista quando il motore li toglie davvero
    local s = store[self]
    for i = #s.children, 1, -1 do if not store[s.children[i]] then table.remove(s.children, i) end end
end

function instMethods.Destroy(self)
    local s = store[self]
    if not s or s.destroyed then return end
    fireIf(s, "Destroying")
    for _, c in ipairs(table.clone(s.children)) do
        if store[c] then instMethods.Destroy(c) else pcall(function() c:Destroy() end) end
    end
    setParent(self, nil)
    s.destroyed = true
    for _, sg in pairs(s.signals) do sg:DisconnectAll() end
end
instMethods.Remove = function(self) setParent(self, nil) end

function instMethods.Clone(self)
    local s = store[self]
    if s.data.Archivable == false then return nil end
    local c = newInstance(s.data.ClassName)
    local cs = store[c]
    for k, v in pairs(s.data) do if k ~= "Parent" then cs.data[k] = v end end
    for k, v in pairs(s.attrs) do cs.attrs[k] = v end
    for _, ch in ipairs(s.children) do
        local cc = instMethods.Clone(ch)
        if cc then setParent(cc, c) end
    end
    return c
end

function instMethods.GetAttribute(self, name) return store[self].attrs[name] end
function instMethods.GetAttributes(self) return table.clone(store[self].attrs) end
function instMethods.SetAttribute(self, name, value)
    local s = store[self]
    local old = s.attrs[name]
    s.attrs[name] = value
    if old ~= value then
        fireIf(s, "AttributeChanged", name)
        local as = s.attrSignals[name]
        if as then as:Fire() end
    end
end
function instMethods.GetAttributeChangedSignal(self, name)
    local s = store[self]
    local sg = s.attrSignals[name]
    if not sg then sg = Signal.new() s.attrSignals[name] = sg end
    return sg
end
function instMethods.GetPropertyChangedSignal(self, prop)
    local s = store[self]
    local sg = s.propSignals[prop]
    if not sg then sg = Signal.new() s.propSignals[prop] = sg end
    return sg
end
function instMethods.GetDebugId(self) return tostring(self):sub(1, 32) end

------------------------------------------------------------------------------
-- Tipi di Roblox che mancavano: Ray, ColorSequence, PhysicalProperties,
-- DateTime, Axes, Faces
------------------------------------------------------------------------------
local Ray = {}
local RayMT = { __metatable = "Ray" }
local rayMethods = {}
RayMT.__index = function(r, k)
    if k == "Unit" then
        local d = rawget(r, "Direction")
        local l = math.sqrt(vec3Extra.Dot(d, d))
        if l < 1e-9 then return r end
        return Ray.new(rawget(r, "Origin"), V(vx(d) / l, vy(d) / l, vz(d) / l))
    end
    return rayMethods[k]
end
function Ray.new(origin, direction)
    return setmetatable({ Origin = origin or V(0, 0, 0), Direction = direction or V(0, 0, 0) }, RayMT)
end
function rayMethods.ClosestPoint(r, p)
    local o, d = r.Origin, r.Direction
    local dd = vec3Extra.Dot(d, d)
    local t = dd > 1e-12 and math.max(0, (vec3Extra.Dot(V(vx(p) - vx(o), vy(p) - vy(o), vz(p) - vz(o)), d)) / dd) or 0
    return V(vx(o) + vx(d) * t, vy(o) + vy(d) * t, vz(o) + vz(d) * t)
end
function rayMethods.Distance(r, p)
    local c = rayMethods.ClosestPoint(r, p)
    local dx, dy, dz = vx(p) - vx(c), vy(p) - vy(c), vz(p) - vz(c)
    return math.sqrt(dx * dx + dy * dy + dz * dz)
end
Internal.Ray = Ray

-- Vector2 (GUI, mouse, AnchorPoint).
local Vector2 = {}
local V2 = { __metatable = "Vector2" }
local v2Methods = {}
local function v2(x, y) return setmetatable({ X = x or 0, Y = y or 0, x = x or 0, y = y or 0 }, V2) end
function Vector2.new(x, y) return v2(x, y) end
V2.__index = function(t, k)
    if k == "Magnitude" then return math.sqrt(t.X * t.X + t.Y * t.Y) end
    if k == "Unit" then
        local m = math.sqrt(t.X * t.X + t.Y * t.Y)
        return m > 1e-9 and v2(t.X / m, t.Y / m) or v2(0, 0)
    end
    return v2Methods[k]
end
local function n2(a) if type(a) == "number" then return a, a end return a.X, a.Y end
V2.__add = function(a, b) local ax, ay = n2(a) local bx, by = n2(b) return v2(ax + bx, ay + by) end
V2.__sub = function(a, b) local ax, ay = n2(a) local bx, by = n2(b) return v2(ax - bx, ay - by) end
V2.__mul = function(a, b) local ax, ay = n2(a) local bx, by = n2(b) return v2(ax * bx, ay * by) end
V2.__div = function(a, b) local ax, ay = n2(a) local bx, by = n2(b) return v2(ax / bx, ay / by) end
V2.__unm = function(a) return v2(-a.X, -a.Y) end
V2.__eq = function(a, b) return a.X == b.X and a.Y == b.Y end
V2.__tostring = function(a) return string.format("%g, %g", a.X, a.Y) end
function v2Methods.Dot(a, b) return a.X * b.X + a.Y * b.Y end
function v2Methods.Cross(a, b) return a.X * b.Y - a.Y * b.X end
function v2Methods.Lerp(a, b, t) return v2(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t) end
function v2Methods.FuzzyEq(a, b, eps) eps = eps or 1e-5 return math.abs(a.X - b.X) <= eps and math.abs(a.Y - b.Y) <= eps end
function v2Methods.Abs(a) return v2(math.abs(a.X), math.abs(a.Y)) end
function v2Methods.Floor(a) return v2(math.floor(a.X), math.floor(a.Y)) end
function v2Methods.Ceil(a) return v2(math.ceil(a.X), math.ceil(a.Y)) end
function v2Methods.Sign(a) return v2(math.sign(a.X), math.sign(a.Y)) end
function v2Methods.Min(a, b) return v2(math.min(a.X, b.X), math.min(a.Y, b.Y)) end
function v2Methods.Max(a, b) return v2(math.max(a.X, b.X), math.max(a.Y, b.Y)) end
function v2Methods.Angle(a, b, signed)
    local ang = math.atan2(a:Cross(b), a:Dot(b))
    return signed and ang or math.abs(ang)
end
Internal.Vector2 = Vector2
Internal.v2install = function()
    Vector2.zero = v2(0, 0) Vector2.one = v2(1, 1) Vector2.xAxis = v2(1, 0) Vector2.yAxis = v2(0, 1)
end

local ColorSequenceKeypoint = {}
function ColorSequenceKeypoint.new(time, color)
    return setmetatable({ Time = time, Value = color }, { __metatable = "ColorSequenceKeypoint" })
end
local ColorSequence = {}
function ColorSequence.new(a, b)
    local kps
    if type(a) == "table" and a[1] ~= nil then
        kps = a
    elseif b ~= nil then
        kps = { ColorSequenceKeypoint.new(0, a), ColorSequenceKeypoint.new(1, b) }
    else
        kps = { ColorSequenceKeypoint.new(0, a), ColorSequenceKeypoint.new(1, a) }
    end
    return setmetatable({ Keypoints = kps }, { __metatable = "ColorSequence" })
end
Internal.ColorSequence = ColorSequence
Internal.ColorSequenceKeypoint = ColorSequenceKeypoint

local PhysicalProperties = {}
function PhysicalProperties.new(density, friction, elasticity, frictionWeight, elasticityWeight)
    if type(density) ~= "number" then density, friction, elasticity = 0.7, 0.3, 0.5 end
    return setmetatable({ Density = density, Friction = friction or 0.3, Elasticity = elasticity or 0.5,
                          FrictionWeight = frictionWeight or 1, ElasticityWeight = elasticityWeight or 1 },
                        { __metatable = "PhysicalProperties" })
end
Internal.PhysicalProperties = PhysicalProperties

local DateTime = {}
local DTMT = { __metatable = "DateTime" }
local dtMethods = {}
DTMT.__index = dtMethods
local function makeDT(ms)
    return setmetatable({ UnixTimestampMillis = ms, UnixTimestamp = math.floor(ms / 1000) }, DTMT)
end
function DateTime.now() return makeDT(math.floor(os.time() * 1000)) end
function DateTime.fromUnixTimestamp(s) return makeDT(math.floor(s * 1000)) end
function DateTime.fromUnixTimestampMillis(ms) return makeDT(math.floor(ms)) end
local function dtTable(t, ms)
    return { Year = t.year, Month = t.month, Day = t.day, Hour = t.hour, Minute = t.min, Second = t.sec,
             Millisecond = ms % 1000 }
end
function dtMethods.ToUniversalTime(d) return dtTable(os.date("!*t", d.UnixTimestamp), d.UnixTimestampMillis) end
function dtMethods.ToLocalTime(d) return dtTable(os.date("*t", d.UnixTimestamp), d.UnixTimestampMillis) end
function dtMethods.ToIsoDate(d) return os.date("!%Y-%m-%dT%H:%M:%SZ", d.UnixTimestamp) end
function dtMethods.FormatUniversalTime(d, fmt) return dtMethods.ToIsoDate(d) end
function dtMethods.FormatLocalTime(d, fmt) return os.date("%Y-%m-%d %H:%M:%S", d.UnixTimestamp) end
Internal.DateTime = DateTime

local function flagsType(name, keys)
    local T = {}
    function T.new(...)
        local o = {}
        for _, k in ipairs(keys) do o[k] = false end
        for _, a in ipairs({ ... }) do
            local n = type(a) == "table" and a.Name or a
            if n ~= nil then o[n] = true end
        end
        return setmetatable(o, { __metatable = name })
    end
    return T
end
Internal.Axes = flagsType("Axes", { "X", "Y", "Z", "Top", "Bottom", "Left", "Right", "Front", "Back" })
Internal.Faces = flagsType("Faces", { "Top", "Bottom", "Left", "Right", "Front", "Back" })

------------------------------------------------------------------------------
-- Servizi che il motore non ha: rispondono come Roblox fuori da Roblox
-- (niente badge, niente gruppi, niente VR) invece di far fallire GetService.
------------------------------------------------------------------------------
local extraServices = {}
local function service(name, t)
    t.Name = name
    t.ClassName = name
    extraServices[name] = setmetatable(t, { __metatable = "Instance" })
end
service("TextService", {
    GetTextSize = function(_, text, size, _font, _frame)
        size = size or 14
        return Vector2 and Vector2.new(#tostring(text) * size * 0.5, size) or { X = #tostring(text) * size * 0.5, Y = size }
    end,
    FilterStringAsync = function(_, text) return { GetNonChatStringForBroadcastAsync = function() return text end,
                                                   GetChatForUserAsync = function() return text end } end,
})
service("PolicyService", {
    GetPolicyInfoForPlayerAsync = function()
        return { AreAdsAllowed = false, ArePaidRandomItemsRestricted = false, AllowedExternalLinkReferences = {},
                 IsPaidItemTradingAllowed = true, IsSubjectToChinaPolicies = false, IsContentSharingAllowed = true }
    end,
})
service("BadgeService", {
    UserHasBadgeAsync = function() return false end,
    AwardBadge = function() return false end,
    GetBadgeInfoAsync = function(_, id) return { Name = "Badge " .. tostring(id), Description = "", IconImageId = 0, IsEnabled = false } end,
})
service("GroupService", {
    GetGroupsAsync = function() return {} end,
    GetGroupInfoAsync = function(_, id) return { Name = "Gruppo " .. tostring(id), Id = id, Roles = {} } end,
})
service("SocialService", { CanSendGameInviteAsync = function() return false end, PromptGameInvite = function() end })
service("VRService", { VREnabled = false, GetUserCFrameEnabled = function() return false end })
service("HapticService", { IsVibrationSupported = function() return false end, IsMotorSupported = function() return false end,
                           SetMotor = function() end, GetMotor = function() return 0 end })
service("GamepadService", { GamepadCursorEnabled = false, EnableGamepadCursor = function() end, DisableGamepadCursor = function() end })
service("Stats", { GetTotalMemoryUsageMb = function() return gcinfo() / 1024 end, DataReceiveKbps = 0, DataSendKbps = 0,
                   HeartbeatTimeMs = 0, PhysicsStepTimeMs = 0, InstanceCount = 0 })
service("KeyframeSequenceProvider", {})
Internal.extraServices = extraServices

local startClock = os.clock()

local function addEnum(name, items)
    if type(Enum) ~= "table" or rawget(Enum, name) ~= nil then return end
    local e = {}
    for i, n in ipairs(items) do e[n] = { Name = n, Value = i - 1, EnumType = name } end
    pcall(function() Enum[name] = e end)
end

------------------------------------------------------------------------------
-- Figli Luau di oggetti del motore (Player, servizi, part): il motore non li
-- conosce, li ricordiamo qui. La chiave e' stabile anche se il motore crea
-- una tabella nuova a ogni accesso (le part: __partIndex).
------------------------------------------------------------------------------
local foreign = {}
local function keyOf(obj)
    if type(obj) ~= "table" then return obj end
    local pi = rawget(obj, "__partIndex")
    if pi ~= nil then return "p" .. tostring(pi) end
    local si = rawget(obj, "__sharedIndex")
    if si ~= nil then return "h" .. tostring(si) end
    local sv = rawget(obj, "__serviceId")
    if sv ~= nil and rawget(obj, "ClassName") ~= nil and rawget(obj, "__sharedIndex") == nil then return "s" .. tostring(sv) end
    return obj
end
function foreignList(obj)
    local k = keyOf(obj)
    local l = foreign[k]
    if not l then l = {} foreign[k] = l end
    return l
end
Internal.foreignChildren = function(obj) return foreign[keyOf(obj)] or {} end

-- Classi Value: .Value e Changed(nuovoValore), come Roblox.
local kValueDefaults = {
    IntValue = function() return 0 end, NumberValue = function() return 0 end, StringValue = function() return "" end,
    BoolValue = function() return false end, ObjectValue = function() return nil end,
    Vector3Value = function() return V(0, 0, 0) end, CFrameValue = function() return CFrame.new() end,
    Color3Value = function() return Color3.new(0, 0, 0) end,
    BrickColorValue = function() return BrickColor and BrickColor.new("Medium stone grey") or nil end,
}

-- Metodi e segnali in piu' per alcune classi Luau.
classMethods = {}
classSignals = {
    Humanoid = { Died = true, HealthChanged = true, MoveToFinished = true, Running = true, Jumping = true, StateChanged = true },
    BindableEvent = { Event = true },
    Sound = { Played = true, Stopped = true, Ended = true, Paused = true, Resumed = true, Loaded = true },
    Player = { CharacterAdded = true, CharacterRemoving = true, CharacterAppearanceLoaded = true, Chatted = true },
}

classMethods.BindableEvent = {
    Fire = function(self, ...) sigOf(store[self], "Event"):Fire(...) end,
}
classMethods.BindableFunction = {
    Invoke = function(self, ...)
        local cb = store[self].data.OnInvoke
        if type(cb) ~= "function" then error("BindableFunction senza OnInvoke", 2) end
        return cb(...)
    end,
}

local function rootPartOf(hum)
    local model = store[hum] and store[hum].data.Parent
    if model == nil then return nil end
    local ok, root = pcall(function() return model.PrimaryPart or model:FindFirstChild("HumanoidRootPart") end)
    return ok and root or nil
end

classMethods.Humanoid = {
    TakeDamage = function(self, amount)
        local d = store[self].data
        if (d.Health or 0) <= 0 then return end
        self.Health = math.max(0, (d.Health or 0) - (amount or 0))
    end,
    MoveTo = function(self, target, part)
        local s = store[self]
        if s.moveConn then s.moveConn:Disconnect() end
        local started = os.clock()
        local RS = game:GetService("RunService")
        s.moveConn = RS.Heartbeat:Connect(function(dt)
            local root = rootPartOf(self)
            if not root then return end
            local goal = part and part.Position or target
            local pos = root.Position
            local dx, dz = vx(goal) - vx(pos), vz(goal) - vz(pos)
            local dist = math.sqrt(dx * dx + dz * dz)
            local step = (s.data.WalkSpeed or 16) * dt
            if dist <= math.max(0.5, step) then
                root.Position = V(vx(goal), vy(pos), vz(goal))
                s.moveConn:Disconnect()
                s.moveConn = nil
                fireIf(s, "MoveToFinished", true)
                return
            end
            root.Position = V(vx(pos) + dx / dist * step, vy(pos), vz(pos) + dz / dist * step)
            if os.clock() - started > 8 then
                s.moveConn:Disconnect()
                s.moveConn = nil
                fireIf(s, "MoveToFinished", false)
            end
        end)
    end,
    Move = function(self, dir) end,
    ChangeState = function(self, st) store[self].data.State = st fireIf(store[self], "StateChanged", st) end,
    GetState = function(self) return store[self].data.State or "Running" end,
    SetStateEnabled = function() end,
    UnequipTools = function() end,
    EquipTool = function(self, tool) end,
    GetAppliedDescription = function() return {} end,
    LoadAnimation = function(self, anim)
        -- Solo il player ha le animazioni per ora: per gli NPC una traccia che non fa nulla.
        local t = { IsPlaying = false, Length = 0, Looped = false, Speed = 1, TimePosition = 0, WeightCurrent = 0 }
        function t:Play() self.IsPlaying = true end
        function t:Stop() self.IsPlaying = false end
        function t:AdjustSpeed(s) self.Speed = s end
        function t:AdjustWeight() end
        t.Stopped = Signal.new() t.Ended = Signal.new() t.DidLoop = Signal.new() t.KeyframeReached = Signal.new()
        function t:GetMarkerReachedSignal() return Signal.new() end
        return t
    end,
}

-- Model: contenitore Luau. Le part dentro restano part vere del motore.
local function modelParts(inst, out)
    out = out or {}
    for _, c in ipairs(store[inst].children) do
        if store[c] then
            modelParts(c, out)
        elseif type(c) == "table" then
            local pi = rawget(c, "__partIndex")
            if type(pi) == "number" and pi >= 0 then table.insert(out, c) end
        end
    end
    return out
end
local function modelBox(self)
    local parts = modelParts(self)
    if #parts == 0 then return nil end
    local x0, y0, z0, x1, y1, z1 = math.huge, math.huge, math.huge, -math.huge, -math.huge, -math.huge
    for _, p in ipairs(parts) do
        local ok, pos, size = pcall(function() return p.Position, p.Size end)
        if ok and pos and size then
            local hx, hy, hz = vx(size) / 2, vy(size) / 2, vz(size) / 2
            x0 = math.min(x0, vx(pos) - hx) y0 = math.min(y0, vy(pos) - hy) z0 = math.min(z0, vz(pos) - hz)
            x1 = math.max(x1, vx(pos) + hx) y1 = math.max(y1, vy(pos) + hy) z1 = math.max(z1, vz(pos) + hz)
        end
    end
    if x0 == math.huge then return nil end
    return CFrame.new((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), V(x1 - x0, y1 - y0, z1 - z0)
end
local function modelPivot(self)
    local d = store[self].data
    local pp = d.PrimaryPart
    if pp ~= nil then
        local ok, cf = pcall(function() return pp.CFrame end)
        if ok and typeof(cf) == "CFrame" then return cf end
        local okp, pos = pcall(function() return pp.Position end)
        if okp and pos then return CFrame.new(vx(pos), vy(pos), vz(pos)) end
    end
    if d.WorldPivot then return d.WorldPivot end
    return (modelBox(self)) or CFrame.new()
end
local function modelTranslate(self, dx, dy, dz)
    for _, p in ipairs(modelParts(self)) do
        local pos = p.Position
        p.Position = V(vx(pos) + dx, vy(pos) + dy, vz(pos) + dz)
    end
    local d = store[self].data
    if d.WorldPivot then
        local w = d.WorldPivot
        d.WorldPivot = CFrame.new(dx, dy, dz) * w
    end
end
classMethods.Model = {
    GetPivot = modelPivot,
    PivotTo = function(self, cf)
        if typeof(cf) == "Vector3" then cf = CFrame.new(vx(cf), vy(cf), vz(cf)) end
        local old = modelPivot(self)
        local delta = cf * old:Inverse()
        for _, p in ipairs(modelParts(self)) do
            local ok, pcf = pcall(function() return p.CFrame end)
            if ok and typeof(pcf) == "CFrame" then
                p.CFrame = delta * pcf
            else
                local w = delta * p.Position
                p.Position = w
            end
        end
        local d = store[self].data
        if d.WorldPivot or d.PrimaryPart == nil then d.WorldPivot = cf end
    end,
    MoveTo = function(self, pos)
        local o = modelPivot(self).Position
        modelTranslate(self, vx(pos) - vx(o), vy(pos) - vy(o), vz(pos) - vz(o))
    end,
    TranslateBy = function(self, v) modelTranslate(self, vx(v), vy(v), vz(v)) end,
    SetPrimaryPartCFrame = function(self, cf)
        if store[self].data.PrimaryPart == nil then error("SetPrimaryPartCFrame: il Model non ha PrimaryPart", 2) end
        classMethods.Model.PivotTo(self, cf)
    end,
    GetPrimaryPartCFrame = function(self)
        local pp = store[self].data.PrimaryPart
        return pp and pp.CFrame or nil
    end,
    GetBoundingBox = function(self)
        local cf, size = modelBox(self)
        return cf or CFrame.new(), size or V(0, 0, 0)
    end,
    GetExtentsSize = function(self)
        local _, size = modelBox(self)
        return size or V(0, 0, 0)
    end,
    GetScale = function() return 1 end,
    ScaleTo = function() end,
    BreakJoints = function() end,
    MakeJoints = function() end,
}

local kClassDefaults = {
    Humanoid = { Health = 100, MaxHealth = 100, WalkSpeed = 16, JumpPower = 50, JumpHeight = 7.2, HipHeight = 2,
                 AutoRotate = true, DisplayName = "", Sit = false, PlatformStand = false, RigType = "R6" },
    Sound = { Volume = 0.5, Playing = false, Looped = false, TimePosition = 0, TimeLength = 0, PlaybackSpeed = 1,
              IsLoaded = true, IsPlaying = false },
}

function initClass(inst, cls)
    local s = store[inst]
    local mk = kValueDefaults[cls]
    if mk then
        s.isValue = true
        s.data.Value = mk()
    end
    local defaults = kClassDefaults[cls]
    if defaults then for k, v in pairs(defaults) do s.data[k] = v end end
end

-- Oggetto del motore creato senza Parent (ClickDetector, ProximityPrompt...):
-- il motore lo collega alla part solo se gliela dai alla creazione. Qui, al
-- primo `obj.Parent = part`, si ricrea attaccato e prende il posto del vecchio.
local function deferParent(r, cls, cnew)
    setmetatable(r, {
        __newindex = function(t, k, v)
            if k ~= "Parent" or v == nil then rawset(t, k, v) return end
            local ok, fresh = pcall(cnew, cls, rawget(t, "Name") or cls, v)
            if ok and type(fresh) == "table" then
                for fk, fv in pairs(fresh) do
                    local mine = rawget(t, fk)
                    if mine == nil or type(fv) == "function" or type(fv) == "table" or (type(fk) == "string" and fk:sub(1, 2) == "__") then
                        rawset(t, fk, fv)
                    end
                end
            end
            setmetatable(t, nil)
            rawset(t, "Parent", v)
            -- Parent Luau (Backpack, Folder dello script): l'oggetto entra tra i suoi figli.
            if isLuaInstance(v) then
                table.insert(store[v].children, t)
                fireIf(store[v], "ChildAdded", t)
            elseif type(v) == "table" and rawget(v, "__partIndex") == nil then
                table.insert(foreignList(v), t)
            end
        end,
    })
end

local function wrapDataStore(ds)
    if type(ds) ~= "table" then return ds end
    local Http = game:GetService("HttpService")
    local kTag = "\1blkjson:"
    local function enc(v)
        if type(v) == "table" then return kTag .. Http:JSONEncode(v) end
        return v
    end
    local function dec(v)
        if type(v) == "string" and v:sub(1, #kTag) == kTag then return Http:JSONDecode(v:sub(#kTag + 1)) end
        return v
    end
    local w = {}
    function w:SetAsync(key, value, ...) return ds:SetAsync(key, enc(value), ...) end
    function w:GetAsync(key, ...) return dec(ds:GetAsync(key, ...)) end
    function w:UpdateAsync(key, fn)
        local new = fn(dec(ds:GetAsync(key)))
        if new ~= nil then ds:SetAsync(key, enc(new)) end
        return new
    end
    function w:IncrementAsync(key, delta) return ds:IncrementAsync(key, delta) end
    function w:RemoveAsync(key)
        local old = dec(ds:GetAsync(key))
        if ds.RemoveAsync then ds:RemoveAsync(key) else ds:SetAsync(key, nil) end
        return old
    end
    w.GetSortedAsync = ds.GetSortedAsync and function(self, ...) return ds:GetSortedAsync(...) end or nil
    return setmetatable(w, { __index = ds, __metatable = "Instance" })
end

-- Player e Character del motore sono tabelle semplici: diamo loro i metodi
-- degli Instance (cercano tra i campi del motore e i figli Luau).
function adoptContainer(t, className)
    if type(t) ~= "table" or rawget(t, "__blkAdopted") then return end
    local function children()
        local out = table.clone(foreignList(t))
        for k, v in pairs(t) do
            if type(k) == "string" and type(v) == "table" and k:sub(1, 2) ~= "__" and k ~= "Parent"
               and (rawget(v, "ClassName") ~= nil or isLuaInstance(v)) then
                table.insert(out, v)
            end
        end
        return out
    end
    local function nameOf(c) return isLuaInstance(c) and store[c].data.Name or c.Name end
    local function classOf(c) return isLuaInstance(c) and store[c].data.ClassName or c.ClassName end
    local m = {
        GetChildren = function() return children() end,
        FindFirstChild = function(_, name)
            for _, c in ipairs(children()) do if nameOf(c) == name then return c end end
            return nil
        end,
        FindFirstChildOfClass = function(_, cls)
            for _, c in ipairs(children()) do if classOf(c) == cls then return c end end
            return nil
        end,
        FindFirstChildWhichIsA = function(_, cls)
            for _, c in ipairs(children()) do
                if classOf(c) == cls or (isLuaInstance(c) and instMethods.IsA(c, cls)) then return c end
            end
            return nil
        end,
        WaitForChild = function(self, name, timeout)
            local f = self:FindFirstChild(name)
            if f or not coroutine.isyieldable() then return f end
            local deadline = os.clock() + (timeout or 5)
            while os.clock() < deadline do
                task.wait(0.05)
                f = self:FindFirstChild(name)
                if f then return f end
            end
            return nil
        end,
        IsA = function(_, cls) return cls == "Instance" or cls == className end,
        GetAttribute = function(_, n) return rawget(t, "__attrs") and rawget(t, "__attrs")[n] end,
        SetAttribute = function(_, n, v)
            local a = rawget(t, "__attrs")
            if not a then a = {} rawset(t, "__attrs", a) end
            a[n] = v
        end,
    }
    for k, f in pairs(m) do if rawget(t, k) == nil then pcall(rawset, t, k, f) end end
    rawset(t, "__blkAdopted", true)
    if rawget(t, "ClassName") == nil then rawset(t, "ClassName", className) end
end

------------------------------------------------------------------------------
-- Giocatori sul server dedicato: un Player Luau per ogni client collegato
-- (LuaRuntime::serverPlayerJoined / Left / Moved, da blocky_servers/ServerWorld.h).
-- Fuori dal server non si usa: li' Players ha il solo LocalPlayer del motore.
------------------------------------------------------------------------------
local serverPlayers = {}   -- UserId (= id del peer) -> Player
local serverOrder = {}     -- in ordine di ingresso, per GetPlayers

local function serverRootOf(p)
    local char = p and store[p] and store[p].data.Character
    local root = char and store[char] and store[char].data.PrimaryPart
    return root
end

function Internal.serverPlayer(id, name)
    local p = serverPlayers[id]
    if p then return p end
    p = newInstance("Player")
    local d = store[p].data
    d.Name = name
    d.DisplayName = name
    d.UserId = id
    d.AccountAge = 0
    d.MembershipType = "None"
    d.Neutral = true
    newInstance("Backpack", p)
    newInstance("PlayerGui", p)
    newInstance("StarterGear", p)
    -- Character: Model con HumanoidRootPart (posizione dalla rete) e Humanoid.
    local char = newInstance("Model")
    store[char].data.Name = name
    local root = newInstance("Part")
    local rd = store[root].data
    rd.Name = "HumanoidRootPart"
    rd.Size = V(2, 2, 1)
    rd.Position = V(0, 3, 0)
    rd.CFrame = CFrame.new(0, 3, 0)
    rd.Anchored = false
    rd.CanCollide = false
    setParent(root, char)
    newInstance("Humanoid", char)
    store[char].data.PrimaryPart = root
    d.Character = char
    serverPlayers[id] = p
    table.insert(serverOrder, p)
    setParent(p, game:GetService("Players"))
    setParent(char, workspace)
    return p
end
function Internal.serverPlayerById(id) return serverPlayers[id] end
function Internal.serverPlayerByName(name)
    for _, p in ipairs(serverOrder) do
        if store[p] and store[p].data.Name == name then return p end
    end
    return nil
end
function Internal.serverRootPart(id) return serverRootOf(serverPlayers[id]) end
function Internal.serverPlayerMoved(id, x, y, z)
    local root = serverRootOf(serverPlayers[id])
    if not root then return end
    -- la rete manda i piedi; HumanoidRootPart sta 3 stud sopra, come nel gioco
    local rd = store[root].data
    rd.Position = V(x, y + 3, z)
    rd.CFrame = CFrame.new(x, y + 3, z)
end
function Internal.serverCharacterAdded(id)
    local p = serverPlayers[id]
    if p then fireIf(store[p], "CharacterAdded", store[p].data.Character) end
end
function Internal.serverPlayerRemoved(id)
    local p = serverPlayers[id]
    if not p then return end
    serverPlayers[id] = nil
    for i, q in ipairs(serverOrder) do
        if q == p then table.remove(serverOrder, i) break end
    end
    local char = store[p].data.Character
    if char then
        fireIf(store[p], "CharacterRemoving", char)
        pcall(instMethods.Destroy, char)
    end
    pcall(instMethods.Destroy, p)
end
-- Il servizio Players del server: GetPlayers & co. vedono i client collegati.
function Internal.enableServerPlayers()
    local P = game:GetService("Players")
    if type(P) ~= "table" then return end
    local cFind = P.FindFirstChild
    local function set(k, v) pcall(rawset, P, k, v) end
    set("GetPlayers", function() return table.clone(serverOrder) end)
    set("GetChildren", function() return table.clone(serverOrder) end)
    set("GetPlayerFromCharacter", function(_, char)
        for _, p in ipairs(serverOrder) do
            if store[p].data.Character == char then return p end
        end
        return nil
    end)
    set("GetPlayerByUserId", function(_, id) return serverPlayers[tonumber(id) or -1] end)
    set("FindFirstChild", function(self, name, ...)
        local p = Internal.serverPlayerByName(name)
        if p then return p end
        if type(cFind) == "function" then return cFind(self, name, ...) end
        return nil
    end)
    set("LocalPlayer", nil)
end

local function installExtras()
    -- Instance.new: chi fa cosa.
    --   classi Luau (Folder, Value, Bindable, Configuration): Instance completo qui
    --   classi che il motore conosce solo come dati: Instance completo coi suoi valori
    --   classi del motore: dal motore, col Parent passato come lo vuole lui
    --     (3o argomento) e, se il Parent arriva dopo, ricreate attaccate
    if type(Instance) == "table" and type(Instance.new) == "function" then
        local cnew = Instance.new
        local kLuaClasses = { Folder = true, BindableEvent = true, BindableFunction = true, Configuration = true, Model = true }
        for k in pairs(kValueDefaults) do kLuaClasses[k] = true end

        local function soundify(t)
            for _, n in ipairs({ "Played", "Stopped", "Ended", "Paused", "Resumed", "Loaded" }) do
                if rawget(t, n) == nil then rawset(t, n, Signal.new()) end
            end
            local function wrap(method, sig)
                local f = rawget(t, method)
                if type(f) ~= "function" then return end
                rawset(t, method, function(self, ...)
                    local res = f(self, ...)
                    rawget(t, sig):Fire(rawget(t, "SoundId"))
                    return res
                end)
            end
            wrap("Play", "Played"); wrap("Stop", "Stopped"); wrap("Pause", "Paused"); wrap("Resume", "Resumed")
            if rawget(t, "IsLoaded") == nil then rawset(t, "IsLoaded", true) end
            if rawget(t, "TimeLength") == nil then rawset(t, "TimeLength", 0) end
        end
        local function modelify(t)
            local gp, pt = rawget(t, "GetPivot"), rawget(t, "PivotTo")
            if type(gp) == "function" then
                rawset(t, "GetPivot", function(self)
                    local p = gp(self)
                    if typeof(p) == "Vector3" then return CFrame.new(p) end
                    return p
                end)
            end
            if type(pt) == "function" then
                rawset(t, "PivotTo", function(self, cf)
                    local ok = pcall(pt, self, cf)
                    if not ok and type(cf) == "table" and cf.Position then pt(self, cf.Position) end
                end)
            end
        end
        local function buttonify(t)
            local conn, once, wait = rawget(t, "Connect"), rawget(t, "Once"), rawget(t, "Wait")
            if type(conn) ~= "function" then return end
            local sig = { Connect = function(_, f) return conn(t, f) end,
                          Once = function(_, f) return (once or conn)(t, f) end,
                          Wait = function() return wait and wait(t) end }
            for _, n in ipairs({ "MouseButton1Click", "Activated", "MouseButton1Down" }) do
                if rawget(t, n) == nil then rawset(t, n, sig) end
            end
        end
        local kPost = { Sound = soundify, Model = modelify, TextButton = buttonify, ImageButton = buttonify }

        Instance.new = function(cls, parent)
            local name = nil
            -- Blocky prendeva il 2o argomento come NOME; Roblox come PARENT.
            if type(parent) == "string" then name, parent = parent, nil end
            if kLuaClasses[cls] then
                local inst = newInstance(cls)
                if name then store[inst].data.Name = name end
                if parent ~= nil then setParent(inst, parent) end
                return inst
            end
            local r = cnew(cls, name or cls, parent)
            -- Agganci per classe (Sound, Model, pulsanti): rawset, quindi anche su
            -- oggetti con metatabella del motore.
            local post = kPost[cls]
            if post and type(r) == "table" then pcall(post, r) end
            if type(r) == "table" and getmetatable(r) == nil and rawget(r, "ClassName") == cls then
                -- Solo dati = solo testi, numeri, booleani. Con funzioni o tabelle
                -- (eventi come MouseClick) e' un oggetto vero del motore.
                local dataOnly = true
                for _, v in pairs(r) do
                    local tv = type(v)
                    if tv == "function" or tv == "table" then dataOnly = false break end
                end
                if dataOnly then
                    local inst = newInstance(cls)
                    local d = store[inst].data
                    for k, v in pairs(r) do
                        if k ~= "ClassName" and k ~= "Parent" and type(k) == "string" and k:sub(1, 2) ~= "__" then d[k] = v end
                    end
                    if parent ~= nil then setParent(inst, parent) end
                    return inst
                end
                -- Oggetti semplici del motore (Tool...): i metodi degli Instance se mancano.
                if rawget(r, "FindFirstChild") == nil then adoptContainer(r, cls) end
                if parent == nil then
                    deferParent(r, cls, function(c, n, p)
                        local fresh = cnew(c, n, p)
                        if post and type(fresh) == "table" then post(fresh) end
                        return fresh
                    end)
                    return r
                end
            end
            if parent ~= nil and type(r) == "table" then
                local ok, err = pcall(function() r.Parent = parent end)
                if not ok then error(err, 2) end
            end
            return r
        end
    end
    -- GetService/FindService anche per i servizi qui sopra.
    if type(game) == "table" and type(game.GetService) == "function" then
        local cget = game.GetService
        game.GetService = function(self, name)
            local ok, r = pcall(cget, self, name)
            if ok and r ~= nil then return r end
            local e = extraServices[name]
            if e then return e end
            error(r, 2)
        end
        game.FindService = function(self, name)
            local ok, r = pcall(cget, self, name)
            if ok and r ~= nil then return r end
            return extraServices[name]
        end
        local rf = newInstance("ReplicatedFirst")
        store[rf].data.RemoveDefaultLoadingScreen = function() end
        extraServices.ReplicatedFirst = rf
        local rs = game.RunService
        if type(rs) == "table" then
            if rawget(rs, "IsRunning") == nil then pcall(function() rs.IsRunning = function() return true end end) end
            if rawget(rs, "IsEdit") == nil then pcall(function() rs.IsEdit = function() return false end end) end
        end
        local players = game.Players
        if type(players) == "table" and rawget(players, "GetPlayerByUserId") == nil and type(players.GetPlayers) == "function" then
            pcall(function()
                players.GetPlayerByUserId = function(self, id)
                    for _, p in ipairs(self:GetPlayers()) do if p.UserId == id then return p end end
                    return nil
                end
            end)
        end
    end
    -- Player e Character: metodi degli Instance, PlayerGui/Backpack, HumanoidRootPart.
    pcall(function()
        local players = game:GetService("Players")
        local player = players.LocalPlayer
        if type(player) ~= "table" then return end
        adoptContainer(player, "Player")
        if rawget(player, "PlayerGui") == nil then newInstance("PlayerGui", player) end
        if rawget(player, "Backpack") == nil then newInstance("Backpack", player) end
        local char = player.Character
        if type(char) == "table" then
            adoptContainer(char, "Model")
            local hum = rawget(char, "Humanoid")
            if type(hum) == "table" then
                if rawget(hum, "ClassName") == nil then pcall(rawset, hum, "ClassName", "Humanoid") end
                if rawget(hum, "Name") == nil then pcall(rawset, hum, "Name", "Humanoid") end
                if rawget(hum, "Parent") == nil then pcall(rawset, hum, "Parent", char) end
            end
            if rawget(char, "HumanoidRootPart") == nil and type(hum) == "table" then
                local root = setmetatable({ Name = "HumanoidRootPart", ClassName = "Part", Parent = char }, {
                    __index = function(_, k)
                        local ok, p = pcall(function() return hum:GetPosition() end)
                        if not ok or p == nil then p = V(0, 0, 0) end
                        if k == "Position" then return p end
                        if k == "CFrame" then return CFrame.new(p) end
                        if k == "Size" then return V(2, 2, 1) end
                        return nil
                    end,
                    __metatable = "Instance",
                })
                rawset(char, "HumanoidRootPart", root)
            end
            if rawget(char, "PrimaryPart") == nil then rawset(char, "PrimaryPart", rawget(char, "HumanoidRootPart")) end
        end
    end)

    -- I servizi trovano anche i figli Luau (Folder, Value... messi li' dagli script).
    for _, svcName in ipairs({ "Workspace", "ReplicatedStorage", "ServerStorage", "ServerScriptService", "StarterGui",
                               "StarterPack", "Lighting", "Teams", "SoundService" }) do
        pcall(function()
            local svc = game:GetService(svcName)
            if type(svc) ~= "table" then return end
            local cFind, cWait, cKids = svc.FindFirstChild, svc.WaitForChild, svc.GetChildren
            local cDesc = svc.GetDescendants
            -- Servizio senza metodi da Instance (SoundService...): li riceve tutti.
            if type(cFind) ~= "function" then adoptContainer(svc, svcName) return end
            local function luaChild(name)
                for _, c in ipairs(foreignList(svc)) do if store[c] and store[c].data.Name == name then return c end end
                return nil
            end
            if type(cFind) == "function" then
                svc.FindFirstChild = function(self, name, rec)
                    local r = cFind(self, name, rec)
                    if r ~= nil then return r end
                    return luaChild(name)
                end
            end
            if type(cWait) == "function" then
                svc.WaitForChild = function(self, name, timeout)
                    local r = luaChild(name)
                    if r then return r end
                    return cWait(self, name, timeout)
                end
            end
            if type(cKids) == "function" then
                svc.GetChildren = function(self)
                    local out = cKids(self) or {}
                    -- Le part del motore le elenca gia' cKids: qui solo gli
                    -- Instance Luau (Folder, Model...), senza doppioni.
                    for _, c in ipairs(foreignList(svc)) do
                        if not (type(c) == "table" and type(rawget(c, "__partIndex")) == "number") then
                            table.insert(out, c)
                        end
                    end
                    return out
                end
            end
            if type(cDesc) == "function" then
                svc.GetDescendants = function(self)
                    local out = cDesc(self) or {}
                    -- Instance Luau messi nel servizio e tutto quello che contengono.
                    for _, c in ipairs(foreignList(svc)) do
                        if isLuaInstance(c) then
                            table.insert(out, c)
                            for _, d in ipairs(instMethods.GetDescendants(c)) do
                                -- le part del motore dentro un Model Luau le ha gia' cDesc
                                if not (type(d) == "table" and type(rawget(d, "__partIndex")) == "number") then table.insert(out, d) end
                            end
                        end
                    end
                    return out
                end
            end
        end)
    end

    -- DataStore: anche tabelle (in JSON), come Roblox.
    pcall(function()
        local dss = game:GetService("DataStoreService")
        for _, fname in ipairs({ "GetDataStore", "GetGlobalDataStore" }) do
            local orig = dss[fname]
            if type(orig) == "function" then
                dss[fname] = function(self, ...) return wrapDataStore(orig(self, ...)) end
            end
        end
    end)

    -- workspace:Raycast(origine, direzione, RaycastParams) come Roblox: la
    -- lunghezza della direzione e' la distanza; Exclude/Include filtrano.
    pcall(function()
        local ws = workspace
        local cRay = ws.Raycast
        if type(cRay) ~= "function" then return end
        local function inList(inst, list)
            for _, x in ipairs(list) do
                if x == inst then return true end
                if type(x) == "table" and type(inst) == "table" then
                    if rawget(x, "__partIndex") ~= nil and rawget(x, "__partIndex") == rawget(inst, "__partIndex") then return true end
                    local ok, desc = pcall(function() return inst:IsDescendantOf(x) end)
                    if ok and desc then return true end
                end
            end
            return false
        end
        ws.Raycast = function(self, origin, direction, params)
            local dist = math.sqrt(vec3Extra.Dot(direction, direction))
            if dist < 1e-6 then return nil end
            local unit = V(vx(direction) / dist, vy(direction) / dist, vz(direction) / dist)
            local list = type(params) == "table" and params.FilterDescendantsInstances or {}
            local include = type(params) == "table" and (params.FilterType == "Include" or params.FilterType == "Whitelist")
            local o, left = origin, dist
            for _ = 1, 16 do
                local res = cRay(self, o, unit, left)
                if res == nil or res.Position == nil then return nil end
                local hitInst = res.Instance
                local keep
                if #list == 0 then keep = not include
                elseif include then keep = inList(hitInst, list)
                else keep = not inList(hitInst, list) end
                local d = res.Distance or math.sqrt(vec3Extra.Dot(res.Position - o, res.Position - o))
                if keep then
                    if res.Normal == nil and type(hitInst) == "table" then
                        pcall(function()
                            local cf, size = hitInst.CFrame, hitInst.Size
                            local lp = cf:PointToObjectSpace(res.Position)
                            local hx, hy, hz = vx(size) / 2, vy(size) / 2, vz(size) / 2
                            local ax, ay, az = math.abs(vx(lp)) / hx, math.abs(vy(lp)) / hy, math.abs(vz(lp)) / hz
                            local n
                            if ax >= ay and ax >= az then n = V(math.sign(vx(lp)), 0, 0)
                            elseif ay >= az then n = V(0, math.sign(vy(lp)), 0)
                            else n = V(0, 0, math.sign(vz(lp))) end
                            res.Normal = cf:VectorToWorldSpace(n)
                        end)
                        if res.Normal == nil then res.Normal = V(-vx(unit), -vy(unit), -vz(unit)) end
                    end
                    return res
                end
                -- colpito qualcosa da ignorare: si riparte poco oltre
                left -= d + 0.01
                if left <= 0 then return nil end
                o = V(vx(res.Position) + vx(unit) * 0.01, vy(res.Position) + vy(unit) * 0.01, vz(res.Position) + vz(unit) * 0.01)
            end
            return nil
        end
    end)

    -- TweenService vero (quello del motore era uno stub: Play non faceva nulla).
    pcall(function()
        local TS = game:GetService("TweenService")
        local RS = game:GetService("RunService")
        local function nameOf(e, def)
            if type(e) == "string" then return e end
            if type(e) == "table" then return e.Name or def end
            return def
        end
        local PI, HALF = math.pi, math.pi / 2
        local function bounceOut(t)
            if t < 1 / 2.75 then return 7.5625 * t * t
            elseif t < 2 / 2.75 then t -= 1.5 / 2.75 return 7.5625 * t * t + 0.75
            elseif t < 2.5 / 2.75 then t -= 2.25 / 2.75 return 7.5625 * t * t + 0.9375
            else t -= 2.625 / 2.75 return 7.5625 * t * t + 0.984375 end
        end
        local easeIn = {
            Linear = function(t) return t end,
            Quad = function(t) return t * t end,
            Cubic = function(t) return t * t * t end,
            Quart = function(t) return t ^ 4 end,
            Quint = function(t) return t ^ 5 end,
            Sine = function(t) return 1 - math.cos(t * HALF) end,
            Exponential = function(t) return t == 0 and 0 or 2 ^ (10 * (t - 1)) end,
            Circular = function(t) return 1 - math.sqrt(1 - t * t) end,
            Back = function(t) local s = 1.70158 return t * t * ((s + 1) * t - s) end,
            Elastic = function(t)
                if t == 0 or t == 1 then return t end
                return -(2 ^ (10 * (t - 1))) * math.sin((t - 1.075) * (2 * PI) / 0.3)
            end,
            Bounce = function(t) return 1 - bounceOut(1 - t) end,
        }
        local function ease(style, dir, t)
            local f = easeIn[style] or easeIn.Quad
            if dir == "In" then return f(t) end
            if dir == "InOut" then
                if t < 0.5 then return f(t * 2) / 2 end
                return 1 - f((1 - t) * 2) / 2
            end
            return 1 - f(1 - t) -- Out
        end
        local function lerpValue(a, b, k)
            if type(a) == "number" and type(b) == "number" then return a + (b - a) * k end
            if type(a) == "boolean" then return k >= 1 and b or a end
            if type(a) == "table" and type(b) == "table" then
                local ok, r = pcall(function() return a:Lerp(b, k) end)
                if ok and r ~= nil then return r end
                if a.R ~= nil and b.R ~= nil then
                    return Color3.new(a.R + (b.R - a.R) * k, a.G + (b.G - a.G) * k, a.B + (b.B - a.B) * k)
                end
                if a.X ~= nil and b.X ~= nil and a.Y ~= nil then
                    return V(vx(a) + (vx(b) - vx(a)) * k, vy(a) + (vy(b) - vy(a)) * k, vz(a) + (vz(b) - vz(a)) * k)
                end
            end
            return k >= 1 and b or a
        end
        TS.Create = function(self, inst, info, goals)
            if type(info) ~= "table" then error("TweenService:Create(oggetto, TweenInfo, obiettivi)", 2) end
            local tween = {
                Instance = inst, TweenInfo = info, PlaybackState = "Begin",
                Completed = Signal.new(),
            }
            local conn, elapsed, from, forward, repeatsLeft = nil, 0, nil, true, info.RepeatCount or 0
            local total = math.max(0, info.Time or 1)
            local style = nameOf(info.EasingStyle, "Quad")
            local dir = nameOf(info.EasingDirection, "Out")
            local function apply(k)
                for prop, goal in pairs(goals) do
                    local v = lerpValue(from[prop], goal, k)
                    pcall(function() inst[prop] = v end)
                end
            end
            local function finish(state)
                if conn then conn:Disconnect() conn = nil end
                tween.PlaybackState = state
                tween.Completed:Fire(state)
            end
            function tween:Play()
                if conn then return end
                if tween.PlaybackState ~= "Paused" then
                    from = {}
                    for prop in pairs(goals) do from[prop] = inst[prop] end
                    elapsed = -(info.DelayTime or 0)
                    forward = true
                    repeatsLeft = info.RepeatCount or 0
                end
                tween.PlaybackState = "Playing"
                conn = RS.Heartbeat:Connect(function(dt)
                    elapsed += dt
                    if elapsed < 0 then return end
                    local t = total > 0 and math.min(1, elapsed / total) or 1
                    local k = ease(style, dir, forward and t or 1 - t)
                    apply(k)
                    if t >= 1 then
                        if info.Reverses and forward then
                            forward = false
                            elapsed = 0
                        elseif repeatsLeft ~= 0 then
                            if repeatsLeft > 0 then repeatsLeft -= 1 end
                            forward = true
                            elapsed = 0
                        else
                            finish("Completed")
                        end
                    end
                end)
            end
            function tween:Pause()
                if conn then conn:Disconnect() conn = nil end
                tween.PlaybackState = "Paused"
            end
            function tween:Cancel()
                if conn then finish("Cancelled") end
            end
            return setmetatable(tween, { __metatable = "Tween" })
        end
    end)

    -- Debris:AddItem rispetta il tempo (quello vecchio cancellava subito).
    pcall(function()
        local D = game:GetService("Debris")
        D.AddItem = function(self, obj, lifetime)
            task.delay(lifetime or 10, function()
                pcall(function() obj:Destroy() end)
            end)
        end
    end)

    -- Teams e Team.
    pcall(function()
        local teams = newInstance("Teams")
        store[teams].data.GetTeams = function(self)
            local out = {}
            for _, c in ipairs(store[teams].children) do if childClass(c) == "Team" then table.insert(out, c) end end
            return out
        end
        extraServices.Teams = teams
        classMethods.Team = {
            GetPlayers = function(self)
                local out = {}
                for _, p in ipairs(game:GetService("Players"):GetPlayers()) do if p.Team == self then table.insert(out, p) end end
                return out
            end,
        }
        kClassDefaults.Team = { AutoAssignable = true }
    end)

    pcall(function()
        local players = game:GetService("Players")
        if rawget(players, "MaxPlayers") == nil then players.MaxPlayers = 8 end
        if rawget(players, "RespawnTime") == nil then players.RespawnTime = 5 end
    end)

    -- Camera: ViewportSize, WorldToScreenPoint, ScreenPointToRay come Roblox.
    pcall(function()
        local cam = workspace.CurrentCamera
        if type(cam) ~= "table" then return end
        local function vp() return rawget(cam, "ViewportSize") or Vector2.new(1280, 720) end
        if rawget(cam, "ViewportSize") == nil then rawset(cam, "ViewportSize", Vector2.new(1280, 720)) end
        local function project(world)
            local cf, size = cam.CFrame, vp()
            local lp = cf:PointToObjectSpace(world)
            local depth = -vz(lp)
            local f = 1 / math.tan(math.rad((cam.FieldOfView or 70) / 2))
            local aspect = size.X / size.Y
            local sx = (vx(lp) / math.max(depth, 1e-4)) * f / aspect
            local sy = (vy(lp) / math.max(depth, 1e-4)) * f
            local px = (sx * 0.5 + 0.5) * size.X
            local py = (1 - (sy * 0.5 + 0.5)) * size.Y
            local on = depth > 0 and px >= 0 and px <= size.X and py >= 0 and py <= size.Y
            return V(px, py, depth), on
        end
        local function ray(x, y, depth)
            local cf, size = cam.CFrame, vp()
            local f = 1 / math.tan(math.rad((cam.FieldOfView or 70) / 2))
            local aspect = size.X / size.Y
            local nx = (x / size.X) * 2 - 1
            local ny = 1 - (y / size.Y) * 2
            local dir = cf:VectorToWorldSpace(V(nx * aspect / f, ny / f, -1))
            local l = math.sqrt(vec3Extra.Dot(dir, dir))
            return Internal.Ray.new(cf.Position, V(vx(dir) / l * (depth or 1), vy(dir) / l * (depth or 1), vz(dir) / l * (depth or 1)))
        end
        for k, fn in pairs({
            WorldToScreenPoint = function(_, w) return project(w) end,
            WorldToViewportPoint = function(_, w) return project(w) end,
            ScreenPointToRay = function(_, x, y, d) return ray(x, y, d) end,
            ViewportPointToRay = function(_, x, y, d) return ray(x, y, d) end,
        }) do
            -- quelli del motore ignorano x, y: questi li sostituiscono
            pcall(rawset, cam, k, fn)
        end
    end)

    -- Enum.RenderPriority con voci vere (.Value, .Name) e BindToRenderStep che le accetta.
    pcall(function()
        local items = { First = 0, Input = 100, Camera = 200, Character = 300, Last = 2000 }
        local e = {}
        for n, v in pairs(items) do e[n] = setmetatable({ Name = n, Value = v, EnumType = "RenderPriority" }, { __metatable = "EnumItem" }) end
        Enum.RenderPriority = e
        local RS = game:GetService("RunService")
        local bind = RS.BindToRenderStep
        if type(bind) == "function" then
            RS.BindToRenderStep = function(self, name, prio, fn)
                if type(prio) == "table" then prio = prio.Value end
                return bind(self, name, prio, fn)
            end
        end
    end)

    -- CollectionService: segnali di tag aggiunto/tolto e GetTags.
    pcall(function()
        local CS = game:GetService("CollectionService")
        local cAdd, cRemove = CS.AddTag, CS.RemoveTag
        local tagsOf, added, removed = {}, {}, {}
        local function sigFor(map, tag)
            local s = map[tag]
            if not s then s = Signal.new() map[tag] = s end
            return s
        end
        CS.AddTag = function(self, inst, tag)
            if cAdd then pcall(cAdd, self, inst, tag) end
            local k = keyOf(inst)
            local set = tagsOf[k]
            if not set then set = {} tagsOf[k] = set end
            if not set[tag] then
                set[tag] = true
                sigFor(added, tag):Fire(inst)
            end
        end
        CS.RemoveTag = function(self, inst, tag)
            if cRemove then pcall(cRemove, self, inst, tag) end
            local set = tagsOf[keyOf(inst)]
            if set and set[tag] then
                set[tag] = nil
                sigFor(removed, tag):Fire(inst)
            end
        end
        CS.GetTags = function(self, inst)
            local out = {}
            for t in pairs(tagsOf[keyOf(inst)] or {}) do table.insert(out, t) end
            table.sort(out)
            return out
        end
        CS.GetInstanceAddedSignal = function(self, tag) return sigFor(added, tag) end
        CS.GetInstanceRemovedSignal = function(self, tag) return sigFor(removed, tag) end
    end)

    -- Enum con le voci come stringhe, come quelle del motore (RaycastParams le legge cosi').
    if type(Enum) == "table" and rawget(Enum, "RaycastFilterType") == nil then
        pcall(function()
            Enum.RaycastFilterType = { Exclude = "Exclude", Include = "Include", Blacklist = "Exclude", Whitelist = "Include" }
        end)
    end
    -- Globali: con _G, perche' qui dentro Ray, DateTime... sono anche nomi locali.
    local function def(name, value)
        local cur = rawget(_G, name)
        -- DateTime del motore: solo now/fromUnixTimestamp, senza metodi: il nostro lo contiene.
        if cur == nil or name == "DateTime" then _G[name] = value end
    end
    def("Ray", Internal.Ray)
    if rawget(_G, "Vector2") == nil or type(rawget(_G, "Vector2")) ~= "table" or rawget(_G, "Vector2").new == nil then
        _G.Vector2 = Internal.Vector2
        Internal.v2install()
    end
    def("ColorSequence", Internal.ColorSequence)
    def("ColorSequenceKeypoint", Internal.ColorSequenceKeypoint)
    def("PhysicalProperties", Internal.PhysicalProperties)
    def("DateTime", Internal.DateTime)
    def("Axes", Internal.Axes)
    def("Faces", Internal.Faces)
    def("elapsedTime", function() return os.clock() - startClock end)
    def("version", function() return "Blocky 0.1" end)
    addEnum("HumanoidStateType", { "FallingDown", "Ragdoll", "GettingUp", "Jumping", "Swimming", "Freefall", "Flying", "Landed",
        "Running", "RunningNoPhysics", "StrafingNoPhysics", "Climbing", "Seated", "PlatformStanding", "Dead", "Physics", "None" })
    addEnum("SortOrder", { "Name", "Custom", "LayoutOrder" })
    addEnum("TextXAlignment", { "Left", "Right", "Center" })
    addEnum("TextYAlignment", { "Top", "Center", "Bottom" })
    addEnum("ScaleType", { "Stretch", "Slice", "Tile", "Fit", "Crop" })
    addEnum("CameraType", { "Fixed", "Attach", "Watch", "Track", "Follow", "Custom", "Scriptable", "Orbital" })
    addEnum("PartType", { "Ball", "Block", "Cylinder" })
    addEnum("FillDirection", { "Horizontal", "Vertical" })
    addEnum("HorizontalAlignment", { "Center", "Left", "Right" })
    addEnum("VerticalAlignment", { "Center", "Top", "Bottom" })
    addEnum("AutomaticSize", { "None", "X", "Y", "XY" })
    addEnum("ZIndexBehavior", { "Global", "Sibling" })
    addEnum("ThumbnailType", { "HeadShot", "AvatarBust", "AvatarThumbnail" })
    addEnum("ThumbnailSize", { "Size48x48", "Size180x180", "Size420x420", "Size60x60", "Size100x100", "Size150x150", "Size352x352" })
    addEnum("DevComputerMovementMode", { "UserChoice", "KeyboardMouse", "ClickToMove", "Scriptable" })
    addEnum("MouseBehavior", { "Default", "LockCenter", "LockCurrentPosition" })
end

-- Seconda fase, chiamata dal motore dopo registerApi (Vector3 c e gia).
function Internal.install()
    Vector3.zero = V(0, 0, 0)
    Vector3.one = V(1, 1, 1)
    Vector3.xAxis = V(1, 0, 0)
    Vector3.yAxis = V(0, 1, 0)
    Vector3.zAxis = V(0, 0, 1)
    CFrame.identity = CFrame.new()
    installTypeof()
    installExtras()
end

return Internal
)LUAU";
}

} // namespace luaprelude
} // namespace blk
