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
-- forEachEvent(function(event)...) - Call the given function on each event.
--                                    See the Event Manipulation section.
--
-- Event Manipulation
-- An event object, such as one passed to a callback function by
-- forEachEvent is a special wrapper object.
--
-- event.raw - The raw C struct representing the current event.
-- event:getSeverity() - Return the number representing the severity.
-- event:getAction() - Return the integer representing the action.
-- event:getConfidence() - Return the number representing the confidence.
-- event:getRuleId() - Return the string representing the rule id.
-- event:getMsg() - Return the string representing the message.
-- event:getSuppress() - Return the string showing the suppression value.
--                       The returned values will be none, false_positive,
--                       replaced, incomplete, partial, or other.
-- event:setSuppress(value) - Set the suppression value. This is one of the
--                            very few values that may be changed in an event.
--                            Events are mostly immutable things.
--                            Allowed values are false_positive, replaced,
--                            incomplete, partial, or other.
-- event:forEachField(function(tag)...) - Pass each field, as a string, to the callback function.
-- event:forEachTag(function(tag)...) - Pass each tag, as a string, to the callback function.
--
-- Logging
--
-- logError(format, ...) - Log an error message.
-- logInfo(format, ...) - Log an info message.
-- logDebug(format, ...)- Log a debug message.
-- 

-- Lua 5.2 and later style class.
ibapi = {}

local ib_event = {}
ib_event.new = function(self, event)
    o = { raw = event }
    setmetatable(o, self)
    self.__index = self
    return o
end
-- String mapping table.
ib_event.suppressMap = {
    none           = ffi.C.IB_LEVENT_SUPPRESS_NONE,
    false_positive = ffi.C.IB_LEVENT_SUPPRESS_FPOS,
    replaced       = ffi.C.IB_LEVENT_SUPPRESS_REPLACED,
    incomplete     = ffi.C.IB_LEVENT_SUPPRESS_INC,
    partial        = ffi.C.IB_LEVENT_SUPPRESS_INC,
    other          = ffi.C.IB_LEVENT_SUPPRESS_OTHER
}
ib_event.suppressRmap = {}
-- Build reverse map.
for k,v in pairs(ib_event.suppressMap) do
    ib_event.suppressRmap[v] = k
end
ib_event.getSeverity = function(self)
    return self.raw.confidence
end
ib_event.getConfidence = function(self)
    return self.raw.confidence
end
ib_event.getAction = function(self)
    return self.raw.action
end
ib_event.getRuleId = function(self)
    return ffi.string(self.raw.rule_id)
end
ib_event.getMsg = function(self)
    return ffi.string(self.raw.msg)
end
ib_event.getSuppress = function(self)
    return ib_event.suppressRmap[self.raw.suppress]
end
-- On an event object set the suppression value using a number or name.
-- value - may be none, false_positive, replaced, incomplete, partial or other.
--         The value of none indicates that there is no suppression of the event.
ib_event.setSuppress = function(self, value)
    if type(value) == "number" then
            print("Setting number")
        self.raw.suppress = value
    else
        self.raw.suppress = ib_event.suppressMap[string.lower(value)] or 0
    end
end
ib_event.forEachField = function(self, func)
    if self.raw.fields ~= nil then
        ibapi.each_list_node(
            self.raw.fields,
            function(charstar)
                func(ffi.string(charstar))
            end,
            "char*")
    end
end
ib_event.forEachTag = function(self, func)
    if self.raw.tags ~= nil then
        ibapi.each_list_node(
            self.raw.tags,
            function(charstar)
                func(ffi.string(charstar))
            end,
            "char*")
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
-- If msg is omitted, then options should contain a key 'msg' that
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
-- type - The rule type that was matched.
--     - observation
--     - unknown (default)
-- confidence - An integer. The default is 0.
-- severity - An integer. The default is 0.
-- msg - If msg is not given, then this should be the alert message.
-- tags - List (table) of tag strings: { 'tag1', 'tag2', ... }
-- fields - List (table) of field name strings: { 'ARGS', ... }
--
ibapi.addEvent = function(self, msg, options)

    local message

    -- If msg is a table, then options are ignored.
    if type(msg) == 'table' then
        options = msg
        message = ffi.cast("char*", msg['msg'] or '-')
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
    local event_type      = ibapi.eventTypeMap[options.type]
    local confidence      = options.confidence or 0
    local severity        = options.severity or 0

    
    ffi.C.ib_logevent_create(event,
                             self.private.ib_tx.mp,
                             rulename,
                             event_type,
                             rec_action,
                             action,
                             confidence,
                             severity,
                             message
                            )

    -- Add tags
    if options.tags ~= nil then
        if type(options.tags) == 'table' then
            for k,v in ipairs(options.tags) do
                ffi.C.ib_logevent_tag_add(event[0], v)
            end
        end
    end

    -- Add field names
    if options.fields ~= nil then
        if type(options.fields) == 'table' then
            for k,v in ipairs(options.fields) do
                ffi.C.ib_logevent_field_add(event[0], v)
            end
        end
    end

    ffi.C.ib_event_add(self.private.ib_tx.epi, event[0])
end

-- Base Class for private functions in ibapi.
local ibapi_private = {}
ibapi_private.new = function(self, ib_rule_exec, ib_engine, ib_tx)
    o = {}
    setmetatable(o, self)
    self.__index = self

    -- Store raw C values.
    o.ib_rule_exec = ffi.cast("const ib_rule_exec_t*", ib_rule_exec)
    o.ib_engine = ffi.cast("ib_engine_t*", ib_engine)
    o.ib_tx = ffi.cast("ib_tx_t*", ib_tx)

    return o
end
-- Return a ib_field_t* to the field named and stored in the DPI.
-- This is used to quickly pull named fields for setting or getting values.
ibapi_private.getDpiField = function(self, name)
    local ib_field = ffi.new("ib_field_t*[1]")

    ffi.C.ib_data_get_ex(self.ib_tx.dpi,
                         name,
                         string.len(name),
                         ib_field)
    return ib_field[0]
end

-- Given an ib_field_t*, this will convert the data into a Lua type or
-- nil if the value is not supported.
ibapi_private.fieldToLua = function(self, field)

    -- Nil, guard against undefined fields.
    if field == nil then
        return nil
    -- Number
    elseif field.type == ffi.C.IB_FTYPE_NUM then
        local value = ffi.new("ib_num_t[1]")
        ffi.C.ib_field_value(field, value)
        return tonumber(value[0])

    -- Unsigned Number
    elseif field.type == ffi.C.IB_FTYPE_UNUM then
        local value = ffi.new("ib_unum_t[1]")
        ffi.C.ib_field_value(field, value)
        return tonumber(value[0])

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
        ibapi.each_list_node(
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


-- The private logging function. This function should only be called
-- by self:logError(...) or self:logDebug(...) or the file and line
-- number will not be accurage because the call stack will be at an
-- unexpected depth.
ibapi_private.log = function(self, level, prefix, msg, ...) 
    local debug_table = debug.getinfo(3, "Sl")
    local file = debug_table.short_src
    local line = debug_table.currentline

    -- Msg must not be nil.
    if msg == nil then msg = "(nil)" end

    if type(msg) ~= 'string' then msg = tostring(msg) end

    -- If we have more arguments, format msg with them.
    if ... ~= nil then msg = string.format(msg, ...) end

    -- Prepend prefix.
    msg = prefix .. " " .. msg

    -- Log the string.
    ffi.C.ib_rule_log_exec(level, self.ib_rule_exec, file, line, msg);
end


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

-- Iterate over the ib_list (of type ib_list_t *) calling the 
-- function func on each ib_field_t* contained in the elements of ib_list.
-- The resulting list data is passed to the callback function
-- as a "ib_field_t*"
ibapi.each_list_node = function(ib_list, func, cast_type)
    local ib_list_node = ffi.cast("ib_list_node_t*", 
                                  ffi.C.ib_list_first(ib_list))
    if cast_type == nil then
      cast_type = "ib_field_t*"
    end

    while ib_list_node ~= nil do
        -- Callback
        func(ffi.cast(cast_type, ffi.C.ib_list_node_data(ib_list_node)))

        -- Next
        ib_list_node = ffi.C.ib_list_node_next(ib_list_node)
    end
end

-- Action Map used by addEvent.
-- Default values is 'unknown'
ibapi.actionMap = {
    allow   = ffi.C.IB_LEVENT_ACTION_ALLOW,
    block   = ffi.C.IB_LEVENT_ACTION_BLOCK,
    ignore  = ffi.C.IB_LEVENT_ACTION_IGNORE,
    log     = ffi.C.IB_LEVENT_ACTION_LOG,
    unknown = ffi.C.IB_LEVENT_ACTION_UNKNOWN
}
setmetatable(ibapi.actionMap, { __index = ibutil.returnUnknown })

-- Event Type Map used by addEvent.
-- Default values is 'unknown'
ibapi.eventTypeMap = {
    observation = ffi.C.IB_LEVENT_TYPE_OBSERVATION,
    unknown     = ffi.C.IB_LEVENT_TYPE_UNKNOWN
}
setmetatable(ibapi.eventTypeMap, { __index = ibutil.returnUnknown })

-- Log an error.
ibapi.logError = function(self, msg, ...) 
    self.private:log(ffi.C.IB_RULE_DLOG_ERROR, "LuaAPI - [ERROR]", msg, ...)
end

-- Log a warning.
ibapi.logWarn = function(self, msg, ...) 
    -- Note: Extra space after "INFO " is for text alignment.
    -- It should be there.
    self.private:log(ffi.C.IB_RULE_DLOG_WARNING, "LuaAPI - [WARN ]", msg, ...)
end

-- Log an info message.
ibapi.logInfo = function(self, msg, ...) 
    -- Note: Extra space after "INFO " is for text alignment.
    -- It should be there.
    self.private:log(ffi.C.IB_RULE_DLOG_INFO, "LuaAPI - [INFO ]", msg, ...)
end

-- Log debug information at level 3.
ibapi.logDebug = function(self, msg, ...) 
    self.private:log(ffi.C.IB_RULE_DLOG_DEBUG, "LuaAPI - [DEBUG]", msg, ...)
end

-- Return a list of all the fields currently defined.
ibapi.getFieldList = function(self)
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

-- Add a string, number, or table to the transaction data provider.
-- If value is a string or a number, it is appended to the end of the
-- list of values available through the data provider.
-- If the value is a table, and the table exists in the data provider,
-- then the values are appended to that table. Otherwise, a new
-- table is created.
ibapi.add = function(self, name, value)
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

ibapi.set = function(self, name, value)

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
        ffi.C.ib_field_setv(ib_field, nval)
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
ibapi.get = function(self, name)
    local ib_field = self.private:getDpiField(name)
    return self.private:fieldToLua(ib_field)
end

-- Given a field name, this will return a list of the field names
-- contained in it. If the requested field is a string or an integer, then
-- a single element list containing name is returned.
ibapi.getNames = function(self, name)
    local ib_field = self.private:getDpiField(name)

    -- To speed things up, we handle a list directly
    if ib_field.type == ffi.C.IB_FTYPE_LIST then
        local t = {}
        local value = ffi.new("ib_list_t*[1]")
        ffi.C.ib_field_value(ib_field, value)
        local ib_list = value[0]

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
ibapi.getValues = function(self, name)
    local ib_field = self.private:getDpiField(name)

    -- To speed things up, we handle a list directly
    if ib_field.type == ffi.C.IB_FTYPE_LIST then
        local t = {}
        local value =  ffi.new("ib_list_t*[1]")
        ffi.C.ib_field_value(ib_field, value)
        local ib_list = value[0]

        ibapi.each_list_node(ib_list, function(data)
            t[#t+1] = self.private:fieldToLua(data)
        end)

        return t
    else
        return { self.private:fieldToLua(ib_field) }
    end
end

--
-- Call function func on each event in the current transaction.
--
ibapi.forEachEvent = function(self, func)
    local tx = self.private.ib_tx
    local list = ffi.new("ib_list_t*[1]")
    ffi.C.ib_event_get_all(self.private.ib_tx.epi, list)

    ibapi.each_list_node(
        list[0],
        function(event)
            func(ib_event:new(event))
        end ,
        "ib_logevent_t*")
end

-- Append a value to the end of the name list. This may be a string
-- or a number. This is used by ib_obj.add to append to a list.
ibapi.appendToList = function(self, listName, fieldName, fieldValue)

    local field = ffi.new("ib_field_t*[1]")
    local cfieldValue

    -- This if block must define fieldType and cfieldValue
    if type(fieldValue) == 'string' then
        -- Create the field
        ffi.C.ib_field_create(field,
                                 self.private.ib_tx.mp,
                                 ffi.cast("char*", fieldName),
                                 #fieldName,
                                 ffi.C.IB_FTYPE_NULSTR,
                                 ffi.cast("char*", fieldValue))

    elseif type(fieldValue) == 'number' then
        local fieldValue_p = ffi.new("ib_num_t[1]", fieldValue)

        ffi.C.ib_field_create(field,
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


-- Create an new ironbee object using the given engine and transaction.
ibapi.new = function(self, ib_rule_exec, ib_engine, ib_tx)
    -- Basic object
    ib_obj = {}
    setmetatable(ib_obj, { __index = self })
    self.__index = self

    -- The private API goes here. Users should not call these functions
    -- directly.
    ib_obj.private = ibapi_private:new(ib_rule_exec, ib_engine, ib_tx)


    return ib_obj
end


return ibapi
