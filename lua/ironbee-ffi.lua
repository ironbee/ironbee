-- =========================================================================
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
-- =========================================================================
-- =========================================================================
--
-- This module allows accessing the IronBee API via luajit FFI. It is
-- loaded through ironbee, and should not be used directly.
--
-- Author: Brian Rectanus <brectanus@qualys.com>
-- =========================================================================

local base = _G
local modules = package.loaded
local ffi = require("ffi")
local debug = require("debug")
local string = require("string")

module("ironbee-ffi")

-- Mark this lib as preloaded
base.package.preload["ironbee-ffi"] = _M

-- ===============================================
-- Setup some module metadata.
-- ===============================================
_COPYRIGHT = "Copyright (C) 2010-2013 Qualys, Inc."
_DESCRIPTION = "IronBee API via luajit FFI"
_VERSION = "0.2"

-- ===============================================
-- Setup the IronBee C definitions.
-- ===============================================
base.require('ironbee-ffi-h')

-- Cache lookup of ffi.C
local c = ffi.C

-- 
-- =========================================================================
-- =========================================================================
-- Implementation Notes:
--   * A "l_" prefix is used here to denote a Lua type.
--
--   * A "c_" prefix is used here to denote a C type.
--
--   * A C type still uses zero based indexes.
--
--   * To create a container for outvars, create a single value array:
--
--       c_pfoo = ffi.new("ib_foo_t *[1]"
--
--     Which, in C, is:
--
--       ib_foo_t **pfoo;
--
--   * Dereference via the first index (which, again, is 0, not 1 in C):
--
--       c_foo = c_pfoo[0]
--
--     Which, in C, is:
--
--       foo = *pfoo;
--
--   * Use "local" for function vars or they will be added to the module
--     table, polluting the namespace.
--
-- =========================================================================
-- =========================================================================
-- 

-- ===============================================
-- Status
-- ===============================================
IB_OK            = ffi.cast("int", c.IB_OK)
IB_DECLINED      = ffi.cast("int", c.IB_DECLINED)
IB_EUNKNOWN      = ffi.cast("int", c.IB_EUNKNOWN)
IB_ENOTIMPL      = ffi.cast("int", c.IB_ENOTIMPL)
IB_EINCOMPAT     = ffi.cast("int", c.IB_EINCOMPAT)
IB_EALLOC        = ffi.cast("int", c.IB_EALLOC)
IB_EINVAL        = ffi.cast("int", c.IB_EINVAL)
IB_ENOENT        = ffi.cast("int", c.IB_ENOENT)
IB_ETRUNC        = ffi.cast("int", c.IB_ETRUNC)
IB_ETIMEDOUT     = ffi.cast("int", c.IB_ETIMEDOUT)
IB_EAGAIN        = ffi.cast("int", c.IB_EAGAIN)
IB_EOTHER        = ffi.cast("int", c.IB_EOTHER)
IB_EBADVAL       = ffi.cast("int", c.IB_EBADVAL)
IB_EEXIST        = ffi.cast("int", c.IB_EEXIST)

-- ===============================================
-- Field Types
-- ===============================================
IB_FTYPE_GENERIC = ffi.cast("int", c.IB_FTYPE_GENERIC)
IB_FTYPE_NUM     = ffi.cast("int", c.IB_FTYPE_NUM)
IB_FTYPE_FLOAT   = ffi.cast("int", c.IB_FTYPE_FLOAT)
IB_FTYPE_NULSTR  = ffi.cast("int", c.IB_FTYPE_NULSTR)
IB_FTYPE_BYTESTR = ffi.cast("int", c.IB_FTYPE_BYTESTR)
IB_FTYPE_LIST    = ffi.cast("int", c.IB_FTYPE_LIST)
IB_FTYPE_SBUFFER = ffi.cast("int", c.IB_FTYPE_SBUFFER)

-- ===============================================
-- Directive Types
-- ===============================================
IB_DIRTYPE_ONOFF = ffi.cast("int", c.IB_DIRTYPE_ONOFF)
IB_DIRTYPE_PARAM1 = ffi.cast("int", c.IB_DIRTYPE_PARAM1)
IB_DIRTYPE_PARAM2 = ffi.cast("int", c.IB_DIRTYPE_PARAM2)
IB_DIRTYPE_LIST = ffi.cast("int", c.IB_DIRTYPE_LIST)
IB_DIRTYPE_SBLK1 = ffi.cast("int", c.IB_DIRTYPE_SBLK1)

-- ===============================================
-- Log Event Definitions
-- ===============================================
IB_LEVENT_TYPE_UNKNOWN = ffi.cast("int", c.IB_LEVENT_TYPE_UNKNOWN)
IB_LEVENT_TYPE_OBSERVATION = ffi.cast("int", c.IB_LEVENT_TYPE_OBSERVATION)
IB_LEVENT_ACTION_UNKNOWN = ffi.cast("int", c.IB_LEVENT_ACTION_UNKNOWN)
IB_LEVENT_ACTION_LOG = ffi.cast("int", c.IB_LEVENT_ACTION_LOG)
IB_LEVENT_ACTION_BLOCK = ffi.cast("int", c.IB_LEVENT_ACTION_BLOCK)
IB_LEVENT_ACTION_IGNORE = ffi.cast("int", c.IB_LEVENT_ACTION_IGNORE)
IB_LEVENT_ACTION_ALLOW = ffi.cast("int", c.IB_LEVENT_ACTION_ALLOW)

-- ===============================================
-- Cast a value as a C "ib_conn_t *".
-- ===============================================
function cast_conn(val)
    return ffi.cast("ib_conn_t *", val);
end

-- ===============================================
-- Cast a value as a C "ib_conndata_t *".
-- ===============================================
function cast_conndata(val)
    return ffi.cast("ib_conndata_t *", val);
end

-- ===============================================
-- Cast a value as a C "ib_tx_t *".
-- ===============================================
function cast_tx(val)
    return ffi.cast("ib_tx_t *", val);
end

-- ===============================================
-- Cast a value as a C "ib_txdata_t *".
-- ===============================================
function cast_txdata(val)
    return ffi.cast("ib_txdata_t *", val);
end

-- ===============================================
-- Cast a value as a C "int".
-- ===============================================
function cast_int(val)
    return ffi.cast("int", val);
end

-- ===============================================
-- Debug Functions.
-- ===============================================
function ib_util_log_debug(fmt, ...)
    local dinfo = debug.getinfo(2)

    c.ib_util_log_ex(7,
                     dinfo.source, dinfo.linedefined, fmt, ...)
end

function ib_log_debug(ib, fmt, ...)
    local dinfo = debug.getinfo(2)
    c.ib_log_ex(ib.cvalue(), 7,
                dinfo.source, dinfo.linedefined, fmt, ...)
end

function ib_log(ib, lvl, fmt, ...)
    c.ib_log_ex(ib.cvalue(), lvl, nil, 0, fmt, ...)
end

function ib_log_error(ib, fmt, ...)
    c.ib_log_ex(ib.cvalue(), 3, nil, 0, fmt, ...)
end

-- ===============================================
-- Lua OO Wrappers around IronBee raw C types
-- TODO: Add metatable w/__tostring for each type
-- ===============================================
function newMpool(val)
    local c_val = ffi.cast("ib_mpool_t *", val)
    return {
        cvalue = function() return c_val end,
    }
end
function newProvider(val)
    local c_val = ffi.cast("ib_provider_t *", val)
    return {
        cvalue = function() return c_val end,
    }
end

function newProviderInst(val)
    local c_val = ffi.cast("ib_provider_inst_t *", val)
    return {
        cvalue = function() return c_val end,
    }
end

function newData(val)
    local c_val = ffi.cast("ib_data_t *", val)
    return {
        cvalue = function() return c_val end,
    }
end

function newContext(val)
    local c_val = ffi.cast("ib_context_t *", val)
    return {
        cvalue = function() return c_val end,
    }
end

function newField(val)
    local c_val = ffi.cast("ib_field_t *", val)
    local c_list
    local t = {
        cvalue = function() return c_val end,
        type = function() return ffi.cast("int", c_val.type) end,
        name = function() return ffi.string(c_val.name, c_val.nlen) end,
        nlen = function() return ffi.cast("size_t", c_val.nlen) end,
    }
    -- TODO: Add metatable w/__tostring for each sub-type
    if c_val.type == c.IB_FTYPE_BYTESTR then
        t["value"] = function()
            c_fval = ffi.cast("const ib_bytestr_t *", c.ib_field_value(c_val))
            return ffi.string(c.ib_bytestr_const_ptr(c_fval), c.ib_bytestr_length(c_fval))
        end
    elseif c_val.type == c.IB_FTYPE_LIST then
        c_list = ffi.cast("ib_list_t *", c.ib_field_value(c_val))
        t["value"] = function()
            -- Loop through and create a table of fields to return
            local l_vals = {}
            local c_node = c.ib_list_first(c_list)
            while c_node ~= nil do
                local c_f = ffi.cast("ib_field_t *", c.ib_list_node_data(c_node))
                local l_fname = ffi.string(c_f.name, c_f.nlen)
                l_vals[l_fname] = newField(c_f)
                c_node = c.ib_list_node_next(c_node)
            end
            return l_vals
        end
--         setmetatable(t, {
--             __newindex = function (t, k, v)
--                 error("attempt to modify a read-only field", 2)
--             end,
--     --
--     -- TODO This may end up not working with non-numeric indexes as
--     --      there could be a name clash (ie myfield["size"] would
--     --      just execute the size() here and not find the "size"
--     --      field???  And numeric indexes are not that useful. So
--     --      better may be a subfield() method???
--     --
--             __index = function (t, k)
--                 -- Be very careful with indexes as there is no protection
--                 local ktype = base.type(k)
--                 if ktype ~= "number" or ktype <= 0 then
--                     -- TODO Instead loop through returning table of matching fields
--                     error("invalid index \"" .. k .. "\"", 2)
--                 end
--                 if c_val.type == c.IB_FTYPE_LIST then
--                     local c_idx;
--                     local size = t.size()
--                     local c_node
--                     -- c_list is now available after calling size()
--                     if k > t.size() then
--                         error("index is too large: " .. k, 2)
--                     end
--                     c_idx = ffi.cast("size_t", k)
--                     -- c_node = c.ib_list_node(c_list, c_idx)
--                     -- return newField(ffi.cast("ib_field_t *", c.ib_list_node_data(c_node)))
--                 elseif k ~= 1 then
--                     -- Any type has one value
--                     return t.value()
--                 end
--             end,
--         })
    elseif c_val.type == c.IB_FTYPE_NULSTR then
        t["value"] = function()
            local c_fval = ffi.cast("const char *", c.ib_field_value(c_val))
            return ffi.string(c_fval[0])
        end
    elseif c_val.type == c.IB_FTYPE_NUM then
        t["value"] = function()
            local c_fval = ffi.cast("ib_num_t *", c.ib_field_value(c_val))
            return base.tonumber(c_fval[0])
        end
    end

    return t
end

function newLogevent(val)
    local c_val = ffi.cast("ib_logevent_t *", val)
    return {
        cvalue = function() return c_val end,
    }
end

function newEngine(val)
    local c_val = ffi.cast("ib_engine_t *", val)
    return {
        cvalue = function() return c_val end,
    }
end

function newConnData(val)
    local c_val = ffi.cast("ib_conndata_t *", val)
    return {
        cvalue = function() return c_val end,
        dlen = function() return ffi.cast("size_t", c_val.dlen) end,
        data = function() return c_val.data end,
    }
end

function newConn(val)
    local c_val = ffi.cast("ib_conn_t *", val)
    return {
        cvalue = function() return c_val end,
        mp = function() return newMpool(c_val.mp) end,
        ib = function() return newEngine(c_val.ib) end,
        ctx = function() return newContext(c_val.ctx) end,
        data = function() return newData(c_val.data) end,
        tx_count = function() return ffi.cast("size_t", c_val.tx_count) end,
    }
end

function newTxData(val)
    local c_val = ffi.cast("ib_txdata_t *", val)
    return {
        cvalue = function() return c_val end,
        dtype = function() return ffi.cast("int", c_val.dtype) end,
        dlen = function() return ffi.cast("size_t", c_val.dlen) end,
        data = function() return c_val.data end,
    }
end

function newTx(val)
    local c_val = ffi.cast("ib_tx_t *", val)
    return {
        cvalue = function() return c_val end,
        mp = function() return newMpool(c_val.mp) end,
        ib = function() return newEngine(c_val.ib) end,
        ctx = function() return newContext(c_val.ctx) end,
        data = function() return newData(c_val.data) end,
        epi = function() return newProviderInst(c_val.epi) end,
        conn = function() return newConn(c_val.conn) end,
        id = function() return ffi.cast("const char *", c_val.id) end,
    }
end

-- ===============================================
-- Get a data field by name.
--
-- data: Data Provider Interface (i.e. conn.data() or tx.data())
-- name: Name of data field
-- ===============================================
function ib_data_get(data, name)
    local c_data = data.cvalue()
    local c_pf = ffi.new("ib_field_t*[1]")
    local rc

    -- Get the named data field.
    rc = c.ib_data_get_ex(c_data, name, string.len(name), c_pf)
    if rc ~= c.IB_OK then
        return nil
    end

    return newField(c_pf[0]);
end

-- ===============================================
-- Get a data field by name with a transformation.
--
-- data: Data Provider Interface (i.e. conn.data() or tx.data())
-- name: Name of data field
-- tfn: Comma separated tfn name string
-- ===============================================
function ib_data_tfn_get(data, name, tfn)
    local c_data = data.cvalue()
    local c_pf = ffi.new("ib_field_t*[1]")
    local rc

    -- Get the named data field.
    rc = c.ib_tfn_data_get_ex(c_data, name, string.len(name), c_pf, tfn)
    if rc ~= c.IB_OK then
        return nil
    end

    return newField(c_pf[0]);
end


-- ===============================================
-- Known provider types
-- ===============================================
IB_PROVIDER_TYPE_LOGGER    = "logger"
IB_PROVIDER_TYPE_PARSER    = "parser"
IB_PROVIDER_TYPE_MATCHER   = "matcher"
IB_PROVIDER_TYPE_LOGEVENT  = "logevent"

-- ===============================================
-- Lookup a provider by type and key.
--
-- ib: Engine
-- type: Provider type
-- key: Provider key
-- ===============================================
function ib_provider_lookup(ib, type, key)
    local c_ib = ib.cvalue()
    local c_ppr = ffi.new("ib_provider_t*[1]")
    local rc

    -- Get the named data field.
    rc = c.ib_provider_lookup(c_ib, type, key, c_ppr)
    if rc ~= c.IB_OK then
        return nil
    end

    return newProvider(c_ppr[0]);
end

function ib_matcher_create(ib, pool, key)
    local c_ib = ib.cvalue()
    local c_pool = pool.cvalue()
    local c_pm = ffi.new("ib_matcher_t*[1]")
    local rc

    rc = c.ib_matcher_create(c_ib, c_pool, key, c_pm)
    if rc ~= c.IB_OK then
        return nil
    end

    -- TODO Probably should return a wrapper???
    return c_pm[0]
end

function ib_matcher_match_field(m, patt, flags, f)
    local cpatt
    local c_f = f.cvalue()
    local rc

    if base.type(patt) == "string" then
        -- TODO Do we need to GC these?
        local errptr = ffi.new("const char *[1]")
        local erroffset = ffi.new("int[1]")
        cpatt = c.ib_matcher_compile(m, patt, errptr, erroffset)
    else
        cpatt = patt
    end

    if cpatt == nil then
        return c.IB_EINVAL
    end

    return c.ib_matcher_match_field(m, cpatt, flags, c_f)
end

function ib_logevent_type_name(num)
    local name = c.ib_logevent_type_name(num)
    if name ~= nil then
        return ffi.string(name)
    end
    return nil
end

function ib_logevent_activity_name(num)
    local name = c.ib_logevent_activity_name(num)
    if name ~= nil then
        return ffi.string(name)
    end
    return nil
end

function ib_logevent_pri_class_name(num)
    local name = c.ib_logevent_pri_class_name(num)
    if name ~= nil then
        return ffi.string(name)
    end
    return nil
end

function ib_logevent_sec_class_name(num)
    local name = c.ib_logevent_sec_class_name(num)
    if name ~= nil then
        return ffi.string(name)
    end
    return nil
end

function ib_logevent_sys_env_name(num)
    local name = c.ib_logevent_sys_env_name(num)
    if name ~= nil then
        return ffi.string(name)
    end
    return nil
end

function ib_logevent_action_name(num)
    local name = c.ib_logevent_action_name(num)
    if name ~= nil then
        return ffi.string(name)
    end
    return nil
end

-- TODO: Make this accept a table (named parameters)???
function ib_logevent_create(pool, rule_id, type, activity,
                            pri_class, sec_class,
                            sys_env, rec_action,
                            confidence, severity,
                            fmt, ...)
    local c_pool = pool.cvalue()
    local c_le = ffi.new("ib_logevent_t*[1]")
    local rc

    rc = c.ib_logevent_create(c_le, c_pool,
                              rule_id,
                              ffi.cast("int", type),
                              ffi.cast("int", activity),
                              ffi.cast("int", pri_class),
                              ffi.cast("int", sec_class),
                              ffi.cast("int", sys_env),
                              ffi.cast("int", rec_action),
                              ffi.cast("uint8_t", confidence),
                              ffi.cast("uint8_t", severity),
                              fmt, ...)
    if rc ~= c.IB_OK then
        return nil
    end

    return newLogevent(c_le[0])
end

function ib_logevent_add(pi, e)
    local c_pi = pi.cvalue()
    local c_e = e.cvalue()

    return c.ib_logevent_add(c_pi, c_e)
end

function ib_logevent_remove(pi, id)
    local c_pi = pi.cvalue()
    local c_id = ffi.cast("uint64_t", id)

    return c.ib_logevent_remove(c_pi, c_id)
end

function ib_logevents_get_all(pi)
    local c_pi = pi.cvalue()
    local c_events = ffi.new("ib_list_t*[1]")
    local rc

    rc = c.ib_logevent_get_all(c_pi, c_events)

    -- Loop through and create a list of logevents to return
    local l_vals = {}
    local c_node = c.ib_list_first(c_events)
    local i = 1
    while c_node ~= nil do
        local c_e = ffi.cast("ib_logevent_t *", c.ib_list_node_data(c_node))
        l_vals[i] = newLogevent(c_e)
        i = i + 1
        c_node = c.ib_list_node_next(c_node)
    end
    return l_vals
end

function ib_logevents_write_all(pi)
    local c_pi = pi.cvalue()

    return c.ib_logevent_write_all(c_pi)
end

-- ===============================================
-- Wrapper function to call Lua Config Functions
-- ===============================================
function _IRONBEE_CALL_CONFIG_HANDLER(ib, modname, funcname, ...)
    local c_ib = ffi.cast("ib_engine_t *", ib)
    local l_ib = newEngine(ib)
    local m

    m = modules[modname]
    if m == nil then
        return c.IB_ENOENT
    end

    return m[funcname](l_ib, ...)
end

-- ===============================================
-- Wrapper function to call Lua Module Functions
-- ===============================================
function _IRONBEE_CALL_MODULE_HANDLER(ib, modname, funcname, ...)
    local c_ib = ffi.cast("ib_engine_t *", ib)
    local l_ib = newEngine(ib)
    local m

    m = modules[modname]
    if m == nil then
        return c.IB_ENOENT
    end

    return m[funcname](l_ib, m, ...)
end

-- ===============================================
-- Wrapper function to call Lua event handler.
-- ===============================================
function _IRONBEE_CALL_EVENT_HANDLER(ib, modname, funcname, event, arg, ...)
    local c_ib = ffi.cast("ib_engine_t *", ib)
    local c_event = ffi.cast("int", event);
    local l_ib = newEngine(ib)
    local l_arg
    local m

    if c_event == c.conn_started_event then
        l_arg = newConn(arg)
    elseif c_event == c.conn_finished_event then
        l_arg = newConn(arg)
    elseif c_event == c.tx_started_event then
        l_arg = newTx(arg)
    elseif c_event == c.tx_process_event then
        l_arg = newTx(arg)
    elseif c_event == c.tx_finished_event then
        l_arg = newTx(arg)
    elseif c_event == c.handle_context_conn_event then
        l_arg = newConn(arg)
    elseif c_event == c.handle_connect_event then
        l_arg = newConn(arg)
    elseif c_event == c.handle_context_tx_event then
        l_arg = newTx(arg)
    elseif c_event == c.handle_request_header_event then
        l_arg = newTx(arg)
    elseif c_event == c.handle_request_event then
        l_arg = newTx(arg)
    elseif c_event == c.handle_response_header_event then
        l_arg = newTx(arg)
    elseif c_event == c.handle_response_event then
        l_arg = newTx(arg)
    elseif c_event == c.handle_disconnect_event then
        l_arg = newConn(arg)
    elseif c_event == c.handle_postprocess_event then
        l_arg = newTx(arg)
    elseif c_event == c.handle_logging_event then
        l_arg = newTx(arg)
    elseif c_event == c.conn_opened_event then
        l_arg = newConn(arg)
    elseif c_event == c.conn_closed_event then
        l_arg = newConn(arg)
    elseif c_event == c.request_started_event then
        l_arg = newTx(arg)
    elseif c_event == c.request_header_finished_event then
        l_arg = newTx(arg)
    elseif c_event == c.request_body_data_event then
        l_arg = newTx(arg)
    elseif c_event == c.request_finished_event then
        l_arg = newTx(arg)
    elseif c_event == c.response_started_event then
        l_arg = newTx(arg)
    elseif c_event == c.response_header_finished_event then
        l_arg = newTx(arg)
    elseif c_event == c.response_body_data_event then
        l_arg = newTx(arg)
    elseif c_event == c.response_finished_event then
        l_arg = newTx(arg)
    elseif c_event == c.handle_logevent_event then
        l_arg = newTx(arg)
    else
        ib_log_error(l_ib,  "Unhandled event for module \"%s\": %d",
                     modname, ffi.cast("int", event))
        return nil
    end

--    ib_log_debug3(l_ib, "Executing event handler for module \"%s\" event=%d",
--                 modname, ffi.cast("int", event))
    m = modules[modname]
    if m == nil then
        return c.IB_ENOENT
    end

    return m[funcname](l_ib, l_arg, ...)
end
