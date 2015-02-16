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

typedef enum {LE_N, LE_RN, LE_ANY} http_lineend_t;

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
    size_t bytes_notified;
    size_t backlog;

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

    /* async notifications */
    struct {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
    } rendezvous;
    int busy:1;
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

/* Wrappers for Ironbee notifications */

/* turn this off to fall back to pre-2015 synchronous notification */
#define ASYNC_NOTIFICATIONS

#ifdef ASYNC_NOTIFICATIONS
ib_status_t tsib_notification_init(ib_engine_t *ib, int nthreads);

#if 0
  /* Would be nice to macro-ise fully, but not while these functions
   * are used as data members of a struct that relies on their signature
   */
ib_status_t tsib_notify_tx(ib_tx_t *tx, void *call, const void *arg3, size_t arg4);
ib_status_t tsib_notify_conn(ib_conn_t *conn, void *call);

#define tsib_notify2(fn,tx) tsib_notify_tx(tx,(void*)fn,NULL,(size_t)-1)
#define tsib_notify3(fn,tx,arg) tsib_notify_tx(tx,(void*)fn,arg,(size_t)-1)
#define tsib_notify4(fn,tx,arg,sz) tsib_notify_tx(tx,(void*)fn,arg,sz)

#define tsib_state_notify_request_header_data(ib,tx,hdr) \
	tsib_notify3(ib_state_notify_request_header_data,tx,hdr)
#define tsib_state_notify_request_header_finished(ib,tx) \
	tsib_notify2(ib_state_notify_request_header_finished,tx)
#define tsib_state_notify_request_body_data(ib,tx,p,len) \
	tsib_notify4(ib_state_notify_request_body_data,tx,p,len)
#define tsib_state_notify_request_finished(ib,tx) \
	tsib_notify2(ib_state_notify_request_finished,tx)
#define tsib_state_notify_response_header_data(ib,tx,hdr) \
	tsib_notify3(ib_state_notify_response_header_data,tx,hdr)
#define tsib_state_notify_response_header_finished(ib,tx) \
	tsib_notify2(ib_state_notify_response_header_finished,tx)
#define tsib_state_notify_response_body_data(ib,tx,data,len) \
	tsib_notify4(ib_state_notify_response_body_data,tx,data,len)
#define tsib_state_notify_response_finished(ib,tx) \
	tsib_notify2(ib_state_notify_response_finished,tx)
#define tsib_state_notify_postprocess(ib,tx) \
        tsib_notify2(ib_state_notify_postprocess,tx)
#define tsib_state_notify_logging(ib,tx) \
        tsib_notify2(ib_state_notify_logging,tx)
#define tsib_state_notify_request_started(ib,tx,x) \
	tsib_notify3(ib_state_notify_request_started,tx,x)
#define tsib_state_notify_response_started(ib,tx,x) \
	tsib_notify3(ib_state_notify_response_started,tx,x)
#define tsib_state_notify_conn_opened(ib,conn) \
	tsib_notify_conn(conn, (void*)ib_state_notify_conn_opened)
#define tsib_state_notify_conn_closed(ib,conn) \
	tsib_notify_conn(conn, (void*)ib_state_notify_conn_closed)
#else
ib_status_t tsib_state_notify_request_header_data(ib_engine_t *ib, ib_tx_t *tx, ib_parsed_headers_t *hdr);
ib_status_t tsib_state_notify_request_header_finished(ib_engine_t *ib, ib_tx_t *tx);
ib_status_t tsib_state_notify_request_body_data(ib_engine_t *ib, ib_tx_t *tx,const char *data,size_t len);
ib_status_t tsib_state_notify_request_finished(ib_engine_t *ib, ib_tx_t *tx);
ib_status_t tsib_state_notify_response_header_data(ib_engine_t *ib, ib_tx_t *tx,ib_parsed_headers_t *hdr);
ib_status_t tsib_state_notify_response_header_finished(ib_engine_t *ib, ib_tx_t *tx);
ib_status_t tsib_state_notify_response_body_data(ib_engine_t *ib, ib_tx_t *tx,const char *data,size_t len);
ib_status_t tsib_state_notify_response_finished(ib_engine_t *ib, ib_tx_t *tx);
ib_status_t tsib_state_notify_postprocess(ib_engine_t *ib, ib_tx_t *tx);
ib_status_t tsib_state_notify_logging(ib_engine_t *ib, ib_tx_t *tx);
ib_status_t tsib_state_notify_request_started(ib_engine_t *ib, ib_tx_t *tx,void *x);
ib_status_t tsib_state_notify_response_started(ib_engine_t *ib, ib_tx_t *tx,void *x);
ib_status_t tsib_state_notify_conn_opened(ib_engine_t *ib,ib_conn_t *conn);
ib_status_t tsib_state_notify_conn_closed(ib_engine_t *ib,ib_conn_t *conn);

void tsib_rendezvous(tsib_txn_ctx *txndata, unsigned int event, int mode);
#endif


#else
/* Fall back to synchronous notification, which we know works */
#define tsib_state_notify_request_header_data ib_state_notify_request_header_data
#define tsib_state_notify_request_header_finished ib_state_notify_request_header_finished
#define tsib_state_notify_request_body_data ib_state_notify_request_body_data
#define tsib_state_notify_request_finished ib_state_notify_request_finished
#define tsib_state_notify_response_header_data ib_state_notify_response_header_data
#define tsib_state_notify_response_header_finished ib_state_notify_response_header_finished
#define tsib_state_notify_response_body_data ib_state_notify_response_body_data
#define tsib_state_notify_response_finished ib_state_notify_response_finished
#define tsib_state_notify_postprocess ib_state_notify_postprocess
#define tsib_state_notify_logging ib_state_notify_logging
#define tsib_state_notify_request_started ib_state_notify_request_started
#define tsib_state_notify_response_started ib_state_notify_response_started
#define tsib_state_notify_conn_opened ib_state_notify_conn_opened
#define tsib_state_notify_conn_closed ib_state_notify_conn_closed
#endif

#endif
