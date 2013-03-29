-- A Test Lua Module
local t = ...

local eventsTable = {
    "conn_started_event",
    "conn_finished_event",
    "tx_started_event",
    "tx_process_event",
    "tx_finished_event",
    "handle_context_conn_event",
    "handle_connect_event",
    "handle_context_tx_event",
    "handle_request_header_event",
    "handle_request_event",
    "handle_response_header_event",
    "handle_response_event",
    "handle_disconnect_event",
    "handle_postprocess_event",
    "handle_logging_event",
    "conn_opened_event",
    "conn_closed_event",
    "request_started_event",
    "request_header_data_event",
    "request_header_finished_event",
    "request_body_data_event",
    "request_finished_event",
    "response_started_event",
    "response_header_data_event",
    "response_header_finished_event",
    "response_body_data_event",
    "response_finished_event",
    "handle_logevent_event",
}

-- Register EVERY callback we can with a pretty simple function.
for k,v in pairs(eventsTable) do
    t[v](t, function(ib)
        ib:logInfo("Inside running callback %s.", v)
        ib:logInfo("Effective configuration...")

        local fn
        if ib.ib_tx == nil then
            fn = function(i,j)
                ib:logInfo("\tEffective config:%s=%s", i, j)
            end
        else
            fn = function(i,j)
                ib:logInfo("\tSetting in DPI %s=%s", i, j)
                ib:set(i, j)
            end
        end

        for i,j in pairs({ "MyLuaDirective", "MyLuaDirective2" }) do
            fn(j, ib.config[j])
        end

        return 0
    end)
end

local IB_OP_FLAG_PHASE = 1
local myAction = t:action("setvar", "A=1", 0)
local myOperator = t:operator("rx", ".*", IB_OP_FLAG_PHASE)

if myAction == nil then
    return ffi.C.IB_EOTHER
end
if myOperator == nil then
    return ffi.C.IB_EOTHER
end

t:response_header_data_event(function(ib)

    local rc
    local result

    local rule_exec = ffi.cast("ib_tx_t*", ib.ib_tx).rule_exec

    rc = myAction(rule_exec)
    ib:logInfo("Action setvar returned %d", rc)
    ib:logInfo("Value of A = %s", tostring(ib:get("A")))

    -- Call operator to fetch A.
    rc, result = myOperator(rule_exec, ib:getDataField("A"))
    ib:logInfo("Operator rx returned rc=%d and result=%d", rc, result)
end)

-- Debug output
t:logInfo("----- t Contents ------")
for k,v in pairs(t) do
    t:logInfo("Table contents: %s=%s", k, v)
end

t:logInfo("----- t.events Contents ------")
-- Debug output
for k,v in pairs(t.events) do
    t:logInfo("Events table contents: %s=%s", k, v)
end

t:logInfo("A test Lua Module.")

-- Register a directive
t:register_param1_directive(
    "MyLuaDirective",
    function(mod, cfg, name, param1)
        mod:logInfo("Got directive %s=%s", name, param1)
        cfg[name] = param1
    end)

-- Register another directive
t:register_param1_directive(
    "MyLuaDirective2",
    function(mod, cfg, name, param1)
        mod:logInfo("Got directive %s=%s", name, param1)
        cfg[name] = param1
    end)

-- Return IB_OK
return 0
