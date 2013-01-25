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


/* The connection data we're concerned with is Ironbee's iconn 
 * We need a function to retrieve it in processing a request
 *
 *  Update: this is much-simplified by the fact we have no threads
 *  and can just look through pool cleanups to find the connection.
 */


#include "ngx_ironbee.h"
#include <ironbee/state_notify.h>

struct ngxib_conn_t {
    ib_conn_t *iconn;
    ib_engine_t *ironbee;
};

static void conn_end(void *arg)
{
    ngxib_conn_t *conn = arg;

    ib_state_notify_conn_closed(conn->ironbee, conn->iconn);
    ib_conn_destroy(conn->iconn);
}

ib_conn_t *ngxib_conn_get(ngxib_req_ctx *rctx, ib_engine_t *ib)
{
    ngx_pool_cleanup_t *cln;
    ib_status_t rc;
    ngx_log_t *prev_log;

    /* Suggested by Maxim Dounin on dev list:
     * Look through pool cleanups for our conn
     * No race condition because no threads!
     */
    for (cln = rctx->r->connection->pool->cleanup;
         cln != NULL; cln = cln->next) {
        if (cln->handler == conn_end) {
            /* Our connection is already initialised and it's here */
            rctx->conn = cln->data;
            return rctx->conn->iconn;
        }
    }

    /* This connection is new, so initialise our conn struct
     * and notify Ironbee.
     *
     * No threads, so no race condition here
     */

    ngx_regex_malloc_init(rctx->r->connection->pool);
    prev_log = ngxib_log(rctx->r->connection->log);

    rctx->conn = ngx_palloc(rctx->r->connection->pool, sizeof(ngxib_conn_t));
    rctx->conn->ironbee = ib;
        
    rc = ib_conn_create(rctx->conn->ironbee, &rctx->conn->iconn, rctx->r->connection);
    ib_state_notify_conn_opened(rctx->conn->ironbee, rctx->conn->iconn);

    cln = ngx_pool_cleanup_add(rctx->r->connection->pool, 0);
    if (cln != NULL) {
        cln->handler = conn_end;
        cln->data = rctx->conn;
    }

    cleanup_return(prev_log) rctx->conn->iconn;
}

ib_status_t ngxib_conn_init(ib_engine_t *ib,
                            ib_state_event_type_t event,
                            ib_conn_t *iconn,
                            void *cbdata)
{
    unsigned char buf[INET6_ADDRSTRLEN];
    ib_status_t rc1, rc2;
    ngx_connection_t *conn = iconn->server_ctx;
    size_t len;

    /* FIXME - this is ipv4-only */
    iconn->remote_port = ((struct sockaddr_in*)conn->sockaddr)->sin_port;
    iconn->local_port = ((struct sockaddr_in*)conn->local_sockaddr)->sin_port;

    /* Get the remote address */
    len = ngx_sock_ntop(conn->sockaddr, buf, INET6_ADDRSTRLEN, 0);
    iconn->remote_ipstr = ngx_palloc(conn->pool, len+1);
    strncpy((char*)iconn->remote_ipstr, (char*)buf, len);
    rc1 = ib_data_add_bytestr(iconn->data, "remote_ip",
                              (uint8_t *)iconn->remote_ipstr, len, NULL);

    /* Get the local address.  Unfortunately this comes from config */
    len = ngx_sock_ntop(conn->local_sockaddr, buf, INET6_ADDRSTRLEN, 0);
    iconn->local_ipstr = ngx_palloc(conn->pool, len+1);
    strncpy((char*)iconn->local_ipstr, (char*)buf, len);
    rc2 = ib_data_add_bytestr(iconn->data, "local_ip",
                              (uint8_t *)iconn->local_ipstr, len, NULL);

    return (rc1 == IB_OK) ? rc2 : rc1;
}

