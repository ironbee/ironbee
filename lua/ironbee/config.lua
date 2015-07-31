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
-- @copyright Qualys, Inc., 2010-2015
-- @license Apache License, Version 2.0
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
-------------------------------------------------------------------
local ibapi        = require('ironbee/api')
local ffi          = require('ffi')
local Waggle       = require('ironbee/waggle')
local Predicate    = require('ironbee/predicate')
local ConfigParser = require('ironbee/config/configuration_parser')

-- The g_config_call_depth is a "global" (file scoped) value
-- that tracks how many blocks we are "in" when doing configurations.
-- If a directive is evaluated in g_config_call_depth > 0,
-- the its execution is deferred by a sub-function call.
local g_config_call_depth = 0


-- This function does the work of considering the type
-- of block_body and applying it correctly.
local block_apply = function(directive_name, block_args, block_body)

    -- Processing block_body requires a function that is
    -- called bewteen block_start and block_process.
    CP:block_process(directive_name, block_args, function(cp)
        -- Block body processing.
        if type(block_body) == 'string' then
            local fn = loadstring(block_body)
            if fn then
                fn()
            else
                local msg = string.format(
                    "%s(%s): Failed to parse block body.",
                    directive_name,
                    table.concat(block_args, ","))
                error(msg)
            end
        elseif type(block_body) == 'function' then
            block_body()
        elseif type(block_body) == 'table' then
            for _, fn in ipairs(block_body) do
                if type(fn) == 'function' then
                    fn()
                end
            end
        end
    end)
end

-- Treat the given key as a directive in the Lua configuration DSL.
--
-- This function is the heart of the Lua configuration DLS. To modify
-- the Lua DSL effectively, one must understand what is going on in this function.
--
-- This function has two modes in which it operates.
-- - Immediate
-- - Deffered.
--
-- In immediate, `g_config_call_depth` is 0 and functions are returned that, when called,
-- immediately apply the configuration directive.
--
-- In deffered, `g_config_call_depth` is > 0 and functions are returned that only capture
-- arguments into closures and return a function that takes no arguments which will, when called,
-- apply the directive. This is expanded on in the next list:
--
-- This will always return a function.
-- - Directives - If g_config_call_depth has a value of 0 when this function is called,
--   then directives are assumed to be immediately executed. For non-block directives
--   a function is returned that will take the following arguments and dispatch them to
--   the C function ib_config_directive_process().
--   When g_config_call_depth is greater than 0, it is assumed that any symbol look up
--   is happening in a table body that will be passed to a DSL block directive.
--   In this situation a function is returned that only captures the arugments to a closure
--   which is then returns a function requiring no arguments to apply the directive.
--   This second function is evaluted by the block DSL function.
-- - Block Directives - If g_config_call_depth has a value of 0 when this function is called,
--   then the block directive is assumed to be executed immediately. A function
--   is returned that will capture the arguments to the block directive in a closure.
--   That function will return a function that takes a single argument, the block body.
--   If the block body is a string, it is passed to the lua function loadstring() and evaluted.
--   If the block is a table, it is assumed to be a list of directive closures (described
--   in the second half of the preceeding bullet point) and each one is called with
--   no arguments to apply the directive in the block context.
--
-- @param[in] cp A lua configuration parser object. This is typically the global CP.
-- @param[in] key The symbol presented the _G table as a key being looked up.
--            This is the directive name we are going to process.
--
-- @return A function that assists in building the DSL.
local handle_symbol_as_directive = function(cp, key)

    -- Pointer to a callable object that takes no arguments
    -- and evaluates a DSL statement.
    -- If application is deferred, this is a function that calls
    -- a function. If evaluation is no deferred, this is
    -- a function that directly applies the directive.
    local dir_fn

    -- If the config is a block, some special processing is needed.
    if cp:is_block(key) then
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
                cp:directive_process(key, { ... })
            end
        else
            dir_fn = function(...)
                local closure_args = { ... }
                return function()
                    cp:directive_process(key, closure_args)
                end
            end
        end
    end

    return dir_fn
end

-- Configuration metatable set on _G during DSL execution.
local gconfig_mt = {}
gconfig_mt.__index = function(self, key)
    -- If a directive exists, return a directive representation for the DSL.
    if CP:directive_exists(key) then
        success, ret = pcall(handle_symbol_as_directive, CP, key)

        if success then
            return ret
        else
            local dbinfo = debug.getinfo(2, 'lSn')
            IB:logError("%s @ %s:%s", tostring(ret), dbinfo.short_src, dbinfo.currentline)
            error(ret)
        end
    else
        -- Debug output.
        local dbinfo = debug.getinfo(2, 'lSn')
        local msg = string.format("Unknown directive: %s @ %s:%s", key, dbinfo.short_src, dbinfo.currentline)
        IB:logDebug(msg)
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
    _G['CP'] = ConfigParser:new(cp)
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
            error("Failed to eval Lua DSL: ".. error_obj)
        else
            error("Unknown error running Lua configuration DSL.")
        end
    end

    -- Teardown.
    _G['P']     = nil
    _G['PUtil'] = nil
    _G['IB']    = nil
    _G['CP']    = nil
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

    local fn = loadfile(file)

    if fn then
        DoInDSL(fn, cp)
    else
        error("Error accessing or parsing file "..file)
    end

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

    local fn = loadstring(configString)

    if fn then
        DoInDSL(fn, cp)
    else
        error("Failed to parse string.")
    end

    return ffi.C.IB_OK
end

-------------------------------------------------------------------
-- Include a configuration function.
--
-- Rules are not committed to the engine until the end of the
-- configuration phase.
--
-- @param[in] cp Configuraiton Parser.
-- @param[in] fn Function to execute in the DSL context.
-------------------------------------------------------------------
M.includeFunction = function(cp, fn)
    DoInDSL(fn, cp)

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
