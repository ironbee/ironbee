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
        if field.type == 1 then
            self:log(7, "[fieldToLua]", "Casting field to number.")
            return ffi.cast("ib_num_t*", value)[0]

        -- Unsigned Number
        elseif field.type == 2 then
            self:log(7, "[fieldToLua]", "Casting field to unsigned number.")
            return ffi.cast("ib_unum_t*", value)[0]

        -- String
        elseif field.type == 3 then
            self:log(7, "[fieldToLua]", "Casting field to string.")
            return ffi.string(ffi.cast("char*", value))

        -- Byte String
        elseif field.type == 4 then
            self:log(7, "[fieldToLua]", "Casting field to byte string.")
            value = ffi.cast("ib_bytestr_t*", value)

            return ffi.string(ffi.C.ib_bytestr_ptr(value),
                              ffi.C.ib_bytestr_length(value))

        -- Lists - not handled.
        elseif field.type == 5 then
            self:log(7, "[fieldToLua]", "Unhandled type: List")
            return nil

        -- Stream buffers - not handled.
        elseif field.type == 6 then
            self:log(7, "[fieldToLua]", "Unhandled type: Stream buffer")
            return nil

        -- Anything else - not handled.
        else
            self:log(7, "[fieldToLua]", "Unhandled type: "..field.type)
            return nil
        end
   end


    -- The private logging function. This function should only be called
    -- by self:log_error(...) or self:log_debug(...) or the file and line
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
    ib_obj.log_error = function(self, msg, ...) 
        self.private:log(0, "LuaAPI - [ERROR]", msg, ...)
    end

    -- Log debug information at level 3.
    ib_obj.log_debug = function(self, msg, ...) 
        self.private:log(3, "LuaAPI - [DEBUG]", msg, ...)
    end

    -- Return a table of fields mapped to a string representation of their
    -- type (string, list, etc).
    ib_obj.fieldTypes = function(self)
      local fields = { }

      local ib_list = ffi.new("ib_list_t*[1]")
      ffi.C.ib_list_create(ib_list, self.private.ib_tx.mp)
      ffi.C.ib_data_get_all(self.private.ib_tx.dpi, ib_list[0])

      local ib_list_node = ffi.new("ib_list_node_t*",
                           ffi.C.ib_list_first(ib_list[0]))

      while ib_list_node ~= nil do
        local data = ffi.C.ib_list_node_data(ib_list_node)
        local field = ffi.cast("ib_field_t*", data)

        name = ffi.string(field.name, field.nlen)
        local type = nil

        if field.type == 1 then
          type = "NUMBER"
        elseif field.type == 2 then
          type = "UNSIGNED NUMBER"
        elseif field.type == 3 then
          type = "STRING"
        elseif field.type == 4 then
          type = "BYTE STRING"
        elseif field.type == 5 then
          type = "LIST"
        elseif field.type == 6 then
          type = "STREAM BUFFER"
        else
          type = "Unknown type "..field.type
        end

        fields[name] = type

        ib_list_node = ffi.C.ib_list_node_next(ib_list_node)
      end

      return fields 
    end

    -- Return a list of all the fields currently defined.
    ib_obj.listFields = function(self)
        local fields = { }
        local i = 1 -- list iterator

        local ib_list = ffi.new("ib_list_t*[1]")
        ffi.C.ib_list_create(ib_list, self.private.ib_tx.mp)
        ffi.C.ib_data_get_all(self.private.ib_tx.dpi, ib_list[0])

        local ib_list_node = ffi.new("ib_list_node_t*",
                             ffi.C.ib_list_first(ib_list[0]))

        while ib_list_node ~= nil do
            local data = ffi.C.ib_list_node_data(ib_list_node)
            local field = ffi.cast("ib_field_t*", data)

            fields[i] = ffi.string(field.name, field.nlen)

            i = i+1
        
            ib_list_node = ffi.C.ib_list_node_next(ib_list_node)
        end

        return fields 
    end

    -- Convert a list in the transaction's DPI into a table of strings.
    ib_obj.getList = function(self, listName)
        local list = {}
        local ib_field = self.private:getDpiField(listName)
        local ib_list  = ffi.cast("ib_list_t*", ffi.C.ib_field_value(ib_field))
        local ib_list_node = ffi.new("ib_list_node_t*",
                                     ffi.C.ib_list_first(ib_list))
        while ib_list_node ~= nil do
            -- Get the data.
            local data = ffi.cast("ib_field_t*", 
                                  ffi.C.ib_list_node_data(ib_list_node))

            -- Convert it to lua and append it to the table.
            list[#list+1] = self.private:fieldToLua(data)

            -- Next item.
            ib_list_node = ffi.C.ib_list_node_next(ib_list_node);
        end

        return list
    end

    -- Convert a list in the transaction's DPI into a table of key, values.
    -- If a name appears twice, the table returned will have a list of values
    -- for that entry.
    ib_obj.getTable = function(self, listName)
        local list = {}
        local ib_field = self.private:getDpiField(listName)
        local ib_list  = ffi.cast("ib_list_t*", ffi.C.ib_field_value(ib_field))
        local ib_list_node = ffi.new("ib_list_node_t*",
                                     ffi.C.ib_list_first(ib_list))
        while ib_list_node ~= nil do
            -- Get the data.
            local data = ffi.cast("ib_field_t*", 
                                  ffi.C.ib_list_node_data(ib_list_node))

            local name = ffi.string(data.name, data.nlen)

            if list[name] == nil then
                -- Convert it to lua and append it to the table.
                list[name] = self.private:fieldToLua(data)
            else
                -- Convert it to lua and append it to the table.
                list[name] = { [1] = list[name],
                               [2] = self.private:fieldToLua(data) }
            end

            -- Next item.
            ib_list_node = ffi.C.ib_list_node_next(ib_list_node);
        end

        return list
    end

    -- Append a value to the end of the name list. This may be a string
    -- or a number.
    ib_obj.appendList = function(self, listName, fieldName, fieldValue)

        local field = ffi.new("ib_field_t*[1]")
        local fieldType
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
            fieldType = ffi.C.IB_FTYPE_NUM
        else
            return
        end

        -- Fetch the list
        local list = self.private:getDpiField(listName)

        -- Append the field
        ffi.C.ib_field_list_add(list, field[0])
    end

    ib_obj.addList = function(self, listName)
        --local list = ffi.new("ib_list_t*[1]")
        --ffi.C.ib_list_create(list, self.private.ib_tx.mp)
        ffi.C.ib_data_add_list_ex(self.private.ib_tx.dpi,
                               ffi.cast("char*", listName),
                               #listName,
                               nil)
    end

    -- Set a string value in the transaction's DPI.
    ib_obj.setString = function(self, name, value)
        ffi.C.ib_data_add_nulstr_ex(
            self.private.ib_tx.dpi,
            ffi.cast("char*", name),
            string.len(name),
            ffi.cast("char*", value),
            nil)
    end

    -- Get a string value from the transaction's DPI.
    ib_obj.getString = function(self, name)
        local ib_field = self.private:getDpiField(name)
        return ffi.string(ffi.C.ib_field_value(ib_field))
    end

    return ib_obj
end


return ibapi
