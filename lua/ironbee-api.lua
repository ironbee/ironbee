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
--
-- Public API Documentation
--
--
-- Data Access
--
-- add(name, value) - add a string, number or table.
-- addEvent([msg], options) - Add a new event.
-- appendToList(list_name, name, value) - append a value to a list.
-- get(name) - return a string, number or table.
-- getFieldList() - Return a list of defined fields.
-- getNames(field) - Returns a list of names in this field.
-- getValues(field) - Returns a list of values in this field.
-- set(name, value) - set a string, number or table.
--
-- Logging
--
-- logError(format, ...) - Log an error message.
-- logInfo(format, ...) - Log an info message.
-- logDebug(format, ...)- Log a debug message.
-- 

-- Lua 5.2 and later style module.
ibapi = {}

local ffi = require("ffi")
local ironbee = require("ironbee-ffi")

-- Private utility functions for the API
local ibutil = {

    -- This is a replacement for __index in a table's metatable.
    -- It will, when receiving an index it does not have an entry for, 
    -- return the 'unknown' entry in the table.
    returnUnknown = function(self, key) 
        if key == 'unknown' then
            return nil
        else
            return self['unknown']
        end
    end
}

-- Iterate over the ib_list (of type ib_list_t*) calling the 
-- function func on each ib_field_t* contained in the elements of ib_list.
ibapi.each_list_node = function(ib_list, func)
    local ib_list_node = ffi.cast("ib_list_node_t*", 
                                  ffi.C.ib_list_first(ib_list))

    while ib_list_node ~= nil do
        -- Callback
        func(ffi.cast("ib_field_t*", ffi.C.ib_list_node_data(ib_list_node)))

        -- Next
        ib_list_node = ffi.C.ib_list_node_next(ib_list_node)
    end
end

-- Activity Map used by addEvent.
-- Default values is 'unknown'
ibapi.activityMap = {
    reconnaissance    = ffi.C.IB_LEVENT_ACT_RECON,
    recon             = ffi.C.IB_LEVENT_ACT_RECON,
    attempted_attack  = ffi.C.IB_LEVENT_ACT_ATTEMPTED_ATTACK,
    attempt           = ffi.C.IB_LEVENT_ACT_ATTEMPTED_ATTACK,
    successful_attack = ffi.C.IB_LEVENT_ACT_SUCCESSFUL_ATTACK,
    success           = ffi.C.IB_LEVENT_ACT_SUCCESSFUL_ATTACK,
    unknown           = ffi.C.IB_LEVENT_ACT_UNKNOWN
}
setmetatable(ibapi.activityMap, { __index = ibutil.returnUnknown })

-- Action Map used by addEvent.
-- Default values is 'unknown'
ibapi.actionMap = {
    block   = ffi.C.IB_LEVENT_ACTION_BLOCK,
    ignore  = ffi.C.IB_LEVENT_ACTION_IGNORE,
    log     = ffi.C.IB_LEVENT_ACTION_LOG,
    unknown = ffi.C.IB_LEVENT_ACTION_UNKNOWN
}
setmetatable(ibapi.actionMap, { __index = ibutil.returnUnknown })

-- Rule Type Map used by addEvent.
-- Default values is 'unknown'
ibapi.ruleTypeMap = {
    alert   = ffi.C.IB_LEVENT_TYPE_ALERT,
    unknown = ffi.C.IB_LEVENT_TYPE_UNKNOWN
}
setmetatable(ibapi.ruleTypeMap, { __index = ibutil.returnUnknown })

-- System Map used by addEvent.
-- Default values is 'unknown'
ibapi.systemMap = {
    public  = ffi.C.IB_LEVENT_SYS_PUBLIC,
    private = ffi.C.IB_LEVENT_SYS_PRIVATE,
    unknown = ffi.C.IB_LEVENT_SYS_UNKNOWN
}
setmetatable(ibapi.systemMap, { __index = ibutil.returnUnknown })

ibapi.primaryClassMap = {
    unknown = ffi.C.IB_LEVENT_PCLASS_UNKNOWN
}
setmetatable(ibapi.primaryClassMap, { __index = ibutil.returnUnknown })

ibapi.secondaryClassMap = {
    unknown = ffi.C.IB_LEVENT_SCLASS_UNKNOWN
}
setmetatable(ibapi.secondaryClassMap, { __index = ibutil.returnUnknown })

-- Create an new ironbee object using the given engine and transaction.
ibapi.new = function(self, ib_engine, ib_tx)
    -- Basic object
    ib_obj = {}

    setmetatable(ib_obj, { __index = self })

    -- The private API goes here. Users should not call these functions
    -- directly.
    ib_obj.private = {}

    -- Store raw C values.
    ib_obj.private.ib_engine = ffi.cast("ib_engine_t*", ib_engine)
    ib_obj.private.ib_tx = ffi.cast("ib_tx_t*", ib_tx)

    -- Return a ib_field_t* to the field named and stored in the DPI.
    -- This is used to quickly pull named fields for setting or getting values.
    ib_obj.private.getDpiField = function(self, name)
        local ib_field = ffi.new("ib_field_t*[1]")

        ffi.C.ib_data_get_ex(self.ib_tx.dpi,
                             name,
                             string.len(name),
                             ib_field)
        return ib_field[0]
    end

    -- Given an ib_field_t*, this will convert the data into a Lua type or
    -- nil if the value is not supported.
    ib_obj.private.fieldToLua = function(self, field)

        -- Get the value of the field. But we must cast and convert it.
        local value = ffi.C.ib_field_value(field)

        -- Number
        if field.type == ffi.C.IB_FTYPE_NUM then
            return tonumber(ffi.cast("const ib_num_t*", value)[0])

        -- Unsigned Number
        elseif field.type == ffi.C.IB_FTYPE_UNUM then
            return tonumber(ffi.cast("const ib_unum_t*", value)[0])

        -- String
        elseif field.type == ffi.C.IB_FTYPE_NULSTR then
            return ffi.string(ffi.cast("const char*", value))

        -- Byte String
        elseif field.type == ffi.C.IB_FTYPE_BYTESTR then
            value = ffi.cast("const ib_bytestr_t*", value)

            return ffi.string(ffi.C.ib_bytestr_const_ptr(value),
                              ffi.C.ib_bytestr_length(value))

        -- Lists
        elseif field.type == ffi.C.IB_FTYPE_LIST then
            local t = {}

            ibapi.each_list_node(
                ffi.cast("ib_list_t*", value),
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


    -- The private logging function. This function should only be called
    -- by self:logError(...) or self:logDebug(...) or the file and line
    -- number will not be accurage because the call stack will be at an
    -- unexpected depth.
    ib_obj.private.log = function(self, level, prefix, msg, ...) 
        local ctx = ffi.C.ib_context_main(self.ib_engine)
        local debug_table = debug.getinfo(3, "Sl")
        local file = debug_table.short_src
        local line = debug_table.currentline

        -- Msg must not be nil.
        if msg == nil then msg = "(nil)" end

        if type(msg) ~= 'string' then msg = tostring(msg) end

        -- If we have more arguments, format msg with them.
        if ... ~= nil then msg = string.format(msg, ...) end

        -- Log the string.
        ffi.C.ib_clog_ex(ctx, level, prefix, file, line, msg)
    end

    -- Log an error.
    ib_obj.logError = function(self, msg, ...) 
        self.private:log(0, "LuaAPI - [ERROR]", msg, ...)
    end

    -- Log an info message.
    ib_obj.logInfo = function(self, msg, ...) 
        -- Note: Extra space after "INFO " is for text alignment.
        -- It should be there.
        self.private:log(4, "LuaAPI - [INFO ]", msg, ...)
    end

    -- Log debug information at level 3.
    ib_obj.logDebug = function(self, msg, ...) 
        self.private:log(7, "LuaAPI - [DEBUG]", msg, ...)
    end

    -- Return a list of all the fields currently defined.
    ib_obj.getFieldList = function(self)
        local fields = { }

        local ib_list = ffi.new("ib_list_t*[1]")
        ffi.C.ib_list_create(ib_list, self.private.ib_tx.mp)
        ffi.C.ib_data_get_all(self.private.ib_tx.dpi, ib_list[0])

        ibapi.each_list_node(ib_list[0], function(field)
            fields[#fields+1] = ffi.string(field.name, field.nlen)
        
            ib_list_node = ffi.C.ib_list_node_next(ib_list_node)
        end)

        return fields 
    end

    -- Append a value to the end of the name list. This may be a string
    -- or a number. This is used by ib_obj.add to append to a list.
    ib_obj.appendToList = function(self, listName, fieldName, fieldValue)

        local field = ffi.new("ib_field_t*[1]")
        local cfieldValue

        -- This if block must define fieldType and cfieldValue
        if type(fieldValue) == 'string' then
            local fieldValue_p = ffi.new("char*[1]", 
                                         ffi.cast("char*", fieldValue))
            -- Create the field
            ffi.C.ib_field_create_ex(field,
                                     self.private.ib_tx.mp,
                                     ffi.cast("char*", fieldName),
                                     #fieldName,
                                     ffi.C.IB_FTYPE_NULSTR,
                                     fieldValue_p)

        elseif type(fieldValue) == 'number' then
            local fieldValue_p = ffi.new("ib_num_t[1]", fieldValue)

            ffi.C.ib_field_create_ex(field,
                                     self.private.ib_tx.mp,
                                     ffi.cast("char*", fieldName),
                                     #fieldName,
                                     ffi.C.IB_FTYPE_NUM,
                                     fieldValue_p)
        else
            return
        end

        -- Fetch the list
        local list = self.private:getDpiField(listName)

        -- Append the field
        ffi.C.ib_field_list_add(list, field[0])
    end

    -- Add a string, number, or table to the transaction data provider.
    -- If value is a string or a number, it is appended to the end of the
    -- list of values available through the data provider.
    -- If the value is a table, and the table exists in the data provider,
    -- then the values are appended to that table. Otherwise, a new
    -- table is created.
    ib_obj.add = function(self, name, value)
        if value == nil then
            -- nop.
        elseif type(value) == 'string' then
            ffi.C.ib_data_add_nulstr_ex(self.private.ib_tx.dpi,
                                        ffi.cast("char*", name),
                                        string.len(name),
                                        ffi.cast("char*", value),
                                        nil)
        elseif type(value) == 'number' then
            ffi.C.ib_data_add_num_ex(self.private.ib_tx.dpi,
                                     ffi.cast("char*", name),
                                     #name,
                                     value,
                                     nil)
        elseif type(value) == 'table' then
            local ib_field = ffi.new("ib_field_t*[1]")
            ffi.C.ib_data_get_ex(self.private.ib_tx.dpi,
                                 name,
                                 string.len(name),
                                 ib_field)
            
            -- If there is a value, but it is not a list, make a new table.
            if ib_field[0] == nil or 
               ib_field[0].type ~= ffi.C.IB_FTYPE_LIST then
                ffi.C.ib_data_add_list_ex(self.private.ib_tx.dpi,
                                          ffi.cast("char*", name),
                                          string.len(name),
                                          ib_field)
            end

            for k,v in ipairs(value) do
                self:appendToList(name, v[1], v[2])
            end
        else
            self:logError("Unsupported type %s", type(value))
        end
    end

    ib_obj.set = function(self, name, value)

        local ib_field = self.private:getDpiField(name)

        if ib_field == nil then
            -- If ib_field == nil, then it doesn't exist and we call add(...).
            -- It is not an error, if value==nil, to pass value to add.
            -- Adding nil is a nop.
            self:add(name, value)
        elseif value == nil then
            -- Delete values when setting a name to nil.
            ffi.C.ib_data_remove_ex(self.private.ib_tx.dpi,
                                    ffi.cast("char*", name),
                                    #name,
                                    nil)
        elseif type(value) == 'string' then
            -- Set a string.
            local nval = ffi.C.ib_mpool_strdup(self.private.ib_tx.mp,
                                               ffi.cast("char*", value))
            local nval_p = ffi.new("char*[1]", nval)
            ffi.C.ib_field_setv(ib_field, nval_p)
        elseif type(value) == 'number' then
            -- Set a number.
            local src = ffi.new("ib_num_t[1]", value)
            local dst = ffi.cast("ib_num_t*",
                                 ffi.C.ib_mpool_alloc(self.private.ib_tx.mp,
                                                      ffi.sizeof("ib_num_t")))
            ffi.copy(dst, src, ffi.sizeof("ib_num_t"))
            ffi.C.ib_field_setv(ib_field, dst)
        elseif type(value) == 'table' then
            -- Delete a table and add it.
            ffi.C.ib_data_remove_ex(self.private.ib_tx.dpi,
                                    ffi.cast("char*", name),
                                    #name,
                                    nil)
            self:add(name, value)
        else
            self:logError("Unsupported type %s", type(value))
        end
    end

    -- Get a value from the transaction's data provider instance.
    -- If that parameter points to a string, a string is returned.
    -- If name points to a number, a number is returned.
    -- If name points to a list of name-value pairs a table is returned
    --    where
    ib_obj.get = function(self, name)
        local ib_field = self.private:getDpiField(name)
        return self.private:fieldToLua(ib_field)
    end

    -- Given a field name, this will return a list of the field names
    -- contained in it. If the requested field is a string or an integer, then
    -- a single element list containing name is returned.
    ib_obj.getNames = function(self, name)
        local ib_field = self.private:getDpiField(name)

        -- To speed things up, we handle a list directly
        if ib_field.type == ffi.C.IB_FTYPE_LIST then
            local t = {}
            local ib_list = ffi.C.ib_field_value(ib_field)

            ibapi.each_list_node(ib_list, function(data)
                t[#t+1] = ffi.string(data.name, data.nlen)
            end)

            return t
        else
            return { ffi.string(ib_field.name, ib_field.nlen) }
        end
    end

    -- Given a field name, this will return a list of the values that are
    -- contained in it. If the requeted field is a string or an integer,
    -- then a single element list containing that value is returned.
    ib_obj.getValues = function(self, name)
        local ib_field = self.private:getDpiField(name)

        -- To speed things up, we handle a list directly
        if ib_field.type == ffi.C.IB_FTYPE_LIST then
            local t = {}
            local ib_list = ffi.C.ib_field_value(ib_field)

            ibapi.each_list_node(ib_list, function(data)
                t[#t+1] = self.private:fieldToLua(data)
            end)

            return t
        else
            return { self.private:fieldToLua(ib_field) }
        end
    end

    -- Add an event. 
    -- The msg argument is typically a string that is the message to log,
    -- followed by a table of options.
    --
    -- If msg is a table, however, then options is ignored and instead
    -- msg is processed as if it were the options argument. Think of this
    -- as the argument msg being optional.
    --
    -- If msg is omitted, then options should contain a key 'message' that
    -- is the message to log.
    --
    -- The options argument should also specify the following (or they will
    -- default to UNKNOWN):
    --
    -- recommended_action - The recommended action.
    --     - block
    --     - ignore
    --     - log
    --     - unknown (default)
    -- action - The action to take. Values are the same as recommended_action.
    -- actvity - The observed activity.
    --     - reconnaissance
    --     - recon
    --     - attempted_attack
    --     - attempt
    --     - successful_attack
    --     - success
    --     - unknown (default)
    -- type - The rule type that was matched.
    --     - alert
    --     - unknown (default)
    -- system - The class or type of system attacked.
    --     - public
    --     - private
    --     - unknown (default)
    -- primaryClass - The classification of attack.
    --     - unknown (default)
    -- secondaryClass - The classification of attack.
    --     - unknown (default)
    -- confidence - An integer. The default is 0.
    -- severity - An integer. The default is 0.
    -- message - If msg is not given, then this should be the alert message.
    --
    ib_obj.addEvent = function(self, msg, options)

        local message

        -- If msg is a table, then options are ignored.
        if type(msg) == 'table' then
            options = msg
            message = ffi.cast("char*", msg['message'] or 'no message')
        else
            message = ffi.cast("char*", msg)
        end

        if options == nil then
            options = {}
        end

        local event = ffi.new("ib_logevent_t*[1]")
        local rulename = ffi.cast("char*", options['rulename'] or 'anonymous')

        -- Map options
        local rec_action      = ibapi.actionMap[options.recommended_action]
        local action          = ibapi.actionMap[options.action]
        local activity        = ibapi.activityMap[options.activity]
        local rule_type       = ibapi.ruleTypeMap[options.type]
        local system          = ibapi.systemMap[options.system]
        local primary_class   = ibapi.primaryClassMap[options.primaryClass]
        local secondary_class = ibapi.secondaryClassMap[options.secondaryClass]
        local confidence      = options.confidence or 0
        local severity        = options.severity or 0

        
        ffi.C.ib_logevent_create(event,
                                 self.private.ib_tx.mp,
                                 rulename,
                                 rule_type,
                                 activity,
                                 primary_class,
                                 secondary_class,
                                 ffi.C.IB_LEVENT_SYS_UNKNOWN,
                                 rec_action,
                                 action,
                                 confidence,
                                 severity,
                                 message
                                )

        ffi.C.ib_event_add(self.private.ib_tx.epi, event[0])
    end

    return ib_obj
end


return ibapi
