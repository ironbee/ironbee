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
 * @brief IronBee --- nginx 1.3 module - connection management
 *
 * @author Nick Kew <nkew@qualys.com>
 */


/* The connection data we're concerned with is IronBee's iconn
 * We need a function to retrieve it in processing a request
 *
 *  Update: this is much-simplified by the fact we have no threads
 *  and can just look through pool cleanups to find the connection.
 */

#include <ironbee/config.h>
#include <ironbee/engine_manager.h>
#include "ngx_ironbee.h"
#include <ironbee/state_notify.h>
#include <nginx.h>

struct ngxib_conn_t {
    ib_conn_t   *iconn;
    ib_engine_t *engine;
    ngx_log_t   *log;
};

/**
 * nginx connection pool cleanup function to notify IronBee
 * and destroy the ironbee connection object.
 *
 * @param[in] arg  the connection rec
 */
static void conn_end(void *arg)
{
    ngxib_conn_t *conn = arg;
    ib_state_notify_conn_closed(conn->engine, conn->iconn);
    ib_conn_destroy(conn->iconn);
    ngxib_release_engine(conn->engine, conn->log);
}

/**
 * Initialize connection parameters
 *
 * @param[in] iconn The IronBee connection
 * @return    IB_OK or error
 */
static ib_status_t conn_init(
    ib_conn_t *iconn
)
{
    unsigned char buf[INET6_ADDRSTRLEN];
    ngx_connection_t *conn = iconn->server_ctx;
    size_t len;

    /* FIXME - this is ipv4-only */
    iconn->remote_port = ((struct sockaddr_in*)conn->sockaddr)->sin_port;
    iconn->local_port = ((struct sockaddr_in*)conn->local_sockaddr)->sin_port;

    /* Get the remote address */
#if ( nginx_version < 1005003 )
    len = ngx_sock_ntop(conn->sockaddr, buf, INET6_ADDRSTRLEN, 0);
#else
    len = ngx_sock_ntop(conn->sockaddr, conn->socklen, buf, INET6_ADDRSTRLEN, 0);
#endif
    iconn->remote_ipstr = ib_mm_memdup_to_str(iconn->mm, buf, len);
    if (iconn->remote_ipstr == NULL) {
        return IB_EALLOC;
    }

    /* Get the local address.  Unfortunately this comes from config */
#if nginx_version < 1005003
    len = ngx_sock_ntop(conn->local_sockaddr, buf, INET6_ADDRSTRLEN, 0);
#else
    len = ngx_sock_ntop(conn->local_sockaddr, conn->socklen, buf, INET6_ADDRSTRLEN, 0);
#endif
    iconn->local_ipstr = ib_mm_memdup_to_str(iconn->mm, buf, len);
    if (iconn->local_ipstr == NULL) {
        return IB_EALLOC;
    }

    return IB_OK;
}

ib_conn_t *ngxib_conn_get(ngxib_req_ctx *rctx)
{
    ngx_pool_cleanup_t *cln;
    ib_status_t rc;
    ib_engine_t *ib;

    /* Suggested by Maxim Dounin on dev list:
     * Look through pool cleanups for our conn
     * No race condition because no threads!
     */
    for (cln = rctx->r->connection->pool->cleanup;
         cln != NULL; cln = cln->next) {
        if (cln->handler == conn_end) {
            /* Our connection is already initialized and it's here */
            rctx->conn = cln->data;
            return rctx->conn->iconn;
        }
    }

    /* This connection is new, so initialize our conn struct
     * and notify IronBee.
     *
     * No threads, so no race condition here
     */

    /* Acquire an IronBee engine */
    /* n.b. pool cleanup for this and the ib_conn is conn_end, added below */
    rc = ngxib_acquire_engine(&ib, rctx->r->connection->log);
    if (rc != IB_OK) {
        cleanup_return NULL;
    }

    ngx_regex_malloc_init(rctx->r->connection->pool);

    rctx->conn = ngx_palloc(rctx->r->connection->pool, sizeof(ngxib_conn_t));
    if (rctx->conn == NULL) {
        ngxib_release_engine(ib, rctx->r->connection->log);
        cleanup_return NULL;
    }
    rctx->conn->engine = ib;
    rctx->conn->log = rctx->r->connection->log;

    rc = ib_conn_create(rctx->conn->engine, &rctx->conn->iconn, rctx->r->connection);
    if (rc != IB_OK) {
        ngxib_release_engine(ib, rctx->r->connection->log);
        cleanup_return NULL;
    }

    /* Initialize the connection */
    rc = conn_init(rctx->conn->iconn);
    if (rc != IB_OK) {
        ngxib_release_engine(ib, rctx->r->connection->log);
        cleanup_return NULL;
    }

    ib_state_notify_conn_opened(rctx->conn->engine, rctx->conn->iconn);

    cln = ngx_pool_cleanup_add(rctx->r->connection->pool, 0);
    if (cln != NULL) {
        cln->handler = conn_end;
        cln->data = rctx->conn;
    }

    cleanup_return rctx->conn->iconn;
}
