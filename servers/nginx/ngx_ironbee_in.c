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
#include "nginx.h"

#include <ironbee/state_notify.h>

/* Buf size for reading from temp file and feeding to IronBee */
#define BUFSIZE 65536

/**
 * Function to reset processing cycle if input data are not yet available.
 *
 * @param[in] r   the nginx request object
 */
static void ngxib_post_handler(ngx_http_request_t *r)
{
    ngxib_req_ctx *ctx = ngx_http_get_module_ctx(r, ngx_ironbee_module);
    if (ctx->body_wait) {
        ib_log_debug_tx(ctx->tx, "Waiting for more input body data.");
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
int ngxib_has_request_body(ngx_http_request_t *r, ngxib_req_ctx *ctx)
{
    if (r->headers_in.content_length_n > 0)
        return r->headers_in.content_length_n;
#if (nginx_version >= 1003000)
    else if (r->headers_in.chunked)
        return -1;
#else
    /* copied from ngx_http_request.c, though I strictly we should
     * parse the header into tokens and accept look for "chunked"
     * among the tokens, rather than assume an exact match
     */
    else if (r->headers_in.transfer_encoding != NULL
             && r->headers_in.transfer_encoding->value.len == 7
             && ngx_strncasecmp(r->headers_in.transfer_encoding->value.data,
                                (u_char *) "chunked", 7) == 0)
        return -1;
#endif
    else
        return 0;
}

/**
 * nginx handler to feed request body (if any) to IronBee
 *
 * @param[in] r   the nginx request object
 * @return    NGX_DECLINED for normal operation
 * @return    NGX_DONE if body is not yet available (processing will resume
 *            on new data)
 * @return    Error status if set by IronBee on sight of request data.
 */
ngx_int_t ngxib_handler(ngx_http_request_t *r)
{
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

    /* We already completed handling of no-body requests
     * when we looked at headers
     */
    if (!ngxib_has_request_body(r, ctx))
        return rv;

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
        cleanup_return NGX_DONE;
    }

    /* We now have the request body.  Feed it to ironbee */
    rb = r->request_body;
    if (!rb) {
        ib_log_error_tx(ctx->tx, "Failed to read request body.");
        cleanup_return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    if (!rb->bufs) {
        /* I think this shouldn't happen */
        /* But rethink if this turns up in logs when all is fine */
        ib_log_error_tx(ctx->tx, "Probable error reading request body.");
    }

    if (rb->temp_file && (rb->temp_file->file.fd != NGX_INVALID_FILE)) {
        /* Reader has put request body in temp file */
        off_t count = 0;
        u_char buf[BUFSIZE];
        size_t buf_len;
        ib_log_debug_tx(ctx->tx, "Reading request body in temp file.");
        while (buf_len = ngx_read_file(&rb->temp_file->file,
                         buf, BUFSIZE, count),
               buf_len > 0) {
            ib_log_debug_tx(ctx->tx, "Feeding %zd bytes request data to ironbee.",
                            buf_len);
            ib_state_notify_request_body_data(ctx->tx->ib, ctx->tx,
                                              (const char*)buf, buf_len);
            count += buf_len;
        }
        if ((int)buf_len == NGX_ERROR) {
            ib_log_error_tx(ctx->tx, "Failed to read request body in temp file.");
        }
    }

    for (link = rb->bufs; link != NULL; link = link->next) {
        size_t len = (link->buf->last - link->buf->pos);
        ib_log_debug_tx(ctx->tx, "Feeding %zd bytes request data to ironbee.",
                        len);
        if (len > 0) {
            ib_state_notify_request_body_data(ctx->tx->ib, ctx->tx,
                                              (const char*)link->buf->pos, len);
        }
    }
    ctx->body_done = 1;
    ib_state_notify_request_finished(ctx->tx->ib, ctx->tx);

    /* If IronBee signaled an error, we can return it */
    if (STATUS_IS_ERROR(ctx->status)) {
        rv = ctx->status;
        ctx->internal_errordoc = 1;
        ib_log_error_tx(ctx->tx, "IronBee set %d reading request body.", (int)rv);
    }

    cleanup_return rv;
}
