script_name("NeoLoader Menu")
script_description("Built-in NeoLoader menu test script")
script_version("1.0")

local menu_open = false

function neo_loader_self_test()
    neo_log("NeoLoader self-test started")
    neo_fs_write("config/self_test.txt", "ok")
    local content = neo_fs_read("config/self_test.txt")
    if content ~= "ok" then
        neo_log("NeoLoader self-test failed: filesystem")
        return false
    end
    neo_event_subscribe("neo_self_test", function() end)
    neo_event_publish("neo_self_test")
    neo_schedule(1, function() neo_log("NeoLoader scheduler callback") end)
    neo_tick(5)
    neo_log("NeoLoader self-test passed")
    return true
end

function neo_menu_open()
    menu_open = true
    neo_log("NeoLoader menu opened")
    return true
end

function neo_menu_close()
    menu_open = false
    neo_log("NeoLoader menu closed")
    return true
end

function neo_menu_toggle()
    if menu_open then
        return neo_menu_close()
    end
    return neo_menu_open()
end

function neo_menu_is_open()
    return menu_open
end

neo_log("NeoLoader built-in menu script loaded")
