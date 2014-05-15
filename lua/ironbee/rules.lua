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
-- IronBee - Rules API.
--
-- @module ironbee.rules
--
-- @copyright Qualys, Inc., 2010-2014
-- @license Apache License, Version 2.0
--
-- @author Sam Baskinger <sbaskinger@qualys.com>
-------------------------------------------------------------------

local ibutil = require('ironbee/util')
local ffi = require('ffi')
local tx = require('ironbee/tx')

local M = {}
M.__index = M

setmetatable(M, tx)

-------------------------------------------------------------------
-- Create a new rules API.
--
-- @tparam engine self Engine object.
-- @tparam cdata[ib_rule_exec_t*] ib_rule_exec IronBee rule execution environment.
-- @tparam cdata[ib_engine_t*] ib_engine IronBee engine.
-- @tparam cdata[ib_tx_t*] ib_tx IronBee transaction.
--
-- @return New Lua rules API.
-------------------------------------------------------------------
M.new = function(self, ib_rule_exec, ib_engine, ib_tx)
    local o = tx:new(ib_engine, ib_tx)

    -- Store raw C values.
    o.ib_rule_exec = ffi.cast("const ib_rule_exec_t*", ib_rule_exec)

    return setmetatable(o, self)
end

-------------------------------------------------------------------
-- Log a formatted error using the IronBee logger.
--
-- @tparam engine self Engine object.
-- @tparam string fmt Format string.
-- @param ... Arguments for the format string.
-------------------------------------------------------------------
M.logError = function(self, fmt, ...)
    self:log(ffi.C.IB_RULE_DLOG_ERROR, "LuaAPI - [ERROR] ", fmt, ...)
end

-------------------------------------------------------------------
-- Log a formatted warning using the IronBee logger.
--
-- @tparam engine self Engine object.
-- @tparam string fmt Format string.
-- @param ... Arguments for the format string.
-------------------------------------------------------------------
M.logWarn = function(self, fmt, ...)
    -- Note: Extra space after "WARN " is for text alignment.
    -- It should be there.
    self:log(ffi.C.IB_RULE_DLOG_WARNING, "LuaAPI - [WARN ] ", fmt, ...)
end

-------------------------------------------------------------------
-- Log a formatted informational message using the IronBee logger.
--
-- @tparam engine self Engine object.
-- @tparam string fmt Format string.
-- @param ... Arguments for the format string.
-------------------------------------------------------------------
M.logInfo = function(self, fmt, ...)
    -- Note: Extra space after "INFO " is for text alignment.
    -- It should be there.
    self:log(ffi.C.IB_RULE_DLOG_INFO, "LuaAPI - [INFO ] ", fmt, ...)
end

-------------------------------------------------------------------
-- Log a formatted debug message using the IronBee logger.
--
-- @tparam engine self Engine object.
-- @tparam string fmt Format string.
-- @param ... Arguments for the format string.
-------------------------------------------------------------------
M.logDebug = function(self, fmt, ...)
    self:log(ffi.C.IB_RULE_DLOG_DEBUG, "LuaAPI - [DEBUG] ", fmt, ...)
end

-------------------------------------------------------------------
-- Log a formatted message using the IronBee rule logger.
--
-- @tparam engine self Engine object.
-- @tparam number level Log level.
-- @tparam string prefix Log message prefix.
-- @tparam string fmt Format string.
-- @param ... Arguments for the format string.
-------------------------------------------------------------------
M.log = function(self, level, prefix, fmt, ...)
    local debug_table = debug.getinfo(3, "Sln")
    local file = debug_table.short_src
    local line = debug_table.currentline
    local func = debug_table.name
    local msg

    -- fmt must not be nil.
    fmt = tostring(fmt)

    -- If we have more arguments, format fmt with them.
    if ... ~= nil then
        local newmsg
        success, newmsg = pcall(string.format, fmt, ...)
        if success then
            msg = newmsg
        else
            error("Error formatting log message: "..newmsg .. ": ".. fmt)
        end
    end

    ffi.C.ib_rule_log_exec(level, self.ib_rule_exec, file, func, line, prefix .. (msg or fmt));
end

return M
