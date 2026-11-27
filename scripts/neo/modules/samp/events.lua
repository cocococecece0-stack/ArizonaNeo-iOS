-- Portable `samp.events` compatibility registry.
--
-- The original MoonLoader/NeoLoader module is a RakNet-backed dispatcher: it
-- reads and writes SA-MP bitstreams and is driven by native `onReceiveRpc`,
-- `onSendRpc`, `onReceivePacket` and `onSendPacket` callbacks. None of those
-- callbacks or RakNet bitstream functions are confirmed on iOS yet.
--
-- This module keeps only the part that is portable and independently useful:
-- the per-script handler table used by scripts such as:
--
--   local sampev = require('lib.samp.events')
--   function sampev.onServerMessage(color, text) ... end
--
-- Handlers are stored exactly as scripts assign them. Nothing in this module
-- invokes a handler on its own. A future confirmed GameBridge adapter must
-- call `dispatch(event_name, ...)` when it receives a real game event. This
-- module never fabricates game data, RPCs or packets.

local handlers = {}

local function check_name(name)
    if type(name) ~= 'string' or name == '' then
        error('samp.events: event name must be a non-empty string', 3)
    end
end

local api = {
    MODULEINFO = {
        name = 'samp.events',
        version = 4,
    },
    INTERFACE = {
        OUTCOMING_RPCS = {},
        OUTCOMING_PACKETS = {},
        INCOMING_RPCS = {},
        INCOMING_PACKETS = {},
    },
    EXPORTS = {},
}

-- Register a handler. The field-assignment form used by original scripts is
-- implemented by the metatable below; this function is an explicit alternative
-- for new code and for tests.
function api.add_handler(name, callback)
    check_name(name)
    if type(callback) ~= 'function' then
        error('samp.events.add_handler: callback must be a function', 2)
    end
    handlers[name] = callback
    return callback
end

function api.remove_handler(name)
    check_name(name)
    local existed = handlers[name] ~= nil
    handlers[name] = nil
    return existed
end

function api.has_handler(name)
    return type(name) == 'string' and type(handlers[name]) == 'function'
end

function api.get_handler(name)
    return handlers[name]
end

-- Called by a confirmed game adapter (or by tests). No native code and no
-- synthetic game data are involved. Handler errors propagate to the caller.
function api.dispatch(name, ...)
    check_name(name)
    local callback = handlers[name]
    if type(callback) ~= 'function' then
        return nil
    end
    return callback(...)
end

-- Remove one handler or all handlers when name is nil.
function api.clear(name)
    if name == nil then
        handlers = {}
    else
        check_name(name)
        handlers[name] = nil
    end
end

local events = setmetatable({}, {
    __index = function(_, key)
        local member = api[key]
        if member ~= nil then
            return member
        end
        return handlers[key]
    end,
    __newindex = function(_, key, value)
        if type(key) ~= 'string' then
            error('samp.events: event name must be a string', 2)
        end
        if value == nil then
            handlers[key] = nil
            return
        end
        if type(value) ~= 'function' then
            error('samp.events.' .. key .. ' must be a function or nil', 2)
        end
        handlers[key] = value
    end,
})

-- Both spellings are used by real scripts. Keep one registry even when the
-- sandboxed require implementation caches modules by requested name.
if type(package) == 'table' and type(package.loaded) == 'table' then
    package.loaded['samp.events'] = events
    package.loaded['lib.samp.events'] = events
end

return events
