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
-- This is an example IronBee lua module.  Essentially, this code is
-- executed on load, allowing the developer to register other functions
-- to get called when states fire at runtime.
--
-- This example just registers a logging function for most states as well
-- as a more complex function to execute when the request headers are
-- ready to process.
--
-- Author: Sam Baskinger <sbaskinger@qualys.com>
-- Author: Brian Rectanus <brectanus@qualys.com>
-- =========================================================================

-- ===============================================
-- Get an IronBee module object, which is passed
-- into the loading module as a parameter.  This
-- is used to access the IronBee Lua API.
-- ===============================================
local ibmod = ...

-- ===============================================
-- Register an IronBee configuration file
-- directive that takes a single string parameter:
--
--   LuaExampleDirective <string>
-- ===============================================
ibmod:register_param1_directive(
    "LuaExampleDirective",
    function(ib_module, module_config, name, param1)
        -- Log that we're configuring the module.
        ibmod:logInfo("Got directive %s=%s", name, param1)

        -- Configuration, in this case, is simply storing the string.
        module_config[name] = param1
    end
)

-- ===============================================
-- Generic function to log an info message when
-- an states fires.
-- ===============================================
local log_state = function(ib, state)
    ib:logInfo(
        "Handling state=%s: LuaExampleDirective=%s",
        ib.state_name,
        ib.config["LuaExampleDirective"])
    return 0
end

-- ===============================================
-- This is called when a connection is started.
-- ===============================================
ibmod:conn_started_state(log_state)

-- ===============================================
-- This is called when a connection is opened.
-- ===============================================
ibmod:conn_opened_state(log_state)

-- ===============================================
-- This is called when a connection context was
-- chosen and is ready to be handled.
-- ===============================================
ibmod:handle_context_conn_state(log_state)

-- ===============================================
-- This is called when the connection is ready to
-- be handled.
-- ===============================================
ibmod:handle_connect_state(log_state)

-- ===============================================
-- This is called when the transaction starts.
-- ===============================================
ibmod:tx_started_state(log_state)

-- ===============================================
-- This is called when a request starts.
-- ===============================================
ibmod:request_started_state(log_state)

-- ===============================================
-- This is called when the transaction context
-- is ready to be handled.
-- ===============================================
ibmod:handle_context_tx_state(log_state)

-- ===============================================
-- This is called when there is new request
-- header data.
-- ===============================================
ibmod:request_header_data_state(log_state)

-- ===============================================
-- This is called when the request header data
-- has all been received.
-- ===============================================
ibmod:request_header_finished_state(log_state)

-- ===============================================
-- This is called when the request headers are
-- available to inspect.
-- ===============================================
ibmod:handle_request_header_state(
    function(ib)
        log_state(ib)

        -- You can get named fields.  Scalar fields
        -- will return scalar values.
        local req_line = ib:get("request_line")
        ib:logInfo("REQUEST_LINE: %s=%s", type(req_line), tostring(req_line))

        -- You can fetch collections as a table of name/value pairs:
        local req_headers = ib:get("request_headers")
        if type(req_headers) == 'table' then
            -- Loop over the key/field
            for k,f in pairs(req_headers) do
                if type(f) == 'table' then
                    -- Fields come as (tables), which you can
                    -- unpack into simple name/value pairs.
                    --
                    -- TODO: Need to make this a bit cleaner
                    name, val = unpack(f)
                    ib:logInfo("REQUEST_HEADERS:%s=%s", tostring(name), tostring(val))
                else
                    ib:logInfo("REQUEST_HEADERS:%s=%s", k, f)
                end
            end
        end

        -- You can access individual subfields within collections directly
        -- via "name:subname" syntax, but these will come as a list
        -- of values (as more than one subname is always allowed):
        local http_host_header = ib:getValues("request_headers:host")
        ib:logInfo("First HTTP Host Header: %s", http_host_header[1])

        -- Request cookies are a collection (table of field objects)
        -- similar to headers:
        local req_cookies = ib:get("request_cookies")
        if type(req_cookies) == 'table' then
            -- Loop over the key/field
            for k,f in pairs(req_cookies) do
                if type(f) == 'table' then
                    -- Fields come as (tables), which you can
                    -- unpack into simple name/value pairs.
                    --
                    -- TODO: Need to make this a bit cleaner
                    name, val = unpack(f)
                    ib:logInfo("REQUEST_COOKIES:%s=%s", tostring(name), tostring(val))
                else
                    ib:logInfo("REQUEST_COOKIES:%s=%s", k, f)
                end
            end
        end

        return 0
    end
)

-- ===============================================
-- This is called when the request body is
-- available.
-- ===============================================
ibmod:request_body_data_state(log_state)

-- ===============================================
-- This is called when the complete request is
-- ready to be handled.
-- ===============================================
ibmod:handle_request_state(log_state)

-- ===============================================
-- This is called when the request is finished.
-- ===============================================
ibmod:request_finished_state(log_state)

-- ===============================================
-- This is called when the transaction is ready
-- to be processed.
-- ===============================================
ibmod:tx_process_state(log_state)

-- ===============================================
-- This is called when the response is started.
-- ===============================================
ibmod:response_started_state(log_state)

-- ===============================================
-- This is called when the response headers are
-- available.
-- ===============================================
ibmod:handle_response_header_state(log_state)

-- ===============================================
-- This is called when the response headers are
-- ready to be handled.
-- ===============================================
ibmod:response_header_data_state(log_state)

-- ===============================================
-- This is called when the response header data
-- has all been received.
-- ===============================================
ibmod:response_header_finished_state(log_state)

-- ===============================================
-- This is called when the response body is
-- available.
-- ===============================================
ibmod:response_body_data_state(log_state)

-- ===============================================
-- This is called when the complete response is
-- ready to be handled.
-- ===============================================
ibmod:handle_response_state(log_state)

-- ===============================================
-- This is called when the response is finished.
-- ===============================================
ibmod:response_finished_state(log_state)

-- ===============================================
-- This is called after the transaction is done
-- and any post processing can be done.
-- ===============================================
ibmod:handle_postprocess_state(log_state)

-- ===============================================
-- This is called after postprocess is complete,
-- to allow for any post-transaction logging.
-- ===============================================
ibmod:handle_logging_state(log_state)

-- ===============================================
-- This is called when the transaction is
-- finished.
-- ===============================================
ibmod:tx_finished_state(log_state)

-- ===============================================
-- This is called when a connection is closed.
-- ===============================================
ibmod:conn_closed_state(log_state)

-- ===============================================
-- This is called when the connection disconnect
-- is ready to handle.
-- ===============================================
ibmod:handle_disconnect_state(log_state)

-- ===============================================
-- This is called when the connection is finished.
-- ===============================================
ibmod:conn_finished_state(log_state)

-- ===============================================
-- This is called when a logevent event has
-- occurred.
-- ===============================================
ibmod:handle_logevent_state(log_state)

-- Report success.
ibmod:logInfo("Module loaded!")

-- Return IB_OK.
return 0
