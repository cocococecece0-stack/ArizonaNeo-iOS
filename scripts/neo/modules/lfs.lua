-- Portable lfs compatibility module backed by the sandbox NeoLoader API.
-- Implements the subset used by original NeoLoader scripts: attributes,
-- dir, mkdir and rmdir-family no-ops for directories (see limitations).
local lfs = {}

local function parent(path)
    return path:match('^(.*)/[^/]+$')
end

function lfs.attributes(path)
    if type(path) ~= 'string' then return nil end
    if neo_fs_exists(path) then
        return { mode = 'file', size = 0 }
    end
    local entries = neo_fs_list(path)
    if type(entries) == 'table' and #entries > 0 then
        return { mode = 'directory', size = 0 }
    end
    return nil
end

function lfs.dir(path)
    local names = neo_fs_list(path)
    if type(names) ~= 'table' then
        return function() return nil end
    end
    local index = 0
    return function()
        index = index + 1
        return names[index]
    end
end

function lfs.mkdir(path)
    return neo_fs_mkdir(path) and true or false
end

function lfs.rmdir(path)
    -- Directory removal is intentionally not exposed by the sandbox.
    -- Returning nil makes callers treat it as unsupported rather than succeed.
    return nil
end

return lfs
