-- Portable subset of the SAMPFUNCS constant table.
--
-- The original `sampfuncs.lua` is a large constant module (RPC ids, packet
-- ids, game-state ids, bitstream types, limits and key codes). Only the
-- protocol constants that are independent of the Android/SA-MP runtime are
-- reproduced here. Values are numeric protocol facts from the SAMPFUNCS
-- table shipped with the analysed NeoLoader installation.
--
-- Like the original module, defined names are also exported as globals so
-- scripts that use `HIGH_PRIORITY` directly keep working. Existing globals
-- are never overwritten.

local sampfuncs = {
    SYSTEM_PRIORITY = 0,
    HIGH_PRIORITY = 1,
    MEDIUM_PRIORITY = 2,
    LOW_PRIORITY = 3,

    UNRELIABLE = 6,
    UNRELIABLE_SEQUENCED = 7,
    RELIABLE = 8,
    RELIABLE_ORDERED = 9,
    RELIABLE_SEQUENCED = 10,

    GAMESTATE_NONE = 0,
    GAMESTATE_WAIT_CONNECT = 1,
    GAMESTATE_AWAIT_JOIN = 2,
    GAMESTATE_CONNECTED = 3,
    GAMESTATE_RESTARTING = 4,
    GAMESTATE_DISCONNECTED = 5,

    BS_TYPE_BYTE = 0,
    BS_TYPE_BOOL = 1,
    BS_TYPE_SHORT = 2,
    BS_TYPE_INT = 3,
    BS_TYPE_FLOAT = 4,
    BS_TYPE_ARRAY = 5,
    BS_TYPE_BITSTREAM = 6,

    ONFOOTSENDRATE = 1,
    INCARSENDRATE = 2,
    AIMSENDRATE = 3,

    MAX_PLAYERS = 1004,
    MAX_VEHICLES = 2000,
    MAX_PICKUPS = 4096,
    MAX_OBJECTS = 1000,
    MAX_GANGZONES = 1024,
    MAX_3DTEXTS = 2048,
    MAX_TEXTDRAWS = 2048,
    MAX_CLIENTCMDS = 144,
    MAX_MENUS = 128,
}

for key, value in pairs(sampfuncs) do
    if rawget(_G, key) == nil then
        _G[key] = value
    end
end

return sampfuncs
