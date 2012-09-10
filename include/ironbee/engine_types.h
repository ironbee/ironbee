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

#include <ironbee/clock.h>
#include <ironbee/hash.h>
#include <ironbee/mpool.h>
#include <ironbee/parsed_content.h>
#include <ironbee/rule_defs.h>
#include <ironbee/stream.h>
#include <ironbee/types.h>
#include <ironbee/uuid.h>

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

/** Provider Definition */
typedef struct ib_provider_def_t ib_provider_def_t;
/** Provider */
typedef struct ib_provider_t ib_provider_t;
/** Provider Instance */
typedef struct ib_provider_inst_t ib_provider_inst_t;

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
typedef struct ib_conndata_t ib_conndata_t;
typedef struct ib_txdata_t ib_txdata_t;
typedef struct ib_tx_t ib_tx_t;
typedef struct ib_site_t ib_site_t;
typedef struct ib_loc_t ib_loc_t;
typedef struct ib_tfn_t ib_tfn_t;
typedef struct ib_logevent_t ib_logevent_t;
typedef struct ib_auditlog_t ib_auditlog_t;
typedef struct ib_auditlog_part_t ib_auditlog_part_t;

typedef struct ib_filter_t ib_filter_t;
typedef struct ib_fdata_t ib_fdata_t;
typedef struct ib_fctl_t ib_fctl_t;

typedef enum {
    IB_FILTER_CONN,
    IB_FILTER_TX,
} ib_filter_type_t;

#define IB_UUID_HEX_SIZE 37

/* Connection Flags */
/// @todo Do we need anymore???
#define IB_CONN_FNONE           (0)
#define IB_CONN_FERROR          (1 << 0) /**< Connection had an error */
#define IB_CONN_FSEENTX         (1 << 1) /**< Connection had transaction */
#define IB_CONN_FSEENDATAIN     (1 << 2) /**< Connection had data in */
#define IB_CONN_FSEENDATAOUT    (1 << 3) /**< Connection had data out */
#define IB_CONN_FOPENED         (1 << 4) /**< Connection opened */
#define IB_CONN_FCLOSED         (1 << 5) /**< Connection closed */

/* Transaction Flags */
#define IB_TX_FNONE             (0)
#define IB_TX_FERROR            (1 <<  0) /**< Transaction had an error */
#define IB_TX_FHTTP09           (1 <<  1) /**< Transaction is HTTP/0.9 */
#define IB_TX_FPIPELINED        (1 <<  2) /**< Transaction is pipelined */
#define IB_TX_FPARSED_DATA      (1 <<  3) /**< Transaction with parsed data */
#define IB_TX_FREQ_STARTED      (1 <<  6) /**< Request started */
#define IB_TX_FREQ_SEENHEADER   (1 <<  7) /**< Request header seen */
#define IB_TX_FREQ_NOBODY       (1 <<  8) /**< Request should not have body */
#define IB_TX_FREQ_SEENBODY     (1 <<  9) /**< Request body seen */
#define IB_TX_FREQ_SEENTRAILER  (1 << 10) /**< Request trailer seen */
#define IB_TX_FREQ_FINISHED     (1 << 11) /**< Request finished */
#define IB_TX_FRES_STARTED      (1 << 12) /**< Response started */
#define IB_TX_FRES_SEENHEADER   (1 << 13) /**< Response header seen */
#define IB_TX_FRES_SEENBODY     (1 << 14) /**< Response body seen */
#define IB_TX_FRES_SEENTRAILER  (1 << 15) /**< Response trailer seen */
#define IB_TX_FRES_FINISHED     (1 << 16) /**< Response finished  */
#define IB_TX_FSUSPICIOUS       (1 << 17) /**< Transaction is suspicious */
#define IB_TX_BLOCK_ADVISORY    (1 << 18) /**< Blocking this tx is advised */
#define IB_TX_BLOCK_PHASE       (1 << 19) /**< Block tx after this phase */
#define IB_TX_BLOCK_IMMEDIATE   (1 << 20) /**< Block tx ASAP */
#define IB_TX_ALLOW_PHASE       (1 << 21) /**< Allow current phase */
#define IB_TX_ALLOW_REQUEST     (1 << 22) /**< Allow all request phases */
#define IB_TX_ALLOW_ALL         (1 << 23) /**< Allow transaction */
#define IB_TX_FPOSTPROCESS      (1 << 24) /**< Post-processing occurred */

/** Capture collection name */
#define IB_TX_CAPTURE           "CAPTURE" /**< Name of the capture collection */
#define IB_DATA_MAX_CAPTURE_NAME       32 /**< Max capture name */

/** Configuration Context Type */
/// @todo Perhaps "context scope" is better (CSCOPE)???
typedef enum {
    IB_CTYPE_ENGINE,
//    IB_CTYPE_SESS,
    IB_CTYPE_CONN,
    IB_CTYPE_TX,
//    IB_CTYPE_CUSTOM,
} ib_ctype_t;

/** Connection Data Structure */
struct ib_conndata_t {
    ib_conn_t          *conn;            /**< Connection */
    size_t              dlen;            /**< Data buffer length */
    uint8_t            *data;            /**< Data buffer */
};

/** Transaction Data Structure */
struct ib_txdata_t {
    size_t              dlen;            /**< Data buffer length */
    uint8_t            *data;            /**< Data buffer */
};

/** Connection Structure */
struct ib_conn_t {
    ib_engine_t        *ib;              /**< Engine handle */
    ib_mpool_t         *mp;              /**< Connection memory pool */
    ib_context_t       *ctx;             /**< Config context */
    void               *server_ctx;      /**< Server context */
    void               *parser_ctx;      /**< Parser context */
    ib_provider_inst_t *dpi;             /**< Data provider instance */
    ib_hash_t          *data;            /**< Generic data store */
//    ib_filter_ctl_t    *fctl;            /**< Connection filter controller */

    ib_timeval_t       tv_created;       /**< Connection created time value */
    struct {
        ib_time_t       started;         /**< Connection started base time */
        // TODO: Is opened/closed different than started/finished?
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

/** Transaction Structure */
struct ib_tx_t {
    ib_engine_t        *ib;              /**< Engine handle */
    ib_mpool_t         *mp;              /**< Transaction memory pool */
    const char         *id;              /**< ID: @sa ib_tx_generate_id() */
    ib_conn_t          *conn;            /**< Connection */
    ib_context_t       *ctx;             /**< Config context */
    void               *sctx;            /**< Server context */
    ib_provider_inst_t *dpi;             /**< Data provider instance */
    ib_provider_inst_t *epi;             /**< Log event provider instance */
    ib_hash_t          *data;            /**< Generic data store */
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
        ib_time_t       logtime;         /**< Auditlog time */
        ib_time_t       finished;        /**< Tx (response) finished time */
    } t;                                 /**< Monotonic clock times */
    ib_tx_t            *next;            /**< Next transaction */
    const char         *hostname;        /**< Hostname used in the request */
    const char         *er_ipstr;        /**< Effective remote IP as string */
    const char         *path;            /**< Path used in the request */
    //struct sockaddr_storage er_addr;   /**< Effective remote address */
    ib_flags_t          flags;           /**< Transaction flags */
    int                 block_status;    /**< TX specific block status to use.*/
    ib_rule_phase_num_t allow_phase;     /**< Phase to allow (skip) */
    ib_rule_log_tx_t   *rule_log_tx;     /**< Rule engine TX log object */

    /* Request */
    ib_parsed_req_line_t *request_line;  /**< Request line */
    ib_parsed_header_wrapper_t *request_header;/**< Request header */
    ib_stream_t        *request_body;    /**< Request body (up to a limit) */

    /* Response */
    ib_parsed_resp_line_t *response_line;/**< Response line */
    ib_parsed_header_wrapper_t *response_header;/**< Response header */
    ib_stream_t        *response_body;   /**< Response body (up to a limit) */
};

/** Site Structure */
struct ib_site_t {
    ib_uuid_t               id;           /**< Site UUID */
    const char              *id_str;      /**< ascii format, for logging */
    ib_engine_t             *ib;          /**< Engine */
    ib_mpool_t              *mp;          /**< Memory pool */
    const char              *name;        /**< Site name */
    /// @todo IPs needs to be IP:Port and be associated with a host
    ib_list_t               *ips;         /**< IP addresses */
    ib_list_t               *hosts;       /**< Hostnames */
    ib_list_t               *locations;   /**< List of locations */
    ib_loc_t                *default_loc; /**< Default location */
};

/** Location Structure */
struct ib_loc_t {
    ib_site_t               *site;        /**< Site */
    /// @todo: use regex
    const char              *path;        /**< Location path */
};


/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _IB_ENGINE_TYPES_H_ */
