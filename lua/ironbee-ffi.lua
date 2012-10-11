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
_COPYRIGHT = "Copyright (C) 2010-2011 Qualys, Inc."
_DESCRIPTION = "IronBee API via luajit FFI"
_VERSION = "0.2"

-- ===============================================
-- Setup the IronBee C definitions.
-- ===============================================
ffi.cdef [[
    /* Util Types */
    typedef void (*ib_void_fn_t)(void);
    typedef struct ib_mpool_t ib_mpool_t;
    typedef struct ib_dso_t ib_dso_t;
    typedef void ib_dso_sym_t;
    typedef struct ib_hash_t ib_hash_t;
    typedef struct ib_list_t ib_list_t;
    typedef struct ib_list_node_t ib_list_node_t;
    typedef uint32_t ib_flags_t;
    typedef uint64_t ib_flags64_t;
    typedef int64_t ib_num_t;
    typedef uint64_t ib_unum_t;
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
        IB_ETRUNC,
        IB_ETIMEDOUT,
        IB_EAGAIN,
        IB_EBADVAL,
        IB_EEXIST,
        IB_EOTHER
    } ib_status_t;
    typedef enum {
        IB_FTYPE_GENERIC,
        IB_FTYPE_NUM,
        IB_FTYPE_UNUM,
        IB_FTYPE_NULSTR,
        IB_FTYPE_BYTESTR,
        IB_FTYPE_LIST,
        IB_FTYPE_SBUFFER
    } ib_ftype_t;
    typedef enum {
        conn_started_event,
        conn_finished_event,
        tx_started_event,
        tx_process_event,
        tx_finished_event,
        handle_context_conn_event,
        handle_connect_event,
        handle_context_tx_event,
        handle_request_header_event,
        handle_request_event,
        handle_response_header_event,
        handle_response_event,
        handle_disconnect_event,
        handle_postprocess_event,
        cfg_started_event,
        cfg_finished_event,
        conn_opened_event,
        conn_data_in_event,
        conn_data_out_event,
        conn_closed_event,
        request_started_event,
        request_header_data_event,
        request_header_finished_event,
        request_body_data_event,
        request_finished_event,
        response_started_event,
        response_header_data_event,
        response_header_finished_event,
        response_body_data_event,
        response_finished_event,
        IB_STATE_EVENT_NUM
    } ib_state_event_type_t;
    typedef enum {
        IB_LEVENT_TYPE_UNKNOWN,
        IB_LEVENT_TYPE_OBSERVATION
    } ib_logevent_type_t;
    typedef enum {
        IB_LEVENT_ACTION_UNKNOWN,
        IB_LEVENT_ACTION_LOG,
        IB_LEVENT_ACTION_BLOCK,
        IB_LEVENT_ACTION_IGNORE,
        IB_LEVENT_ACTION_ALLOW
    } ib_logevent_action_t;
    typedef enum {
        IB_LOG_EMERGENCY,
        IB_LOG_ALERT,
        IB_LOG_CRITICAL,
        IB_LOG_ERROR,
        IB_LOG_WARNING,
        IB_LOG_NOTICE,
        IB_LOG_INFO,
        IB_LOG_DEBUG,
        IB_LOG_DEBUG2,
        IB_LOG_DEBUG3,
        IB_LOG_TRACE,
        IB_LOG_LEVEL_NUM
    } ib_log_level_t;

    /* Engine Types */
    typedef struct ib_engine_t ib_engine_t;
    typedef struct ib_context_t ib_context_t;
    typedef struct ib_conn_t ib_conn_t;
    typedef struct ib_conndata_t ib_conndata_t;
    typedef struct ib_tx_t ib_tx_t;
    typedef struct ib_txdata_t ib_txdata_t;
    typedef struct ib_tfn_t ib_tfn_t;
    typedef struct ib_logevent_t ib_logevent_t;
    typedef struct ib_uuid_t ib_uuid_t;
    typedef struct ib_server_t ib_server_t;
    typedef struct ib_provider_def_t ib_provider_def_t;
    typedef struct ib_provider_t ib_provider_t;
    typedef struct ib_provider_inst_t ib_provider_inst_t;
    typedef struct ib_matcher_t ib_matcher_t;
    typedef struct ib_filter_t ib_filter_t;
    typedef struct ib_fdata_t ib_fdata_t;
    typedef struct ib_fctl_t ib_fctl_t;
    typedef struct ib_stream_t ib_stream_t;
    typedef struct ib_sdata_t ib_sdata_t;
    typedef uint64_t ib_time_t;

    typedef struct {
        uint32_t tv_sec;
        uint32_t tv_usec;
    } ib_timeval_t;

    /* Parsed Object Types */
    typedef struct ib_parsed_req_line_t ib_parsed_req_line_t;
    typedef struct ib_parsed_resp_line_t ib_parsed_resp_line_t;
    typedef struct ib_parsed_name_value_pair_list_t ib_parsed_name_value_pair_list_t;
    typedef struct ib_parsed_name_value_pair_list_wrapper_t ib_parsed_name_value_pair_list_wrapper_t;
    typedef struct ib_parsed_name_value_pair_list_wrapper_t ib_parsed_header_wrapper_t;
    typedef struct ib_parsed_name_value_pair_list_wrapper_t ib_parsed_trailer_wrapper_t;

    /** Function called when a provider is registered. */
    typedef ib_status_t (*ib_provider_register_fn_t)(ib_engine_t *ib,
                                                     ib_provider_t *pr);

    /** Function called when a provider instance is created. */
    typedef ib_status_t (*ib_provider_inst_init_fn_t)(ib_provider_inst_t *pi,
                                                      void *data);
    typedef enum {
        IB_STREAM_DATA,
        IB_STREAM_FLUSH,
        IB_STREAM_EOH,
        IB_STREAM_EOB,
        IB_STREAM_EOS,
        IB_STREAM_ERROR
    } ib_sdata_type_t;
    typedef enum {
        IB_FILTER_CONN,
        IB_FILTER_TX
    } ib_filter_type_t;

    /* Universal Unique ID Structure */
    struct ib_uuid_t {
        uint32_t  time_low;
        uint16_t  time_mid;
        uint16_t  time_hi_and_ver;
        uint8_t   clk_seq_hi_res;
        uint8_t   clk_seq_low;
        uint8_t   node[6];
    };

    struct ib_server_t {
        uint32_t                 vernum;
        uint32_t                 abinum;
        const char              *version;
        const char              *filename;
        const char              *name;
    };

    /** Connection Structure */
    struct ib_conn_t {
        ib_engine_t        *ib;
        ib_mpool_t         *mp;
        ib_context_t       *ctx;
        void               *server_ctx;
        void               *parser_ctx;
        ib_provider_inst_t *dpi;
        ib_hash_t          *data;
        ib_timeval_t        tv_created;
        struct {
            ib_time_t       started;
            ib_time_t       finished;
        } t;
        const char         *remote_ipstr;
        uint16_t            remote_port;
        const char         *local_ipstr;
        uint16_t            local_port;
        size_t              tx_count;
        ib_tx_t            *tx_first;
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

    /* Transaction Data Structure */
    struct ib_txdata_t {
        ib_engine_t        *ib;
        ib_mpool_t         *mp;
        ib_tx_t            *tx;
        size_t              dalloc;
        size_t              dlen;
        const char         *data;
    };

    /* Transaction Structure */
    struct ib_tx_t {
        ib_engine_t        *ib;
        ib_mpool_t         *mp;
        const char         *id;
        ib_conn_t          *conn;
        ib_context_t       *ctx;
        void               *pctx;
        ib_provider_inst_t *dpi;
        ib_provider_inst_t *epi;
        ib_hash_t          *data;
        ib_fctl_t          *fctl;
        ib_timeval_t        tv_created;
        struct {
            ib_time_t       started;
            ib_time_t       request_started;
            ib_time_t       request_header;
            ib_time_t       request_body;
            ib_time_t       request_finished;
            ib_time_t       response_started;
            ib_time_t       response_header;
            ib_time_t       response_body;
            ib_time_t       response_finished;
            ib_time_t       postprocess;
            ib_time_t       logtime;
            ib_time_t       finished;
        } t;
        ib_tx_t            *next;
        const char         *hostname;
        const char         *er_ipstr;
        const char         *path;
        ib_flags_t          flags;

        ib_parsed_req_line_t *request_line;
        ib_parsed_header_wrapper_t *request_header;
        ib_stream_t        *request_body;

        ib_parsed_resp_line_t *response_line;
        ib_parsed_header_wrapper_t *response_header;
        ib_stream_t        *response_body;
    };

    /* Parsed Name Value Pair List Node */
    struct ib_parsed_name_value_pair_list_t {
        ib_bytestr_t *name;
        ib_bytestr_t *value;
        ib_parsed_name_value_pair_list_t *next;
    }

    /* Parsed Name Value Pair List */
    struct ib_parsed_name_value_pair_list_wrapper_t {
        ib_mpool_t *mpool;
        ib_parsed_name_value_pair_list_t *head;
        ib_parsed_name_value_pair_list_t *tail;
        size_t size;
    };

    /* Parsed Request Line */
    struct ib_parsed_req_line_t {
        ib_bytestr_t *raw;
        ib_bytestr_t *method;
        ib_bytestr_t *uri;
        ib_bytestr_t *protocol;
    };

    /* Parsed Response Line */
    struct ib_parsed_resp_line_t {
        ib_bytestr_t *raw;
        ib_bytestr_t *protocol;
        ib_bytestr_t *status;
        ib_bytestr_t *msg;
    };

    /* Data Field Structure */
    struct ib_field_t {
        ib_mpool_t         *mp;
        ib_ftype_t          type;
        const char         *name;
        size_t              nlen;
        const char         *tfn;
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

    struct ib_logevent_t {
        ib_mpool_t              *mp;
        const char              *rule_id;
        const char              *msg;
        ib_list_t               *tags;
        ib_list_t               *fields;
        uint32_t                 event_id;
        ib_logevent_type_t       type;
        ib_logevent_action_t     rec_action;
        ib_logevent_action_t     action;
        void                    *data;
        size_t                   data_len;
        uint8_t                  confidence;
        uint8_t                  severity;
    };

    /* Rule and Operator Structures and Types. */

    typedef enum {
        PHASE_INVALID = -1,
        PHASE_NONE,
        PHASE_REQUEST_HEADER,
        PHASE_REQUEST_BODY,
        PHASE_RESPONSE_HEADER,
        PHASE_RESPONSE_BODY,
        PHASE_POSTPROCESS,
        PHASE_STR_REQUEST_HEADER,
        PHASE_STR_REQUEST_BODY,
        PHASE_STR_RESPONSE_HEADER,
        PHASE_STR_RESPONSE_BODY,
        IB_RULE_PHASE_COUNT,
    } ib_rule_phase_num_t;
    typedef enum {
        IB_RULE_DLOG_ALWAYS,
        IB_RULE_DLOG_ERROR,
        IB_RULE_DLOG_WARNING,
        IB_RULE_DLOG_NOTICE,
        IB_RULE_DLOG_INFO,
        IB_RULE_DLOG_DEBUG,
        IB_RULE_DLOG_TRACE,
    } ib_rule_dlog_level_t;
    typedef enum {
        FLAG_OP_SET,
        FLAG_OP_OR,
        FLAG_OP_CLEAR,
    } ib_rule_flagop_t;
    typedef enum {
        RULE_ACTION_TRUE,
        RULE_ACTION_FALSE,
    } ib_rule_action_t;
    typedef enum {
        RULE_ENABLE_ID,
        RULE_ENABLE_TAG,
        RULE_ENABLE_ALL,
    } ib_rule_enable_type_t;
    typedef struct {
        const char            *id;
        const char            *full_id;
        const char            *chain_id;
        const char            *msg;
        const char            *data;
        ib_list_t             *tags;
        ib_rule_phase_num_t    phase;
        uint8_t                severity;
        uint8_t                confidence;
        uint16_t               revision;
        ib_flags_t             flags;
        const char            *config_file;
        unsigned int           config_line;
    } ib_rule_meta_t;
    struct ib_rule_target_t {
        const char            *field_name;
        const char            *target_str;
        ib_list_t             *tfn_list;
    };
    typedef struct ib_rule_phase_meta_t ib_rule_phase_meta_t;
    typedef struct ib_rule_t ib_rule_t;
    typedef struct ib_operator_inst_t ib_operator_inst_t;
    typedef struct ib_rule_exec_t ib_rule_exec_t;
    struct ib_rule_t {
        ib_rule_meta_t         meta;
        const ib_rule_phase_meta_t *phase_meta;
        ib_operator_inst_t    *opinst;
        ib_list_t             *target_fields;
        ib_list_t             *true_actions;
        ib_list_t             *false_actions;
        ib_list_t             *parent_rlist;
        ib_context_t          *ctx;
        ib_rule_t             *chained_rule;
        ib_rule_t             *chained_from;
        ib_flags_t             flags;
    };
    typedef ib_status_t (* ib_operator_create_fn_t)(
        ib_engine_t        *ib,
        ib_context_t       *ctx,
        const ib_rule_t    *rule,
        ib_mpool_t         *pool,
        const char         *parameters,
        ib_operator_inst_t *op_inst
    );
    typedef ib_status_t (* ib_operator_destroy_fn_t)(
        ib_operator_inst_t *op_inst
    );
    typedef ib_status_t (* ib_operator_execute_fn_t)(
        const ib_rule_exec_t *rule_exec,
        void                 *data,
        ib_flags_t            flags,
        ib_field_t           *field,
        ib_num_t             *result
    );
    typedef struct ib_operator_t ib_operator_t;

    struct ib_operator_t {
        char                    *name;
        ib_flags_t               flags;
        void                    *cd_create;
        void                    *cd_destroy;
        void                    *cd_execute;
        ib_operator_create_fn_t  fn_create;
        ib_operator_destroy_fn_t fn_destroy;
        ib_operator_execute_fn_t fn_execute;
    };

struct ib_operator_inst_t {
    struct ib_operator_t *op;      /**< Pointer to the operator type */
    ib_flags_t            flags;   /**< Operator instance flags */
    void                 *data;    /**< Data passed to the execute function */
    char                 *params;  /**< Parameters passed to create */
    ib_field_t           *fparam;  /**< Parameters as a field */
};
ib_status_t ib_operator_register(
    ib_engine_t              *ib,
    const char               *name,
    ib_flags_t                flags,
    ib_operator_create_fn_t   fn_create,
    void                     *cd_create,
    ib_operator_destroy_fn_t  fn_destroy,
    void                     *cd_destroy,
    ib_operator_execute_fn_t  fn_execute,
    void                     *cd_execute
);
ib_status_t ib_operator_inst_create(
    ib_engine_t         *ib,
    ib_context_t        *ctx,
    const ib_rule_t     *rule,
    ib_flags_t           required_op_flags,
    const char          *name,
    const char          *parameters,
    ib_flags_t           flags,
    ib_operator_inst_t **op_inst
);
ib_status_t ib_operator_inst_destroy(
    ib_operator_inst_t *op_inst
);
ib_status_t ib_operator_execute(
    const ib_rule_exec_t     *rule_exec,
    const ib_operator_inst_t *op_inst,
    ib_field_t               *field,
    ib_num_t                 *result
);

    /* Rule Structures and Types. */

typedef struct ib_rule_target_t ib_rule_target_t;

/**
 * Rule execution logging data
 */
typedef struct ib_rule_log_exec_t ib_rule_log_exec_t;
typedef struct ib_rule_log_tx_t ib_rule_log_tx_t;

    typedef struct {
        const char            *id;
        const char            *full_id;
        const char            *chain_id;
        const char            *msg;
        const char            *data;
        ib_list_t             *tags;
        ib_rule_phase_num_t    phase;
        uint8_t                severity;
        uint8_t                confidence;
        uint16_t               revision;
        ib_flags_t             flags;
        const char            *config_file;
        unsigned int           config_line;
    } ib_rule_meta_t;
    typedef struct ib_rule_phase_meta_t ib_rule_phase_meta_t;
    typedef struct {
        ib_rule_t             *rule;
        ib_flags_t             flags;
    } ib_rule_ctx_data_t;
    
    /**
    * Rule engine parser data
    */
    typedef struct {
        ib_rule_t             *previous;     /**< Previous rule parsed */
    } ib_rule_parser_data_t;
    
    /**
    * Ruleset for a single phase.
    *  rule_list is a list of pointers to ib_rule_ctx_data_t objects.
    */
    typedef struct {
        ib_rule_phase_num_t         phase_num;   /**< Phase number */
        const ib_rule_phase_meta_t *phase_meta;  /**< Rule phase meta-data */
        ib_list_t                  *rule_list;   /**< Rules to execute in phase */
    } ib_ruleset_phase_t;
        
    /**
    * Set of rules for all phases.
    * The elements of the phases list are ib_rule_ctx_data_t objects.
    */
    typedef struct {
        ib_ruleset_phase_t     phases[IB_RULE_PHASE_COUNT];
    } ib_ruleset_t;
    
    /**
    * Data on enable directives.
    */
    typedef struct {
        ib_rule_enable_type_t  enable_type;  /**< Enable All / by ID / by Tag */
        const char            *enable_str;   /**< String of ID or Tag */
        const char            *file;         /**< Configuration file of enable */
        unsigned int           lineno;       /**< Line number in config file */
    } ib_rule_enable_t;
    
    /**
    * Rules data for each context.
    */
    struct ib_rule_context_t {
        ib_ruleset_t           ruleset;      /**< Rules to exec */
        ib_list_t             *rule_list;    /**< All rules owned by context */
        ib_hash_t             *rule_hash;    /**< Hash of rules (by rule-id) */
        ib_list_t             *enable_list;  /**< Enable All/IDs/tags */
        ib_list_t             *disable_list; /**< All/IDs/tags disabled */
        ib_rule_parser_data_t  parser_data;  /**< Rule parser specific data */
    };
    
    /**
    * Rule engine data.
    */
    struct ib_rule_engine_t {
        ib_list_t             *rule_list; /**< All rules owned by this context */
        ib_hash_t             *rule_hash; /**< Hash of rules (by rule-id) */
    };


    struct ib_rule_exec_t {
        ib_engine_t            *ib;
        ib_tx_t                *tx;
        ib_rule_t              *rule;
        ib_rule_target_t       *target;
        ib_num_t                result;
        ib_rule_log_tx_t       *tx_log;
        ib_rule_log_exec_t     *exec_log;
        ib_list_t              *rule_stack;
        ib_list_t              *value_stack;
    };
    
    const char *ib_logevent_type_name(ib_logevent_type_t num);
    const char *ib_logevent_action_name(ib_logevent_action_t num);

    /* Field */
    ib_status_t ib_field_create(ib_field_t **pf,
                                ib_mpool_t *mp,
                                const char *name,
                                size_t nlen,
                                ib_ftype_t type,
                                void *in_pval);
    ib_status_t ib_field_copy(ib_field_t **pf,
                              ib_mpool_t *mp,
                              const char *name,
                              size_t nlen,
                              const ib_field_t *src);
    ib_status_t ib_field_list_add(ib_field_t *f, ib_field_t *val);
    ib_status_t ib_field_value(const ib_field_t *f, void *out_val);
    ib_status_t ib_field_mutable_value(const ib_field_t *f, 
                                       void *out_val);
    ib_status_t ib_field_setv(ib_field_t *f, void *in_pval);

    /* Context */
    ib_context_t *ib_context_engine(ib_engine_t *ib);
    ib_context_t *ib_context_main(ib_engine_t *ib);

    /* List */
    ib_status_t ib_list_create(ib_list_t **plist, ib_mpool_t *pool);
    ib_status_t ib_list_push(ib_list_t *list, void *data);
    ib_status_t ib_list_pop(ib_list_t *list, void *pdata);
    ib_status_t ib_list_unshift(ib_list_t *list, void *data);
    ib_status_t ib_list_shift(ib_list_t *list, void *pdata);
    void ib_list_clear(ib_list_t *list);
    size_t ib_list_elements(ib_list_t *list);
    ib_list_node_t *ib_list_first(ib_list_t *list);
    ib_list_node_t *ib_list_last(ib_list_t *list);
    ib_list_node_t *ib_list_node_next(ib_list_node_t *node);
    ib_list_node_t *ib_list_node_prev(ib_list_node_t *node);
    void *ib_list_node_data(ib_list_node_t *node);

    /* Providers */
    ib_status_t ib_provider_define(ib_engine_t *ib,
                                   const char *type,
                                   ib_provider_register_fn_t fn_reg,
                                   void *api);
    ib_status_t ib_provider_register(ib_engine_t *ib,
                                     const char *type,
                                     const char *key,
                                     ib_provider_t **ppr,
                                     void *iface,
                                     ib_provider_inst_init_fn_t fn_init);
    ib_status_t ib_provider_lookup(ib_engine_t *ib,
                                   const char *type,
                                   const char *key,
                                   ib_provider_t **ppr);
    ib_status_t ib_provider_instance_create(ib_engine_t *ib,
                                            const char *type,
                                            const char *key,
                                            ib_provider_inst_t **ppi,
                                            ib_mpool_t *pool,
                                            void *data);

    /* Matchers */
    ib_status_t ib_matcher_create(ib_engine_t *ib,
                                  ib_mpool_t *pool,
                                  const char *key,
                                  ib_matcher_t **pm);
    void *ib_matcher_compile(ib_matcher_t *m,
                             const char *patt,
                             const char **errptr,
                             int *erroffset);
    ib_status_t ib_matcher_match_buf(ib_matcher_t *m,
                                     void *cpatt,
                                     ib_flags_t flags,
                                     const uint8_t *data,
                                     size_t dlen);
    ib_status_t ib_matcher_match_field(ib_matcher_t *m,
                                       void *cpatt,
                                       ib_flags_t flags,
                                       ib_field_t *f);


    /* Logevent */
    ib_status_t ib_logevent_create(ib_logevent_t **ple,
                                   ib_mpool_t *pool,
                                   const char *rule_id,
                                   ib_logevent_type_t type,
                                   ib_logevent_action_t rec_action,
                                   ib_logevent_action_t action,
                                   uint8_t confidence,
                                   uint8_t severity,
                                   const char *fmt,
                                   ...);
    ib_status_t ib_logevent_tag_add(ib_logevent_t *le,
                                    const char *tag);
    ib_status_t ib_logevent_field_add(ib_logevent_t *le,
                                      const char *field);
    ib_status_t ib_logevent_data_set(ib_logevent_t *le,
                                     void *data,
                                     size_t dlen);
    ib_status_t ib_event_add(ib_provider_inst_t *pi,
                             ib_logevent_t *e);
    ib_status_t ib_event_remove(ib_provider_inst_t *pi,
                                uint32_t id);
    ib_status_t ib_event_get_all(ib_provider_inst_t *pi,
                                 ib_list_t **pevents);
    ib_status_t ib_event_write_all(ib_provider_inst_t *pi);


    /* Byte String */
    size_t ib_bytestr_length(const ib_bytestr_t *bs);
    size_t ib_bytestr_size(const ib_bytestr_t *bs);
    uint8_t *ib_bytestr_ptr(ib_bytestr_t *bs);
    const uint8_t *ib_bytestr_const_ptr(const ib_bytestr_t *bs);

    /* Data Access */
    ib_status_t ib_data_get_all(ib_provider_inst_t *dpi, ib_list_t* ib_list);
    ib_status_t ib_data_get_ex(ib_provider_inst_t *dpi,
                               const char *name,
                               size_t nlen,
                               ib_field_t **pf);
    ib_status_t ib_data_tfn_get_ex(ib_provider_inst_t *dpi,
                                   const char *name,
                                   size_t nlen,
                                   ib_field_t **pf,
                                   const char *tfn);

    ib_status_t ib_data_add_nulstr_ex(ib_provider_inst_t *dpi,
                                      const char *name,
                                      size_t nlen,
                                      char *val,
                                      ib_field_t **pf);

    ib_status_t ib_data_add_num_ex(ib_provider_inst_t *dpi,
                                   const char *name,
                                   size_t nlen,
                                   ib_num_t val,
                                   ib_field_t **pf);

    ib_status_t ib_data_add_list_ex(ib_provider_inst_t *dpi,
                                 const char *name,
                                 size_t nlen,
                                 ib_field_t **pf);
    ib_status_t ib_data_remove_ex(ib_provider_inst_t *dpi,
                                  const char *name,
                                  size_t nlen,
                                  ib_field_t **pf);
    /* Transformations */
    ib_status_t ib_tfn_lookup(ib_engine_t *ib,
                              const char *name,
                              ib_tfn_t **ptfn);
    ib_status_t ib_tfn_transform(ib_tfn_t *tfn,
                                 uint8_t *data_in,
                                 size_t dlen_in,
                                 uint8_t **data_out,
                                 size_t *dlen_out,
                                 ib_flags_t *pflags);

    /* Config */
    typedef struct ib_cfgparser_t ib_cfgparser_t;
    typedef struct ib_dirmap_init_t ib_dirmap_init_t;
    typedef struct ib_site_t ib_site_t;
    typedef struct ib_loc_t ib_loc_t;
    typedef enum {
        IB_DIRTYPE_ONOFF,                    /**< Boolean param directive */
        IB_DIRTYPE_PARAM1,                   /**< One param directive */
        IB_DIRTYPE_PARAM2,                   /**< Two param directive */
        IB_DIRTYPE_LIST,                     /**< List param directive */
        IB_DIRTYPE_SBLK1,                    /**< One param subblock directive */
    } ib_dirtype_t;
    typedef ib_status_t (*ib_config_cb_blkend_fn_t)(ib_cfgparser_t *cp,
                                                    const char *name,
                                                    void *cbdata);
    typedef ib_status_t (*ib_config_cb_onoff_fn_t)(ib_cfgparser_t *cp,
                                                   const char *name,
                                                   int onoff,
                                                   void *cbdata);
    typedef ib_status_t (*ib_config_cb_param1_fn_t)(ib_cfgparser_t *cp,
                                                    const char *name,
                                                    const char *p1,
                                                    void *cbdata);
    typedef ib_status_t (*ib_config_cb_param2_fn_t)(ib_cfgparser_t *cp,
                                                    const char *name,
                                                    const char *p1,
                                                    const char *p2,
                                                    void * cbdata);
    typedef ib_status_t (*ib_config_cb_list_fn_t)(ib_cfgparser_t *cp,
                                                  const char *name,
                                                  const ib_list_t *list,
                                                  void *cbdata);
    typedef ib_status_t (*ib_config_cb_sblk1_fn_t)(ib_cfgparser_t *cp,
                                                   const char *name,
                                                   const char *p1,
                                                   void *cbdata);

    ib_status_t ib_config_register_directive(ib_engine_t *ib,
                                             const char *name,
                                             ib_dirtype_t type,
                                             ib_void_fn_t fn_config,
                                             ib_config_cb_blkend_fn_t fn_blkend,
                                             void *cbdata);

    /* Lua Specific API */
    typedef struct modlua_wrapper_cbdata_t modlua_wrapper_cbdata_t;
    struct modlua_wrapper_cbdata_t {
        const char         *fn_config_modname;
        const char         *fn_config_name;
        const char         *fn_blkend_modname;
        const char         *fn_blkend_name;
        const char         *cbdata_type;
        void               *cbdata;
    };
    ib_void_fn_t modlua_config_wrapper(void);
    ib_config_cb_blkend_fn_t modlua_blkend_wrapper(void);

    /* Misc */
    ib_status_t ib_engine_create(ib_engine_t **pib, void *server);
    ib_status_t ib_context_create_main(ib_engine_t *ib);

    /* Logging */
    void ib_log_ex(ib_engine_t *ib,
                   ib_log_level_t level,
                   const char *file,
                   int line,
                   const char *fmt,
                   ...);
    void ib_log_tx_ex(const ib_tx_t *tx,
                      ib_log_level_t level,
                      const char* file,
                      int line,
                      const char* fmt,
                      ...);
    void ib_util_log_ex(int level, const char *prefix,
                        const char *file, int line,
                        const char *fmt, ...);

    /* Mpool */
    char * ib_mpool_strdup(ib_mpool_t * mp, const char * src);
    char * ib_mpool_alloc(ib_mpool_t * mp, size_t size);
]]

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
IB_FTYPE_UNUM    = ffi.cast("int", c.IB_FTYPE_UNUM)
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

    c.ib_util_log_ex(7, "LuaFFI - ",
                     dinfo.source, dinfo.linedefined, fmt, ...)
end

function ib_log_debug(ib, fmt, ...)
    local dinfo = debug.getinfo(2)

    c.ib_log_ex(ib.cvalue(), 7, nil, "LuaFFI - ",
                 dinfo.source, dinfo.linedefined, fmt, ...)
end

function ib_log(ib, lvl, fmt, ...)
    c.ib_log_ex(ib.cvalue(), lvl, nil, "LuaFFI -", nil, 0, fmt, ...)
end

function ib_log_error(ib, fmt, ...)
    c.ib_log_ex(ib.cvalue(), 3, nil, "LuaFFI - ", nil, 0, fmt, ...)
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
        dpi = function() return newProviderInst(c_val.dpi) end,
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
        dpi = function() return newProviderInst(c_val.dpi) end,
        epi = function() return newProviderInst(c_val.epi) end,
        conn = function() return newConn(c_val.conn) end,
        id = function() return ffi.cast("const char *", c_val.id) end,
    }
end

-- ===============================================
-- Get a data field by name.
--
-- dpi: Data Provider Interface (i.e. conn.dpi() or tx.dpi())
-- name: Name of data field
-- ===============================================
function ib_data_get(dpi, name)
    local c_dpi = dpi.cvalue()
--    local c_ib = c_dpi.pr.ib
    local c_pf = ffi.new("ib_field_t*[1]")
    local rc

    -- Get the named data field.
    rc = c.ib_data_get_ex(c_dpi, name, string.len(name), c_pf)
    if rc ~= c.IB_OK then
        local c_ctx = c.ib_context_main(c_dpi.pr.ib)
        local dinfo = debug.getinfo(2)

        c.ib_log_ex(c_dpi.pr.ib, 4, nil, "LuaFFI - ",
                     dinfo.source, dinfo.linedefined, "Failed to get field \"" .. name .. "\": " .. rc)
        return nil
    end

    return newField(c_pf[0]);
end

-- ===============================================
-- Get a data field by name with a transformation.
--
-- dpi: Data Provider Interface (i.e. conn.dpi() or tx.dpi())
-- name: Name of data field
-- tfn: Comma separated tfn name string
-- ===============================================
function ib_data_tfn_get(dpi, name, tfn)
    local c_dpi = dpi.cvalue()
--    local c_ib = c_dpi.pr.ib
    local c_pf = ffi.new("ib_field_t*[1]")
    local rc

    -- Get the named data field.
    rc = c.ib_data_tfn_get_ex(c_dpi, name, string.len(name), c_pf, tfn)
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
IB_PROVIDER_TYPE_DATA      = "data"
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
                            sys_env, rec_action, action,
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
                              ffi.cast("int", action),
                              ffi.cast("uint8_t", confidence),
                              ffi.cast("uint8_t", severity),
                              fmt, ...)
    if rc ~= c.IB_OK then
        return nil
    end

    return newLogevent(c_le[0])
end

function ib_event_add(pi, e)
    local c_pi = pi.cvalue()
    local c_e = e.cvalue()

    return c.ib_event_add(c_pi, c_e)
end

function ib_event_remove(pi, id)
    local c_pi = pi.cvalue()
    local c_id = ffi.cast("uint64_t", id)

    return c.ib_event_remove(c_pi, c_id)
end

function ib_events_get_all(pi)
    local c_pi = pi.cvalue()
    local c_events = ffi.new("ib_list_t*[1]")
    local rc

    rc = c.ib_event_get_all(c_pi, c_events)

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

function ib_events_write_all(pi)
    local c_pi = pi.cvalue()

    return c.ib_event_write_all(c_pi)
end

function ib_config_register_directive(ib,
                                      name, dirtype,
                                      fn_config_name, fn_blkend_name,
                                      cbdata)
    local c_ib = ib.cvalue()
    local c_cbdata = ffi.new("modlua_wrapper_cbdata_t")
    local c_dirtype
    local cbdata_type = base.type(cbdata);
    local c_dirtype
    local dot_idx

    c_cbdata.cbdata = cbdata;
    c_cbdata.cbdata_type = cbdata_type;

    if dirtype == 1 then
        c_dirtype = IB_DIRTYPE_SBLK;
    else
        c_dirtype = IB_DIRTYPE_LIST;
    end

    if fn_config_name ~= nil then
        dot_idx = string.find(fn_config_name, ".", 1, true)
        c_cbdata.fn_config_modname = string.sub(fn_config_name, 1, dot_idx - 1)
        c_cbdata.fn_config_name = string.sub(fn_config_name, (string.len(fn_config_name) - dot_idx) * -1)
    else
        c_cbdata.fn_config_modname = nil;
        c_cbdata.fn_config_name = nil;
    end

    if fn_blkend_name ~= nil then
        dot_idx = string.find(fn_blkend_name, ".", 1, true)
        c_cbdata.fn_blkend_modname = string.sub(fn_blkend_name, 1, dot_idx - 1)
        c_cbdata.fn_blkend_name = string.sub(fn_blkend_name, (string.len(fn_blkend_name) - dot_idx) * -1)
    else
        c_cbdata.fn_blkend_modname = nil;
        c_cbdata.fn_blkend_name = nil;
    end

    return c.ib_config_register_directive(c_ib,
                                          name, c_dirtype,
                                          c.modlua_config_wrapper(),
                                          c.modlua_blkend_wrapper(),
                                          c_cbdata)
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
    elseif c_event == c.conn_opened_event then
        l_arg = newConn(arg)
    elseif c_event == c.conn_data_in_event then
        l_arg = newConnData(arg)
    elseif c_event == c.conn_data_out_event then
        l_arg = newConnData(arg)
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
