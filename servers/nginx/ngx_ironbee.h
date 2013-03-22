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
 * @brief IronBee --- nginx 1.3 module
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#ifndef NGXIB_H
#define NGXIB_H

#include <ngx_http.h>
#include <ironbee/engine.h>
#include <ironbee/provider.h>

/* HTTP statuses we'll support when Ironbee asks us to return them */
#define STATUS_IS_ERROR(code) ( ((code) >= 200) && ((code) <  600) )

/* Placeholder for generic conversion of an Ironbee error to an nginx error */
#define IB2NG(x) x

/* A struct used to track a connection across requests (required because
 * there's no connection API in nginx).
 */
typedef struct ngxib_conn_t ngxib_conn_t;

/* Buffering status for request/response bodies */
typedef enum { IOBUF_NOBUF, IOBUF_DISCARD, IOBUF_BUFFER } iobuf_t;

/* The main per-request record for the plugin */
typedef struct ngxib_req_ctx {
    ngx_http_request_t *r;        /* The nginx request struct */
    ngxib_conn_t *conn;           /* Connection tracking */
    ib_tx_t *tx;                  /* The Ironbee request struct */
    int status;                   /* Request status set by ironbee */
    iobuf_t output_buffering;     /* Output buffer management */
    ngx_chain_t *response_buf;    /* Output buffer management */
    ngx_chain_t *response_ptr;    /* Output buffer management */
    int body_done:1;              /* State flags */
    int body_wait:1;              /* State flags */
    int has_request_body:1;       /* State flags */
    int tested_request_body:1;    /* State flags */
    int output_filter_init:1;     /* State flags */
    int output_filter_done:1;     /* State flags */
    int hdrs_in:1;                /* State flags */
    int hdrs_out:1;               /* State flags */
    int start_response:1;         /* State flags */
    int internal_errordoc:1;      /* State flags */
} ngxib_req_ctx;

/* Determine whether a connection is known to Ironbee.  If yes, retrieve it;
 * if no then initialise it and retreive it.
 */
ib_conn_t *ngxib_conn_get(ngxib_req_ctx *rctx, ib_engine_t *ib);

/* Ironbee's callback to initialise its connection rec */
ib_status_t ngxib_conn_init(ib_engine_t *ib,
                            ib_state_event_type_t event,
                            ib_conn_t *iconn,
                            void *cbdata);

/* Ironbee log function to write to nginx's error log */
void ngxib_logger(const ib_engine_t *ib, ib_log_level_t level,
                  const char *file, int line, const char *fmt,
                  va_list ap, void *dummy);

/* Dummy funvtion to set Ironbee log level */
ib_log_level_t ngxib_loglevel(const ib_engine_t *ib, void *cbdata);

/* Set/retrieve global log descriptor.  A fudge for the absence of appdata
 * pointer in ironbee logger API.
 */
ngx_log_t *ngxib_log(ngx_log_t *log);

int ngxib_has_request_body(ngx_http_request_t *r, ngxib_req_ctx *ctx);



/* Misc symbols that need visibility across source files */
extern ngx_module_t  ngx_ironbee_module;         /* The module struct */
ngx_int_t ngxib_handler(ngx_http_request_t *r);  /* Handler for Request Data */
ib_engine_t *ngxib_engine(void);                 /* The ironbee engine */
ib_server_t *ngxib_server(void);                 /* The ironbee server */


/* Return from a function that has set globals, ensuring those
 * globals are tidied up after use.  An ugly but necessary hack.
 * Would become completely untenable if a threaded nginx happens.
 */
#define cleanup_return(log) return ngxib_log(log),ngx_regex_malloc_done(),


#endif
