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

#include <ironbee/engine.h>
#include <ironbee/provider.h>
#include <ngx_http.h>

#define STATUS_IS_ERROR(code) ( ((code) >= 200) && ((code) <  600) )
#define IB2NG(x) x

typedef struct ngxib_conn_t ngxib_conn_t;

typedef enum { IOBUF_NOBUF, IOBUF_DISCARD, IOBUF_BUFFER } iobuf_t;

typedef struct ngxib_req_ctx {
    ngx_http_request_t *r;
    ngxib_conn_t *conn;
    ib_tx_t *tx;
    int state;
    int status;
    iobuf_t output_buffering;
    ngx_chain_t *response_buf;
    ngx_chain_t *response_ptr;
    int body_done:1;
    int body_wait:1;
    int has_request_body:1;
    int tested_request_body:1;
} ngxib_req_ctx;

ib_conn_t *ngxib_conn_get(ngxib_req_ctx *rctx, ib_engine_t *ib);

ib_status_t ngxib_conn_init(ib_engine_t *ib,
                            ib_state_event_type_t event,
                            ib_conn_t *iconn,
                            void *cbdata);

ib_server_t *ngxib_server(void);

#if 0
IB_PROVIDER_IFACE_TYPE(logger) *ngxib_logger_iface(void);
#else
void ngxib_logger(const ib_engine_t *ib, ib_log_level_t level,
                  const char *file, int line, const char *fmt,
                  va_list ap, void *dummy);
ib_log_level_t ngxib_loglevel(const ib_engine_t *ib, void *cbdata);
#endif
ngx_log_t *ngxib_log(ngx_log_t *log);

ngx_int_t ngxib_handler(ngx_http_request_t *r);

extern ngx_module_t  ngx_ironbee_module;

ib_engine_t *ngxib_engine(void);


#define cleanup_return(log) return ngxib_log(log),ngx_regex_malloc_done(),

#define HDRS_IN IB_SERVER_REQUEST
#define HDRS_OUT IB_SERVER_RESPONSE
#define START_RESPONSE 0x04

#define OUTPUT_FILTER_INIT 0x100
#define OUTPUT_FILTER_DONE 0x200

#endif
