-- Portable jsoncfg compatibility module backed by the sandboxed NeoLoader API.
-- Uses dkjson (original NeoLoader dependency) for JSON encoding/decoding.
local jsoncfg = {}
local json = require('dkjson')

function jsoncfg.load(default, filename, extension)
    local data = default or {}
    if type(filename) ~= 'string' then return data end
    local suffix = extension or '.json'
    local path = 'config/' .. filename
    if path:sub(-#suffix) ~= suffix then path = path .. suffix end
    local content = neo_fs_read(path)
    if type(content) ~= 'string' then return data end
    local decoded = json.decode(content)
    if type(decoded) ~= 'table' then return data end
    for k, v in pairs(decoded) do data[k] = v end
    return data
end

function jsoncfg.save(cfg, filename, extension)
    if type(cfg) ~= 'table' or type(filename) ~= 'string' then return false end
    local suffix = extension or '.json'
    local path = 'config/' .. filename
    if path:sub(-#suffix) ~= suffix then path = path .. suffix end
    return neo_fs_write(path, json.encode(cfg)) and true or false
end

return jsoncfg
