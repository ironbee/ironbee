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

-- Configuration metatable set on _G during DSL execution.
local gconfig_mt = {}
function gconfig_mt:__index(key)
end

-- Setup the configuration DLS, run the function provided, tear down the DSL.
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
    _G['CP'] = cp
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
            ib:logError("Failure evaluating Lua DLS: %s", error_obj)
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
