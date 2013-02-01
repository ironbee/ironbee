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
 * @brief IronBee --- nginx 1.3 module - intercepting incoming request payloads
 *
 * @author Nick Kew <nkew@qualys.com>
 */


#include "ngx_ironbee.h"

#include <ironbee/state_notify.h>

/**
 * Function to reset processing cycle if input data are not yet available.
 *
 * @param[in] r   the nginx request object
 */
static void ngxib_post_handler(ngx_http_request_t *r)
{
    ngxib_req_ctx *ctx = ngx_http_get_module_ctx(r, ngx_ironbee_module);
    if (ctx->body_wait) {
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                      "Waiting for more input body data");
        ctx->body_wait = 0;
        ngx_http_core_run_phases(r);
    }
}

/**
 * Function to determine whether there is a request body
 * Checks for either a Content-Length header or Chunked encoding
 *
 * @param[in] r   the nginx request object
 * @param[in] ctx the ngx request ctx for the ironbee module
 * @return    0 for no body, -1 for chunked body, or content-length
 */
static int has_request_body(ngx_http_request_t *r, ngxib_req_ctx *ctx)
{
    if (r->headers_in.content_length_n > 0)
        return r->headers_in.content_length_n;
    else if (r->headers_in.chunked)
        return -1;
    else
        return 0;
}

/**
 * nginx handler to feed request body (if any) to Ironbee
 *
 * @param[in] r   the nginx request object
 * @return    NGX_DECLINED for normal operation
 * @return    NGX_DONE if body is not yet available (processing will restart
 *            on new data)
 * @return    Error status if set by Ironbee on sight of request data.
 */
ngx_int_t ngxib_handler(ngx_http_request_t *r)
{
    ngx_log_t *prev_log;
    ib_txdata_t itxdata;
    ngx_chain_t *link;
    ngxib_req_ctx *ctx;
    ngx_int_t rv = NGX_DECLINED;
    ngx_http_request_body_t *rb;
    /* Don't process internal requests */
    if (r->internal)
        return rv;

    ctx = ngx_http_get_module_ctx(r, ngx_ironbee_module);
    if (ctx->body_done)
        return rv;

    if (!has_request_body(r, ctx))
        return rv;

    prev_log = ngxib_log(r->connection->log);
    ngx_regex_malloc_init(r->pool);

    /* We can now read the body.
     * This may come asynchronously in many chunks, so we need
     * to check for AGAIN and return DONE if waiting.
     * We pass it a handler to go round again while waiting.
     *
     * TODO: figure out how to pass data to ironbee asynchronously
     */
    rv = ngx_http_read_client_request_body(r, ngxib_post_handler);
    if (rv == NGX_AGAIN) {
        ctx->body_wait = 1;
        cleanup_return(prev_log) NGX_DONE;
    }

    /* We now have the request body.  Feed it to ironbee */
    rb = r->request_body;
    if (!rb) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Error reading request body");
        cleanup_return(prev_log) NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    if (!rb->bufs) {
        /* I think this shouldn't happen */
        /* But rethink if this turns up in logs when all is fine */
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Probable error reading request body");
    }

    if (rb->temp_file) {
        /* Reader has put request body in temp file */
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Request body in temp file: not yet supported");

    }

    for (link = rb->bufs; link != NULL; link = link->next) {
        itxdata.data = link->buf->pos;
        itxdata.dlen = (link->buf->last - link->buf->pos);
        ngx_log_error(NGX_LOG_DEBUG, r->connection->log, 0,
                      "Feeding %d bytes request data to ironbee", itxdata.dlen);
        if (itxdata.dlen > 0) {
            ngx_regex_malloc_init(r->pool);
            ib_state_notify_request_body_data(ngxib_engine(), ctx->tx, &itxdata);
            ngx_regex_malloc_done();
        }
    }
    ctx->body_done = 1;
    ib_state_notify_request_finished(ngxib_engine(), ctx->tx);

    /* If Ironbee signalled an error, we can return it */
    if (STATUS_IS_ERROR(ctx->status)) {
        rv = ctx->status;
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "Ironbee set %d reading request body", rv);
    }

    cleanup_return(prev_log) rv;
}
