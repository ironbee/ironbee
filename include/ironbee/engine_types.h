/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ****************************************************************************/

#ifndef _IB_ENGINE_TYPES_H_
#define _IB_ENGINE_TYPES_H_

/**
 * @file
 * @brief IronBee --- Engine Types.
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include <ironbee/array.h>
#include <ironbee/clock.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/parsed_content.h>
#include <ironbee/rule_defs.h>
#include <ironbee/stream.h>
#include <ironbee/types.h>
#include <ironbee/uuid.h>
#include <ironbee/var.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup IronBeeEngine
 * @{
 */

/** Engine Handle */
typedef struct ib_engine_t ib_engine_t;

/** Module Structure */
typedef struct ib_module_t ib_module_t;

/** Configuration Context. */
typedef struct ib_context_t ib_context_t;

/** Default String Values */
typedef struct ib_default_string_t ib_default_string_t;
struct ib_default_string_t {
    const char *empty;
    const char *unknown;
    const char *core;
    const char *root_path;
    const char *uri_root_path;
};
extern const ib_default_string_t ib_default_string;
#define IB_DSTR_EMPTY ib_default_string.empty
#define IB_DSTR_UNKNOWN ib_default_string.unknown
#define IB_DSTR_CORE ib_default_string.core
#define IB_DSTR_ROOT_PATH ib_default_string.root_path
#define IB_DSTR_URI_ROOT_PATH ib_default_string.uri_root_path

/* Public type declarations */
typedef struct ib_conn_t ib_conn_t;
typedef struct ib_tx_t ib_tx_t;
typedef struct ib_tfn_t ib_tfn_t;
typedef struct ib_logevent_t ib_logevent_t;
typedef struct ib_auditlog_t ib_auditlog_t;
typedef struct ib_auditlog_part_t ib_auditlog_part_t;
typedef struct ib_cfgparser_t ib_cfgparser_t;

typedef struct ib_site_t ib_site_t;
typedef struct ib_site_service_t ib_site_service_t;
typedef struct ib_site_host_t ib_site_host_t;
typedef struct ib_site_location_t ib_site_location_t;

typedef struct ib_filter_t ib_filter_t;
typedef struct ib_fdata_t ib_fdata_t;
typedef struct ib_fctl_t ib_fctl_t;

typedef enum {
    IB_FILTER_CONN,
    IB_FILTER_TX,
} ib_filter_type_t;

#define IB_UUID_HEX_SIZE 37

/* Connection Flags */
// @todo Do we need anymore???
#define IB_CONN_FNONE           (0)
#define IB_CONN_FERROR          (1 << 0) /**< Connection had an error */
#define IB_CONN_FSEENTX         (1 << 1) /**< Connection had transaction */
#define IB_CONN_FSEENDATAIN     (1 << 2) /**< Connection had data in */
#define IB_CONN_FSEENDATAOUT    (1 << 3) /**< Connection had data out */
#define IB_CONN_FOPENED         (1 << 4) /**< Connection opened */
#define IB_CONN_FCLOSED         (1 << 5) /**< Connection closed */

/* Transaction Flags */
#define IB_TX_FNONE              (0ULL)
#define IB_TX_FERROR             (1ULL <<  0) /**< Transaction had an error */
#define IB_TX_FHTTP09            (1ULL <<  1) /**< Transaction is HTTP/0.9 */
#define IB_TX_FPIPELINED         (1ULL <<  2) /**< Transaction is pipelined */
#define IB_TX_FREQ_STARTED       (1ULL <<  3) /**< Request started */
#define IB_TX_FREQ_SEENLINE      (1ULL <<  4) /**< Request line seen */
#define IB_TX_FREQ_SEENHEADER    (1ULL <<  5) /**< Request header seen */
#define IB_TX_FREQ_NOBODY        (1ULL <<  6) /**< Req should not have body */
#define IB_TX_FREQ_SEENBODY      (1ULL <<  7) /**< Request body seen */
#define IB_TX_FREQ_SEENTRAILER   (1ULL <<  8) /**< Request trailer seen */
#define IB_TX_FREQ_FINISHED      (1ULL <<  9) /**< Request finished */
#define IB_TX_FREQ_HAS_DATA      (1ULL << 10) /**< Non-empty request */
#define IB_TX_FRES_STARTED       (1ULL << 11) /**< Response started */
#define IB_TX_FRES_SEENLINE      (1ULL << 12) /**< Response line seen */
#define IB_TX_FRES_SEENHEADER    (1ULL << 13) /**< Response header seen */
#define IB_TX_FRES_SEENBODY      (1ULL << 14) /**< Response body seen */
#define IB_TX_FRES_SEENTRAILER   (1ULL << 15) /**< Response trailer seen */
#define IB_TX_FRES_FINISHED      (1ULL << 16) /**< Response finished  */
#define IB_TX_FRES_HAS_DATA      (1ULL << 17) /**< Non-empty response */
#define IB_TX_FSUSPICIOUS        (1ULL << 18) /**< Transaction is suspicious */
#define IB_TX_BLOCK_ADVISORY     (1ULL << 19) /**< Blocking is advised */
#define IB_TX_BLOCK_PHASE        (1ULL << 20) /**< Block tx after this phase */
#define IB_TX_BLOCK_IMMEDIATE    (1ULL << 21) /**< Block tx ASAP */
#define IB_TX_ALLOW_PHASE        (1ULL << 22) /**< Allow current phase */
#define IB_TX_ALLOW_REQUEST      (1ULL << 23) /**< Allow all request phases */
#define IB_TX_ALLOW_ALL          (1ULL << 24) /**< Allow transaction */
#define IB_TX_FPOSTPROCESS       (1ULL << 25) /**< Post-processing occurred */
#define IB_TX_FLOGGING           (1ULL << 26) /**< Logging occurred */
#define IB_TX_FINSPECT_REQHDR    (1ULL << 27) /**< Inspect request header */
#define IB_TX_FINSPECT_REQBODY   (1ULL << 28) /**< Inspect request body */
#define IB_TX_FINSPECT_RSPHDR    (1ULL << 29) /**< Inspect response header */
#define IB_TX_FINSPECT_RSPBODY   (1ULL << 30) /**< Inspect response body */
#define IB_TX_FINSPECT_REQURI    (1ULL << 31) /**< Inspect request uri. */
#define IB_TX_FINSPECT_REQPARAMS (1ULL << 32) /**< Inspect request params. */
#define IB_TX_FBLOCKING_MODE     (1ULL << 33) /**< Blocking mode employed. */
#define IB_TX_FBLOCKED           (1ULL << 34) /**< Transaction blocked. */

/** Capture collection name */
#define IB_TX_CAPTURE           "CAPTURE" /**< Name of the capture collection */
#define IB_DATA_MAX_CAPTURE_NAME       32 /**< Max capture name */

/** Configuration Context Type */
typedef enum {
    IB_CTYPE_ENGINE,
    IB_CTYPE_MAIN,
    IB_CTYPE_SITE,
    IB_CTYPE_LOCATION,
    IB_CTYPE_CUSTOM,
} ib_ctype_t;

/**
 * Enumerate the blocking mode that IronBee's core module should use.
 */
typedef enum ib_block_method_t {
    IB_BLOCK_METHOD_STATUS, /**< Block using an HTTP status code response. */
    IB_BLOCK_METHOD_CLOSE,  /**< Block by closing the transport (TCP) layer. */
} ib_block_method_t;

/** Connection Structure */
struct ib_conn_t {
    ib_engine_t        *ib;              /**< Engine handle */
    ib_mpool_t         *mp;              /**< Connection memory pool */
    ib_context_t       *ctx;             /**< Config context */
    void               *server_ctx;      /**< Server context */
    ib_array_t         *module_data;     /**< Per module data. */

    ib_timeval_t       tv_created;       /**< Connection created time value */
    struct {
        ib_time_t       started;         /**< Connection started base time */
        // @todo Is opened/closed different than started/finished?
        ib_time_t       finished;        /**< Connection finished time */
    } t;

    const char         *remote_ipstr;    /**< Remote IP as string */
    uint16_t            remote_port;     /**< Remote port */

    const char         *local_ipstr;     /**< Local IP as string */
    uint16_t            local_port;      /**< Local port */

    size_t              tx_count;        /**< Transaction count */

    ib_tx_t            *tx_first;        /**< First transaction in the list */
    ib_tx_t            *tx;              /**< Pending transaction(s) */
    ib_tx_t            *tx_last;         /**< Last transaction in the list */

    ib_flags_t          flags;           /**< Connection flags */
};

/**
 * Actions available in @ref ib_tx_limits_t when a limit is exceeded.
 */
typedef enum ib_tx_limits_actions_t {
    IB_CORE_LIMIT_ROLLOVER, /**< Continue processing. */
    IB_CORE_LIMIT_REJECT    /**< Reject the extra data. */
} ib_tx_limits_actions_t;

/**
 * Structure used in @ref ib_core_cfg_t to hold all limit values.
 */
struct ib_tx_limits_t {
    /**
     * Limit the size of the request body, in bytes.
     *
     * A value of < 0 indicates no limit.
     */
    ssize_t request_body_buffer_limit;

    /**
     * Reject or RollOver.
     */
    ib_tx_limits_actions_t request_body_buffer_limit_action;

    /**
     * Limit the size of the response body, in bytes.
     *
     * A value of < 0 indicates no limit.
     */
    ssize_t response_body_buffer_limit;

    /**
     * Reject or RollOver.
     */
    ib_tx_limits_actions_t response_body_buffer_limit_action;

    /**
     * Limit the size of the request body logged, in bytes.
     *
     * A value of < 0 indicates no limit.
     */
    ssize_t request_body_log_limit;

    /**
     * Limit the size of the response body logged, in bytes.
     *
     * A value of < 0 indicates no limit.
     */
    ssize_t response_body_log_limit;
};
typedef struct ib_tx_limits_t ib_tx_limits_t;

/** Transaction Structure */
struct ib_tx_t {
    ib_engine_t        *ib;              /**< Engine handle */
    ib_mpool_t         *mp;              /**< Transaction memory pool */
    const char         *id;              /**< ID: @sa ib_tx_generate_id() */
    ib_conn_t          *conn;            /**< Connection */
    ib_context_t       *ctx;             /**< Config context */
    void               *sctx;            /**< Server context */
    ib_var_store_t     *var_store;       /**< Var Store */
    ib_list_t          *logevents;       /**< Log events */
    ib_array_t         *module_data;     /**< Per-module data */
    ib_fctl_t          *fctl;            /**< Transaction filter controller */
    ib_timeval_t       tv_created;       /**< Tx created time value */
    struct {
        ib_time_t       started;         /**< Tx started base time */
        ib_time_t       request_started; /**< Request started time */
        ib_time_t       request_header;  /**< Request header time */
        ib_time_t       request_body;    /**< Request body time */
        ib_time_t       request_finished;/**< Request finished time */
        ib_time_t       response_started;/**< Response started time */
        ib_time_t       response_header; /**< Response header time */
        ib_time_t       response_body;   /**< Response body time */
        ib_time_t       response_finished;/**< Response finished time */
        ib_time_t       postprocess;     /**< Postprocess time */
        ib_time_t       logtime;         /**< Logging time */
        ib_time_t       finished;        /**< Tx (response) finished time */
    } t;                                 /**< Monotonic clock times */
    ib_tx_t            *next;            /**< Next transaction */
    const char         *hostname;        /**< Hostname used in the request */
    const char         *er_ipstr;        /**< Effective remote IP as string */
    const char         *path;            /**< Path used in the request */
    ib_flags_t          flags;           /**< Transaction flags */
    ib_num_t            auditlog_parts;  /**< Audit log parts */
    ib_num_t            block_status;    /**< TX-specific block status to use.*/
    ib_block_method_t   block_method;    /**< TX-specific block mode to use.*/

    ib_rule_exec_t     *rule_exec;       /**< Rule engine execution object */
    ib_list_t          *managed_collections;/**< ib_managed_collection_t list*/

    /* Limits. */

    /**
     * Limits on tx.
     *
     * Set after site selected.
     */
    ib_tx_limits_t      limits;

    /* Request */
    ib_parsed_req_line_t *request_line;  /**< Request line */
    ib_parsed_headers_t *request_header;/**< Request header */
    ib_stream_t        *request_body;    /**< Request body (up to a limit) */

    /* Response */
    ib_parsed_resp_line_t *response_line; /**< Response line */
    ib_parsed_headers_t *response_header; /**< Response header */
    ib_stream_t        *response_body;   /**< Response body (up to a limit) */
};


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_TYPES_H_ */
