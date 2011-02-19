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
local ffi = require("ffi")
local io = require("io")
local string = require("string")

module("ironbee-ffi")

-- Mark this lib as preloaded
base.package.preload["ironbee-ffi"] = _M

-- TODO: remove this need to register with engine
base["ironbee-module"] = _M

-- ===============================================
-- Setup some module metadata.
-- ===============================================
_COPYRIGHT = "Copyright (C) 2010-2011 Qualys, Inc."
_DESCRIPTION = "IronBee API via luajit FFI"
_VERSION = "0.1"

-- ===============================================
-- Setup the IronBee C definitions.
-- ===============================================
ffi.cdef[[
    /* Util Types */
    typedef struct ib_mpool_t ib_mpool_t;
    typedef struct ib_dso_t ib_dso_t;
    typedef void ib_dso_sym_t;
    typedef struct ib_hash_t ib_hash_t;
    typedef uint32_t ib_ftype_t;
    typedef uint32_t ib_flags_t;
    typedef uint64_t ib_flags64_t;
    typedef struct ib_cfgmap_t ib_cfgmap_t;
    typedef struct ib_cfgmap_init_t ib_cfgmap_init_t;
    typedef struct ib_field_t ib_field_t;
    typedef struct ib_field_val_t ib_field_val_t;
    typedef struct ib_bytestr_t ib_bytestr_t;
    typedef enum {
        IB_OK,
        IB_DECLINED,
        IB_EUNKNOWN,
        IB_ENOTIMPL,
        IB_EINCOMPAT,
        IB_EALLOC,
        IB_EINVAL,
        IB_ENOENT,
        IB_ETIMEDOUT,
    } ib_status_t;
    typedef enum {
        IB_FTYPE_GENERIC,
        IB_FTYPE_NUM,
        IB_FTYPE_NULSTR,
        IB_FTYPE_BYTESTR,
        IB_FTYPE_LIST
    } ib_ftype_t;
    typedef enum {
        IB_TXDATA_HTTP_LINE,
        IB_TXDATA_HTTP_HEADER,
        IB_TXDATA_HTTP_BODY,
        IB_TXDATA_HTTP_TRAILER
    } ib_txdata_type_t;

    /* Engine Types */
    typedef struct ib_engine_t ib_engine_t;
    typedef struct ib_context_t ib_context_t;
    typedef struct ib_conn_t ib_conn_t;
    typedef struct ib_conndata_t ib_conndata_t;
    typedef struct ib_tx_t ib_tx_t;
    typedef struct ib_txdata_t ib_txdata_t;
    typedef struct ib_tfn_t ib_tfn_t;
    typedef struct ib_logevent_t ib_logevent_t;
    typedef struct ib_plugin_t ib_plugin_t;
    typedef struct ib_provider_def_t ib_provider_def_t;
    typedef struct ib_provider_t ib_provider_t;
    typedef struct ib_provider_inst_t ib_provider_inst_t;


    /** Function called when a provider is registered. */
    typedef ib_status_t (*ib_provider_register_fn_t)(ib_engine_t *ib,
                                                     ib_provider_t *pr);

    /** Function called when a provider instance is created. */
    typedef ib_status_t (*ib_provider_inst_init_fn_t)(ib_provider_inst_t *pi,
                                                      void *data);

    struct ib_plugin_t {
        int                      vernum;
        int                      abinum;
        const char              *version;
        const char              *filename;
        const char              *name;
    };

    /** Connection Structure */
    struct ib_conn_t {
        ib_engine_t        *ib;
        ib_mpool_t         *mp;
        ib_context_t       *ctx;
        void               *pctx;
        ib_provider_inst_t *dpi;
        ib_hash_t          *data;

        const char         *remote_ipstr;
        int                 remote_port;

        const char         *local_ipstr;
        int                 local_port;

        ib_tx_t            *tx;
        ib_tx_t            *tx_last;

        ib_flags_t          flags;
    };

    /* Connection Data Structure */
    struct ib_conndata_t {
        ib_engine_t        *ib;
        ib_mpool_t         *mp;
        ib_conn_t          *conn;
        size_t              dalloc;
        size_t              dlen;
        const char         *data;
    };

    /* Transaction Structure */
    struct ib_tx_t {
        ib_engine_t        *ib;
        ib_mpool_t         *mp;
        ib_conn_t          *conn;
        ib_context_t       *ctx;
        void               *pctx;
        ib_provider_inst_t *dpi;
        ib_hash_t          *data;
        ib_tx_t            *next;
        const char         *hostname;
        const char         *path;
        ib_flags_t          flags;
    };

    /* Transaction Data Structure */
    struct ib_txdata_t {
        ib_engine_t        *ib;
        ib_mpool_t         *mp;
        ib_tx_t            *tx;
        ib_txdata_type_t    dtype;
        size_t              dalloc;
        size_t              dlen;
        const char         *data;
    };


    /* Data Field Structure */
    struct ib_field_t {
        ib_mpool_t         *mp;
        ib_ftype_t          type;
        const char         *name;
        size_t              nlen;
        void               *pval;
        ib_field_val_t     *val;
    };

    /* Provider Structures */
    struct ib_provider_def_t {
        ib_mpool_t                *mp;
        const char                *type;
        ib_provider_register_fn_t  fn_reg;
        void                      *api;
    };
    struct ib_provider_t {
        ib_engine_t               *ib;
        ib_mpool_t                *mp;
        const char                *type;
        void                      *data;
        void                      *iface;
        void                      *api;
        ib_provider_inst_init_fn_t fn_init;
    };
    struct ib_provider_inst_t {
        ib_mpool_t                *mp;
        ib_provider_t             *pr;
        void                      *data;
    };



    /* Context */
    ib_context_t *ib_context_engine(ib_engine_t *ib);
    ib_context_t *ib_context_main(ib_engine_t *ib);

    /* Byte String */
    size_t ib_bytestr_length(ib_bytestr_t *bs);
    size_t ib_bytestr_size(ib_bytestr_t *bs);
    uint8_t *ib_bytestr_ptr(ib_bytestr_t *bs);

    /* Data Access */
    ib_status_t ib_data_get_ex(ib_provider_inst_t *dpi,
                               const char *name,
                               size_t nlen,
                               ib_field_t **pf);

    /* Misc */
    ib_status_t ib_engine_create(ib_engine_t **pib, void *plugin);
    ib_status_t ib_context_create_main(ib_context_t **pctx,
                                       ib_engine_t *ib);

    /* Logging */
    void ib_clog_ex(ib_context_t *ctx,
                    int level,
                    const char *prefix,
                    const char *file,
                    int line,
                    const char *fmt,
                    ...);
]]

-- Cache lookup of ffi.C
local c = ffi.C

-- 
-- =========================================================================
-- =========================================================================
-- Implementation Notes:
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
-- TODO: Figure out a way around this.
-- ===============================================
function register_module(m)
    base["ironbee-module"] = m
end

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
function ib_log_debug(ib, lvl, fmt, ...)
    local c_ib = ffi.cast("ib_engine_t *", ib)
    local c_ctx = c.ib_context_main(c_ib)

    c.ib_clog_ex(c_ctx, 4, "LuaFFI: ", nil, 0, fmt, ...)
end

-- ===============================================
-- Convert an IronBee field to an appropriate Lua
-- type.
--
-- NOTE: This currently makes a copy of the data.
-- ===============================================
function field_convert(c_f)
    if c_f.type == c.IB_FTYPE_BYTESTR then
        c_val = ffi.cast("ib_bytestr_t **", c_f.pval)[0]
        return ffi.string(c.ib_bytestr_ptr(c_val), c.ib_bytestr_length(c_val))
    elseif c_f.type == c.IB_FTYPE_LIST then
        -- TODO: Loop through and create a table of converted values
    elseif c_f.type == c.IB_FTYPE_NULSTR then
        c_val = ffi.cast("const char **", c_f.pval)[0]
        return ffi.string(c_val[0])
    elseif c_f.type == c.IB_FTYPE_NUM then
        c_val = ffi.cast("int64_t **", c_f.pval)[0]
        return base.tonumber(c_val[0])
    end

    return nil
end

-- ===============================================
-- ===============================================
function ib_data_get(dpi, name)
    local c_dpi = ffi.cast("ib_provider_inst_t *", dpi)
--    local c_ib = c_dpi.pr.ib
    local c_pf = ffi.new("ib_field_t*[1]")
    local rc
    local c_val

    -- Get the named data field.
    rc = c.ib_data_get_ex(c_dpi, name, string.len(name), c_pf)
    if rc ~= 0 then
        return nil
    end

    return field_convert(c_pf[0]);
end

