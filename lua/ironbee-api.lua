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
--
-- Author: Sam Baskinger <sbaskinger@qualys.com>
--
-- =========================================================================


-- Lua 5.2 and later style module.
ibapi = {}

local ffi = require("ffi")
local ironbee = require("ironbee-ffi")

-- Create an new ironbee object using the given engine and transaction.
ibapi.new = function(ib_engine, ib_tx)
    -- Basic object
    ib_obj = {}

    -- The private API goes here. Users should not call these functions
    -- directly.
    ib_obj.private = {}

    -- Store raw C values.
    ib_obj.private.ib_engine = ffi.cast("ib_engine_t*", ib_engine)
    ib_obj.private.ib_tx = ffi.cast("ib_tx_t*", ib_tx)

    -- The private logging function. This function should only be called
    -- by self:log_error(...) or self:log_debug(...) or the file and line
    -- number will not be accurage because the call stack will be at an
    -- unexpected depth.
    ib_obj.private.log = function(self, level, prefix, msg, ...) 
        local ctx = ffi.C.ib_context_main(self.ib_engine)
        local debug_table = debug.getinfo(3, "Sl")
        local file = debug_table.short_src
        local line = debug_table.currentline
        ffi.C.ib_clog_ex(ctx, level, prefix, file, line, msg, ...)
    end

    -- Log an error.
    ib_obj.log_error = function(self, msg, ...) 
        self.private:log(0, "LuaAPI - [ERROR]", msg, ...)
    end

    -- Log debug information at level 3.
    ib_obj.log_debug = function(self, msg, ...) 
        self.private:log(3, "LuaAPI - [DEBUG]", msg, ...)
    end

    ib_obj.setString = function(self, name, value)
        ffi.C.ib_data_add_nulstr_ex(
            self.private.ib_tx.dpi,
            ffi.cast("char*", name),
            string.len(name),
            ffi.cast("char*", value),
            nil)
    end

    ib_obj.getString = function(self, name)
        local ib_field = ffi.new("ib_field_t*[1]")

        ffi.C.ib_data_get_ex(self.private.ib_tx.dpi,
                             name,
                             string.len(name),
                             ib_field)

        return ffi.string(ffi.C.ib_field_value(ib_field[0]))
    end

    return ib_obj
end


return ibapi
