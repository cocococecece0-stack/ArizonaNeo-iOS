#pragma once

#include <string>

struct NeoModuleEntry { const char* name; const char* source; };

static const NeoModuleEntry NEO_MODULES[] = {
    {"sha256", R"NEOMOD_DELIM(
-- Pure LuaJIT SHA-256 implementation using the built-in 'bit' library.
-- Provides sha256.digest(str), sha256.hex(str), sha256.file(path).

local bit = bit or require("bit")
local band = bit.band
local bor = bit.bor
local bxor = bit.bxor
local bnot = bit.bnot
local rshift = bit.rshift
local ror = bit.ror or function(x, n)
    return bor(rshift(x, n), bit.lshift(x, 32 - n))
end

local M = {}

-- Initial hash values
local H_INIT = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
}

-- Round constants
local K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
}

local function to_u32(val)
    return band(val, 0xFFFFFFFF)
end

local function add(a, b)
    return to_u32(a + b)
end

local function add4(a, b, c, d)
    return to_u32(a + b + c + d)
end

local function add5(a, b, c, d, e)
    return to_u32(a + b + c + d + e)
end

local function S0(x)
    return bxor(ror(x, 2), ror(x, 13), ror(x, 22))
end

local function S1(x)
    return bxor(ror(x, 6), ror(x, 11), ror(x, 25))
end

local function s0(x)
    return bxor(ror(x, 7), ror(x, 18), rshift(x, 3))
end

local function s1(x)
    return bxor(ror(x, 17), ror(x, 19), rshift(x, 10))
end

local function ch(x, y, z)
    return bxor(band(x, y), band(bnot(x), z))
end

local function maj(x, y, z)
    return bxor(band(x, y), band(x, z), band(y, z))
end

function M.new()
    local h0, h1, h2, h3, h4, h5, h6, h7 = unpack(H_INIT)
    local buffer = ""
    local total_len = 0
    local w = {}

    local self = {}

    local function process_chunk(chunk)
        for i = 0, 15 do
            local idx = i * 4 + 1
            local b1, b2, b3, b4 = string.byte(chunk, idx, idx + 3)
            w[i] = to_u32(b1 * 16777216 + b2 * 65536 + b3 * 256 + b4)
        end
        for i = 16, 63 do
            w[i] = add4(s1(w[i - 2]), w[i - 7], s0(w[i - 15]), w[i - 16])
        end

        local a, b, c, d, e, f, g, h = h0, h1, h2, h3, h4, h5, h6, h7
        for i = 0, 63 do
            local t1 = add5(h, S1(e), ch(e, f, g), K[i + 1], w[i])
            local t2 = add(S0(a), maj(a, b, c))
            h = g
            g = f
            f = e
            e = add(d, t1)
            d = c
            c = b
            b = a
            a = add(t1, t2)
        end

        h0 = add(h0, a)
        h1 = add(h1, b)
        h2 = add(h2, c)
        h3 = add(h3, d)
        h4 = add(h4, e)
        h5 = add(h5, f)
        h6 = add(h6, g)
        h7 = add(h7, h)
    end

    function self:update(data)
        buffer = buffer .. data
        total_len = total_len + #data
        while #buffer >= 64 do
            process_chunk(buffer:sub(1, 64))
            buffer = buffer:sub(65)
        end
        return self
    end

    function self:final()
        local bit_len = total_len * 8
        buffer = buffer .. string.char(0x80)
        local pad_len = (56 - (#buffer % 64)) % 64
        buffer = buffer .. string.rep(string.char(0), pad_len)

        -- Append 64-bit length (big-endian)
        local high = math.floor(bit_len / 4294967296)
        local low = bit_len % 4294967296
        local len_bytes = string.char(
            band(rshift(high, 24), 0xFF), band(rshift(high, 16), 0xFF),
            band(rshift(high, 8), 0xFF), band(high, 0xFF),
            band(rshift(low, 24), 0xFF), band(rshift(low, 16), 0xFF),
            band(rshift(low, 8), 0xFF), band(low, 0xFF)
        )
        buffer = buffer .. len_bytes

        while #buffer >= 64 do
            process_chunk(buffer:sub(1, 64))
            buffer = buffer:sub(65)
        end

        local function hex32(val)
            local u = (val < 0) and (val + 0x100000000) or val
            return string.format("%08x", u)
        end

        return hex32(h0) .. hex32(h1) .. hex32(h2) .. hex32(h3) ..
               hex32(h4) .. hex32(h5) .. hex32(h6) .. hex32(h7)
    end

    return self
end

function M.hex(data)
    local hasher = M.new()
    hasher:update(data)
    return hasher:final()
end

function M.file(filepath)
    local f, err = io.open(filepath, "rb")
    if not f then
        return nil, err
    end
    local hasher = M.new()
    while true do
        local chunk = f:read(65536)
        if not chunk or #chunk == 0 then break end
        hasher:update(chunk)
    end
    f:close()
    return hasher:final()
end

return M

)NEOMOD_DELIM"},
    {"base64", R"NEOMOD_DELIM(
local base64 = {}

local extract = _G.bit32 and _G.bit32.extract
if not extract then
	if _G.bit then
		local shl, shr, band = _G.bit.lshift, _G.bit.rshift, _G.bit.band
		extract = function( v, from, width )
			return band( shr( v, from ), shl( 1, width ) - 1 )
		end
	elseif _G._VERSION == "Lua 5.1" then
		extract = function( v, from, width )
			local w = 0
			local flag = 2^from
			for i = 0, width-1 do
				local flag2 = flag + flag
				if v % flag2 >= flag then
					w = w + 2^i
				end
				flag = flag2
			end
			return w
		end
	else
		extract = load[[return function( v, from, width )
			return ( v >> from ) & ((1 << width) - 1)
		end]]()
	end
end

function base64.makeencoder( s62, s63, spad )
	local encoder = {}
	for b64code, char in pairs{[0]='A','B','C','D','E','F','G','H','I','J',
		'K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y',
		'Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n',
		'o','p','q','r','s','t','u','v','w','x','y','z','0','1','2',
		'3','4','5','6','7','8','9',s62 or '+',s63 or'/',spad or'='} do
		encoder[b64code] = char:byte()
	end
	return encoder
end

function base64.makedecoder( s62, s63, spad )
	local decoder = {}
	for b64code, charcode in pairs( base64.makeencoder( s62, s63, spad )) do
		decoder[charcode] = b64code
	end
	return decoder
end

local DEFAULT_ENCODER = base64.makeencoder()
local DEFAULT_DECODER = base64.makedecoder()

local char, concat = string.char, table.concat

function base64.encode( str, encoder, usecaching )
	encoder = encoder or DEFAULT_ENCODER
	local t, k, n = {}, 1, #str
	local lastn = n % 3
	local cache = {}
	for i = 1, n-lastn, 3 do
		local a, b, c = str:byte( i, i+2 )
		local v = a*0x10000 + b*0x100 + c
		local s
		if usecaching then
			s = cache[v]
			if not s then
				s = char(encoder[extract(v,18,6)], encoder[extract(v,12,6)], encoder[extract(v,6,6)], encoder[extract(v,0,6)])
				cache[v] = s
			end
		else
			s = char(encoder[extract(v,18,6)], encoder[extract(v,12,6)], encoder[extract(v,6,6)], encoder[extract(v,0,6)])
		end
		t[k] = s
		k = k + 1
	end
	if lastn == 2 then
		local a, b = str:byte( n-1, n )
		local v = a*0x10000 + b*0x100
		t[k] = char(encoder[extract(v,18,6)], encoder[extract(v,12,6)], encoder[extract(v,6,6)], encoder[64])
	elseif lastn == 1 then
		local v = str:byte( n )*0x10000
		t[k] = char(encoder[extract(v,18,6)], encoder[extract(v,12,6)], encoder[64], encoder[64])
	end
	return concat( t )
end

function base64.decode( b64, decoder, usecaching )
	decoder = decoder or DEFAULT_DECODER
	local pattern = '[^%w%+%/%=]'
	if decoder then
		local s62, s63
		for charcode, b64code in pairs( decoder ) do
			if b64code == 62 then s62 = charcode
			elseif b64code == 63 then s63 = charcode
			end
		end
		pattern = ('[^%%w%%%s%%%s%%=]'):format( char(s62), char(s63) )
	end
	b64 = b64:gsub( pattern, '' )
	local cache = usecaching and {}
	local t, k = {}, 1
	local n = #b64
	local padding = b64:sub(-2) == '==' and 2 or b64:sub(-1) == '=' and 1 or 0
	for i = 1, padding > 0 and n-4 or n, 4 do
		local a, b, c, d = b64:byte( i, i+3 )
		local s
		if usecaching then
			local v0 = a*0x1000000 + b*0x10000 + c*0x100 + d
			s = cache[v0]
			if not s then
				local v = decoder[a]*0x40000 + decoder[b]*0x1000 + decoder[c]*0x40 + decoder[d]
				s = char( extract(v,16,8), extract(v,8,8), extract(v,0,8))
				cache[v0] = s
			end
		else
			local v = decoder[a]*0x40000 + decoder[b]*0x1000 + decoder[c]*0x40 + decoder[d]
			s = char( extract(v,16,8), extract(v,8,8), extract(v,0,8))
		end
		t[k] = s
		k = k + 1
	end
	if padding == 1 then
		local a, b, c = b64:byte( n-3, n-1 )
		local v = decoder[a]*0x40000 + decoder[b]*0x1000 + decoder[c]*0x40
		t[k] = char( extract(v,16,8), extract(v,8,8))
	elseif padding == 2 then
		local a, b = b64:byte( n-3, n-2 )
		local v = decoder[a]*0x40000 + decoder[b]*0x1000
		t[k] = char( extract(v,16,8))
	end
	return concat( t )
end

return base64

)NEOMOD_DELIM"},
    {"binaryheap", R"NEOMOD_DELIM(
local assert = assert
local floor = math.floor
local _ENV = nil

local M = {}

M.binaryHeap = function(swap, erase, lt)

  local heap = {
      values = {},
      erase = erase,
      swap = swap,
      lt = lt,
    }

  function heap:bubbleUp(pos)
    local values = self.values
    while pos>1 do
      local parent = floor(pos/2)
      if not lt(values[pos], values[parent]) then
          break
      end
      swap(self, parent, pos)
      pos = parent
    end
  end

  function heap:sinkDown(pos)
    local values = self.values
    local last = #values
    while true do
      local min = pos
      local child = 2 * pos

      for c = child, child + 1 do
        if c <= last and lt(values[c], values[min]) then min = c end
      end

      if min == pos then break end

      swap(self, pos, min)
      pos = min
    end
  end

  return heap
end

do end

local update

update = function(self, pos, newValue)
  assert(newValue ~= nil, "cannot add 'nil' as value")
  assert(pos >= 1 and pos <= #self.values, "illegal position")
  self.values[pos] = newValue
  if pos > 1 then self:bubbleUp(pos) end
  if pos < #self.values then self:sinkDown(pos) end
end

local remove

remove = function(self, pos)
  local last = #self.values
  if pos < 1 then
    return

  elseif pos < last then
    local v = self.values[pos]
    self:swap(pos, last)
    self:erase(last)
    self:bubbleUp(pos)
    self:sinkDown(pos)
    return v

  elseif pos == last then
    local v = self.values[pos]
    self:erase(last)
    return v

  else
    return
  end
end

local insert

insert = function(self, value)
  assert(value ~= nil, "cannot add 'nil' as value")
  local pos = #self.values + 1
  self.values[pos] = value
  self:bubbleUp(pos)
end

local pop

pop = function(self)
  if self.values[1] ~= nil then
    return remove(self, 1)
  end
end

local peek

peek = function(self)
  return self.values[1]
end

local size

size = function(self)
  return #self.values
end

local function swap(heap, a, b)
  heap.values[a], heap.values[b] = heap.values[b], heap.values[a]
end

local function erase(heap, pos)
  heap.values[pos] = nil
end

local function plainHeap(lt)
  local h = M.binaryHeap(swap, erase, lt)
  h.peek = peek
  h.pop = pop
  h.size = size
  h.remove = remove
  h.insert = insert
  h.update = update
  return h
end

M.minHeap = function(lt)
  if not lt then
    lt = function(a,b) return (a < b) end
  end
  return plainHeap(lt)
end

M.maxHeap = function(gt)
  if not gt then
    gt = function(a,b) return (a > b) end
  end
  return plainHeap(gt)
end

do end

local updateU

function updateU(self, payload, newValue)
  return update(self, self.reverse[payload], newValue)
end

local insertU

function insertU(self, value, payload)
  assert(self.reverse[payload] == nil, "duplicate payload")
  local pos = #self.values + 1
  self.reverse[payload] = pos
  self.payloads[pos] = payload
  return insert(self, value)
end

local removeU

function removeU(self, payload)
  local pos = self.reverse[payload]
  if pos ~= nil then
    return remove(self, pos), payload
  end
end

local popU

function popU(self)
  if self.values[1] then
    local payload = self.payloads[1]
    local value = remove(self, 1)
    return payload, value
  end
end

local peekU

peekU = function(self)
  return self.payloads[1], self.values[1]
end

local peekValueU

peekValueU = function(self)
  return self.values[1]
end

local valueByPayload

valueByPayload = function(self, payload)
  return self.values[self.reverse[payload]]
end

local sizeU

sizeU = function(self)
  return #self.values
end

local function swapU(heap, a, b)
  local pla, plb = heap.payloads[a], heap.payloads[b]
  heap.reverse[pla], heap.reverse[plb] = b, a
  heap.payloads[a], heap.payloads[b] = plb, pla
  swap(heap, a, b)
end

local function eraseU(heap, pos)
  local payload = heap.payloads[pos]
  heap.reverse[payload] = nil
  heap.payloads[pos] = nil
  erase(heap, pos)
end

local function uniqueHeap(lt)
  local h = M.binaryHeap(swapU, eraseU, lt)
  h.payloads = {}
  h.reverse = {}
  h.peek = peekU
  h.peekValue = peekValueU
  h.valueByPayload = valueByPayload
  h.pop = popU
  h.size = sizeU
  h.remove = removeU
  h.insert = insertU
  h.update = updateU
  return h
end

M.minUnique = function(lt)
  if not lt then
    lt = function(a,b) return (a < b) end
  end
  return uniqueHeap(lt)
end

M.maxUnique = function(gt)
  if not gt then
    gt = function(a,b) return (a > b) end
  end
  return uniqueHeap(gt)
end

return M

)NEOMOD_DELIM"},
    {"moonloader", R"NEOMOD_DELIM(
TAG = {
	TYPE_INFO = 1,
	TYPE_DEBUG = 2,
	TYPE_ERROR = 3,
	TYPE_WARN = 4,
	TYPE_SYSTEM = 5,
	TYPE_FATAL = 6,
	TYPE_EXCEPTION = 7
}

local download_status = {
	STATUS_FINDINGRESOURCE = 1,
	STATUS_CONNECTING = 2,
	STATUS_REDIRECTING = 3,
	STATUS_BEGINDOWNLOADDATA = 4,
	STATUS_DOWNLOADINGDATA = 5,
	STATUS_ENDDOWNLOADDATA = 6,
	STATUS_BEGINDOWNLOADCOMPONENTS = 7,
	STATUS_INSTALLINGCOMPONENTS = 8,
	STATUS_ENDDOWNLOADCOMPONENTS = 9,
	STATUS_USINGCACHEDCOPY = 10,
	STATUS_SENDINGREQUEST = 11,
	STATUS_CLASSIDAVAILABLE = 12,
	STATUS_MIMETYPEAVAILABLE = 13,
	STATUS_CACHEFILENAMEAVAILABLE = 14,
	STATUS_BEGINSYNCOPERATION = 15,
	STATUS_ENDSYNCOPERATION = 16,
	STATUS_BEGINUPLOADDATA = 17,
	STATUS_UPLOADINGDATA = 18,
	STATUS_ENDUPLOADDATA = 19,
	STATUS_PROTOCOLCLASSID = 20,
	STATUS_ENCODING = 21,
	STATUS_VERIFIEDMIMETYPEAVAILABLE = 22,
	STATUS_CLASSINSTALLLOCATION = 23,
	STATUS_DECODING = 24,
	STATUS_LOADINGMIMEHANDLER = 25,
	STATUS_CONTENTDISPOSITIONATTACH = 26,
	STATUS_FILTERREPORTMIMETYPE = 27,
	STATUS_CLSIDCANINSTANTIATE = 28,
	STATUS_IUNKNOWNAVAILABLE = 29,
	STATUS_DIRECTBIND = 30,
	STATUS_RAWMIMETYPE = 31,
	STATUS_PROXYDETECTING = 32,
	STATUS_ACCEPTRANGES = 33,
	STATUS_COOKIE_SENT = 34,
	STATUS_COMPACT_POLICY_RECEIVED = 35,
	STATUS_COOKIE_SUPPRESSED = 36,
	STATUS_COOKIE_STATE_UNKNOWN = 37,
	STATUS_COOKIE_STATE_ACCEPT = 38,
	STATUS_COOKIE_STATE_REJECT = 39,
	STATUS_COOKIE_STATE_PROMPT = 40,
	STATUS_COOKIE_STATE_LEASH = 41,
	STATUS_COOKIE_STATE_DOWNGRADE = 42,
	STATUS_POLICY_HREF = 43,
	STATUS_P3P_HEADER = 44,
	STATUS_SESSION_COOKIE_RECEIVED = 45,
	STATUS_PERSISTENT_COOKIE_RECEIVED = 46,
	STATUS_SESSION_COOKIES_ALLOWED = 47,
	STATUS_CACHECONTROL = 48,
	STATUS_CONTENTDISPOSITIONFILENAME = 49,
	STATUS_MIMETEXTPLAINMISMATCH = 50,
	STATUS_PUBLISHERAVAILABLE = 51,
	STATUS_DISPLAYNAMEAVAILABLE = 52,
	STATUS_SSLUX_NAVBLOCKED = 53,
	STATUS_SERVER_MIMETYPEAVAILABLE = 54,
	STATUS_SNIFFED_CLASSIDAVAILABLE = 55,
	STATUS_64BIT_PROGRESS = 56,
	STATUSEX_STARTBINDING = 57,
	STATUSEX_ENDDOWNLOAD = 58,
	STATUSEX_LOWRESOURCE = 59,
	STATUSEX_DATAAVAILABLE = 60,
}

local font_flags = {
	NONE      = 0x0,
	BOLD      = 0x1,
	ITALICS   = 0x2,
	BORDER    = 0x4,
	SHADOW    = 0x8,
	UNDERLINE = 0x10,
	STRIKEOUT = 0x20
}

local d3d_prim_type = {
	POINTLIST     = 1,
	LINELIST      = 2,
	LINESTRIP     = 3,
	TRIANGLELIST  = 4,
	TRIANGLESTRIP = 5,
	TRIANGLEFAN   = 6
}

local audiostream_state = {
	STOP   = 0,
	PLAY   = 1,
	PAUSE  = 2,
	RESUME = 3
}

local audiostream_status = {
	STOPPED = -1,
	PLAYING =  1,
	PAUSED  =  2
}

if not rawget(_G, 'Vector3D') then
	local status, v3d = pcall(require, 'vector3d')
	if status and v3d then
		_G.Vector3D = v3d
	end
end

if not rawget(_G, 'matrix3x3') then
	local status, m3 = pcall(require, 'matrix3x3')
	if status and m3 then
		_G.matrix3x3 = m3
	end
end

return {
	message_prefix = TAG,
	download_status = download_status,
	font_flag = font_flags,
	d3d_prim_type = d3d_prim_type,
	audiostream_state = audiostream_state,
	audiostream_status = audiostream_status,
}

)NEOMOD_DELIM"},
    {"vector3d", R"NEOMOD_DELIM(
local EPSILON = 0.0001
local function isNearZero(f)
	return math.abs(f) < EPSILON
end

local mt = {}
mt.__index = mt

function mt:get()
	return self.x, self.y, self.z
end

function mt:length()
	return math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
end

function mt:normalize()
	local len = self:length()
	if len < EPSILON then return 0 end
	self.x = self.x / len
	self.y = self.y / len
	self.z = self.z / len
	return len
end

function mt:zeroNearZero()
	if isNearZero(self.x) then self.x = 0 end
	if isNearZero(self.y) then self.y = 0 end
	if isNearZero(self.z) then self.z = 0 end
end
mt.zero_near_zero = mt.zeroNearZero

function mt:dotProduct(v)
	local vx = type(v) == "table" and (v.x or v[1] or 0) or tonumber(v) or 0
	local vy = type(v) == "table" and (v.y or v[2] or 0) or tonumber(v) or 0
	local vz = type(v) == "table" and (v.z or v[3] or 0) or tonumber(v) or 0
	return self.x * vx + self.y * vy + self.z * vz
end
mt.dot_product = mt.dotProduct

function mt:crossProduct(v)
	local vx = type(v) == "table" and (v.x or v[1] or 0) or tonumber(v) or 0
	local vy = type(v) == "table" and (v.y or v[2] or 0) or tonumber(v) or 0
	local vz = type(v) == "table" and (v.z or v[3] or 0) or tonumber(v) or 0
	local x, y, z = self.x, self.y, self.z
	self.x = y * vz - vy * z
	self.y = z * vx - vz * x
	self.z = x * vy - vx * y
end
mt.cross_product = mt.crossProduct

local function get_xyz(val)
	if type(val) == "number" then
		return val, val, val
	end
	if type(val) == "table" or type(val) == "cdata" then
		local ok, vx = pcall(function() return val.x or val[0] or val[1] end)
		local ok2, vy = pcall(function() return val.y or val[1] or val[2] end)
		local ok3, vz = pcall(function() return val.z or val[2] or val[3] end)
		return tonumber(vx) or 0, tonumber(vy) or 0, tonumber(vz) or 0
	end
	return 0, 0, 0
end

function mt:__add(v)
	local vx, vy, vz = get_xyz(v)
	return Vector3D(self.x + vx, self.y + vy, self.z + vz)
end

function mt:__mul(v)
	if type(v) == "number" then
		return Vector3D(self.x * v, self.y * v, self.z * v)
	end
	local vx, vy, vz = get_xyz(v)
	return Vector3D(self.x * vx, self.y * vy, self.z * vz)
end

function mt:__div(v)
	if type(v) == "number" then
		if v == 0 then return Vector3D(0, 0, 0) end
		return Vector3D(self.x / v, self.y / v, self.z / v)
	end
	local vx, vy, vz = get_xyz(v)
	return Vector3D(vx ~= 0 and self.x / vx or 0, vy ~= 0 and self.y / vy or 0, vz ~= 0 and self.z / vz or 0)
end

function mt:__sub(v)
	local vx, vy, vz = get_xyz(v)
	return Vector3D(self.x - vx, self.y - vy, self.z - vz)
end

function mt:__tostring()
	return string.format("Vector3D(%.3f, %.3f, %.3f)", self.x, self.y, self.z)
end

Vector3D = function(x, y, z)
	local obj = {
		x = tonumber(x) or 0,
		y = tonumber(y) or 0,
		z = tonumber(z) or 0
	}
	return setmetatable(obj, mt)
end

return Vector3D

)NEOMOD_DELIM"},
    {"matrix3x3", R"NEOMOD_DELIM(
local Vector3D = require "vector3d"

Matrix3X3 = function(rightX, rightY, rightZ, frontX, frontY, frontZ, upX, upY, upZ)
	local mt = {}
	local obj = {}
	if rightX ~= nil then
		obj.right = Vector3D(rightX, rightY, rightZ)
		obj.front = Vector3D(frontX, frontY, frontZ)
		obj.up = Vector3D(upX, upY, upZ)
	else
		obj.right = Vector3D(1, 0, 0)
		obj.front = Vector3D(0, 1, 0)
		obj.up = Vector3D(0, 0, 1)
	end

	function obj:get()
		return obj.right.x, obj.right.y, obj.right.z,
			obj.front.x, obj.front.y, obj.front.z,
			obj.up.x, obj.up.y, obj.up.z
	end

	function obj:vectorMul(v)
		return Vector3D(
			self.right.x * v.x + self.front.x * v.y + self.up.x * v.z,
			self.right.y * v.x + self.front.y * v.y + self.up.y * v.z,
			self.right.z * v.x + self.front.z * v.y + self.up.z * v.z)
	end

	function obj:rotate(v, theta)
		local tsin, tcos = math.sin(theta), math.cos(theta)
		local rightX, rightY, rightZ, frontX, frontY, frontZ, upX, upY, upZ

		rightX = tcos + (1.0 - tcos) * v.x * v.x
		rightY = (1.0 - tcos) * v.x * v.y - tsin * v.z
		rightZ = (1.0 - tcos) * v.x * v.z + tsin * v.y

		frontX = (1.0 - tcos) * v.y * v.x + tsin * v.z
		frontY = tcos + (1.0 - tcos) * v.y * v.y
		frontZ = (1.0 - tcos) * v.y * v.z - tsin * v.x

		upX = (1.0 - tcos) * v.z * v.x - tsin * v.y
		upY = (1.0 - tcos) * v.z * v.y + tsin * v.x
		upZ = tcos + (1.0 - tcos) * v.z * v.z
		return Matrix3X3(rightX, rightY, rightZ, frontX, frontY, frontZ, upX, upY, upZ) * self
	end

	obj.vector_mul = obj.vectorMul

	function mt:__mul(m)
		local mat = Matrix3X3()
		mat.right = self:vectorMul(m.right)
		mat.front = self:vectorMul(m.front)
		mat.up = self:vectorMul(m.up)
		return mat
	end

	setmetatable(obj, mt)
	return obj
end

return Matrix3X3

)NEOMOD_DELIM"},
    {"dkjson", R"NEOMOD_DELIM(
-- Module options:
local always_use_lpeg = false
local register_global_module_table = false
local global_module_name = 'json'

--[==[

David Kolf's JSON module for Lua 5.1 - 5.4

Version 2.8


For the documentation see the corresponding readme.txt or visit
<http://dkolf.de/dkjson-lua/>.

You can contact the author by sending an e-mail to 'david' at the
domain 'dkolf.de'.


Copyright (C) 2010-2024 David Heiko Kolf

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

--]==]

-- global dependencies:
local pairs, type, tostring, tonumber, getmetatable, setmetatable =
      pairs, type, tostring, tonumber, getmetatable, setmetatable
local error, require, pcall, select = error, require, pcall, select
local floor, huge = math.floor, math.huge
local strrep, gsub, strsub, strbyte, strchar, strfind, strlen, strformat =
      string.rep, string.gsub, string.sub, string.byte, string.char,
      string.find, string.len, string.format
local strmatch = string.match
local concat = table.concat

local json = { version = "dkjson 2.8" }

local jsonlpeg = {}

if register_global_module_table then
  if always_use_lpeg then
    _G[global_module_name] = jsonlpeg
  else
    _G[global_module_name] = json
  end
end

local _ENV = nil -- blocking globals in Lua 5.2 and later

pcall (function()
  -- Enable access to blocked metatables.
  -- Don't worry, this module doesn't change anything in them.
  local debmeta = require "debug".getmetatable
  if debmeta then getmetatable = debmeta end
end)

json.null = setmetatable ({}, {
  __tojson = function () return "null" end
})

local function isarray (tbl)
  local max, n, arraylen = 0, 0, 0
  for k,v in pairs (tbl) do
    if k == 'n' and type(v) == 'number' then
      arraylen = v
      if v > max then
        max = v
      end
    else
      if type(k) ~= 'number' or k < 1 or floor(k) ~= k then
        return false
      end
      if k > max then
        max = k
      end
      n = n + 1
    end
  end
  if max > 10 and max > arraylen and max > n * 2 then
    return false -- don't create an array with too many holes
  end
  return true, max
end

local escapecodes = {
  ["\""] = "\\\"", ["\\"] = "\\\\", ["\b"] = "\\b", ["\f"] = "\\f",
  ["\n"] = "\\n",  ["\r"] = "\\r",  ["\t"] = "\\t"
}

local function escapeutf8 (uchar)
  local value = escapecodes[uchar]
  if value then
    return value
  end
  local a, b, c, d = strbyte (uchar, 1, 4)
  a, b, c, d = a or 0, b or 0, c or 0, d or 0
  if a <= 0x7f then
    value = a
  elseif 0xc0 <= a and a <= 0xdf and b >= 0x80 then
    value = (a - 0xc0) * 0x40 + b - 0x80
  elseif 0xe0 <= a and a <= 0xef and b >= 0x80 and c >= 0x80 then
    value = ((a - 0xe0) * 0x40 + b - 0x80) * 0x40 + c - 0x80
  elseif 0xf0 <= a and a <= 0xf7 and b >= 0x80 and c >= 0x80 and d >= 0x80 then
    value = (((a - 0xf0) * 0x40 + b - 0x80) * 0x40 + c - 0x80) * 0x40 + d - 0x80
  else
    return ""
  end
  if value <= 0xffff then
    return strformat ("\\u%.4x", value)
  elseif value <= 0x10ffff then
    -- encode as UTF-16 surrogate pair
    value = value - 0x10000
    local highsur, lowsur = 0xD800 + floor (value/0x400), 0xDC00 + (value % 0x400)
    return strformat ("\\u%.4x\\u%.4x", highsur, lowsur)
  else
    return ""
  end
end

local function fsub (str, pattern, repl)
  -- gsub always builds a new string in a buffer, even when no match
  -- exists. First using find should be more efficient when most strings
  -- don't contain the pattern.
  if strfind (str, pattern) then
    return gsub (str, pattern, repl)
  else
    return str
  end
end

local function quotestring (value)
  -- based on the regexp "escapable" in https://github.com/douglascrockford/JSON-js
  value = fsub (value, "[%z\1-\31\"\\\127]", escapeutf8)
  if strfind (value, "[\194\216\220\225\226\239]") then
    value = fsub (value, "\194[\128-\159\173]", escapeutf8)
    value = fsub (value, "\216[\128-\132]", escapeutf8)
    value = fsub (value, "\220\143", escapeutf8)
    value = fsub (value, "\225\158[\180\181]", escapeutf8)
    value = fsub (value, "\226\128[\140-\143\168-\175]", escapeutf8)
    value = fsub (value, "\226\129[\160-\175]", escapeutf8)
    value = fsub (value, "\239\187\191", escapeutf8)
    value = fsub (value, "\239\191[\176-\191]", escapeutf8)
  end
  return "\"" .. value .. "\""
end
json.quotestring = quotestring

local function replace(str, o, n)
  local i, j = strfind (str, o, 1, true)
  if i then
    return strsub(str, 1, i-1) .. n .. strsub(str, j+1, -1)
  else
    return str
  end
end

-- locale independent num2str and str2num functions
local decpoint, numfilter

local function updatedecpoint ()
  decpoint = strmatch(tostring(0.5), "([^05+])")
  -- build a filter that can be used to remove group separators
  numfilter = "[^0-9%-%+eE" .. gsub(decpoint, "[%^%$%(%)%%%.%[%]%*%+%-%?]", "%%%0") .. "]+"
end

updatedecpoint()

local function num2str (num)
  return replace(fsub(tostring(num), numfilter, ""), decpoint, ".")
end

local function str2num (str)
  local num = tonumber(replace(str, ".", decpoint))
  if not num then
    updatedecpoint()
    num = tonumber(replace(str, ".", decpoint))
  end
  return num
end

local function addnewline2 (level, buffer, buflen)
  buffer[buflen+1] = "\n"
  buffer[buflen+2] = strrep ("  ", level)
  buflen = buflen + 2
  return buflen
end

function json.addnewline (state)
  if state.indent then
    state.bufferlen = addnewline2 (state.level or 0,
                           state.buffer, state.bufferlen or #(state.buffer))
  end
end

local encode2 -- forward declaration

local function addpair (key, value, prev, indent, level, buffer, buflen, tables, globalorder, state)
  local kt = type (key)
  if kt ~= 'string' and kt ~= 'number' then
    return nil, "type '" .. kt .. "' is not supported as a key by JSON."
  end
  if prev then
    buflen = buflen + 1
    buffer[buflen] = ","
  end
  if indent then
    buflen = addnewline2 (level, buffer, buflen)
  end
  -- When Lua is compiled with LUA_NOCVTN2S this will fail when
  -- numbers are mixed into the keys of the table. JSON keys are always
  -- strings, so this would be an implicit conversion too and the failure
  -- is intentional.
  buffer[buflen+1] = quotestring (key)
  buffer[buflen+2] = ":"
  return encode2 (value, indent, level, buffer, buflen + 2, tables, globalorder, state)
end

local function appendcustom(res, buffer, state)
  local buflen = state.bufferlen
  if type (res) == 'string' then
    buflen = buflen + 1
    buffer[buflen] = res
  end
  return buflen
end

local function exception(reason, value, state, buffer, buflen, defaultmessage)
  defaultmessage = defaultmessage or reason
  local handler = state.exception
  if not handler then
    return nil, defaultmessage
  else
    state.bufferlen = buflen
    local ret, msg = handler (reason, value, state, defaultmessage)
    if not ret then return nil, msg or defaultmessage end
    return appendcustom(ret, buffer, state)
  end
end

function json.encodeexception(reason, value, state, defaultmessage)
  return quotestring("<" .. defaultmessage .. ">")
end

encode2 = function (value, indent, level, buffer, buflen, tables, globalorder, state)
  local valtype = type (value)
  local valmeta = getmetatable (value)
  valmeta = type (valmeta) == 'table' and valmeta -- only tables
  local valtojson = valmeta and valmeta.__tojson
  if valtojson then
    if tables[value] then
      return exception('reference cycle', value, state, buffer, buflen)
    end
    tables[value] = true
    state.bufferlen = buflen
    local ret, msg = valtojson (value, state)
    if not ret then return exception('custom encoder failed', value, state, buffer, buflen, msg) end
    tables[value] = nil
    buflen = appendcustom(ret, buffer, state)
  elseif value == nil then
    buflen = buflen + 1
    buffer[buflen] = "null"
  elseif valtype == 'number' then
    local s
    if value ~= value or value >= huge or -value >= huge then
      -- This is the behaviour of the original JSON implementation.
      s = "null"
    else
      s = num2str (value)
    end
    buflen = buflen + 1
    buffer[buflen] = s
  elseif valtype == 'boolean' then
    buflen = buflen + 1
    buffer[buflen] = value and "true" or "false"
  elseif valtype == 'string' then
    buflen = buflen + 1
    buffer[buflen] = quotestring (value)
  elseif valtype == 'table' then
    if tables[value] then
      return exception('reference cycle', value, state, buffer, buflen)
    end
    tables[value] = true
    level = level + 1
    local isa, n = isarray (value)
    if n == 0 and valmeta and valmeta.__jsontype == 'object' then
      isa = false
    end
    local msg
    if isa then -- JSON array
      buflen = buflen + 1
      buffer[buflen] = "["
      for i = 1, n do
        buflen, msg = encode2 (value[i], indent, level, buffer, buflen, tables, globalorder, state)
        if not buflen then return nil, msg end
        if i < n then
          buflen = buflen + 1
          buffer[buflen] = ","
        end
      end
      buflen = buflen + 1
      buffer[buflen] = "]"
    else -- JSON object
      local prev = false
      buflen = buflen + 1
      buffer[buflen] = "{"
      local order = valmeta and valmeta.__jsonorder or globalorder
      if order then
        local used = {}
        n = #order
        for i = 1, n do
          local k = order[i]
          local v = value[k]
          if v ~= nil then
            used[k] = true
            buflen, msg = addpair (k, v, prev, indent, level, buffer, buflen, tables, globalorder, state)
            if not buflen then return nil, msg end
            prev = true -- add a seperator before the next element
          end
        end
        for k,v in pairs (value) do
          if not used[k] then
            buflen, msg = addpair (k, v, prev, indent, level, buffer, buflen, tables, globalorder, state)
            if not buflen then return nil, msg end
            prev = true -- add a seperator before the next element
          end
        end
      else -- unordered
        for k,v in pairs (value) do
          buflen, msg = addpair (k, v, prev, indent, level, buffer, buflen, tables, globalorder, state)
          if not buflen then return nil, msg end
          prev = true -- add a seperator before the next element
        end
      end
      if indent then
        buflen = addnewline2 (level - 1, buffer, buflen)
      end
      buflen = buflen + 1
      buffer[buflen] = "}"
    end
    tables[value] = nil
  else
    return exception ('unsupported type', value, state, buffer, buflen,
      "type '" .. valtype .. "' is not supported by JSON.")
  end
  return buflen
end

function json.encode (value, state)
  state = state or {}
  local oldbuffer = state.buffer
  local buffer = oldbuffer or {}
  state.buffer = buffer
  updatedecpoint()
  local ret, msg = encode2 (value, state.indent, state.level or 0,
                   buffer, state.bufferlen or 0, state.tables or {}, state.keyorder, state)
  if not ret then
    error (msg, 2)
  elseif oldbuffer == buffer then
    state.bufferlen = ret
    return true
  else
    state.bufferlen = nil
    state.buffer = nil
    return concat (buffer)
  end
end

local function loc (str, where)
  local line, pos, linepos = 1, 1, 0
  while true do
    pos = strfind (str, "\n", pos, true)
    if pos and pos < where then
      line = line + 1
      linepos = pos
      pos = pos + 1
    else
      break
    end
  end
  return strformat ("line %d, column %d", line, where - linepos)
end

local function unterminated (str, what, where)
  return nil, strlen (str) + 1, "unterminated " .. what .. " at " .. loc (str, where)
end

local function scanwhite (str, pos)
  while true do
    pos = strfind (str, "%S", pos)
    if not pos then return nil end
    local sub2 = strsub (str, pos, pos + 1)
    if sub2 == "\239\187" and strsub (str, pos + 2, pos + 2) == "\191" then
      -- UTF-8 Byte Order Mark
      pos = pos + 3
    elseif sub2 == "//" then
      pos = strfind (str, "[\n\r]", pos + 2)
      if not pos then return nil end
    elseif sub2 == "/*" then
      pos = strfind (str, "*/", pos + 2)
      if not pos then return nil end
      pos = pos + 2
    else
      return pos
    end
  end
end

local escapechars = {
  ["\""] = "\"", ["\\"] = "\\", ["/"] = "/", ["b"] = "\b", ["f"] = "\f",
  ["n"] = "\n", ["r"] = "\r", ["t"] = "\t"
}

local function unichar (value)
  if value < 0 then
    return nil
  elseif value <= 0x007f then
    return strchar (value)
  elseif value <= 0x07ff then
    return strchar (0xc0 + floor(value/0x40),
                    0x80 + (floor(value) % 0x40))
  elseif value <= 0xffff then
    return strchar (0xe0 + floor(value/0x1000),
                    0x80 + (floor(value/0x40) % 0x40),
                    0x80 + (floor(value) % 0x40))
  elseif value <= 0x10ffff then
    return strchar (0xf0 + floor(value/0x40000),
                    0x80 + (floor(value/0x1000) % 0x40),
                    0x80 + (floor(value/0x40) % 0x40),
                    0x80 + (floor(value) % 0x40))
  else
    return nil
  end
end

local function scanstring (str, pos)
  local lastpos = pos + 1
  local buffer, n = {}, 0
  while true do
    local nextpos = strfind (str, "[\"\\]", lastpos)
    if not nextpos then
      return unterminated (str, "string", pos)
    end
    if nextpos > lastpos then
      n = n + 1
      buffer[n] = strsub (str, lastpos, nextpos - 1)
    end
    if strsub (str, nextpos, nextpos) == "\"" then
      lastpos = nextpos + 1
      break
    else
      local escchar = strsub (str, nextpos + 1, nextpos + 1)
      local value
      if escchar == "u" then
        value = tonumber (strsub (str, nextpos + 2, nextpos + 5), 16)
        if value then
          local value2
          if 0xD800 <= value and value <= 0xDBff then
            -- we have the high surrogate of UTF-16. Check if there is a
            -- low surrogate escaped nearby to combine them.
            if strsub (str, nextpos + 6, nextpos + 7) == "\\u" then
              value2 = tonumber (strsub (str, nextpos + 8, nextpos + 11), 16)
              if value2 and 0xDC00 <= value2 and value2 <= 0xDFFF then
                value = (value - 0xD800)  * 0x400 + (value2 - 0xDC00) + 0x10000
              else
                value2 = nil -- in case it was out of range for a low surrogate
              end
            end
          end
          value = value and unichar (value)
          if value then
            if value2 then
              lastpos = nextpos + 12
            else
              lastpos = nextpos + 6
            end
          end
        end
      end
      if not value then
        value = escapechars[escchar] or escchar
        lastpos = nextpos + 2
      end
      n = n + 1
      buffer[n] = value
    end
  end
  if n == 1 then
    return buffer[1], lastpos
  elseif n > 1 then
    return concat (buffer), lastpos
  else
    return "", lastpos
  end
end

local scanvalue -- forward declaration

local function scantable (what, closechar, str, startpos, nullval, objectmeta, arraymeta)
  local tbl, n = {}, 0
  local pos = startpos + 1
  if what == 'object' then
    setmetatable (tbl, objectmeta)
  else
    setmetatable (tbl, arraymeta)
  end
  while true do
    pos = scanwhite (str, pos)
    if not pos then return unterminated (str, what, startpos) end
    local char = strsub (str, pos, pos)
    if char == closechar then
      return tbl, pos + 1
    end
    local val1, err
    val1, pos, err = scanvalue (str, pos, nullval, objectmeta, arraymeta)
    if err then return nil, pos, err end
    pos = scanwhite (str, pos)
    if not pos then return unterminated (str, what, startpos) end
    char = strsub (str, pos, pos)
    if char == ":" then
      if val1 == nil then
        return nil, pos, "cannot use nil as table index (at " .. loc (str, pos) .. ")"
      end
      pos = scanwhite (str, pos + 1)
      if not pos then return unterminated (str, what, startpos) end
      local val2
      val2, pos, err = scanvalue (str, pos, nullval, objectmeta, arraymeta)
      if err then return nil, pos, err end
      tbl[val1] = val2
      pos = scanwhite (str, pos)
      if not pos then return unterminated (str, what, startpos) end
      char = strsub (str, pos, pos)
    else
      n = n + 1
      tbl[n] = val1
    end
    if char == "," then
      pos = pos + 1
    end
  end
end

scanvalue = function (str, pos, nullval, objectmeta, arraymeta)
  pos = pos or 1
  pos = scanwhite (str, pos)
  if not pos then
    return nil, strlen (str) + 1, "no valid JSON value (reached the end)"
  end
  local char = strsub (str, pos, pos)
  if char == "{" then
    return scantable ('object', "}", str, pos, nullval, objectmeta, arraymeta)
  elseif char == "[" then
    return scantable ('array', "]", str, pos, nullval, objectmeta, arraymeta)
  elseif char == "\"" then
    return scanstring (str, pos)
  else
    local pstart, pend = strfind (str, "^%-?[%d%.]+[eE]?[%+%-]?%d*", pos)
    if pstart then
      local number = str2num (strsub (str, pstart, pend))
      if number then
        return number, pend + 1
      end
    end
    pstart, pend = strfind (str, "^%a%w*", pos)
    if pstart then
      local name = strsub (str, pstart, pend)
      if name == "true" then
        return true, pend + 1
      elseif name == "false" then
        return false, pend + 1
      elseif name == "null" then
        return nullval, pend + 1
      end
    end
    return nil, pos, "no valid JSON value at " .. loc (str, pos)
  end
end

local function optionalmetatables(...)
  if select("#", ...) > 0 then
    return ...
  else
    return {__jsontype = 'object'}, {__jsontype = 'array'}
  end
end

function json.decode (str, pos, nullval, ...)
  local objectmeta, arraymeta = optionalmetatables(...)
  return scanvalue (str, pos, nullval, objectmeta, arraymeta)
end

function json.use_lpeg ()
  local g = require ("lpeg")

  if type(g.version) == 'function' and g.version() == "0.11" then
    error "due to a bug in LPeg 0.11, it cannot be used for JSON matching"
  end

  local pegmatch = g.match
  local P, S, R = g.P, g.S, g.R

  local function ErrorCall (str, pos, msg, state)
    if not state.msg then
      state.msg = msg .. " at " .. loc (str, pos)
      state.pos = pos
    end
    return false
  end

  local function Err (msg)
    return g.Cmt (g.Cc (msg) * g.Carg (2), ErrorCall)
  end

  local function ErrorUnterminatedCall (str, pos, what, state)
    return ErrorCall (str, pos - 1, "unterminated " .. what, state)
  end

  local SingleLineComment = P"//" * (1 - S"\n\r")^0
  local MultiLineComment = P"/*" * (1 - P"*/")^0 * P"*/"
  local Space = (S" \n\r\t" + P"\239\187\191" + SingleLineComment + MultiLineComment)^0

  local function ErrUnterminated (what)
    return g.Cmt (g.Cc (what) * g.Carg (2), ErrorUnterminatedCall)
  end

  local PlainChar = 1 - S"\"\\\n\r"
  local EscapeSequence = (P"\\" * g.C (S"\"\\/bfnrt" + Err "unsupported escape sequence")) / escapechars
  local HexDigit = R("09", "af", "AF")
  local function UTF16Surrogate (match, pos, high, low)
    high, low = tonumber (high, 16), tonumber (low, 16)
    if 0xD800 <= high and high <= 0xDBff and 0xDC00 <= low and low <= 0xDFFF then
      return true, unichar ((high - 0xD800)  * 0x400 + (low - 0xDC00) + 0x10000)
    else
      return false
    end
  end
  local function UTF16BMP (hex)
    return unichar (tonumber (hex, 16))
  end
  local U16Sequence = (P"\\u" * g.C (HexDigit * HexDigit * HexDigit * HexDigit))
  local UnicodeEscape = g.Cmt (U16Sequence * U16Sequence, UTF16Surrogate) + U16Sequence/UTF16BMP
  local Char = UnicodeEscape + EscapeSequence + PlainChar
  local String = P"\"" * (g.Cs (Char ^ 0) * P"\"" + ErrUnterminated "string")
  local Integer = P"-"^(-1) * (P"0" + (R"19" * R"09"^0))
  local Fractal = P"." * R"09"^0
  local Exponent = (S"eE") * (S"+-")^(-1) * R"09"^1
  local Number = (Integer * Fractal^(-1) * Exponent^(-1))/str2num
  local Constant = P"true" * g.Cc (true) + P"false" * g.Cc (false) + P"null" * g.Carg (1)
  local SimpleValue = Number + String + Constant
  local ArrayContent, ObjectContent

  -- The functions parsearray and parseobject parse only a single value/pair
  -- at a time and store them directly to avoid hitting the LPeg limits.
  local function parsearray (str, pos, nullval, state)
    local obj, cont
    local start = pos
    local npos
    local t, nt = {}, 0
    repeat
      obj, cont, npos = pegmatch (ArrayContent, str, pos, nullval, state)
      if cont == 'end' then
        return ErrorUnterminatedCall (str, start, "array", state)
      end
      pos = npos
      if cont == 'cont' or cont == 'last' then
        nt = nt + 1
        t[nt] = obj
      end
    until cont ~= 'cont'
    return pos, setmetatable (t, state.arraymeta)
  end

  local function parseobject (str, pos, nullval, state)
    local obj, key, cont
    local start = pos
    local npos
    local t = {}
    repeat
      key, obj, cont, npos = pegmatch (ObjectContent, str, pos, nullval, state)
      if cont == 'end' then
        return ErrorUnterminatedCall (str, start, "object", state)
      end
      pos = npos
      if cont == 'cont' or cont == 'last' then
        t[key] = obj
      end
    until cont ~= 'cont'
    return pos, setmetatable (t, state.objectmeta)
  end

  local Array = P"[" * g.Cmt (g.Carg(1) * g.Carg(2), parsearray)
  local Object = P"{" * g.Cmt (g.Carg(1) * g.Carg(2), parseobject)
  local Value = Space * (Array + Object + SimpleValue)
  local ExpectedValue = Value + Space * Err "value expected"
  local ExpectedKey = String + Err "key expected"
  local End = P(-1) * g.Cc'end'
  local ErrInvalid = Err "invalid JSON"
  ArrayContent = (Value * Space * (P"," * g.Cc'cont' + P"]" * g.Cc'last'+ End + ErrInvalid)  + g.Cc(nil) * (P"]" * g.Cc'empty' + End  + ErrInvalid)) * g.Cp()
  local Pair = g.Cg (Space * ExpectedKey * Space * (P":" + Err "colon expected") * ExpectedValue)
  ObjectContent = (g.Cc(nil) * g.Cc(nil) * P"}" * g.Cc'empty' + End + (Pair * Space * (P"," * g.Cc'cont' + P"}" * g.Cc'last' + End + ErrInvalid) + ErrInvalid)) * g.Cp()
  local DecodeValue = ExpectedValue * g.Cp ()

  jsonlpeg.version = json.version
  jsonlpeg.encode = json.encode
  jsonlpeg.null = json.null
  jsonlpeg.quotestring = json.quotestring
  jsonlpeg.addnewline = json.addnewline
  jsonlpeg.encodeexception = json.encodeexception
  jsonlpeg.using_lpeg = true

  function jsonlpeg.decode (str, pos, nullval, ...)
    local state = {}
    state.objectmeta, state.arraymeta = optionalmetatables(...)
    local obj, retpos = pegmatch (DecodeValue, str, pos, nullval, state)
    if state.msg then
      return nil, state.pos, state.msg
    else
      return obj, retpos
    end
  end

  -- cache result of this function:
  json.use_lpeg = function () return jsonlpeg end
  jsonlpeg.use_lpeg = json.use_lpeg

  return jsonlpeg
end

if always_use_lpeg then
  return json.use_lpeg()
end

return json


)NEOMOD_DELIM"},
    {"encoding", R"NEOMOD_DELIM(
local cp1251_to_utf8 = {
	["\128"] = "\208\130",
	["\129"] = "\208\131",
	["\130"] = "\226\128\154",
	["\131"] = "\209\147",
	["\132"] = "\226\128\158",
	["\133"] = "\226\128\166",
	["\134"] = "\226\128\160",
	["\135"] = "\226\128\161",
	["\136"] = "\226\130\172",
	["\137"] = "\226\128\176",
	["\138"] = "\208\137",
	["\139"] = "\226\128\185",
	["\140"] = "\208\138",
	["\141"] = "\208\140",
	["\142"] = "\208\139",
	["\143"] = "\208\143",
	["\144"] = "\209\146",
	["\145"] = "\226\128\152",
	["\146"] = "\226\128\153",
	["\147"] = "\226\128\156",
	["\148"] = "\226\128\157",
	["\149"] = "\226\128\162",
	["\150"] = "\226\128\147",
	["\151"] = "\226\128\148",
	["\152"] = "\127",
	["\153"] = "\226\132\162",
	["\154"] = "\209\153",
	["\155"] = "\226\128\186",
	["\156"] = "\209\154",
	["\157"] = "\209\156",
	["\158"] = "\209\155",
	["\159"] = "\209\159",
	["\160"] = "\194\160",
	["\161"] = "\208\142",
	["\162"] = "\209\158",
	["\163"] = "\208\136",
	["\164"] = "\194\164",
	["\165"] = "\210\144",
	["\166"] = "\194\166",
	["\167"] = "\194\167",
	["\168"] = "\208\129",
	["\169"] = "\194\169",
	["\170"] = "\208\132",
	["\171"] = "\194\171",
	["\172"] = "\194\172",
	["\173"] = "\194\173",
	["\174"] = "\194\174",
	["\175"] = "\208\135",
	["\176"] = "\194\176",
	["\177"] = "\194\177",
	["\178"] = "\208\134",
	["\179"] = "\209\150",
	["\180"] = "\210\145",
	["\181"] = "\194\181",
	["\182"] = "\194\182",
	["\183"] = "\194\183",
	["\184"] = "\209\145",
	["\185"] = "\226\132\150",
	["\186"] = "\209\148",
	["\187"] = "\194\187",
	["\188"] = "\209\152",
	["\189"] = "\208\133",
	["\190"] = "\209\149",
	["\191"] = "\209\151",
	["\192"] = "\208\144",
	["\193"] = "\208\145",
	["\194"] = "\208\146",
	["\195"] = "\208\147",
	["\196"] = "\208\148",
	["\197"] = "\208\149",
	["\198"] = "\208\150",
	["\199"] = "\208\151",
	["\200"] = "\208\152",
	["\201"] = "\208\153",
	["\202"] = "\208\154",
	["\203"] = "\208\155",
	["\204"] = "\208\156",
	["\205"] = "\208\157",
	["\206"] = "\208\158",
	["\207"] = "\208\159",
	["\208"] = "\208\160",
	["\209"] = "\208\161",
	["\210"] = "\208\162",
	["\211"] = "\208\163",
	["\212"] = "\208\164",
	["\213"] = "\208\165",
	["\214"] = "\208\166",
	["\215"] = "\208\167",
	["\216"] = "\208\168",
	["\217"] = "\208\169",
	["\218"] = "\208\170",
	["\219"] = "\208\171",
	["\220"] = "\208\172",
	["\221"] = "\208\173",
	["\222"] = "\208\174",
	["\223"] = "\208\175",
	["\224"] = "\208\176",
	["\225"] = "\208\177",
	["\226"] = "\208\178",
	["\227"] = "\208\179",
	["\228"] = "\208\180",
	["\229"] = "\208\181",
	["\230"] = "\208\182",
	["\231"] = "\208\183",
	["\232"] = "\208\184",
	["\233"] = "\208\185",
	["\234"] = "\208\186",
	["\235"] = "\208\187",
	["\236"] = "\208\188",
	["\237"] = "\208\189",
	["\238"] = "\208\190",
	["\239"] = "\208\191",
	["\240"] = "\209\128",
	["\241"] = "\209\129",
	["\242"] = "\209\130",
	["\243"] = "\209\131",
	["\244"] = "\209\132",
	["\245"] = "\209\133",
	["\246"] = "\209\134",
	["\247"] = "\209\135",
	["\248"] = "\209\136",
	["\249"] = "\209\137",
	["\250"] = "\209\138",
	["\251"] = "\209\139",
	["\252"] = "\209\140",
	["\253"] = "\209\141",
	["\254"] = "\209\142",
	["\255"] = "\209\143"
}

local utf8_to_cp1251 = {
	["\208\130"] = "\128",
	["\208\131"] = "\129",
	["\226\128\154"] = "\130",
	["\209\147"] = "\131",
	["\226\128\158"] = "\132",
	["\226\128\166"] = "\133",
	["\226\128\160"] = "\134",
	["\226\128\161"] = "\135",
	["\226\130\172"] = "\136",
	["\226\128\176"] = "\137",
	["\208\137"] = "\138",
	["\226\128\185"] = "\139",
	["\208\138"] = "\140",
	["\208\140"] = "\141",
	["\208\139"] = "\142",
	["\208\143"] = "\143",
	["\209\146"] = "\144",
	["\226\128\152"] = "\145",
	["\226\128\153"] = "\146",
	["\226\128\156"] = "\147",
	["\226\128\157"] = "\148",
	["\226\128\162"] = "\149",
	["\226\128\147"] = "\150",
	["\226\128\148"] = "\151",
	["\226\132\162"] = "\153",
	["\209\153"] = "\154",
	["\226\128\186"] = "\155",
	["\209\154"] = "\156",
	["\209\156"] = "\157",
	["\209\155"] = "\158",
	["\209\159"] = "\159",
	["\194\160"] = "\160",
	["\208\142"] = "\161",
	["\209\158"] = "\162",
	["\208\136"] = "\163",
	["\194\164"] = "\164",
	["\210\144"] = "\165",
	["\194\166"] = "\166",
	["\194\167"] = "\167",
	["\208\129"] = "\168",
	["\194\169"] = "\169",
	["\208\132"] = "\170",
	["\194\171"] = "\171",
	["\194\172"] = "\172",
	["\194\173"] = "\173",
	["\194\174"] = "\174",
	["\208\135"] = "\175",
	["\194\176"] = "\176",
	["\194\177"] = "\177",
	["\208\134"] = "\178",
	["\209\150"] = "\179",
	["\210\145"] = "\180",
	["\194\181"] = "\181",
	["\194\182"] = "\182",
	["\194\183"] = "\183",
	["\209\145"] = "\184",
	["\226\132\150"] = "\185",
	["\209\148"] = "\186",
	["\194\187"] = "\187",
	["\209\152"] = "\188",
	["\208\133"] = "\189",
	["\209\149"] = "\190",
	["\209\151"] = "\191",
	["\208\144"] = "\192",
	["\208\145"] = "\193",
	["\208\146"] = "\194",
	["\208\147"] = "\195",
	["\208\148"] = "\196",
	["\208\149"] = "\197",
	["\208\150"] = "\198",
	["\208\151"] = "\199",
	["\208\152"] = "\200",
	["\208\153"] = "\201",
	["\208\154"] = "\202",
	["\208\155"] = "\203",
	["\208\156"] = "\204",
	["\208\157"] = "\205",
	["\208\158"] = "\206",
	["\208\159"] = "\207",
	["\208\160"] = "\208",
	["\208\161"] = "\209",
	["\208\162"] = "\210",
	["\208\163"] = "\211",
	["\208\164"] = "\212",
	["\208\165"] = "\213",
	["\208\166"] = "\214",
	["\208\167"] = "\215",
	["\208\168"] = "\216",
	["\208\169"] = "\217",
	["\208\170"] = "\218",
	["\208\171"] = "\219",
	["\208\172"] = "\220",
	["\208\173"] = "\221",
	["\208\174"] = "\222",
	["\208\175"] = "\223",
	["\208\176"] = "\224",
	["\208\177"] = "\225",
	["\208\178"] = "\226",
	["\208\179"] = "\227",
	["\208\180"] = "\228",
	["\208\181"] = "\229",
	["\208\182"] = "\230",
	["\208\183"] = "\231",
	["\208\184"] = "\232",
	["\208\185"] = "\233",
	["\208\186"] = "\234",
	["\208\187"] = "\235",
	["\208\188"] = "\236",
	["\208\189"] = "\237",
	["\208\190"] = "\238",
	["\208\191"] = "\239",
	["\209\128"] = "\240",
	["\209\129"] = "\241",
	["\209\130"] = "\242",
	["\209\131"] = "\243",
	["\209\132"] = "\244",
	["\209\133"] = "\245",
	["\209\134"] = "\246",
	["\209\135"] = "\247",
	["\209\136"] = "\248",
	["\209\137"] = "\249",
	["\209\138"] = "\250",
	["\209\139"] = "\251",
	["\209\140"] = "\252",
	["\209\141"] = "\253",
	["\209\142"] = "\254",
	["\209\143"] = "\255"
}

local u8_cache = {}
local u8_cache_count = 0
local s_native_u8 = nil
local function is_valid_utf8_multibyte(s)
	local i, len = 1, #s
	local has_high = false
	while i <= len do
		local c = s:byte(i)
		if c <= 0x7F then
			i = i + 1
		elseif c >= 0xC2 and c <= 0xDF then
			if i + 1 > len then return false end
			local c2 = s:byte(i + 1)
			if c2 < 0x80 or c2 > 0xBF then return false end
			-- In CP1251, 0xC4..0xCF are Russian capital letters (Д..П)
			-- If followed by 0x80..0xBF (like ё=0xB8, Ё=0xA8), it is CP1251 Russian text, NOT UTF-8!
			if c >= 0xC4 and c <= 0xCF then return false end
			-- Вё and ВЁ in CP1251
			if c == 0xC2 and (c2 == 0xB8 or c2 == 0xA8) then return false end
			has_high = true
			i = i + 2
		elseif c >= 0xE0 and c <= 0xEF then
			if i + 2 > len then return false end
			local c2, c3 = s:byte(i + 1), s:byte(i + 2)
			if c2 < 0x80 or c2 > 0xBF or c3 < 0x80 or c3 > 0xBF then return false end
			has_high = true
			i = i + 3
		elseif c >= 0xF0 and c <= 0xF4 then
			if i + 3 > len then return false end
			local c2, c3, c4 = s:byte(i + 1), s:byte(i + 2), s:byte(i + 3)
			if c2 < 0x80 or c2 > 0xBF or c3 < 0x80 or c3 > 0xBF or c4 < 0x80 or c4 > 0xBF then return false end
			has_high = true
			i = i + 4
		else
			return false
		end
	end
	return has_high
end

local function cp1251_to_u8(s)
	if type(s) ~= "string" or s == "" then return s or "" end
	local cached = u8_cache[s]
	if cached then return cached end
	if not s:find("[\128-\255]") or is_valid_utf8_multibyte(s) then
		if u8_cache_count < 32768 then
			u8_cache[s] = s
			u8_cache_count = u8_cache_count + 1
		end
		return s
	end
	if s_native_u8 == nil then
		s_native_u8 = rawget(_G, "monet_cp1251_to_utf8") or rawget(_G, "ansiToUtf8") or false
	end
	local res
	if s_native_u8 then
		res = s_native_u8(s)
	else
		local len = #s
		local i = 1
		local parts = {}
		local part_start = 1
		while i <= len do
			local c = s:byte(i)
			if c <= 0x7F then
				i = i + 1
			elseif c >= 0xF0 and c <= 0xF4 and i + 3 <= len and s:byte(i + 1) >= 0x80 and s:byte(i + 1) <= 0xBF and s:byte(i + 2) >= 0x80 and s:byte(i + 2) <= 0xBF and s:byte(i + 3) >= 0x80 and s:byte(i + 3) <= 0xBF then
				-- 4-byte UTF-8 emoji
				i = i + 4
			elseif c >= 0xE0 and c <= 0xEF and i + 2 <= len and s:byte(i + 1) >= 0x80 and s:byte(i + 1) <= 0xBF and s:byte(i + 2) >= 0x80 and s:byte(i + 2) <= 0xBF then
				-- 3-byte UTF-8 (FontAwesome, symbols, CJK, etc.)
				i = i + 3
			elseif i + 1 <= len and (
				(c == 0xD0 and ((s:byte(i + 1) >= 0x90 and s:byte(i + 1) <= 0xBF) or s:byte(i + 1) == 0x81 or s:byte(i + 1) == 0x84 or s:byte(i + 1) == 0x86 or s:byte(i + 1) == 0x87 or s:byte(i + 1) == 0x8E))
				or (c == 0xD1 and ((s:byte(i + 1) >= 0x80 and s:byte(i + 1) <= 0x8F) or s:byte(i + 1) == 0x91 or s:byte(i + 1) == 0x94 or s:byte(i + 1) == 0x96 or s:byte(i + 1) == 0x97 or s:byte(i + 1) == 0x9E))
				or (c == 0xC2 and s:byte(i + 1) >= 0xA0 and s:byte(i + 1) <= 0xBF and s:byte(i + 1) ~= 0xB8 and s:byte(i + 1) ~= 0xA8)
				or (c == 0xC3 and s:byte(i + 1) >= 0x80 and s:byte(i + 1) <= 0xBF)
			) then
				-- 2-byte UTF-8 (Cyrillic / Latin-1)
				i = i + 2
			else
				if i > part_start then
					table.insert(parts, s:sub(part_start, i - 1))
				end
				table.insert(parts, cp1251_to_utf8[string.char(c)] or "?")
				i = i + 1
				part_start = i
			end
		end
		if part_start <= len then
			table.insert(parts, s:sub(part_start, len))
		end
		res = table.concat(parts)
	end
	if u8_cache_count < 32768 then
		u8_cache[s] = res
		u8_cache_count = u8_cache_count + 1
	end
	return res
end

local cp1251_cache = {}
local cp1251_cache_count = 0
local s_native_cp1251 = nil
local function u8_to_cp1251(s)
	if type(s) ~= "string" or s == "" then return s or "" end
	local cached = cp1251_cache[s]
	if cached then return cached end
	local res
	if not s:find("[\128-\255]") then
		res = s
	else
		if s_native_cp1251 == nil then
			s_native_cp1251 = rawget(_G, "monet_utf8_to_cp1251") or rawget(_G, "utf8ToAnsi") or false
		end
		if s_native_cp1251 then
			res = s_native_cp1251(s)
		else
			s = s:gsub("[\224-\239][\128-\191][\128-\191]", utf8_to_cp1251)
			res = (s:gsub("[\192-\223][\128-\191]", utf8_to_cp1251))
		end
	end
	if cp1251_cache_count < 16384 then
		cp1251_cache[s] = res
		cp1251_cache_count = cp1251_cache_count + 1
	end
	return res
end

local encoding = {
	default = "CP1251"
}

local aliases = {
	UTF7 = "UTF-7",
	UTF8 = "UTF-8",
	UTF16 = "UTF-16",
	UTF32 = "UTF-32",
	CP1251 = "CP1251",
	WINDOWS1251 = "CP1251",
	["WINDOWS-1251"] = "CP1251",
	["WIN-1251"] = "CP1251",
	["1251"] = "CP1251",
	["CP-1251"] = "CP1251",
}

local function normalize_encoding_name(e)
	if not e then return nil end
	local direct = aliases[e]
	if direct then return direct end
	local upper = string.upper(e)
	direct = aliases[upper]
	if direct then return direct end
	e = string.upper(string.gsub(e, "_", "-"))
	if aliases[e] then return aliases[e] end
	return e
end

local iconv = nil
local function get_iconv()
	if not iconv then
		local ok, mod = pcall(require, "iconv")
		if ok then iconv = mod end
	end
	return iconv
end

local converter = {}
function converter.new(enc)
	local private = {
		encoder = {},
		decoder = {},
	}

	local public = {
		encoding = enc
	}

	function public:encode(str, enc)
		if str == nil then return "" end
		if type(str) ~= "string" then str = tostring(str) end
		if str == "" then return "" end
		if not enc and self.encoding == "UTF-8" and encoding.default == "CP1251" then
			return cp1251_to_u8(str)
		end
		if not enc and self.encoding == "CP1251" and encoding.default == "CP1251" then
			return str
		end
		if enc then enc = normalize_encoding_name(enc)
		else enc = normalize_encoding_name(encoding.default)
		end
		if not enc or enc == self.encoding then return str end
		if self.encoding == "UTF-8" and enc == "CP1251" then
			return cp1251_to_u8(str)
		elseif self.encoding == "CP1251" and enc == "UTF-8" then
			return u8_to_cp1251(str)
		end
		local ic = get_iconv()
		if not ic then return str end
		local ok, res = pcall(function()
			local cd = private.encoder[enc]
			if not cd then
				cd = ic.new(self.encoding .. "//IGNORE", enc)
				if not cd then return str end
				private.encoder[enc] = cd
			end
			return cd:iconv(str)
		end)
		if ok and res then return res end
		return str
	end

	function public:decode(str, enc)
		if str == nil then return "" end
		if type(str) ~= "string" then str = tostring(str) end
		if str == "" then return "" end
		if not enc and self.encoding == "UTF-8" and encoding.default == "CP1251" then
			return u8_to_cp1251(str)
		end
		if not enc and self.encoding == "CP1251" and encoding.default == "CP1251" then
			return str
		end
		if enc then enc = normalize_encoding_name(enc)
		else enc = normalize_encoding_name(encoding.default)
		end
		if not enc or enc == self.encoding then return str end
		if self.encoding == "UTF-8" and enc == "CP1251" then
			return u8_to_cp1251(str)
		elseif self.encoding == "CP1251" and enc == "UTF-8" then
			return cp1251_to_u8(str)
		end
		local ic = get_iconv()
		if not ic then return str end
		local ok, res = pcall(function()
			local cd = private.decoder[enc]
			if not cd then
				cd = ic.new(enc .. "//IGNORE", self.encoding)
				if not cd then return str end
				private.decoder[enc] = cd
			end
			return cd:iconv(str)
		end)
		if ok and res then return res end
		return str
	end

	local mt = {}
	if enc == "UTF-8" then
		function mt:__call(str, enc_override)
			if not enc_override and encoding.default == "CP1251" then
				if str == nil then return "" end
				if type(str) ~= "string" then str = tostring(str) end
				return cp1251_to_u8(str)
			end
			return self:encode(str, enc_override)
		end
	elseif enc == "CP1251" then
		function mt:__call(str, enc_override)
			if not enc_override and encoding.default == "CP1251" then
				if str == nil then return "" end
				if type(str) ~= "string" then str = tostring(str) end
				return u8_to_cp1251(str)
			end
			return self:encode(str, enc_override)
		end
	else
		function mt:__call(str, enc_override)
			return self:encode(str, enc_override)
		end
	end

	setmetatable(public, mt)
	return public
end

setmetatable(encoding, {
	__index = function(table, index)
		assert(type(index) == "string")
		local enc = normalize_encoding_name(index)
		local already_loaded = rawget(table, enc)
		if already_loaded then
			table[index] = already_loaded
			return already_loaded
		else
			local conv = converter.new(enc)
			table[index] = conv
			table[enc] = conv
			return conv
		end
	end
})

return encoding

)NEOMOD_DELIM"},
    {"inicfg", R"NEOMOD_DELIM(
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

)NEOMOD_DELIM"},
    {"md5", R"NEOMOD_DELIM(
local md5 = {
  _VERSION     = "md5.lua 1.1.0",
  _DESCRIPTION = "MD5 computation in Lua (5.1-3, LuaJIT)",
  _URL         = "https://github.com/kikito/md5.lua",
  _LICENSE     = [[
    MIT LICENSE

    Copyright (c) 2013 Enrique García Cota + Adam Baldwin + hanzao + Equi 4 Software

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the
    "Software"), to deal in the Software without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, sublicense, and/or sell copies of the Software, and to
    permit persons to whom the Software is furnished to do so, subject to
    the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  ]]
}

local char, byte, format, rep, sub =
  string.char, string.byte, string.format, string.rep, string.sub
local bit_or, bit_and, bit_not, bit_xor, bit_rshift, bit_lshift

local ok, bit = pcall(require, 'bit')
local ok_ffi, ffi = pcall(require, 'ffi')
if ok then
  bit_or, bit_and, bit_not, bit_xor, bit_rshift, bit_lshift = bit.bor, bit.band, bit.bnot, bit.bxor, bit.rshift, bit.lshift
else
  ok, bit = pcall(require, 'bit32')

  if ok then

    bit_not = bit.bnot

    local tobit = function(n)
      return n <= 0x7fffffff and n or -(bit_not(n) + 1)
    end

    local normalize = function(f)
      return function(a,b) return tobit(f(tobit(a), tobit(b))) end
    end

    bit_or, bit_and, bit_xor = normalize(bit.bor), normalize(bit.band), normalize(bit.bxor)
    bit_rshift, bit_lshift = normalize(bit.rshift), normalize(bit.lshift)

  else

    local function tbl2number(tbl)
      local result = 0
      local power = 1
      for i = 1, #tbl do
        result = result + tbl[i] * power
        power = power * 2
      end
      return result
    end

    local function expand(t1, t2)
      local big, small = t1, t2
      if(#big < #small) then
        big, small = small, big
      end

      for i = #small + 1, #big do
        small[i] = 0
      end
    end

    local to_bits

    bit_not = function(n)
      local tbl = to_bits(n)
      local size = math.max(#tbl, 32)
      for i = 1, size do
        if(tbl[i] == 1) then
          tbl[i] = 0
        else
          tbl[i] = 1
        end
      end
      return tbl2number(tbl)
    end

    to_bits = function (n)
      if(n < 0) then

        return to_bits(bit_not(math.abs(n)) + 1)
      end

      local tbl = {}
      local cnt = 1
      local last
      while n > 0 do
        last      = n % 2
        tbl[cnt]  = last
        n         = (n-last)/2
        cnt       = cnt + 1
      end

      return tbl
    end

    bit_or = function(m, n)
      local tbl_m = to_bits(m)
      local tbl_n = to_bits(n)
      expand(tbl_m, tbl_n)

      local tbl = {}
      for i = 1, #tbl_m do
        if(tbl_m[i]== 0 and tbl_n[i] == 0) then
          tbl[i] = 0
        else
          tbl[i] = 1
        end
      end

      return tbl2number(tbl)
    end

    bit_and = function(m, n)
      local tbl_m = to_bits(m)
      local tbl_n = to_bits(n)
      expand(tbl_m, tbl_n)

      local tbl = {}
      for i = 1, #tbl_m do
        if(tbl_m[i]== 0 or tbl_n[i] == 0) then
          tbl[i] = 0
        else
          tbl[i] = 1
        end
      end

      return tbl2number(tbl)
    end

    bit_xor = function(m, n)
      local tbl_m = to_bits(m)
      local tbl_n = to_bits(n)
      expand(tbl_m, tbl_n)

      local tbl = {}
      for i = 1, #tbl_m do
        if(tbl_m[i] ~= tbl_n[i]) then
          tbl[i] = 1
        else
          tbl[i] = 0
        end
      end

      return tbl2number(tbl)
    end

    bit_rshift = function(n, bits)
      local high_bit = 0
      if(n < 0) then

        n = bit_not(math.abs(n)) + 1
        high_bit = 0x80000000
      end

      local floor = math.floor

      for i=1, bits do
        n = n/2
        n = bit_or(floor(n), high_bit)
      end
      return floor(n)
    end

    bit_lshift = function(n, bits)
      if(n < 0) then

        n = bit_not(math.abs(n)) + 1
      end

      for i=1, bits do
        n = n*2
      end
      return bit_and(n, 0xFFFFFFFF)
    end
  end
end

local lei2str

if ok_ffi then
  local ct_IntType = ffi.typeof("int[1]")
  lei2str = function(i) return ffi.string(ct_IntType(i), 4) end
else
  lei2str = function (i)
    local f=function (s) return char( bit_and( bit_rshift(i, s), 255)) end
    return f(0)..f(8)..f(16)..f(24)
  end
end

local function str2bei(s)
  local v=0
  for i=1, #s do
    v = v * 256 + byte(s, i)
  end
  return v
end

local str2lei

if ok_ffi then
  local ct_constcharptr = ffi.typeof("const char*")
  local ct_constintptr = ffi.typeof("const int*")
  str2lei = function(s)
    local int = ct_constcharptr(s)
    return ffi.cast(ct_constintptr, int)[0]
  end
else
  str2lei = function(s)
    local v=0
    for i = #s,1,-1 do
      v = v*256 + byte(s, i)
    end
    return v
    end
end

local function cut_le_str(s)
  return {
    str2lei(sub(s, 1, 4)),
    str2lei(sub(s, 5, 8)),
    str2lei(sub(s, 9, 12)),
    str2lei(sub(s, 13, 16)),
    str2lei(sub(s, 17, 20)),
    str2lei(sub(s, 21, 24)),
    str2lei(sub(s, 25, 28)),
    str2lei(sub(s, 29, 32)),
    str2lei(sub(s, 33, 36)),
    str2lei(sub(s, 37, 40)),
    str2lei(sub(s, 41, 44)),
    str2lei(sub(s, 45, 48)),
    str2lei(sub(s, 49, 52)),
    str2lei(sub(s, 53, 56)),
    str2lei(sub(s, 57, 60)),
    str2lei(sub(s, 61, 64)),
  }
end

local CONSTS = {
  0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
  0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
  0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
  0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
  0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
  0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
  0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
  0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
  0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
  0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
  0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
  0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
  0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
  0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
  0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
  0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
  0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476
}

local f=function (x,y,z) return bit_or(bit_and(x,y),bit_and(-x-1,z)) end
local g=function (x,y,z) return bit_or(bit_and(x,z),bit_and(y,-z-1)) end
local h=function (x,y,z) return bit_xor(x,bit_xor(y,z)) end
local i=function (x,y,z) return bit_xor(y,bit_or(x,-z-1)) end
local z=function (ff,a,b,c,d,x,s,ac)
  a=bit_and(a+ff(b,c,d)+x+ac,0xFFFFFFFF)

  return bit_or(bit_lshift(bit_and(a,bit_rshift(0xFFFFFFFF,s)),s),bit_rshift(a,32-s))+b
end

local function transform(A,B,C,D,X)
  local a,b,c,d=A,B,C,D
  local t=CONSTS

  a=z(f,a,b,c,d,X[ 0], 7,t[ 1])
  d=z(f,d,a,b,c,X[ 1],12,t[ 2])
  c=z(f,c,d,a,b,X[ 2],17,t[ 3])
  b=z(f,b,c,d,a,X[ 3],22,t[ 4])
  a=z(f,a,b,c,d,X[ 4], 7,t[ 5])
  d=z(f,d,a,b,c,X[ 5],12,t[ 6])
  c=z(f,c,d,a,b,X[ 6],17,t[ 7])
  b=z(f,b,c,d,a,X[ 7],22,t[ 8])
  a=z(f,a,b,c,d,X[ 8], 7,t[ 9])
  d=z(f,d,a,b,c,X[ 9],12,t[10])
  c=z(f,c,d,a,b,X[10],17,t[11])
  b=z(f,b,c,d,a,X[11],22,t[12])
  a=z(f,a,b,c,d,X[12], 7,t[13])
  d=z(f,d,a,b,c,X[13],12,t[14])
  c=z(f,c,d,a,b,X[14],17,t[15])
  b=z(f,b,c,d,a,X[15],22,t[16])

  a=z(g,a,b,c,d,X[ 1], 5,t[17])
  d=z(g,d,a,b,c,X[ 6], 9,t[18])
  c=z(g,c,d,a,b,X[11],14,t[19])
  b=z(g,b,c,d,a,X[ 0],20,t[20])
  a=z(g,a,b,c,d,X[ 5], 5,t[21])
  d=z(g,d,a,b,c,X[10], 9,t[22])
  c=z(g,c,d,a,b,X[15],14,t[23])
  b=z(g,b,c,d,a,X[ 4],20,t[24])
  a=z(g,a,b,c,d,X[ 9], 5,t[25])
  d=z(g,d,a,b,c,X[14], 9,t[26])
  c=z(g,c,d,a,b,X[ 3],14,t[27])
  b=z(g,b,c,d,a,X[ 8],20,t[28])
  a=z(g,a,b,c,d,X[13], 5,t[29])
  d=z(g,d,a,b,c,X[ 2], 9,t[30])
  c=z(g,c,d,a,b,X[ 7],14,t[31])
  b=z(g,b,c,d,a,X[12],20,t[32])

  a=z(h,a,b,c,d,X[ 5], 4,t[33])
  d=z(h,d,a,b,c,X[ 8],11,t[34])
  c=z(h,c,d,a,b,X[11],16,t[35])
  b=z(h,b,c,d,a,X[14],23,t[36])
  a=z(h,a,b,c,d,X[ 1], 4,t[37])
  d=z(h,d,a,b,c,X[ 4],11,t[38])
  c=z(h,c,d,a,b,X[ 7],16,t[39])
  b=z(h,b,c,d,a,X[10],23,t[40])
  a=z(h,a,b,c,d,X[13], 4,t[41])
  d=z(h,d,a,b,c,X[ 0],11,t[42])
  c=z(h,c,d,a,b,X[ 3],16,t[43])
  b=z(h,b,c,d,a,X[ 6],23,t[44])
  a=z(h,a,b,c,d,X[ 9], 4,t[45])
  d=z(h,d,a,b,c,X[12],11,t[46])
  c=z(h,c,d,a,b,X[15],16,t[47])
  b=z(h,b,c,d,a,X[ 2],23,t[48])

  a=z(i,a,b,c,d,X[ 0], 6,t[49])
  d=z(i,d,a,b,c,X[ 7],10,t[50])
  c=z(i,c,d,a,b,X[14],15,t[51])
  b=z(i,b,c,d,a,X[ 5],21,t[52])
  a=z(i,a,b,c,d,X[12], 6,t[53])
  d=z(i,d,a,b,c,X[ 3],10,t[54])
  c=z(i,c,d,a,b,X[10],15,t[55])
  b=z(i,b,c,d,a,X[ 1],21,t[56])
  a=z(i,a,b,c,d,X[ 8], 6,t[57])
  d=z(i,d,a,b,c,X[15],10,t[58])
  c=z(i,c,d,a,b,X[ 6],15,t[59])
  b=z(i,b,c,d,a,X[13],21,t[60])
  a=z(i,a,b,c,d,X[ 4], 6,t[61])
  d=z(i,d,a,b,c,X[11],10,t[62])
  c=z(i,c,d,a,b,X[ 2],15,t[63])
  b=z(i,b,c,d,a,X[ 9],21,t[64])

  return bit_and(A+a,0xFFFFFFFF),bit_and(B+b,0xFFFFFFFF),
         bit_and(C+c,0xFFFFFFFF),bit_and(D+d,0xFFFFFFFF)
end

local function md5_update(self, s)
  self.pos = self.pos + #s
  s = self.buf .. s
  for ii = 1, #s - 63, 64 do
    local X = cut_le_str(sub(s,ii,ii+63))
    assert(#X == 16)
    X[0] = table.remove(X,1)
    self.a,self.b,self.c,self.d = transform(self.a,self.b,self.c,self.d,X)
  end
  self.buf = sub(s, math.floor(#s/64)*64 + 1, #s)
  return self
end

local function md5_finish(self)
  local msgLen = self.pos
  local padLen = 56 - msgLen % 64

  if msgLen % 64 > 56 then padLen = padLen + 64 end

  if padLen == 0 then padLen = 64 end

  local s = char(128) .. rep(char(0),padLen-1) .. lei2str(bit_and(8*msgLen, 0xFFFFFFFF)) .. lei2str(math.floor(msgLen/0x20000000))
  md5_update(self, s)

  assert(self.pos % 64 == 0)
  return lei2str(self.a) .. lei2str(self.b) .. lei2str(self.c) .. lei2str(self.d)
end

function md5.new()
  return { a = CONSTS[65], b = CONSTS[66], c = CONSTS[67], d = CONSTS[68],
           pos = 0,
           buf = '',
           update = md5_update,
           finish = md5_finish }
end

function md5.tohex(s)
  return format("%08x%08x%08x%08x", str2bei(sub(s, 1, 4)), str2bei(sub(s, 5, 8)), str2bei(sub(s, 9, 12)), str2bei(sub(s, 13, 16)))
end

function md5.sum(s)
  return md5.new():update(s):finish()
end

function md5.sumhexa(s)
  return md5.tohex(md5.sum(s))
end

return md5

)NEOMOD_DELIM"},
    {"basexx", R"NEOMOD_DELIM(
local function divide_string( str, max )
  local result = {}

  local start = 1
  for i = 1, #str do
     if i % max == 0 then
        table.insert( result, str:sub( start, i ) )
        start = i + 1
     elseif i == #str then
        table.insert( result, str:sub( start, i ) )
     end
  end

  return result
end

local function number_to_bit( num, length )
  local bits = {}

  while num > 0 do
     local rest = math.floor( math.fmod( num, 2 ) )
     table.insert( bits, rest )
     num = ( num - rest ) / 2
  end

  while #bits < length do
     table.insert( bits, "0" )
  end

  return string.reverse( table.concat( bits ) )
end

local function ignore_set( str, set )
  if set then
     str = str:gsub( "["..set.."]", "" )
  end
  return str
end

local function pure_from_bit( str )
  return ( str:gsub( '........', function ( cc )
              return string.char( tonumber( cc, 2 ) )
           end ) )
end

local function unexpected_char_error( str, pos )
  local c = string.sub( str, pos, pos )
  return string.format( "unexpected character at position %d: '%s'", pos, c )
end

local basexx = {}

local bitMap = { o = "0", i = "1", l = "1" }

function basexx.from_bit( str, ignore )
  str = ignore_set( str, ignore )
  str = string.lower( str )
  str = str:gsub( '[ilo]', function( c ) return bitMap[ c ] end )
  local pos = string.find( str, "[^01]" )
  if pos then return nil, unexpected_char_error( str, pos ) end

  return pure_from_bit( str )
end

function basexx.to_bit( str )
  return ( str:gsub( '.', function ( c )
              local byte = string.byte( c )
              local bits = {}
              for _ = 1,8 do
                 table.insert( bits, byte % 2 )
                 byte = math.floor( byte / 2 )
              end
              return table.concat( bits ):reverse()
           end ) )
end

function basexx.from_hex( str, ignore )
  str = ignore_set( str, ignore )
  local pos = string.find( str, "[^%x]" )
  if pos then return nil, unexpected_char_error( str, pos ) end

  return ( str:gsub( '..', function ( cc )
              return string.char( tonumber( cc, 16 ) )
           end ) )
end

function basexx.to_hex( str )
  return ( str:gsub( '.', function ( c )
              return string.format('%02X', string.byte( c ) )
           end ) )
end

local function from_basexx( str, alphabet, bits )
  local result = {}
  for i = 1, #str do
     local c = string.sub( str, i, i )
     if c ~= '=' then
        local index = string.find( alphabet, c, 1, true )
        if not index then
           return nil, unexpected_char_error( str, i )
        end
        table.insert( result, number_to_bit( index - 1, bits ) )
     end
  end

  local value = table.concat( result )
  local pad = #value % 8
  return pure_from_bit( string.sub( value, 1, #value - pad ) )
end

local function to_basexx( str, alphabet, bits, pad )
  local bitString = basexx.to_bit( str )

  local chunks = divide_string( bitString, bits )
  local result = {}
  for _,value in ipairs( chunks ) do
     if ( #value < bits ) then
        value = value .. string.rep( '0', bits - #value )
     end
     local pos = tonumber( value, 2 ) + 1
     table.insert( result, alphabet:sub( pos, pos ) )
  end

  table.insert( result, pad )
  return table.concat( result )
end

local base32Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"
local base32PadMap = { "", "======", "====", "===", "=" }

function basexx.from_base32( str, ignore )
  str = ignore_set( str, ignore )
  return from_basexx( string.upper( str ), base32Alphabet, 5 )
end

function basexx.to_base32( str )
  return to_basexx( str, base32Alphabet, 5, base32PadMap[ #str % 5 + 1 ] )
end

local crockfordAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
local crockfordMap = { O = "0", I = "1", L = "1" }

function basexx.from_crockford( str, ignore )
  str = ignore_set( str, ignore )
  str = string.upper( str )
  str = str:gsub( '[ILOU]', function( c ) return crockfordMap[ c ] end )
  return from_basexx( str, crockfordAlphabet, 5 )
end

function basexx.to_crockford( str )
  return to_basexx( str, crockfordAlphabet, 5, "" )
end

local base64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"..
                      "abcdefghijklmnopqrstuvwxyz"..
                      "0123456789+/"
local base64PadMap = { "", "==", "=" }

function basexx.from_base64( str, ignore )
  str = ignore_set( str, ignore )
  return from_basexx( str, base64Alphabet, 6 )
end

function basexx.to_base64( str )
  return to_basexx( str, base64Alphabet, 6, base64PadMap[ #str % 3 + 1 ] )
end

local url64Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"..
                     "abcdefghijklmnopqrstuvwxyz"..
                     "0123456789-_"

function basexx.from_url64( str, ignore )
  str = ignore_set( str, ignore )
  return from_basexx( str, url64Alphabet, 6 )
end

function basexx.to_url64( str )
  return to_basexx( str, url64Alphabet, 6, "" )
end

local function length_error( len, d )
  return string.format( "invalid length: %d - must be a multiple of %d", len, d )
end

local z85Decoder = { 0x00, 0x44, 0x00, 0x54, 0x53, 0x52, 0x48, 0x00,
                    0x4B, 0x4C, 0x46, 0x41, 0x00, 0x3F, 0x3E, 0x45,
                    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x40, 0x00, 0x49, 0x42, 0x4A, 0x47,
                    0x51, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
                    0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
                    0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
                    0x3B, 0x3C, 0x3D, 0x4D, 0x00, 0x4E, 0x43, 0x00,
                    0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
                    0x21, 0x22, 0x23, 0x4F, 0x00, 0x50, 0x00, 0x00 }

function basexx.from_z85( str, ignore )
  str = ignore_set( str, ignore )
  if ( #str % 5 ) ~= 0 then
     return nil, length_error( #str, 5 )
  end

  local result = {}

  local value = 0
  for i = 1, #str do
     local index = string.byte( str, i ) - 31
     if index < 1 or index >= #z85Decoder then
        return nil, unexpected_char_error( str, i )
     end
     value = ( value * 85 ) + z85Decoder[ index ]
     if ( i % 5 ) == 0 then
        local divisor = 256 * 256 * 256
        while divisor ~= 0 do
           local b = math.floor( value / divisor ) % 256
           table.insert( result, string.char( b ) )
           divisor = math.floor( divisor / 256 )
        end
        value = 0
     end
  end

  return table.concat( result )
end

local z85Encoder = "0123456789"..
                  "abcdefghijklmnopqrstuvwxyz"..
                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"..
                  ".-:+=^!/*?&<>()[]{}@%$#"

function basexx.to_z85( str )
  if ( #str % 4 ) ~= 0 then
     return nil, length_error( #str, 4 )
  end

  local result = {}

  local value = 0
  for i = 1, #str do
     local b = string.byte( str, i )
     value = ( value * 256 ) + b
     if ( i % 4 ) == 0 then
        local divisor = 85 * 85 * 85 * 85
        while divisor ~= 0 do
           local index = ( math.floor( value / divisor ) % 85 ) + 1
           table.insert( result, z85Encoder:sub( index, index ) )
           divisor = math.floor( divisor / 85 )
        end
        value = 0
     end
  end

  return table.concat( result )
end

return basexx

)NEOMOD_DELIM"},
    {"bitex", R"NEOMOD_DELIM(
local bit = require "bit"
local int32max = 0xFFFFFFFF
local bitex = {}

function bitex.bextract(v, off, bits)
	return bit.band(bit.band(bit.rshift(v, off), bit.rshift(int32max, 32 - bits)), int32max)
end

function bitex.breplace(v, n, off, bits)
	n = bitex.bextract(n, 0, bits)
	return bit.bor(bit.band(v, bit.bnot(bit.lshift(bit.rshift(int32max, 32 - bits), off))), bit.lshift(n, off))
end

return bitex

)NEOMOD_DELIM"},
    {"vkeys", R"NEOMOD_DELIM(
local vkeys = {
	VK_LBUTTON = 0x01,
	VK_RBUTTON = 0x02,
	VK_CANCEL = 0x03,
	VK_MBUTTON = 0x04,
	VK_XBUTTON1 = 0x05,
	VK_XBUTTON2 = 0x06,
	VK_BACK = 0x08,
	VK_TAB = 0x09,
	VK_CLEAR = 0x0C,
	VK_RETURN = 0x0D,
	VK_SHIFT = 0x10,
	VK_CONTROL = 0x11,
	VK_MENU = 0x12,
	VK_PAUSE = 0x13,
	VK_CAPITAL = 0x14,
	VK_ESCAPE = 0x1B,
	VK_CONVERT = 0x1C,
	VK_NONCONVERT = 0x1D,
	VK_ACCEPT = 0x1E,
	VK_MODECHANGE = 0x1F,
	VK_SPACE = 0x20,
	VK_PRIOR = 0x21,
	VK_NEXT = 0x22,
	VK_END = 0x23,
	VK_HOME = 0x24,
	VK_LEFT = 0x25,
	VK_UP = 0x26,
	VK_RIGHT = 0x27,
	VK_DOWN = 0x28,
	VK_SELECT = 0x29,
	VK_PRINT = 0x2A,
	VK_EXECUTE = 0x2B,
	VK_SNAPSHOT = 0x2C,
	VK_INSERT = 0x2D,
	VK_DELETE = 0x2E,
	VK_HELP = 0x2F,

	VK_0 = 0x30,
	VK_1 = 0x31,
	VK_2 = 0x32,
	VK_3 = 0x33,
	VK_4 = 0x34,
	VK_5 = 0x35,
	VK_6 = 0x36,
	VK_7 = 0x37,
	VK_8 = 0x38,
	VK_9 = 0x39,

	VK_A = 0x41,
	VK_B = 0x42,
	VK_C = 0x43,
	VK_D = 0x44,
	VK_E = 0x45,
	VK_F = 0x46,
	VK_G = 0x47,
	VK_H = 0x48,
	VK_I = 0x49,
	VK_J = 0x4A,
	VK_K = 0x4B,
	VK_L = 0x4C,
	VK_M = 0x4D,
	VK_N = 0x4E,
	VK_O = 0x4F,
	VK_P = 0x50,
	VK_Q = 0x51,
	VK_R = 0x52,
	VK_S = 0x53,
	VK_T = 0x54,
	VK_U = 0x55,
	VK_V = 0x56,
	VK_W = 0x57,
	VK_X = 0x58,
	VK_Y = 0x59,
	VK_Z = 0x5A,

	VK_LWIN = 0x5B,
	VK_RWIN = 0x5C,
	VK_APPS = 0x5D,
	VK_SLEEP = 0x5F,

	VK_NUMPAD0 = 0x60,
	VK_NUMPAD1 = 0x61,
	VK_NUMPAD2 = 0x62,
	VK_NUMPAD3 = 0x63,
	VK_NUMPAD4 = 0x64,
	VK_NUMPAD5 = 0x65,
	VK_NUMPAD6 = 0x66,
	VK_NUMPAD7 = 0x67,
	VK_NUMPAD8 = 0x68,
	VK_NUMPAD9 = 0x69,
	VK_MULTIPLY = 0x6A,
	VK_ADD = 0x6B,
	VK_SEPARATOR = 0x6C,
	VK_SUBTRACT = 0x6D,
	VK_DECIMAL = 0x6E,
	VK_DIVIDE = 0x6F,

	VK_F1 = 0x70,
	VK_F2 = 0x71,
	VK_F3 = 0x72,
	VK_F4 = 0x73,
	VK_F5 = 0x74,
	VK_F6 = 0x75,
	VK_F7 = 0x76,
	VK_F8 = 0x77,
	VK_F9 = 0x78,
	VK_F10 = 0x79,
	VK_F11 = 0x7A,
	VK_F12 = 0x7B,
	VK_F13 = 0x7C,
	VK_F14 = 0x7D,
	VK_F15 = 0x7E,
	VK_F16 = 0x7F,
	VK_F17 = 0x80,
	VK_F18 = 0x81,
	VK_F19 = 0x82,
	VK_F20 = 0x83,
	VK_F21 = 0x84,
	VK_F22 = 0x85,
	VK_F23 = 0x86,
	VK_F24 = 0x87,

	VK_NUMLOCK = 0x90,
	VK_SCROLL = 0x91,
	VK_LSHIFT = 0xA0,
	VK_RSHIFT = 0xA1,
	VK_LCONTROL = 0xA2,
	VK_RCONTROL = 0xA3,
	VK_LMENU = 0xA4,
	VK_RMENU = 0xA5,

	VK_OEM_1 = 0xBA,
	VK_OEM_PLUS = 0xBB,
	VK_OEM_COMMA = 0xBC,
	VK_OEM_MINUS = 0xBD,
	VK_OEM_PERIOD = 0xBE,
	VK_OEM_2 = 0xBF,
	VK_OEM_3 = 0xC0,
	VK_OEM_4 = 0xDB,
	VK_OEM_5 = 0xDC,
	VK_OEM_6 = 0xDD,
	VK_OEM_7 = 0xDE,
	VK_OEM_8 = 0xDF
}

local id_to_name = {}
for k, v in pairs(vkeys) do
	id_to_name[v] = k
end

function vkeys.key_to_name(id)
	return id_to_name[id]
end

function vkeys.name_to_key(name)
	return vkeys[name]
end

return vkeys

)NEOMOD_DELIM"},
    {"timerwheel", R"NEOMOD_DELIM(
local default_now
if ngx then
  default_now = ngx.now
else
  local ok, socket = pcall(require, "socket")
  if ok then
    default_now = socket.gettime
  else
    default_now = nil
  end
end

local ok, new_tab = pcall(require, "table.new")
if not ok then
  new_tab = function(narr, nrec) return {} end
end

local default_err_handler = function(err)
  io.stderr:write(debug.traceback("TimerWheel callback failed with: " .. tostring(err)).."\n")
end

local math_floor = math.floor
local math_huge = math.huge
local EMPTY = {}

local _M = {}

function _M.new(opts)
  assert(opts ~= _M, "new should not be called with colon ':' notation")

  opts = opts or EMPTY
  assert(type(opts) == "table", "expected options to be a table")

  local precision = opts.precision or 0.050
  local ringsize  = opts.ringsize or 72000
  local now       = opts.now or default_now
  local err_handler = opts.err_handler or default_err_handler
  opts = nil

  assert(type(precision) == "number" and precision > 0,
    "expected 'precision' to be number > 0")
  assert(type(ringsize) == "number" and ringsize > 0 and math_floor(ringsize) == ringsize,
    "expected 'ringsize' to be an integer number > 0")
  assert(type(now) == "function",
    "expected 'now' to be a function, got: " .. type(now))
  assert(type(err_handler) == "function",
    "expected 'err_handler' to be a function, got: " .. type(err_handler))

  local start     = now()
  local position  = 1
  local id_count  = 0
  local id_list   = {}
  local rings     = {}
  local rings_n   = 0
  local count     = 0
  local wheel     = {}

  local tables    = {}
  local tables_n  = 0

  function wheel:step()
    local new_position = math_floor((now() - start) / precision) + 1
    local ring = rings[1] or EMPTY

    while position < new_position do

      local slot = ring[position]
      ring[position] = nil

      position = position + 1
      if position > ringsize then

        for i = 1, rings_n do

          rings[i] = rings[i + 1]
        end
        rings_n = rings_n - 1

        ring = rings[1] or EMPTY
        start = start + ringsize * precision
        position = 1
        new_position = new_position - ringsize
      end

      if slot then

        local ids = slot.ids
        local args = slot.arg
        for i = 1, slot.n do
          local id  = slot[i];  slot[i]  = nil; slot[id] = nil
          local cb  = ids[id];  ids[id]  = nil
          local arg = args[id]; args[id] = nil
          id_list[id] = nil
          count = count - 1
          xpcall(cb, err_handler, arg)
        end

        slot.n = 0

        tables_n = tables_n + 1
        tables[tables_n] = slot
      end

    end
    return true
  end

  function wheel:count()
    return count
  end

  function wheel:set(expire_in, cb, arg)
    local time_expire = now() + expire_in
    local pos = math_floor((time_expire - start) / precision) + 1
    if pos < position then

      pos = position
    end
    local ring_idx = math_floor((pos - 1) / ringsize) + 1
    local slot_idx = pos - (ring_idx - 1) * ringsize

    local ring = rings[ring_idx]
    if not ring then
      ring = new_tab(ringsize, 0)
      rings[ring_idx] = ring
      if ring_idx > rings_n then
        rings_n = ring_idx
      end
    end

    local slot = ring[slot_idx]
    if not slot then
      if tables_n == 0 then
        slot = { n = 0, ids = {}, arg = {} }
      else
        slot = tables[tables_n]
        tables_n = tables_n - 1
      end
      ring[slot_idx] = slot
    end

    local id = id_count - 1
    id_count = id

    slot.ids[id] = cb or error("the callback parameter is required", 2)
    slot.arg[id] = arg
    local idx = slot.n + 1
    slot.n = idx
    slot[idx] = id
    slot[id] = idx
    id_list[id] = slot
    count = count + 1

    return id
  end

  function wheel:cancel(id)
    local slot = id_list[id]
    if slot then
      local idx = slot[id]
      slot[id] = nil
      slot.ids[id] = nil
      slot.arg[id] = nil
      local n = slot.n
      if idx ~= n then
        local moved_id = slot[n]
        slot[idx] = moved_id
        slot[moved_id] = idx
      end
      slot[n] = nil
      slot.n = n - 1
      id_list[id] = nil
      count = count - 1
      return true
    end
    return false
  end

  function wheel:peek(max_ahead)
    if count == 0 then
      return nil
    end
    local time_now = now()

    if max_ahead then
      max_ahead = math_floor((time_now + max_ahead - start) / precision)
    else
      max_ahead = math_huge
    end

    local position_idx = position
    local ring_idx = 1
    local ring = rings[ring_idx] or EMPTY
    local ahead_count = 0
    while ahead_count < max_ahead do

      local slot = ring[position_idx]
      if slot then
        if slot[1] then

          return ((ring_idx - 1) * ringsize + position_idx) * precision +
                 start - time_now
        end
      end

      position_idx = position_idx + 1
      ahead_count = ahead_count + 1
      if position_idx > ringsize then
        position_idx = 1
        ring_idx = ring_idx + 1
        ring = rings[ring_idx] or EMPTY
      end
    end

    return nil
  end

  if _G._TEST then

    wheel._tables = tables
  end

  return wheel
end

return _M

)NEOMOD_DELIM"},
    {"jsoncfg", R"NEOMOD_DELIM(
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

)NEOMOD_DELIM"},
};

inline bool neo_module_source(const std::string& name, std::string& out) {
    for (const auto& entry : NEO_MODULES) {
        if (name == entry.name) { out = entry.source; return true; }
    }
    return false;
}
