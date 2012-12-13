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
-- This module is the modlua framekwork code for managing Lua modules in
-- IronBee.
--
-- Author: Sam Baskinger <sbaskinger@qualys.com>
-- =========================================================================

local ffi = require("ffi")
local ibapi = require("ironbee-api")
local debug = require("debug")
local string = require("string")

-- The module to define.
local M = {}

-- Table of loaded lua modules objects.
-- These are stored by lua module index after the lua module
-- is registered with the ib_engine.
local lua_modules = {}

-- Similar to lua_modules, but the keys here are by name.
local lua_modules_by_name = {}

-- Transation table of state event strings to integers.
local stateToInt = {
    ["conn_started_event"] = tonumber(ffi.C.conn_started_event),
    ["conn_finished_event"] = tonumber(ffi.C.conn_finished_event),
    ["tx_started_event"] = tonumber(ffi.C.tx_started_event),
    ["tx_process_event"] = tonumber(ffi.C.tx_process_event),
    ["tx_finished_event"] = tonumber(ffi.C.tx_finished_event),
    ["handle_context_conn_event"] = tonumber(ffi.C.handle_context_conn_event),
    ["handle_connect_event"] = tonumber(ffi.C.handle_connect_event),
    ["handle_context_tx_event"] = tonumber(ffi.C.handle_context_tx_event),
    ["handle_request_header_event"] = tonumber(ffi.C.handle_request_header_event),
    ["handle_request_event"] = tonumber(ffi.C.handle_request_event),
    ["handle_response_header_event"] = tonumber(ffi.C.handle_response_header_event),
    ["handle_response_event"] = tonumber(ffi.C.handle_response_event),
    ["handle_disconnect_event"] = tonumber(ffi.C.handle_disconnect_event),
    ["handle_postprocess_event"] = tonumber(ffi.C.handle_postprocess_event),
    ["conn_opened_event"] = tonumber(ffi.C.conn_opened_event),
    ["conn_data_in_event"] = tonumber(ffi.C.conn_data_in_event),
    ["conn_data_out_event"] = tonumber(ffi.C.conn_data_out_event),
    ["conn_closed_event"] = tonumber(ffi.C.conn_closed_event),
    ["request_started_event"] = tonumber(ffi.C.request_started_event),
    ["request_header_data_event"] = tonumber(ffi.C.request_header_data_event),
    ["request_header_finished_event"] = tonumber(ffi.C.request_header_finished_event),
    ["request_body_data_event"] = tonumber(ffi.C.request_body_data_event),
    ["request_finished_event"] = tonumber(ffi.C.request_finished_event),
    ["response_started_event"] = tonumber(ffi.C.response_started_event),
    ["response_header_data_event"] = tonumber(ffi.C.response_header_data_event),
    ["response_header_finished_event"] = tonumber(ffi.C.response_header_finished_event),
    ["response_body_data_event"] = tonumber(ffi.C.response_body_data_event),
    ["response_finished_event"] = tonumber(ffi.C.response_finished_event)
}

-- Build reverse map of stateToInt.
local intToState = { }
for k,v in pairs(stateToInt) do
    intToState[v] = k
end

local log = function(ib, level, msg, ...) 
    local debug_table = debug.getinfo(2, "Sl")
    local file = debug_table.short_src
    local line = debug_table.currentline

    -- Msg must not be nil.
    if msg == nil then msg = "(nil)" end

    if type(msg) ~= 'string' then msg = tostring(msg) end

    -- If we have more arguments, format msg with them.
    if ... ~= nil then msg = string.format(msg, ...) end

    -- LOG!
    ffi.C.ib_log_ex(ib, level, file, line, msg);
end

-- This module reports some errors.
-- @param[in] ib IronBee C data.
-- @param[in] msg Message.
local log_error = function(ib, msg, ...)
    log(ib, ffi.C.IB_LOG_ERROR, msg, ...)
end

local log_info = function(ib, msg, ...)
    log(ib, ffi.C.IB_LOG_INFO, msg, ...)
end

local log_debug = function(ib, msg, ...)
    log(ib, ffi.C.IB_LOG_DEBUG, msg, ...)
end

-- ===============================================
-- Setup some module metadata.
-- ===============================================
M._COPYRIGHT = "Copyright (C) 2010-2013 Qualys, Inc."
M._DESCRIPTION = "IronBee Lua Module Framework."
M._VERSION = "1.0"

-- @param[in] ib IronBee engine C data.
M.load_module = function(ib, module_index, name, module_function)
    -- Build callback table to pass to module.
    local t = { ["events"] = {} }

    -- Table t has a list of registration functions. All they do is
    -- force the user to spell the callbacks correctly.
    for k,v in pairs(stateToInt) do
        t[k] = function(self, func)
            -- Assign the user's function to the callback key integer.
            self.events[v] = func
        end
    end

    -- Ask the user to build the module.
    local rc = module_function(t)

    -- Ensure that modules that break our return contract don't cause
    -- too much trouble.
    if (type(rc) ~= "number") then
        log_error(ib, "Non numeric return value from module initialization.")
        return tonumber(ffi.C.IB_EINVAL)
    end

    -- Register the module to the internal table.
    lua_modules_by_name[name] = t
    lua_modules[module_index] = t

    return tonumber(rc)
end

-- Return the callback for a module.
--
-- @param[in] ib Currently unused.
-- @param[in] module_index The index number of the lua module.
-- @param[in] event The numeric value of the event being called.
-- @returns The callback handler or nil on error of any sort.
M.get_callback = function(ib, module_index, event)
    local  t = lua_modules[module_index]

    if t == nil then
        log_error(ib, "No module for module index %d", module_index)
        return nil
    end

    local handler = t.events[event]

    if handler == nil then
        log_info(
            ib,
            "No handler registered for module %d event %d",
            module_index,
            event)
        return nil
    end

    return handler
end

-- This function is called by C to dispatch an event to a lua module.
--
-- @param[in] handler Function to call. This should take @a args as input.
-- @param[in] args A table of arguments. This will contains, at least,
--            event and ib_engine. Event will be an integer representing
--            the event being dispatched and ib_engine will be the
--            running IronBee Engine.
--
-- @returns And integer representation of an ib_status_t.
--   - IB_OK on success.
--   
M.dispatch_module = function(handler, args)

    -- Set event name.
    args.event_name = intToState[tonumber(args.event)]

    -- Dispatch
    log_debug(args.ib_engine, "Running callback for %s.", args.event_name)

    local rc = handler(args)

    log_debug(args.ib_engine, "Ran callback for %s.", args.event_name)

    -- Ensure that modules that break our return contract don't cause
    -- too much trouble.
    if (type(rc) ~= "number") then
        log_error(
            args.ib_engine,
            "Non numeric return value from module. Returning IB_OK.")
        rc = 0
    end

    return tonumber(rc)
end

return M
