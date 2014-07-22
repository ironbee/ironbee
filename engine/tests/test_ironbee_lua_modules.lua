-- A Test Lua Module
local t = ...

local statesTable = {
    "conn_started_state",
    "conn_finished_state",
    "tx_started_state",
    "tx_process_state",
    "tx_finished_state",
    "handle_context_conn_state",
    "handle_connect_state",
    "handle_context_tx_state",
    "handle_request_header_state",
    "handle_request_state",
    "handle_response_header_state",
    "handle_response_state",
    "handle_disconnect_state",
    "handle_postprocess_state",
    "handle_logging_state",
    "conn_opened_state",
    "conn_closed_state",
    "request_started_state",
    "request_header_data_state",
    "request_header_finished_state",
    "request_body_data_state",
    "request_finished_state",
    "response_started_state",
    "response_header_data_state",
    "response_header_finished_state",
    "response_body_data_state",
    "response_finished_state",
    "handle_logstate_state",
}

-- Register EVERY callback we can with a pretty simple function.
for k,v in pairs(statesTable) do
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

local IB_OP_CAPABILITY_NON_STREAM = 1
local myAction = t:action("setvar", "A=1", 0)
local myOperator = t:operator("rx", ".*", IB_OP_CAPABILITY_NON_STREAM)

if myAction == nil then
    return ffi.C.IB_EOTHER
end
if myOperator == nil then
    return ffi.C.IB_EOTHER
end

t:response_header_data_state(function(ib)

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

t:logInfo("----- t.states Contents ------")
-- Debug output
for k,v in pairs(t.states) do
    t:logInfo("States table contents: %s=%s", k, v)
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
