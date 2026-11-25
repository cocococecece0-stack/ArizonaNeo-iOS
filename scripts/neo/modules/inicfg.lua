-- Portable inicfg compatibility module backed by the sandboxed NeoLoader API.
-- Implements the subset used by original NeoLoader scripts:
--   inicfg.load(default, file) -> table
--   inicfg.save(cfg, file) -> boolean
local inicfg = {}

local function read(path)
    local content = neo_fs_read(path)
    if type(content) ~= 'string' then return nil end
    return content
end

local function value(str)
    local lwr = str:lower()
    if lwr == 'true' then return true end
    if lwr == 'false' then return false end
    return tonumber(str) or str
end

function inicfg.load(default, file)
    local data = default or {}
    if type(file) ~= 'string' then return data end
    local content = read('config/' .. file)
    if not content then return data end
    local section = nil
    for line in content:gmatch('[^\r\n]+') do
        local sec = line:match('^%s*%[([^%[%]]+)%]%s*$')
        if sec then
            section = tonumber(sec) or sec
            if type(data[section]) ~= 'table' then data[section] = {} end
        else
            local key, val = line:match('^%s*([^=%s]+)%s-=%s*(.*)$')
            if key then
                local target = section and data[section] or data
                target[key] = value(val)
            end
        end
    end
    return data
end

function inicfg.save(cfg, file)
    if type(file) ~= 'string' or type(cfg) ~= 'table' then return false end
    local parts = {}
    for k, v in pairs(cfg) do
        if type(v) == 'table' then
            parts[#parts + 1] = '[' .. tostring(k) .. ']'
            for sk, sv in pairs(v) do
                parts[#parts + 1] = tostring(sk) .. '=' .. tostring(sv)
            end
            parts[#parts + 1] = ''
        else
            parts[#parts + 1] = tostring(k) .. '=' .. tostring(v)
        end
    end
    return neo_fs_write('config/' .. file, table.concat(parts, '\n')) and true or false
end

return inicfg
