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
};

inline bool neo_module_source(const std::string& name, std::string& out) {
    for (const auto& entry : NEO_MODULES) {
        if (name == entry.name) { out = entry.source; return true; }
    }
    return false;
}
