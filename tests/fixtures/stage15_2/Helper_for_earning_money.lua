require "lib.moonloader"
local events = require("lib.samp.events")

local stats = { taxi = 0, trucker = 0, collector = 0 }

-- Трекер движения
local lastX, lastY, lastZ = nil, nil, nil
local movedDist = 0.0
local MOVE_THRESHOLD = 50.0

-- Форматирование денег с точками: 1250000 -> "1.250.000"
local function formatMoney(n)
    local s = tostring(math.floor(n))
    local sign = ""
    if s:sub(1, 1) == "-" then
        sign = "-"
        s = s:sub(2)
    end
    local out = s:reverse():gsub("(%d%d%d)", "%1."):reverse()
    -- убираем точку в начале, если число кратно 3 разрядам
    if out:sub(1, 1) == "." then out = out:sub(2) end
    return sign .. out
end

-- Извлечь максимальное число из строки
local function extractMoney(text)
    local max, cur, hasDigits = 0, 0, false
    for i = 1, #text do
        local b = string.byte(text, i)
        if b and b >= 48 and b <= 57 then
            cur = cur * 10 + (b - 48)
            hasDigits = true
        elseif b == 46 then
            -- точка, пропускаем
        else
            if hasDigits and cur > max then max = cur end
            cur, hasDigits = 0, false
        end
    end
    if hasDigits and cur > max then max = cur end
    if max > 0 then return max end
    return nil
end

-- Определение работы
local function detectJob(text)
    if string.find(text, "\210\224\234\241\232") then return "taxi" end
    if string.find(text, "\195\240\243\231") then return "trucker" end
    if string.find(text, "\200\237\234\224\241\241\224\242\238\240") then return "collector" end
    return nil
end

-- Точные фразы выплат
local function isPayout(text)
    if string.find(text, "\202\235\232\229\237\242 \238\239\235\224\242\232\235") then return true end
    if string.find(text, "\231\224\240\239\235\224\242\224 \231\224 \240\229\233\241") then return true end
    if string.find(text, "\207\238\235\243\247\229\237\238 \231\224 \238\225\241\235\243\230\232\226\224\237\232\229") then return true end
    return false
end

function events.onServerMessage(color, text)
    if not isPayout(text) then return end
    local amount = extractMoney(text)
    if not amount then return end
    local job = detectJob(text)
    if not job then return end
    if movedDist < MOVE_THRESHOLD then return end

    stats[job] = stats[job] + amount
    movedDist = 0.0

    sampAddChatMessage("[Zarabotok] " .. job .. " +" .. formatMoney(amount) ..
        " | Total: " .. formatMoney(stats[job]), 0x00FF00)
end

function cmdZarabotok()
    local txt =
        "\210\224\234\241\232: " .. formatMoney(stats.taxi) .. "\n" ..
        "\196\224\235\252\237\238\225\238\233\249\232\234: " .. formatMoney(stats.trucker) .. "\n" ..
        "\200\237\234\224\241\241\224\242\238\240: " .. formatMoney(stats.collector) ..
        "\n\n\196\235\237 \241\225\240\238\241\224: /resetstat"
    sampShowDialog(8888,
        "\199\224\240\224\225\238\242\238\234 \237\224 \240\224\225\238\242\224\245",
        txt,
        "\206\202",
        "\199\224\236\240\251\242\252",
        0)
end

function cmdResetStat()
    stats.taxi, stats.trucker, stats.collector = 0, 0, 0
    sampAddChatMessage("[Zarabotok] \209\242\224\242\232\241\242\232\234\224 \241\225\240\238\248\229\237\224", 0x00FF00)
end

function main()
    while not isSampAvailable() do wait(100) end
    sampRegisterChatCommand("zarabotok", cmdZarabotok)
    sampRegisterChatCommand("resetstat", cmdResetStat)
    sampAddChatMessage("[Zarabotok] Loaded. /zarabotok | /resetstat", -1)

    while true do
        wait(500)
        local x, y, z = getCharCoordinates(PLAYER_PED)
        if lastX then
            local dx, dy, dz = x - lastX, y - lastY, z - lastZ
            movedDist = movedDist + math.sqrt(dx*dx + dy*dy + dz*dz)
        end
        lastX, lastY, lastZ = x, y, z
    end
end