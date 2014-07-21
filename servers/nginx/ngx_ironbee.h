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
#include <ironbee/config.h>
#include <ironbee/engine_state.h>
#include <ironbee/engine_types.h>
#include <ironbee/engine.h>

/* HTTP statuses we'll support when IronBee asks us to return them */
#define STATUS_IS_ERROR(code) ( ((code) >= 200) && ((code) <  600) )

/* Placeholder for generic conversion of an IronBee error to an nginx error */
#define IB2NG(x) x

/* A struct used to track a connection across requests (required because
 * there's no connection API in nginx).
 */
typedef struct ngxib_conn_t ngxib_conn_t;

/* Buffering status for request/response bodies */
typedef enum { IOBUF_NOBUF, IOBUF_DISCARD, IOBUF_BUFFER_ALL,
               IOBUF_BUFFER_FLUSHALL, IOBUF_BUFFER_FLUSHPART } iobuf_t;
#define IOBUF_BUFFERED(x) (((x) == IOBUF_BUFFER_ALL) || ((x) == IOBUF_BUFFER_FLUSHALL) || ((x) == IOBUF_BUFFER_FLUSHPART))

/* The main per-request record for the plugin */
typedef struct ngxib_req_ctx {
    ngx_http_request_t *r;        /* The nginx request struct */
    ngxib_conn_t *conn;           /* Connection tracking */
    ib_tx_t *tx;                  /* The IronBee request struct */
    int status;                   /* Request status set by ironbee */
    iobuf_t output_buffering;     /* Output buffer management */
    ngx_chain_t *response_buf;    /* Output buffer management */
    ngx_chain_t *response_ptr;    /* Output buffer management */
    size_t output_buffered;       /* Output buffer management */
    size_t output_limit;          /* Output buffer management */
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

/**
 * Acquire an IronBee engine
 *
 * @param[out] pengine Pointer to acquired engine
 * @param[in] log Nginx log object
 *
 * @returns Status code
 */
ib_status_t ngxib_acquire_engine(
    ib_engine_t  **pengine,
    ngx_log_t     *log
);

/**
 * Release an IronBee engine
 *
 * @param[in] engine Engine to release
 * @param[in] log Nginx log object
 *
 * @returns Status code
 */
ib_status_t ngxib_release_engine(
    ib_engine_t   *engine,
    ngx_log_t     *log
);

/**
 * function to return the ironbee connection rec after ensuring it exists
 *
 * Determine whether a connection is known to IronBee.  If yes, retrieve it;
 * if no then initialize it and retrieve it.
 *
 * This function will acquire an engine from the engine manager if required.
 *
 * Since nginx has no connection API, we have to hook into each request.
 * This function looks to see if the IronBee connection rec has already
 * been initialized, and if so returns it.  If it doesn't yet exist,
 * it will be created and IronBee notified of the new connection.
 * A cleanup is added to nginx's connection pool, and is also used
 * in ngxib_conn_get to search for the connection.
 *
 * @param[in] rctx  The module request ctx
 * @return          The ironbee connection
 */
ib_conn_t *ngxib_conn_get(ngxib_req_ctx *rctx);

/* IronBee's callback to initialize its connection rec */
ib_status_t ngxib_conn_init(ib_engine_t *ib,
                            ib_conn_t *iconn,
                            ib_state_t state,
                            void *cbdata);

/**
 * Export the server object
 */
ib_server_t *ib_plugin(void);

int ngxib_has_request_body(ngx_http_request_t *r, ngxib_req_ctx *ctx);




/* Misc symbols that need visibility across source files */
extern ngx_module_t  ngx_ironbee_module;         /* The module struct */
ngx_int_t ngxib_handler(ngx_http_request_t *r);  /* Handler for Request Data */

/* new stuff for module */
typedef struct module_data_t {
    struct ib_manager_t   *manager;      /**< IronBee engine manager object */
    int                    ib_log_active;
    ngx_log_t             *log;
    int                    log_level;
} module_data_t;

ib_status_t ngxib_module(ib_module_t**, ib_engine_t*, void*);


/* Return from a function that has set globals, ensuring those
 * globals are tidied up after use.  An ugly but necessary hack.
 * Would become completely untenable if a threaded nginx happens.
 */
//#define cleanup_return(log) return ngxib_log(log),ngx_regex_malloc_done(),
#define cleanup_return return ngx_regex_malloc_done(),


#endif
