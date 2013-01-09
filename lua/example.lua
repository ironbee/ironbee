-- =========================================================================
-- =========================================================================
-- Licensed to Qualys, Inc. (QUALYS) under one or more
-- contributor license agreements.  See the NOTICE file distributed with
-- this work for additional information regarding copyright ownership.
-- QUALYS licenses this file to You under the Apache License, Version 2.0
-- (the "License"); you may not use this file except in compliance with
-- the License.  You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
-- =========================================================================
-- =========================================================================
--
-- This is an example IronBee lua module using the new FFI interface.
--
-- Author: Brian Rectanus <brectanus@qualys.com>
-- =========================================================================


local mod_init = ...

-- Register a directive
mod_init:register_param1_directive(
    "LuaExampleDirective",
    function(ib_module, module_config, name, param1)
        -- Log that we're configuring the module.
        ib_module:logInfo("Got directive %s=%s", name, param1)

        -- Configuration, in this case, is simply storing the string.
        module_config[name] = param1
    end)

-- ===============================================
-- This is called when a connection is started.
-- ===============================================
mod_init:conn_started_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when a connection is opened.
-- ===============================================
mod_init:conn_opened_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when a connection context was
-- chosen and is ready to be handled.
-- ===============================================
mod_init:handle_context_conn_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the connection is ready to
-- be handled.
-- ===============================================
mod_init:handle_connect_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the transaction starts.
-- ===============================================
mod_init:tx_started_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when a request starts.
-- ===============================================
mod_init:request_started_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the transaction context
-- is ready to be handled.
-- ===============================================
mod_init:handle_context_tx_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the request headers are
-- available to inspect.
-- ===============================================
mod_init:handle_request_header_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])

    local req_line = ib:get("request_line")

    ib:logDebug("Request line is a field type: %s", type(req_line))

    local req_headers = ib:get("request_headers")

    if type(req_headers) == 'table' then
        for k,f in pairs(req_headers) do
            if type(f) == 'table' then
                ironbee.ib_log_debug(ib, "REQUEST_HEADERS.%s=<list>", k)
            else
                ironbee.ib_log_debug(ib, "REQUEST_HEADERS.%s=%s", k, f)
            end
        end
    end

    -- You can access individual subfields within collections directly
    -- via "name.subname" syntax:
    local http_host_header = ib:get("request_headers.host")

    -- Request cookies are a collection (table of field objects)
    local req_cookies = ib:get("request_cookies")

    return 0
end

-- ===============================================
-- This is called when the request body is
-- available.
-- ===============================================
mod_init:request_body_data_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the complete request is
-- ready to be handled.
-- ===============================================
mod_init:handle_request_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the request is finished.
-- ===============================================
mod_init:request_finished_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the transaction is ready
-- to be processed.
-- ===============================================
mod_init:tx_process_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the response is started.
-- ===============================================
mod_init:response_started_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the response headers are
-- available.
-- ===============================================
mod_init:handle_response_header_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the response headers are
-- ready to be handled.
-- ===============================================
mod_init:response_header_data_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the response body is
-- available.
-- ===============================================
mod_init:response_body_data_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the complete response is
-- ready to be handled.
-- ===============================================
mod_init:handle_response_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the response is finished.
-- ===============================================
mod_init:response_finished_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called after the transaction is done
-- and any post processing can be done.
-- ===============================================
mod_init:handle_postprocess_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the transaction is
-- finished.
-- ===============================================
mod_init:tx_finished_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when a connection is closed.
-- ===============================================
mod_init:conn_closed_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when the connection disconnect
-- is ready to handle.
-- ===============================================
mod_init:handle_disconnect_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when there is incoming data for
-- the connection.
-- ===============================================
mod_init:conn_data_in_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when there is outgoing data for
-- the connection.
-- ===============================================
mod_init:conn_data_out_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

mod_init:conn_finished_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

mod_init:request_header_data_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

mod_init:request_header_finished_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

mod_init:response_header_finished_event(function(ib)
    ib:logInfo(
        "Module configuration: LuaExampleDirective=%s",
        ib.config["LuaExampleDirective"])
    return 0
end

-- Report success.
t:logInfo("Module loaded!")

-- Return IB_OK.
return 0
