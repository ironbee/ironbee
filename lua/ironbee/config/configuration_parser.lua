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
-- IronBee - Configuration Parser API
--
-- IronBee configuration parser API.
--
-- @module ironbee.configuration_parser.
--
-- @copyright Qualys, Inc., 2010-2014
-- @license Apache License, Version 2.0
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
-------------------------------------------------------------------

local ibutil = require('ironbee/util')
local ffi = require('ffi')
local ibcutil = require('ibcutil')

local M = {}
M.__index = M

-------------------------------------------------------------------
-- Create a new Engine.
--
-- @tparam engine self Engine object.
-- @tparam cdata[ib_engine_t*] ib_engine IronBee engine.
--
-- @return New Lua engine API.
-------------------------------------------------------------------
M.new = function(self, ib_cp)
    -- Store raw C values.
    local o = {
        -- The raw IronBee configuration parser pointer.
        ib_cp = ffi.cast("ib_cfgparser_t *", ib_cp)
    }

    return setmetatable(o, self)
end

-- Apply a configuration directive given a configuration parser.
local config_directive_process = function(cp, directive, ...)

    local arg_list = ffi.new("ib_list_t *[1]")

    -- Put arguments in a list.
    local rc = ffi.C.ib_list_create(arg_list, cp.mm)
    if rc ~= ffi.C.IB_OK then
        error("Failed to create argument list object.")
    end

    for _, arg in ipairs({...}) do
        rc = ffi.C.ib_list_push(arg_list[0], ffi.cast("char *", arg))
        if rc ~= ffi.C.IB_OK then
            error("Failed to push string to arg list.")
        end
    end

    local rc = ffi.C.ib_config_directive_process(
        ffi.cast("ib_cfgparser_t *", cp),
        "DIRNAME",
        ib_list_dirlist
    )

    if (rc ~= ffi.C.IB_OK) then
        error("Failed to apply directive ".. name)
    end
end


-- Return true if the named directive exists.
--
-- @param[in] name The directive name.
--
-- @returns
-- - True if the directive exists.
-- - False otherwise.
M.dir_exists = function(self, name)
    return ffi.C.IB_OK == ffi.C.ib_config_directive_exists(self.ib_cp, name)
end

-- Return true if the named directive is a block directive.
--
-- @param[in] name The directive name.
--
-- @returns
-- - True if the type of the directive is IB_DIRTYPE_SBLK1.
-- - False otherwise.
M.is_block = function(self, name)
    local tp = ffi.new("ib_dirtype_t[1]")
    local rc = ffi.C.ib_config_directive_type(self.ib_cp, name, tp)

    if rc ~= ffi.C.IB_OK then
        return false
    end

    return tp[0] == ffi.C.IB_DIRTYPE_SBLK1
end

-- Take an ib_cfgparser_t* and a Lua list and build an ib_list_t of args.
-- The list is returned or error() is called on failure.
local build_arg_list = function(ib_cp, args)
    local ib_args = ffi.new("ib_list_t*[1]")

    local rc = ffi.C.ib_list_create(ib_args, ib_cp.mm)
    if rc ~= ffi.C.IB_OK then
        error("Failed to create argument list.")
    end

    for _, v in ipairs(args) do
        rc = ffi.C.ib_list_push(ib_args[0], ffi.cast("char *", v))
        if rc ~= ffi.C.IB_OK then
            error("Failed to build argument list: " .. tostring(v))
        end
    end

    return ib_args[0]
end

-- Call ib_config_directive_process on name(args).
--
-- @param[in] name The name of the directive to call.
-- @param[in] args A Lua list of string arguments to the directive.
--
-- Calls error() on failure.
M.directive_process = function(self, name, args)

    local ib_args = build_arg_list(self.ib_cp, args)

    rc = ffi.C.ib_config_directive_process(self.ib_cp, name, ib_args)
    if rc ~= ffi.C.IB_OK then
        error("Failed to process directive "..name)
    end
end

-- Call ib_config_block_{start,process} on name(args) with the given body function.
--
-- @param[in] name The name of the directive to call.
-- @param[in] args A Lua list of string arguments to the directive.
-- @param[in] body_fn A function to apply any directives that should occure
--            in between ib_config_block_start() and
--            ib_config_block_process().
--
-- Calls error() on failure.
M.block_process = function(self, name, args, body_fn)

    local ib_args = build_arg_list(self.ib_cp, args)

    rc = ffi.C.ib_config_block_start(self.ib_cp, name, ib_args)
    if rc ~= ffi.C.IB_OK then
        error("Failed to process directive "..name)
    end

    body_fn(self, name, args)

    rc = ffi.C.ib_config_block_process(self.ib_cp, name)
    if rc ~= ffi.C.IB_OK then
        error("Failed to process directive "..name)
    end
end

return M