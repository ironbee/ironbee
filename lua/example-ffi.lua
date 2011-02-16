----------------------------------------------------------------------------
-- Define local versions of globals that will be used
----------------------------------------------------------------------------
local base = _G
local ffi = require("ffi")
local ironbee = require("ironbee")

----------------------------------------------------------------------------
-- Declare the rest of the file as a module and register the module
-- table with ironbee.
----------------------------------------------------------------------------
module(...)
ironbee.register_module(_M)

----------------------------------------------------------------------------
-- Setup the IronBee C definitions
----------------------------------------------------------------------------
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

    /* Engine Types */
    typedef struct ib_engine_t ib_engine_t;
    typedef struct ib_context_t ib_context_t;
    typedef struct ib_conn_t ib_conn_t;
    typedef struct ib_conndata_t ib_conndata_t;
    typedef struct ib_tx_t ib_tx_t;
    typedef struct ib_tfn_t ib_tfn_t;
    typedef struct ib_logevent_t ib_logevent_t;
    typedef struct ib_plugin_t ib_plugin_t;
    typedef struct ib_provider_inst_t ib_provider_inst_t;
    typedef enum ib_status_t {
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

    struct ib_plugin_t {
        int                      vernum;
        int                      abinum;
        const char              *version;
        const char              *filename;
        const char              *name;
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
local ibffi = ffi.C

----------------------------------------------------------------------------
-- Setup some module metadata
----------------------------------------------------------------------------
_COPYRIGHT = "Copyright (C) 2010-2011 Qualys, Inc."
_DESCRIPTION = "IronBee example Lua module"
_VERSION = "0.1"

----------------------------------------------------------------------------
-- Event Handlers
--
-- NOTE: As a best practice, you should avoid using the "onEvent" prefix
-- in any public functions that are NOT to be used as event handlers as
-- these may be treated specially by the engine.
----------------------------------------------------------------------------

-- This is called when the request headers are avalable to inspect.
function onEventHandleRequestHeaders(...)
    local ib, tx = ...
    local _tx = ffi.cast("ib_tx_t *", tx);

    -- Should wrap this (it is a ib_log_debug macro in C)
    ibffi.ib_clog_ex(_tx.ctx, 4, nil, nil, 0, "LuaFFI: %s.onEventHandleRequestHeaders", _NAME)
 
    -- Do something interesting

    return 0
end

