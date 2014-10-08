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
    local o = { ib_cp = ib_cp }

    return setmetatable(o, self)
end

-- Apply a configuration directive given a configuration parser.
local config_directive_process = function(cp, directive, ...)
    local rc = ffi.C.ib_config_directive_process(
        ffi.cast("ib_cfgparser_t *", cp),
        "DIRNAME",
        ib_list_dirlist
    )

    if (rc ~= ffi.C.IB_OK) then
        error("Failed to apply directive ".. name)
    end
end



return M