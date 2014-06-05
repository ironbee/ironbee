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
-- IronBee - Engine API
--
-- IronBee engine API.
--
-- @module ironbee.engine
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
M.new = function(self, ib_engine)
    local o = {}

    -- Store raw C values.
    o.ib_engine = ib_engine

    return setmetatable(o, self)
end

-------------------------------------------------------------------
-- Convert an IronBee field to a lua type.
--
-- Given an ib_field_t*, this will convert the data into a Lua type or
-- nil if the value is not supported.
--
-- @tparam engine self Engine object.
-- @tparam cdata[ib_field_t*] field IronBee C field.
--
-- @return Value of the field as a Lua type.
-------------------------------------------------------------------
M.fieldToLua = function(self, field)

    -- Nil, guard against undefined fields.
    if field == nil then
        return nil
    -- Protect against structures without a type field.
    elseif not ffi.istype("ib_field_t*", field) then
        self:logError(
            "Cdata type  ib_field_t * exepcted. Got %s",
            tostring(field))
        return nil
    -- Number
    elseif field.type == ffi.C.IB_FTYPE_NUM then
        local value = ffi.new("ib_num_t[1]")
        ffi.C.ib_field_value(field, value)
        return tonumber(value[0])

    -- Time
    elseif field.type == ffi.C.IB_FTYPE_TIME then
        local value = ffi.new("ib_time_t[1]")
        ffi.C.ib_field_value(field, value)
        return tonumber(value[0])

    -- Float Number
    elseif field.type == ffi.C.IB_FTYPE_FLOAT then
        local value = ffi.new("ib_float_t[1]")
        ffi.C.ib_field_value(field, value)
        return ibcutil.from_ib_float(value);

    -- String
    elseif field.type == ffi.C.IB_FTYPE_NULSTR then
        local value = ffi.new("const char*[1]")
        ffi.C.ib_field_value(field, value)
        return ffi.string(value[0])

    -- Byte String
    elseif field.type == ffi.C.IB_FTYPE_BYTESTR then
        local value = ffi.new("const ib_bytestr_t*[1]")
        ffi.C.ib_field_value(field, value)
        return ffi.string(ffi.C.ib_bytestr_const_ptr(value[0]),
                          ffi.C.ib_bytestr_length(value[0]))

    -- Lists
    elseif field.type == ffi.C.IB_FTYPE_LIST then
        local t = {}
        local value = ffi.new("ib_list_t*[1]")

        ffi.C.ib_field_value(field, value)
        ibutil.each_list_node(
            value[0],
            function(data)
                t[#t+1] = { ffi.string(data.name, data.nlen),
                            self:fieldToLua(data) }
            end)

        return t

    -- Stream buffers - not handled.
    elseif field.type == ffi.C.IB_FTYPE_SBUFFER then
        return nil

    -- Anything else - not handled.
    else
        return nil
    end
end

-------------------------------------------------------------------
-- Log a formatted error using the IronBee logger.
--
-- @tparam engine self Engine object.
-- @tparam string fmt Format string.
-- @param ... Arguments for the format string.
-------------------------------------------------------------------
M.logError = function(self, fmt, ...)
    self:log(ffi.C.IB_LOG_ERROR, "LuaAPI - [ERROR] ", fmt, ...)
end

-------------------------------------------------------------------
-- Log a formatted warning using the IronBee logger.
--
-- @tparam engine self Engine object.
-- @tparam string fmt Format string.
-- @param ... Arguments for the format string.
-------------------------------------------------------------------
M.logWarn = function(self, fmt, ...)
    -- Note: Extra space after "INFO " is for text alignment.
    -- It should be there.
    self:log(ffi.C.IB_LOG_WARNING, "LuaAPI - [WARN ] ", fmt, ...)
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
    self:log(ffi.C.IB_LOG_INFO, "LuaAPI - [INFO ] ", fmt, ...)
end

-------------------------------------------------------------------
-- Log a formatted debug message using the IronBee logger.
--
-- @tparam engine self Engine object.
-- @tparam string fmt Format string.
-- @param ... Arguments for the format string.
-------------------------------------------------------------------
M.logDebug = function(self, fmt, ...)
    self:log(ffi.C.IB_LOG_DEBUG, "LuaAPI - [DEBUG] ", fmt, ...)
end

-------------------------------------------------------------------
-- Log a formatted message using the IronBee logger.
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

    ffi.C.ib_log_ex(self.ib_engine, level, file, func, line, "%s", prefix .. (msg or fmt));
end

-------------------------------------------------------------------
-- Return a function that executes an operator instance.
--
-- @tparam engine self Engine object.
-- @tparam string name The name of the operator.
-- @tparam string param The parameter to pass the operator.
-- @tparam number flags The flags to pass the operator.
--
-- @return A function that takes a cdata[ib_rule_exec_t*] and a cdata[ib_field_t*].
--   If the ib_rule_exec_t is nil, then the ib_operator_t this
--   wraps is destroyed cleanly. Otherwise, that operator is executed.
--   The returned function, when executed, returns 2 values.
--   First, an ib_status_t value, normally IB_OK. The second
--   value is the result of the operator execution or 0 when the
--   operator is destroyed (tx was equal to nil).
-------------------------------------------------------------------
M.operator = function(self, name, param, flags)
    local op = ffi.new('ib_rule_operator_t*[1]')
    local inst = ffi.new('void*[1]')
    local rc = ffi.C.ib_operator_lookup(self.ib_engine, name, op)
    if rc ~= ffi.C.IB_OK then
        self:logError("Failed to lookup operator %s(%d).", name, tonumber(rc))
        return nil
    end
    local rc = ffi.C.ib_operator_inst_create(
        op[0],
        ffi.C.ib_context_main(self.ib_engine),
        flags,
        param,
        inst)
    if rc ~= ffi.C.IB_OK then
        rc = tonumber(rc)
        self:logError("Failed to create operator %s(%d):%s.", name, rc, param);
        return nil
    end

    return function(tx, field)
        if tx == nil then
            ffi.C.ib_operator_inst_destroy(op[0], inst[0])
            return ffi.C.IB_OK, 0
        else
            local res = ffi.new('ib_num_t[1]')
            local rc = ffi.C.ib_operator_inst_execute(
                op[0],
                inst[0],
                tx,
                field, -- input field
                nil,   -- capture field
                res)
            return tonumber(rc), tonumber(res[0])
        end
    end
end

-------------------------------------------------------------------
-- Return a function that executes a stream operator instance.
--
-- @tparam engine self Engine object.
-- @tparam string name The name of the stream operator.
-- @tparam string param The parameter to pass the stream operator.
-- @tparam number flags The flags to pass the stream operator.
--
-- @return A function that takes a cdata[ib_rule_exec_t*] and a cdata[ib_field_t*].
--   If the ib_rule_exec_t is nil, then the ib_operator_t this
--   wraps is destroyed cleanly. Otherwise, that stream operator is executed.
--   The returned function, when executed, returns 2 values.
--   First, an ib_status_t value, normally IB_OK. The second
--   value is the result of the stream operator execution or 0 when the
--   stream operator is destroyed (tx was equal to nil).
-------------------------------------------------------------------
M.stream_operator = function(self, name, param, flags)
    local op = ffi.new('ib_rule_operator_t*[1]')
    local inst = ffi.new('void*[1]')
    local rc = ffi.C.ib_operator_stream_lookup(self.ib_engine, name, op)
    if rc ~= ffi.C.IB_OK then
        self:logError("Failed to lookup operator %s(%d).", name, tonumber(rc))
        return nil
    end
    local rc = ffi.C.ib_operator_inst_create(
        op[0],
        ffi.C.ib_context_main(self.ib_engine),
        flags,
        param,
        inst)
    if rc ~= ffi.C.IB_OK then
        rc = tonumber(rc)
        self:logError("Failed to create operator %s(%d):%s.", name, rc, param);
        return nil
    end

    return function(tx, field)
        if tx == nil then
            ffi.C.ib_operator_inst_destroy(op[0], inst[0])
            return ffi.C.IB_OK, 0
        else
            local res = ffi.new('ib_num_t[1]')
            local rc = ffi.C.ib_operator_inst_execute(
                op[0],
                inst[0],
                tx,
                field, -- input field
                nil,   -- capture field
                res)
            return tonumber(rc), tonumber(res[0])
        end
    end
end

-------------------------------------------------------------------
-- Return a function that executes an action instance.
--
-- @tparam engine self Engine object.
-- @tparam context context Context object.
-- @tparam string name Name of the action.
-- @tparam string param Parameter to pass to the action.
-- @tparam number flags Flags passed to the action createion.
--
-- @return A function that takes 1 parameter.
--   If the parameter is a cdata[ib_rule_exec_t*], then the
--   action is evaluated. If parameter is nil,
--   then the action is destroyed cleanly.
-------------------------------------------------------------------
M.action = function(self, context, name, param, flags)
    local inst = ffi.new('ib_action_inst_t*[1]')
    local rc = ffi.C.ib_action_inst_create(
        context,
        name,
        param,
        flags,
        inst)
    if rc ~= ffi.C.IB_OK then
        rc = tonumber(rc)
        self:logError("Failed to create action %s(%d):%s.", name, rc, param);
        return nil
    end

    return function(rule_exec)
        if rule_exec == nil then
            ffi.C.ib_action_inst_destroy(inst[0])
            return tonumber(ffi.C.IB_OK)
        else
            return tonumber(ffi.C.ib_action_execute(rule_exec, inst[0]))
        end
    end
end


-------------------------------------------------------------------
-- Call a configuration directive.
--
-- @tparam engine self Engine object.
-- @tparam string name Name of directive.
-- @param ... Arguments of directive (will be converted to strings).
-------------------------------------------------------------------
M.config_directive_process = function(self, name, ...)
    if CP == nil then
      self:logError(
          "config_directive_process() called in environment without CP."
      )
      return nil
    end
    local args = ffi.new("ib_list_t*[1]")
    rc = ffi.C.ib_list_create(
        args,
        ffi.C.ib_engine_mm_main_get(self.ib_engine)
    )
    if rc ~= ffi.C.IB_OK then
        self:logError("Failed to create new cdata<ib_list_t>.")
        return rc
    end

    for _,v in ipairs({...}) do
        ffi.C.ib_list_push(args[0], ffi.cast("char*", tostring(v)))
    end

    rc = ffi.C.ib_config_directive_process(
        ffi.cast("ib_cfgparser_t*", CP),
        ffi.cast("char*", name),
        args[0]
    )
    if rc ~= ffi.C.IB_OK then
        self:logError(
            "Failed to execute directive %s",
            name
        )
    end
    return rc
end

-------------------------------------------------------------------
-- Engine version string.
--
-- @tparam engine self Engine object.
-- @treturn string Engine version string.
-------------------------------------------------------------------
M.version = function(self)
    local c_str = ffi.C.ib_engine_version()
    if c_str == nil then
        return nil
    end

    return ffi.string(c_str)
end

-------------------------------------------------------------------
-- Engine product name.
--
-- @tparam engine self Engine object.
--
-- @treturn string Engine product name.
-------------------------------------------------------------------
M.product_name = function(self)
    local c_str = ffi.C.ib_engine_product_name()
    if c_str == nil then
        return nil
    end

    return ffi.string(c_str)
end

-------------------------------------------------------------------
-- Engine version number.
--
-- @tparam engine self Engine object.
--
-- @treturn number Engine version number.
-------------------------------------------------------------------
M.version_number = function(self)
    return tonumber(ffi.C.ib_engine_version_number())
end

-------------------------------------------------------------------
-- Engine ABI number.
--
-- @tparam engine self Engine object.
--
-- @treturn number Engine ABI number.
-------------------------------------------------------------------
M.abi_number = function(self)
    return tonumber(ffi.C.ib_engine_abi_number())
end

return M

