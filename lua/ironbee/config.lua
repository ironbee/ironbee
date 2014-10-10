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
--
-- =========================================================================

-------------------------------------------------------------------
-- IronBee - Config
--
-- IronBee configuration.
--
-- @module ironbee.config
--
-- @copyright Qualys, Inc., 2010-2014
-- @license Apache License, Version 2.0
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
-------------------------------------------------------------------
local ibapi      = require('ironbee/api')
local ffi        = require('ffi')
local Waggle     = require('ironbee/waggle')
local Predicate  = require('ironbee/predicate')
local cfg_parser = require('ironbee/config/configuration_parser')

-- The g_config_call_depth is a "global" (file scoped) value
-- that tracks how many blocks we are "in" when doing configurations.
-- If a directive is evaluated in g_config_call_depth > 0,
-- the its execution is deferred by a sub-function call.
local g_config_call_depth = 0

local dsl_error_fn = function(msg)
    local level = 1
    local info = debug.getinfo(level, 'lSn')

    CP:logError("--[[FATAL: %s %s", msg, "--]]")
    CP:logError("--[[FATAL-Stack Trace---------")
    while info do
        CP:logError("-- %s:%s", info.short_src, info.currentline)
        level = level + 1
        info = debug.getinfo(level, 'lSn')
    end
    CP:logError("--]]")
end

-- This function does the work of considering the type
-- of block_body and applying it correctly.
local block_apply = function(directive_name, block_args, block_body)

    -- Processing block_body requires a function that is
    -- called bewteen block_start and block_process.
    CP:block_process(directive_name, block_args, function(cp)

        -- Block body processing.
        if type(block_body) == 'string' then
            local fn = loadstring(block_body)
            local success, ret = xpcall(fn, dsl_error_fn)

            if not success then
                local err_string = "Failed to apply function body for "..block_args[1]
                error(err_string .. ": ".. tostring(ret))
            end
        elseif type(block_body) == 'function' then
            local success, ret = xpcall(block_body, dsl_error_fn)

            if not success then
                local err_string = "Failed to apply function body for "..block_args[1]
                error(err_string .. ": ".. tostring(ret))
            end
        elseif type(block_body) == 'table' then
            for _, fn in ipairs(block_body) do
                if type(fn) == 'function' then
                    local success, ret = xpcall(fn, dsl_error_fn)
                    if not success then
                        local err_string = "Failed to apply function in body for "..block_args[1]
                        error(err_string .. ": ".. tostring(ret))
                    end
                end
            end
        end
    end)
end

-- Configuration metatable set on _G during DSL execution.
local gconfig_mt = {}
gconfig_mt.__index = function(self, key)
    -- If a directive exists, return a directive representation for the DSL.
    if CP:dir_exists(key) then

        -- Pointer to a callable object that takes no arguments
        -- and evaluates a DSL statement.
        -- If application is deferred, this is a function that calls
        -- a function. If evaluation is no deferred, this is
        -- a function that directly applies the directive.
        local dir_fn

        -- If the config is a block, some special processing is needed.
        if CP:is_block(key) then
            g_config_call_depth = g_config_call_depth + 1

            -- Function that will be immediately called by the DSL.
            -- Eg: Site('MySite')
            if g_config_call_depth <= 1 then
                dir_fn = function(...)
                    local block_args = {...}

                    -- After Site('MySite') is called, this function
                    -- will be implicitly called to handle the site body.
                    -- Eg: Site('MySite') [[ This is block_body. ]]
                    return function(block_body)
                        g_config_call_depth = g_config_call_depth - 1

                        -- Processing block_body requires a function that is
                        -- called bewteen block_start and block_process.
                        block_apply(key, block_args, block_body)
                    end
                end
            else
                dir_fn = function(...)
                    -- First function call captures block_args into a closure.
                    local block_args = {...}

                    -- Second function call captures block_body into a closure.
                    return function(block_body)
                        -- What is returned is the closure that execute witout arguments
                        -- because we've captured them.
                        return function()
                            g_config_call_depth = g_config_call_depth - 1

                            block_apply(key, block_args, block_body)
                        end
                    end
                end
            end
        else
            if g_config_call_depth < 1 then
                dir_fn = function(...)
                    CP:directive_process(key, { ... })
                end
            else
                dir_fn = function(...)
                    local closure_args = { ... }
                    return function()
                        CP:directive_process(key, closure_args)
                    end
                end
            end
        end

        return dir_fn
    end

    -- Unknown directive
    return nil
end

-- Setup the configuration DSL, run the function provided, tear down the DSL.
--
-- @param[in] f Function to run after the DSL is installed in _G.
-- @param[in] cp Configuration parser.
local DoInDSL = function(f, cp)
    local ib = ibapi.engineapi:new(ffi.cast("ib_cfgparser_t*", cp).ib)
    local ctx = ibapi.ctxapi:new(ffi.cast("ib_cfgparser_t*", cp).cur_ctx)

    local SkipTable = {
        __index = 1,
    }

    local to_nil = {}

    -- Setup _G's metatable.
    local prev_gconfig_mt = getmetatable(_G)
    setmetatable(_G, gconfig_mt)

    -- Setup all Waggle symbols.
    for k,v in pairs(Waggle) do
        if not SkipTable[k] then
            _G[k] = v
            table.insert(to_nil, k)
        end
    end

    -- Currently the Predicate naming convention is to ucfirst() and camel
    -- case functions that are sexpr based and to lowercase utility
    -- functions.  This is a bit unclear and it is planned to split Predicate
    -- namespaces into P and PUtil.  For now, create a wrapper to alias
    -- the functions and generate deprecation warnings to allow for this
    -- transition to occur before the namespace split.
    --
    -- TODO Remove this when P/PUtil is split.
    --
    _G['P'] = { }
    local p_mt = {
        __index = Predicate
    }
    setmetatable(_G['P'], p_mt)
    _G['PUtil'] = { }
    local putil_mt = {
        __index = Predicate.Util
    }
    setmetatable(_G['PUtil'], putil_mt)

    _G['IB'] = ib
    _G['CP'] = cfg_parser:new(cp)
    local succeeded, error_obj = pcall(f)

    if not succeeded then
        if 'table' == type(error_obj) then
            ib:logError(
                "Failed to eval Lua DSL for rule %s rev %d: %s",
                error_obj.sig_id,
                error_obj.sig_rev,
                error_obj.msg)
            error("Failed to eval Lua DSL. See preceeding message.")
        elseif 'string' == type(error_obj) then
            ib:logError("Failure evaluating Lua DSL: %s", error_obj)
            error("Failed to eval Lua DSL. See preceeding message.")
        else
            error("Unknown error running Lua configuration DSL.")
        end
    end

    -- Teardown.
    _G['P'] = nil
    _G['PUtil'] = nil
    _G['IB'] = nil
    _G['CP'] = nil
    for _, k in ipairs(to_nil) do
        _G[k] = nil
    end

    -- Teardown _G's metatable.
    setmetatable(_G, prev_gconfig_mt)
end

-- Begin the formal module definition.

local M   = {}
M.__index = M

---
-------------------------------------------------------------------
-- Include a configuration file.
--
-- Rules are not committed to the engine until the end of the
-- configuration phase.
--
-- @param[in] cp Configuraiton Parser.
-- @param[in] file String point to the file to operate on.
-------------------------------------------------------------------
M.includeFile = function(cp, file)
    local ib = ibapi.engineapi:new(ffi.cast("ib_cfgparser_t*", cp).ib)

    ib:logInfo("Including lua file %s", file)
    DoInDSL(loadfile(file), cp)

    return ffi.C.IB_OK
end

-------------------------------------------------------------------
-- Include a configuration string.
--
-- Rules are not committed to the engine until the end of the
-- configuration phase.
--
-- @param[in] cp Configuraiton Parser.
-- @param[in] configString String to treat as Lua configuration.
-------------------------------------------------------------------
M.includeString = function(cp, configString)
    local ib = ibapi.engineapi:new(ffi.cast("ib_cfgparser_t*", cp).ib)

    ib:logInfo("Including configuration.")
    DoInDSL(loadstring(configString), cp)

    return ffi.C.IB_OK
end

-------------------------------------------------------------------
-- Build and add all rules configured to the engine.
--
-- @tparam cdata[ib_engine_t*] ib_engine IronBee engine ib_engine_t*.
--
-- @return Status code.
-------------------------------------------------------------------
M.build_rules = require('ironbee/config/build_rule')

return M
