-- Portable bit compatibility module for runtimes without LuaJIT's bit library.
-- Uses Lua 5.3+ bitwise operators when available. All operations are masked to
-- 32 bits, matching the semantics expected by NeoLoader Lua modules.
local bit = {}

local MASK = 0xFFFFFFFF

local function toint(value)
    local u = tonumber(value) or 0
    return (u % (MASK + 1))
end

function bit.band(a, b)
    return ((toint(a) & toint(b)) % (MASK + 1))
end

function bit.bor(a, b)
    return ((toint(a) | toint(b)) % (MASK + 1))
end

function bit.bxor(a, b)
    return ((toint(a) ~ toint(b)) % (MASK + 1))
end

function bit.bnot(a)
    return ((~toint(a)) % (MASK + 1))
end

function bit.lshift(a, n)
    return ((toint(a) << (toint(n) % 32)) % (MASK + 1))
end

function bit.rshift(a, n)
    return ((toint(a) >> (toint(n) % 32)) % (MASK + 1))
end

function bit.ror(a, n)
    n = toint(n) % 32
    if n == 0 then return toint(a) end
    return bit.bor(bit.rshift(a, n), bit.lshift(a, 32 - n))
end

function bit.rol(a, n)
    n = toint(n) % 32
    if n == 0 then return toint(a) end
    return bit.bor(bit.lshift(a, n), bit.rshift(a, 32 - n))
end

return bit
