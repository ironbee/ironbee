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
-- This module is the modlua framework code for managing Lua modules in
-- IronBee.
--
-- Author: Sam Baskinger <sbaskinger@qualys.com>
-- =========================================================================

local ffi      = require("ffi")
local debug    = require("debug")
local string   = require("string")
local ibutil   = require('ironbee/util')
local ibengine = require("ironbee/engine")
local ibtx     = require("ironbee/tx")

-- The module to define.
local _M = {}

-- ===============================================
-- Setup some module metadata.
-- ===============================================
_M._COPYRIGHT = "Copyright (C) 2010-2014 Qualys, Inc."
_M._DESCRIPTION = "IronBee Lua Module Framework."
_M._VERSION = "1.0"

-- Table of loaded lua modules objects.
-- These are stored by lua module index after the lua module
-- is registered with the ib_engine.
local lua_modules = {}

-- Similar to lua_modules, but the keys here are by name.
local lua_modules_by_name = {}

-- A map of Lua module config directives to callback tables.
-- A callback table, t,contains t.fn which is the actuall
-- Lua callback function. The table also contains t.type, the
-- C enum type of the callback. This is not very useful for
-- Lua directives, though.
local lua_module_directives = {}

-- Transation table of state event strings to integers.
local stateToInt = {
    ["conn_started_event"]             = tonumber(ffi.C.conn_started_event),
    ["conn_finished_event"]            = tonumber(ffi.C.conn_finished_event),
    ["tx_started_event"]               = tonumber(ffi.C.tx_started_event),
    ["tx_process_event"]               = tonumber(ffi.C.tx_process_event),
    ["tx_finished_event"]              = tonumber(ffi.C.tx_finished_event),
    ["handle_context_conn_event"]      = tonumber(ffi.C.handle_context_conn_event),
    ["handle_connect_event"]           = tonumber(ffi.C.handle_connect_event),
    ["handle_context_tx_event"]        = tonumber(ffi.C.handle_context_tx_event),
    ["handle_request_header_event"]    = tonumber(ffi.C.handle_request_header_event),
    ["handle_request_event"]           = tonumber(ffi.C.handle_request_event),
    ["handle_response_header_event"]   = tonumber(ffi.C.handle_response_header_event),
    ["handle_response_event"]          = tonumber(ffi.C.handle_response_event),
    ["handle_disconnect_event"]        = tonumber(ffi.C.handle_disconnect_event),
    ["handle_postprocess_event"]       = tonumber(ffi.C.handle_postprocess_event),
    ["handle_logging_event"]           = tonumber(ffi.C.handle_logging_event),
    ["conn_opened_event"]              = tonumber(ffi.C.conn_opened_event),
    ["conn_closed_event"]              = tonumber(ffi.C.conn_closed_event),
    ["request_started_event"]          = tonumber(ffi.C.request_started_event),
    ["request_header_data_event"]      = tonumber(ffi.C.request_header_data_event),
    ["request_header_finished_event"]  = tonumber(ffi.C.request_header_finished_event),
    ["request_body_data_event"]        = tonumber(ffi.C.request_body_data_event),
    ["request_finished_event"]         = tonumber(ffi.C.request_finished_event),
    ["response_started_event"]         = tonumber(ffi.C.response_started_event),
    ["response_header_data_event"]     = tonumber(ffi.C.response_header_data_event),
    ["response_header_finished_event"] = tonumber(ffi.C.response_header_finished_event),
    ["response_body_data_event"]       = tonumber(ffi.C.response_body_data_event),
    ["response_finished_event"]        = tonumber(ffi.C.response_finished_event),
    ["handle_logevent_event"]          = tonumber(ffi.C.handle_logevent_event),
    ["context_open_event"]             = tonumber(ffi.C.context_open_event),
    ["context_close_event"]            = tonumber(ffi.C.context_close_event),
    ["context_destroy_event"]          = tonumber(ffi.C.context_destroy_event),
    ["engine_shutdown_initiated"]      = tonumber(ffi.C.engine_shutdown_initiated_event),
}

-- Build reverse map of stateToInt.
local intToState = { }
for k,v in pairs(stateToInt) do
    intToState[v] = k
end

-- ########################################################################
-- Module API
-- ########################################################################
-- API passed to user code building a module.
local moduleapi = {}
moduleapi.__index = moduleapi
setmetatable(moduleapi, ibengine)

-- Make a new moduleapi.
-- @param[in] self The class table.
-- @param[in] ib The IronBee engine. Because
--            a moduleapi object is a descendent of ibengine
--            it needs to run that constructor which takes an ib_engine_t.
-- @param[in] name The module name. While not required, this is
--            currently always the Lua module file name.
-- @param[in] index The module index in the IronBee engine.
-- @param[in] register_directive A function that will
--            do the work in C to register a directive with the IronBee
--            engine. May be null if this is used after the
--            ironbe configuration phase.
moduleapi.new = function(self, ib, mod, name, index, cregister_directive)
    local t = ibengine:new(ib)

    -- The module name.
    t.name = name

    -- The module index in the IronBee engine.
    t.index = index

    -- IronBee module structure pointer. Lua need never unpack this.
    t.ib_module = ffi.cast("ib_module_t *", mod)

    -- Where event callbacks are stored.
    t.events = {}

    -- Directives to register after the module is loaded.
    t.directives = {}

    -- Store the c register directive callback.
    t.cregister_directive = cregister_directive

    return setmetatable(t, self);
end

-- Schedule a directive to be registered after the module is done loading.
--
-- If the cregister_directive is nil then this call has no effect.
--
-- @param[in] self The object.
-- @param[in] name The name of the directive.
-- @param[in] dirtype The C enum type of the directive.
-- @param[in] fn The function to call.
moduleapi.register_directive = function(self, name, dirtype, fn, flagmap)

    -- If there is no registration directive, bail out.
    if self.cregister_directive == nil then
        return ffi.C.IB_OK
    end

    lua_module_directives[name] = {
        type = dirtype,
        fn = fn,
        mod = self,
    }

    local rc

    if (flagmap == nil) then
        rc,msg = self:cregister_directive(name, dirtype)
    else
        rc,msg = self:cregister_directive(name, dirtype, flagmap)
    end

    if rc ~= ffi.C.IB_OK then
        self:logError(
            "Failed to register directive %s: %d - %s",
            name,
            rc,
            tostring(msg)
            )
    end

    return rc
end

-- Register a directive to be handled by a particular Lua function.
-- @param[in] self The object.
-- @param[in] name The name of the directive.
-- @param[in] fn The function to call.
moduleapi.register_onoff_directive = function(self, name, fn)
    return self:register_directive(name, ffi.C.IB_DIRTYPE_ONOFF, fn)
end
-- Register a directive to be handled by a particular Lua function.
-- @param[in] self The object.
-- @param[in] name The name of the directive.
-- @param[in] fn The function to call.
moduleapi.register_param1_directive = function(self, name, fn)
    return self:register_directive(name, ffi.C.IB_DIRTYPE_PARAM1, fn)
end
-- Register a directive to be handled by a particular Lua function.
-- @param[in] self The object.
-- @param[in] name The name of the directive.
-- @param[in] fn The function to call.
moduleapi.register_param2_directive = function(self, name, fn)
    return self:register_directive(name, ffi.C.IB_DIRTYPE_PARAM2, fn)
end
-- Register a directive to be handled by a particular Lua function.
-- @param[in] self The object.
-- @param[in] name The name of the directive.
-- @param[in] fn The function to call.
moduleapi.register_list_directive = function(self, name, fn)
    return self:register_directive(name, ffi.C.IB_DIRTYPE_LIST, fn)
end

-- Register a directive to be handled by a particular Lua function.
-- @param[in] self The object.
-- @param[in] name The name of the directive.
-- @param[in] fn The function to call.
-- @param[in] flagmap
moduleapi.register_opflags_directive = function(self, name, fn, flagmap)
    return self:register_directive(name, ffi.C.IB_DIRTYPE_OPFLAGS, fn, flagmap)
end
-- Register a directive to be handled by a particular Lua function.
-- @param[in] self The object.
-- @param[in] name The name of the directive.
-- @param[in] fn The function to call.
moduleapi.register_subblock_directive = function(self, name, fn)
    return self:register_directive(name, ffi.C.IB_DIRTYPE_SBLK1, fn)
end

moduleapi.num = function(self, name, num)
    return { "ib_num_t "..name..";\n", name, num }
end
moduleapi.string = function(self, name, lua_str)
    local mm = ffi.C.ib_engine_mm_main_get(self.ib_engine)

    local c_str = ffi.C.ib_mm_strdup(mm, lua_str)

    return { "char * "..name..";\n", name, c_str }
end
moduleapi.float = function(self, name, flt)
    return { "ib_float_t "..name..";\n", name, flt }
end
moduleapi.void = function(self, name, ptr)
    return { "void * "..name..";\n", name, ptr }
end

-- Declear the module configuration by taking a list of named types.
--
-- Named types are created by calling
--   - moduleapi.num("name"),
--   - moduleapi.string("name"),
--   - moduleapi.void("name"),
--
-- It is an error to call this twice.
--
moduleapi.declare_config = function(self, config_table)

    self.config_name = string.format("ib_luamod_%d_config_t", self.index)

    local struct_body = ""
    for k, v in ipairs(config_table) do
        struct_body = struct_body .. v[1]
    end

    local struct =
        string.format(
            "typedef struct %s { %s } %s;",
            self.config_name,
            struct_body,
            self.config_name)

    ffi.cdef(struct)

    -- After the configuration is declared, set it up in the module.
    local mm = ffi.C.ib_engine_mm_main_get(self.ib_engine)
    local sz = ffi.sizeof(self.config_name, 1)
    local default_config =
        ffi.cast(self.config_name .. "*", ffi.C.ib_mm_alloc(mm, sz))
    local rc = ffi.C.ib_module_config_initialize(
        self.ib_module,
        default_config,
        sz)

    -- Assign default configurations.
    for k, v in ipairs(config_table) do
        if #v > 1 then
            default_config[v[2]] = v[3]
        end
    end

    return default_config
end

-- The module api provides the user functions to register callbacks by.
for k,v in pairs(stateToInt) do
    moduleapi[k] = function(self, func)
        -- Assign the user's function to the callback key integer.
        self:logDebug("Registering function for event %s=%d", k, v)
        self.events[v] = func
    end
end
-- ########################################################################
-- End Module API
-- ########################################################################

-- This loads a Lua module.
--
-- A single argument is passed to the user's script and may be accessed
-- by: local t = ...
--
-- Then the user may register event callbacks by calling something like:
-- t:tx_finished_event(function(api) api:do_things() end)
--
-- Module configurations are available via api.config. This configuration
-- table is built-up by directives.
--
-- @param[in] ib IronBee engine C data.
-- @param[in] module_index The index of the module in the engine.
-- @param[in] name The name of the module. Typically the file name.
-- @param[in] cregister_directive A C function that Lua will call
--            to register directives. This may be nil
--            if this is called outside of the ironbee configuration
--            process.
-- @param[in] module_function The user's module code as a function.
--            This is called and passed a new IronBee Engine API
--            table with added functions for registering
--            callback functions. Each callback function takes
--            a single argument, the function to callback to when the
--            event occurs in the IronBee engine.
--            The callbacks registration functions are:
--            - conn_started_event
--            - conn_finished_event
--            - tx_started_event
--            - tx_process_event
--            - tx_finished_event
--            - handle_context_conn_event
--            - handle_connect_event
--            - handle_context_tx_event
--            - handle_request_header_event
--            - handle_request_event
--            - handle_response_header_event
--            - handle_response_event
--            - handle_disconnect_event
--            - handle_postprocess_event
--            - handle_logging_event
--            - conn_opened_event
--            - conn_closed_event
--            - request_started_event
--            - request_header_data_event
--            - request_header_finished_event
--            - request_body_data_event
--            - request_finished_event
--            - response_started_event
--            - response_header_data_event
--            - response_header_finished_event
--            - response_body_data_event
--            - response_finished_event
--            - handle_logevent_event
--            - context_open_event
--            - context_close_event
--            - context_destroy_event
--            - engine_shutdown_initiated_event
_M.load_module = function(
    ib,
    ib_module,
    name,
    index,
    cregister_directive,
    module_function)

    -- Build callback table to pass to module.
    local t = moduleapi:new(ib, ib_module, name, index, cregister_directive)

    -- Ask the user to build the module.
    local rc = module_function(t)

    -- Ensure that modules that break our return contract don't cause
    -- too much trouble.
    if (type(rc) ~= "number") then
        t:logError("Non numeric return value from module initialization.")
        return tonumber(ffi.C.IB_EINVAL)
    end

    -- Register the module to the internal table.
    lua_modules_by_name[name] = t
    lua_modules[index] = t

    return tonumber(rc)
end

--
-- Using the passed in c pointer, retrieve the Lua module configuration.
--
-- @param[in] ib_ctx IronBee context. An ib_context_t.
-- @param[in] ib_module IronBee module to fetch. An ib_module_t.
--
-- @returns
-- The module configuration, nil if none, and calls error(msg) on an error.
local module_config_get = function(ib_ctx, ib_module)
    -- Get the moduleapi object.
    local lua_module = lua_modules[tonumber(ib_module.idx)]
    if lua_module == nil then
        error(string.format("Cannot find module %d.", tonumber(ib_module.idx)))
    end

    -- If a configuration name is specified, fetch the config for this contex.
    if lua_module.config_name then
        local cfg = ffi.new(lua_module.config_name .. " *[1]")
        ffi.C.ib_context_module_config(ib_ctx, ib_module, cfg)
        return cfg[0]
    end

    return nil
end

-- Set a Lua module's configuration.
--
-- @param[in] cp Configuration parser. An @ref ib_cfgparser_t.
-- @param[in] ctx Current configuration context. An @ref ib_context_t.
-- @param[in] mod Module structure to check. An @ref ib_module_t.
-- @param[in] name Configuration name.
-- @param[in] val Value.
--
-- @returns
-- - IB_OK On success.
-- - Other on error and logs to @a cp.
--
_M.set = function(cp, ctx, mod, name, val)
    cp  = ffi.cast("ib_cfgparser_t *", cp)
    ctx = ffi.cast("ib_context_t *", ctx)
    mod = ffi.cast("ib_module_t *", mod)

    local cfg = module_config_get(ctx, mod);

    if ffi.typeof('ib_num_t') == ffi.typeof(cfg[name]) then
        cfg[name] = tonumber(val)
    elseif ffi.typeof('ib_float_t') == ffi.typeof(cfg[name]) then
        cfg[name] = tonumber(val)
    else
        cfg[name] = ffi.C.ib_mm_strdup(
            ffi.C.ib_engine_mm_main_get(cp.ib),
            val)
    end

    return ffi.C.IB_OK
end

-- Return the callback for a module.
--
-- @param[in] ib Currently unused.
-- @param[in] module_index The index number of the lua module.
-- @param[in] event The numeric value of the event being called.
-- @returns The callback handler or nil on error of any sort.
_M.get_callback = function(ib, module_index, event)
    local  t = lua_modules[module_index]

    -- Since we only use the ib argument for logging, we defer
    -- creating an ibengine table until we detect an error to log.

    if t == nil then
        local ibe = ibengine:new(ib)
        ibe:logError("No module for module index %d", module_index)
        return nil
    end

    local handler = t.events[event]

    return handler
end

-- The HookData object holds data pointers passed to hook callbacks.
-- Wrappers are also provided to convert pointers, if necessary.
_M.HookData = {}
function _M.HookData:new(event, ...)
    self.__index = self

    local o = { ... }

    -- Header data event. Args.data contains a ib_parsed_header_t*.
    if event == ffi.C.request_header_data_event  or
       event == ffi.C.response_header_data_event then
        o.ib_header_data = ffi.cast("ib_parsed_header_t *", o[1])

    elseif event == ffi.C.request_started_event then
        o.ib_parsed_req_line = ffi.cast("ib_parsed_req_line_t *", o[1])

    elseif event == ffi.C.response_started_event then
        o.ib_parsed_resp_line = ffi.cast("ib_parsed_resp_line_t *", o[1])

    elseif event == ffi.C.request_body_data_event then
        o.ib_request_data     = ffi.cast("const char *", o[1])
        o.ib_request_data_len = o[2]

    elseif event == ffi.C.response_body_data_event then
        o.ib_response_data     = ffi.cast("const char *", o[1])
        o.ib_response_data_len = o[2]

    end

    return setmetatable(o, self)
end

function _M.HookData:get_response_body_data()
    return ffi.string(self.ib_response_data, self.ib_response_data_len)
end

function _M.HookData:get_request_body_data()
    return ffi.string(self.ib_request_data, self.ib_request_data_len)
end

-- This function is called by C to dispatch an event to a lua module.
--
-- @param[in] handler Function to call. This should take @a args as input.
-- @param[in] ib_engine The IronBee engine.
-- @param[in] ib_module The ib_module pointer.
-- @param[in] event An integer representing the event type enum.
-- @param[in] ctxlst List of contexts.
-- @param[in] ib_conn The connection pointer. May be nil for null callbacks.
-- @param[in] ib_tx The transaction pointer. May be nil.
-- @param[in] ib_ctx The configuration context in the case of
--            context events. If this is nil, then the main context
--            is fetched out of ib_engine.
--
-- @returns And integer representation of an ib_status_t.
--   - IB_OK on success.
--
_M.dispatch_module = function(
    handler,
    ib_engine,
    ib_module,
    event,
    ctxlst,
    ib_conn,
    ib_tx,
    ib_ctx,
    ...)

    ib_engine = ffi.cast("ib_engine_t *", ib_engine)
    ib_module = ffi.cast("ib_module_t *", ib_module)

    local args

    if ib_tx == nil then
        args = ibengine:new(ib_engine)
    else
        args = ibtx:new(ib_engine, ib_tx)
    end

    -- Get the moduleapi object.
    args.config = module_config_get(ib_ctx, ib_module);

    -- Event type.
    args.event = event

    -- Event name.
    args.event_name = intToState[tonumber(args.event)]

    -- Capture extra arguments that may have been passed.
    -- We examine the event argument to decode these.
    args.data = _M.HookData:new(event,  ... )

    if ib_conn ~= nil then
        -- Connection.
        args.ib_conn = ffi.cast("ib_conn_t*", ib_conn)
    end

    if ib_ctx == nil then
        -- Configuration context for context events.
        args.ib_ctx = ffi.C.ib_context_main(ib_engine)
    else
        -- Configuration context for context events.
        args.ib_ctx = ffi.cast("ib_context_t*", ib_ctx)
    end

    -- Dispatch
    args:logDebug("Running callback for %s.", args.event_name)

    -- Do the dispatch.
    local success, rc = pcall(handler, args)

    args:logDebug("Ran callback for %s.", args.event_name)

    -- If true, then there are no Lua errors.
    if success then
        -- If rc == IB_OK, all is well.
        if rc ~= ffi.C.IB_OK then
            args:logError(
                "Callback for %s exited with %s.",
                args.event_name,
                ffi.string(ffi.C.ib_status_to_string(rc)))
        end

    -- Lua error occured. Rc should contain the message.
    else
        args:logError(
            "Callback for %s failed: %s",
            args.event_name,
            tostring(rc))
    end


    -- Ensure that modules that break our return contract don't cause
    -- too much trouble.
    if (type(rc) ~= "number") then
        args:logError("Non numeric return value from module. Returning IB_OK.")
        rc = 0
    end

    -- Return IB_OK.
    return 0
end

-- ########################################################################
-- modlua API Directive Callbacks.
-- ########################################################################
_M.modlua_config_cb_blkend = function(ib, modidx, ctxlst, name)
    local cfg = _M.create_configuration(ib, modidx, ctxlst)

    local directive_table = lua_module_directives[name]

    directive_table.fn(lua_modules[modidx], cfg, name)

    return ffi.C.IB_OK
end
_M.modlua_config_cb_onoff = function(ib, modidx, ctxlst, name, onoff)
    local cfg = _M.create_configuration(ib, modidx, ctxlst)

    local directive_table = lua_module_directives[name]

    directive_table.fn(lua_modules[modidx], cfg, name, onoff)

    return ffi.C.IB_OK
end
_M.modlua_config_cb_param1 = function(ib, modidx, ctxlst, name, p1)
    local cfg = _M.create_configuration(ib, modidx, ctxlst)

    local directive_table = lua_module_directives[name]

    directive_table.fn(lua_modules[modidx], cfg, name, p1)

    return ffi.C.IB_OK
end
_M.modlua_config_cb_param2 = function(ib, modidx, ctxlst, name, p1, p2)
    local cfg = _M.create_configuration(ib, modidx, ctxlst)

    local directive_table = lua_module_directives[name]

    directive_table.fn(lua_modules[modidx], cfg, name, p1, p2)

    return ffi.C.IB_OK
end
_M.modlua_config_cb_list = function(ib, modidx, ctxlst, name, list)
    local cfg = _M.create_configuration(ib, modidx, ctxlst)

    local directive_table = lua_module_directives[name]

    -- Parameter list passed to callback.
    local plist = {}

    ibutil.each_list_node(
        ffi.cast("ib_list_t*", list),
        function(s)
            table.insert(plist, v)
        end,
        "char *")

    directive_table.fn(lua_modules[modidx], cfg, name, plist)

    return ffi.C.IB_OK
end
_M.modlua_config_cb_opflags = function(ib, modidx, ctxlst, name, flags)
    local cfg = _M.create_configuration(ib, modidx, ctxlst)

    local directive_table = lua_module_directives[name]

    directive_table.fn(lua_modules[modidx], cfg, name, flags)

    return ffi.C.IB_OK
end
_M.modlua_config_cb_sblk1 = function(ib, modidx, ctxlst, name, p1)
    local cfg = _M.create_configuration(ib, modidx, ctxlst)

    local directive_table = lua_module_directives[name]

    directive_table.fn(lua_modules[modidx], cfg, name, p1)

    return ffi.C.IB_OK
end
-- ########################################################################
-- END modlua API Directive Callbacks.
-- ########################################################################

return _M
