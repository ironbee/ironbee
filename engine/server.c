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
 * @brief IronBee -- Server API
 *
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/server.h>

ib_status_t ib_server_error_response(
    const ib_server_t *svr,
    ib_tx_t           *tx,
    int                status
)
{
    return (svr && svr->err_fn) ?
           svr->err_fn(tx, status, svr->err_data) :
           IB_ENOTIMPL;
}

ib_status_t ib_server_error_header(
    const ib_server_t *svr,
    ib_tx_t           *tx,
    const char        *name,
    size_t             name_length,
    const char        *value,
    size_t             value_length
)
{
    return (svr && svr->err_hdr_fn) ?
           svr->err_hdr_fn(
               tx,
               name, name_length,
               value, value_length,
               svr->err_hdr_data
           ) :
           IB_ENOTIMPL;
}

ib_status_t ib_server_error_body(
    const ib_server_t *svr,
    ib_tx_t           *tx,
    const char        *data,
    size_t             dlen
)
{
    return (svr && svr->err_body_fn) ?
           svr->err_body_fn(tx, data, dlen, svr->err_body_data) :
           IB_ENOTIMPL;
}

ib_status_t ib_server_header(
    const ib_server_t         *svr,
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    ib_server_header_action_t  action,
    const char                *name,
    size_t                     name_length,
    const char                *value,
    size_t                     value_length,
    ib_rx_t                   *rx
)
{
    return (svr && svr->hdr_fn) ?
           svr->hdr_fn(
               tx,
               dir,
               action,
               name, name_length,
               value, value_length,
               rx,
               svr->hdr_data
           ) :
           IB_ENOTIMPL;
}

#ifdef HAVE_FILTER_DATA_API

ib_status_t ib_server_filter_init(
    const ib_server_t *svr,
    ib_tx_t           *tx,
    ib_direction_t     dir
)
{
    return (svr && svr->init_fn) ?
           svr->init_fn(tx, dir, svr->init_data) :
           IB_ENOTIMPL;
}

ib_status_t ib_server_filter_data(
    const ib_server_t *svr,
    ib_tx_t           *tx,
    ib_direction_t     dir,
    const char        *block,
    size_t             len
)
{
    return (svr && svr->data_fn) ?
           svr->data_fn(tx, dir, data, len, svr->data_data) :
           IB_ENOTIMPL;
}

#endif /* HAVE_FILTER_DATA_API */

ib_status_t ib_server_close(
    const ib_server_t *svr,
    ib_conn_t         *conn,
    ib_tx_t           *tx
)
{
    return (svr && svr->close_fn) ?
           svr->close_fn(conn, tx, svr->close_data) :
           IB_ENOTIMPL;
}
