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

/**
 * @file
 * @brief IronBee --- Apache Traffic Server Plugin
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#ifndef TS_IB_H
#define TS_IB_H

#define ADDRSIZE 48 /* what's the longest IPV6 addr ? */
#define DEFAULT_LOG "ts-ironbee"
#define DEFAULT_TXLOG "txlogs/tx-ironbee"

/** Engine manager API wrappers for runtime events */
ib_status_t tsib_manager_engine_acquire(ib_engine_t**);
ib_status_t tsib_manager_engine_cleanup(void);
ib_status_t tsib_manager_engine_create(void);
ib_status_t tsib_manager_engine_release(ib_engine_t*);

typedef enum {
    HDR_OK,
    HDR_ERROR,
    HDR_HTTP_100,
    HDR_HTTP_STATUS
} tsib_hdr_outcome;
#define HDR_OUTCOME_IS_HTTP_OR_ERROR(outcome, data) \
    (((outcome) == HDR_HTTP_STATUS  || (outcome) == HDR_ERROR) && (data)->status >= 200 && (data)->status < 600)
#define HTTP_CODE(num) ((num) >= 200 && (num) < 600)

typedef struct tsib_ssn_ctx tsib_ssn_ctx;

/* a stream edit for the input or output filter */
typedef struct edit_t edit_t;
struct edit_t {
    size_t start;
    size_t bytes;
    const char *repl;
    size_t repl_len;
};

typedef struct tsib_filter_ctx tsib_filter_ctx;
struct tsib_filter_ctx {
    /* data filtering stuff */
    TSVIO output_vio;
    TSIOBuffer output_buffer;
    size_t buffered;
    /* Nobuf - no buffering
     * Discard - transmission aborted, discard remaining data
     * buffer - buffer everything until EOS or abortedby error
     */
    enum { IOBUF_NOBUF, IOBUF_DISCARD, IOBUF_BUFFER_ALL,
           IOBUF_BUFFER_FLUSHALL, IOBUF_BUFFER_FLUSHPART } buffering;
    size_t buf_limit;
    TSIOBuffer buffer;
    TSIOBufferReader reader;
    size_t bytes_done;

    ib_vector_t *edits;
    off_t offs;
    int have_edits:1;
};

#define IBD_REQ IB_SERVER_REQUEST
#define IBD_RESP IB_SERVER_RESPONSE

typedef struct hdr_action_t hdr_action_t;
struct hdr_action_t {
    ib_server_header_action_t action;
    ib_server_direction_t dir;
    const char *hdr;
    const char *value;
    struct hdr_action_t *next;
};

typedef struct hdr_list hdr_list;
struct hdr_list {
    char *hdr;
    char *value;
    struct hdr_list *next;
};

typedef struct tsib_txn_ctx tsib_txn_ctx;
struct tsib_txn_ctx {
    tsib_ssn_ctx *ssn;
    ib_tx_t *tx;
    TSHttpTxn txnp;
    tsib_filter_ctx in;
    tsib_filter_ctx out;
    int status;
    hdr_action_t *hdr_actions;
    hdr_list *err_hdrs;
    char *err_body;      /* this one can't be const */
    size_t err_body_len; /* Length of err_body. */

    TSVConn in_data_cont;
    TSVConn out_data_cont;
};

typedef struct tsib_direction_data_t tsib_direction_data_t;
struct tsib_direction_data_t {
    ib_server_direction_t dir;

    const char *type_label;
    const char *dir_label;
    TSReturnCode (*hdr_get)(TSHttpTxn, TSMBuffer *, TSMLoc *);

    ib_status_t (*ib_notify_header)(ib_engine_t*, ib_tx_t*,
                 ib_parsed_headers_t*);
    ib_status_t (*ib_notify_header_finished)(ib_engine_t*, ib_tx_t*);
    ib_status_t (*ib_notify_body)(ib_engine_t*, ib_tx_t*, const char*, size_t);
    ib_status_t (*ib_notify_end)(ib_engine_t*, ib_tx_t*);
    ib_status_t (*ib_notify_post)(ib_engine_t*, ib_tx_t*);
    ib_status_t (*ib_notify_log)(ib_engine_t*, ib_tx_t*);
};

typedef struct ibd_ctx ibd_ctx;

/* Cross-source-file interfaces */
extern ib_server_t ibplugin;

int ironbee_plugin(TSCont contp, TSEvent event, void *edata);
int out_data_event(TSCont contp, TSEvent event, void *edata);
int in_data_event(TSCont contp, TSEvent event, void *edata);
tsib_hdr_outcome process_hdr(tsib_txn_ctx *data,
                           TSHttpTxn txnp,
                           tsib_direction_data_t *ibd);

extern tsib_direction_data_t tsib_direction_client_req;
extern tsib_direction_data_t tsib_direction_client_resp;
extern tsib_direction_data_t tsib_direction_server_resp;
#endif
