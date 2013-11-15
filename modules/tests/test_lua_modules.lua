mod = ...

mod:logInfo("Configurating Lua module #%d", mod.index)

-- Declare the configuration.
local default_config = mod:declare_config {
    mod:num("counter", 1),
    mod:string("name", "my config"),
    mod:void("pointer", nil),
}

-- Detect a successful default value by setting the default to 100.
if default_config.counter ~= 1 then
    return -1
end

-- We changed our minds. We would like a default configuration value of 100.
default_config.counter = 100

mod:tx_started_event(function(tx, event)
    local val = tonumber(tx.config.counter) + 1
    local var = 'LUA_MODULE_COUNTER'

    tx:logInfo("Setting %s to %d.", var, val)
    tx:set(var, val)
    tx:logInfo("Got %s = %s.", var, tx:get(var))

    return 0
end)

return 0
