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
 * @brief IronBee --- State Notification Implementation
 */

#include "ironbee_config_auto.h"

#include <ironbee/state_notify.h>
#include "state_notify_private.h"

#include "engine_private.h"

#include <ironbee/context.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/field.h>

#include <assert.h>

static ib_status_t ib_state_notify_null(
    ib_engine_t *ib,
    ib_state_event_type_t event
)
{
    assert(ib != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, event, IB_STATE_HOOK_NULL);
    if (rc != IB_OK) {
        return rc;
    }

    ib_log_debug3(ib, "NULL EVENT: %s", ib_state_event_name(event));

    IB_LIST_LOOP_CONST(ib->hooks[event], node) {
        const ib_hook_t *hook = (const ib_hook_t *)node->data;
        rc = hook->callback.null(ib, event, hook->cbdata);
        if (rc != IB_OK) {
            ib_log_error(ib, "Hook returned error: %s=%s",
                         ib_state_event_name(event),
                         ib_status_to_string(rc));
            break;
        }
    }

    return rc;
}

static ib_status_t ib_state_notify_context(
    ib_engine_t *ib,
    ib_context_t *ctx,
    ib_state_event_type_t event
)
{
    assert(ib != NULL);
    assert(ctx != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, event, IB_STATE_HOOK_CTX);
    if (rc != IB_OK) {
        return rc;
    }

    ib_log_debug3(ib, "CTX EVENT: %s", ib_state_event_name(event));

    IB_LIST_LOOP_CONST(ib->hooks[event], node) {
        const ib_hook_t *hook = (const ib_hook_t *)node->data;
        rc = hook->callback.ctx(ib, ctx, event, hook->cbdata);
        if (rc != IB_OK) {
            ib_log_error(ib, "Hook returned error: %s=%s",
                         ib_state_event_name(event),
                         ib_status_to_string(rc));
            break;
        }
    }

    return rc;
}

static ib_status_t ib_state_notify_conn(
    ib_engine_t *ib,
    ib_conn_t *conn,
    ib_state_event_type_t event
)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(conn != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, event, IB_STATE_HOOK_CONN);
    if (rc != IB_OK) {
        return rc;
    }

    ib_log_debug3(ib, "CONN EVENT: %s", ib_state_event_name(event));

    IB_LIST_LOOP_CONST(ib->hooks[event], node) {
        const ib_hook_t *hook = (const ib_hook_t *)node->data;
        rc = hook->callback.conn(ib, conn, event, hook->cbdata);
        if (rc != IB_OK) {
            ib_log_error(ib, "Hook returned error: %s=%s",
                         ib_state_event_name(event),
                         ib_status_to_string(rc));
            break;
        }
    }

    if ((rc != IB_OK) || (conn->ctx == NULL)) {
        return rc;
    }

    return rc;
}

static ib_status_t ib_state_notify_req_line(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_event_type_t event,
    ib_parsed_req_line_t *line
)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(line != NULL);
    assert(line->raw != NULL);
    assert(line->method != NULL);
    assert(line->uri != NULL);
    assert(line->protocol != NULL);

    const ib_list_node_t *node;
    ib_status_t rc;

    rc = ib_hook_check(ib, event, IB_STATE_HOOK_REQLINE);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "ib_hook_check() failed: %s",
                        ib_status_to_string(rc));
        return rc;
    }

    /* Is this a HTTP/0.9 request (has no protocol specification)? */
    if (ib_bytestr_length(line->protocol) == 0) {
        ib_tx_flags_set(tx, IB_TX_FHTTP09);
    }

    tx->request_line = line;

    IB_LIST_LOOP_CONST(ib->hooks[event], node) {
        const ib_hook_t *hook = (const ib_hook_t *)node->data;
        rc = hook->callback.requestline(ib, tx, event, line, hook->cbdata);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Hook returned error: %s=%s",
                            ib_state_event_name(event),
                            ib_status_to_string(rc));
            break;
        }
    }

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        return rc;
    }

    return rc;
}

static ib_status_t ib_state_notify_resp_line(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_state_event_type_t event,
                                             ib_parsed_resp_line_t *line)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert((line == NULL) || (line->raw != NULL));
    assert((line == NULL) || (line->protocol != NULL));
    assert((line == NULL) || (line->status != NULL));
    assert((line == NULL) || (line->msg != NULL));

    const ib_list_node_t *node;
    ib_status_t rc;

    rc = ib_hook_check(ib, event, IB_STATE_HOOK_RESPLINE);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "ib_hook_check() failed: %s",
                        ib_status_to_string(rc));
        return rc;
    }

    /* Validate response line data.
     *
     * The response line may be NULL only for HTTP/0.9 requests
     * which contain neither a line nor headers.
     */
    if ((line == NULL) && !ib_tx_flags_isset(tx, IB_TX_FHTTP09)) {
        ib_log_notice_tx(tx, "Invalid response line");
        return IB_OK;
    }

    tx->response_line = line;

    IB_LIST_LOOP_CONST(ib->hooks[event], node) {
        const ib_hook_t *hook = (const ib_hook_t *)node->data;
        rc = hook->callback.responseline(ib, tx, event, line, hook->cbdata);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Hook returned error: %s=%s",
                            ib_state_event_name(event),
                            ib_status_to_string(rc));
            break;
        }
    }

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        return rc;
    }

    return rc;
}

static ib_status_t ib_state_notify_tx(ib_engine_t *ib,
                                      ib_state_event_type_t event,
                                      ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, event, IB_STATE_HOOK_TX);
    if (rc != IB_OK) {
        return rc;
    }

    ib_log_debug3_tx(tx, "TX EVENT: %s", ib_state_event_name(event));

    /* This transaction is now the current (for pipelined). */
    tx->conn->tx = tx;

    IB_LIST_LOOP_CONST(ib->hooks[event], node) {
        const ib_hook_t *hook = (const ib_hook_t *)node->data;
        rc = hook->callback.tx(ib, tx, event, hook->cbdata);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Hook returned error: %s=%s",
                            ib_state_event_name(event),
                            ib_status_to_string(rc));
            break;
        }
    }

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        return rc;
    }

    return rc;
}

ib_status_t ib_state_notify_request_started(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_parsed_req_line_t *line)
{
    assert(ib != NULL);
    assert(tx != NULL);

    ib_status_t rc;

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FREQ_STARTED)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified event: %s",
                        ib_state_event_name(request_started_event));
        return IB_EINVAL;
    }

    /* Mark the time. */
    tx->t.request_started = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FREQ_STARTED);

    /* Notify everybody */
    rc = ib_state_notify_tx(ib, tx_started_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Notify the request line if it's present */
    if ( (line == NULL) ||
         (line->raw == NULL) ||
         (ib_bytestr_const_ptr(line->raw) == NULL) )
    {
        ib_log_debug_tx(tx, "Request started with no line");
    }
    else {
        ib_tx_flags_set(tx, IB_TX_FREQ_HAS_DATA);
        rc = ib_state_notify_req_line(ib, tx, request_started_event, line);
        if (rc != IB_OK) {
            return rc;
        }
        ib_tx_flags_set(tx, IB_TX_FREQ_SEENLINE);
    }

    return IB_OK;
}

ib_status_t ib_state_notify_conn_opened(ib_engine_t *ib,
                                        ib_conn_t *conn)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(conn != NULL);

    ib_status_t rc;

    if (ib_conn_flags_isset(conn, IB_CONN_FOPENED)) {
        ib_log_error(ib, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(conn_opened_event));
        return IB_EINVAL;
    }

    ib_conn_flags_set(conn, IB_CONN_FOPENED);

    rc = ib_state_notify_conn(ib, conn, conn_started_event);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, conn_opened_event);
    if (rc != IB_OK) {
        return rc;
    }

    /* Select the connection context to use. */
    rc = ib_ctxsel_select_context(ib, conn, NULL, &conn->ctx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, handle_context_conn_event);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, handle_connect_event);
    return rc;
}

ib_status_t ib_state_notify_conn_closed(ib_engine_t *ib,
                                        ib_conn_t *conn)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(conn != NULL);

    ib_status_t rc;

    /* Validate. */
    if (ib_conn_flags_isset(conn, IB_CONN_FCLOSED)) {
        ib_log_error(ib, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(conn_closed_event));
        return IB_EINVAL;
    }

    /* Notify any pending transaction events on connection close event. */
    if (conn->tx != NULL) {
        ib_tx_t *tx = conn->tx;

        if (    ib_tx_flags_isset(tx, IB_TX_FREQ_STARTED)
            && !ib_tx_flags_isset(tx, IB_TX_FREQ_FINISHED))
        {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_event_name(request_finished_event));
            ib_state_notify_request_finished(ib, tx);
        }

        if (    ib_tx_flags_isset(tx, IB_TX_FRES_STARTED)
            && !ib_tx_flags_isset(tx, IB_TX_FRES_FINISHED))
        {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_event_name(response_finished_event));
            ib_state_notify_response_finished(ib, tx);
        }

        if (!ib_tx_flags_isset(tx, IB_TX_FPOSTPROCESS)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_event_name(handle_postprocess_event));
            ib_state_notify_postprocess(ib, tx);
        }

        if (!ib_tx_flags_isset(tx, IB_TX_FLOGGING)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_event_name(handle_logging_event));
            ib_state_notify_logging(ib, tx);
        }
    }

    /* Mark the time. */
    conn->t.finished = ib_clock_get_time();

    ib_conn_flags_set(conn, IB_CONN_FCLOSED);

    rc = ib_state_notify_conn(ib, conn, conn_closed_event);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, handle_disconnect_event);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, conn_finished_event);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

static ib_status_t ib_state_notify_header_data(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_state_event_type_t event,
                                               ib_parsed_header_wrapper_t *header)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(header != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, event, IB_STATE_HOOK_HEADER);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "ib_hook_check() failed: %s",
                        ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug3_tx(tx, "HEADER EVENT: %s", ib_state_event_name(event));

    IB_LIST_LOOP_CONST(ib->hooks[event], node) {
        const ib_hook_t *hook = (const ib_hook_t *)node->data;
        rc = hook->callback.headerdata(ib, tx, event,
                                       header->head, hook->cbdata);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Hook returned error: %s=%s",
                            ib_state_event_name(event),
                            ib_status_to_string(rc));
            break;
        }
    }

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        return rc;
    }

    return rc;
}

static ib_status_t ib_state_notify_txdata(ib_engine_t *ib,
                                          ib_tx_t *tx,
                                          ib_state_event_type_t event,
                                          ib_txdata_t *txdata)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(txdata != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, event, IB_STATE_HOOK_TXDATA);
    if (rc != IB_OK) {
        return rc;
    }

    if (ib_log_get_level(ib) >= 9) {
        ib_log_debug3_tx(tx, "TX DATA EVENT: %s", ib_state_event_name(event));
    }

    /* This transaction is now the current (for pipelined). */
    tx->conn->tx = tx;

    IB_LIST_LOOP_CONST(ib->hooks[event], node) {
        const ib_hook_t *hook = (const ib_hook_t *)node->data;
        rc = hook->callback.txdata(ib, tx, event, txdata, hook->cbdata);
        if (rc != IB_OK) {
            ib_log_error_tx(tx, "Hook returned error: %s=%s",
                            ib_state_event_name(event),
                            ib_status_to_string(rc));
            break;
        }
    }

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        return rc;
    }

    return rc;
}

ib_status_t ib_state_notify_request_header_data(ib_engine_t *ib,
                                                ib_tx_t *tx,
                                                ib_parsed_header_wrapper_t *header)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(header != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_event_name(request_header_data_event));
        return IB_OK;
    }

    /* Mark the time. */
    if (tx->t.request_started == 0) {
        tx->t.request_started = ib_clock_get_time();
    }

    if ( tx->request_header == NULL ) {
        tx->request_header = header;
    }
    else {
        rc = ib_parsed_name_value_pair_list_append(tx->request_header,
                                                   header);

        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_header_data(ib, tx, request_header_data_event, header);
    return rc;
}

ib_status_t ib_state_notify_request_header_finished(ib_engine_t *ib,
                                                    ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_event_name(request_header_finished_event));
        return IB_OK;
    }

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FREQ_SEENHEADER)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified event: %s",
                        ib_state_event_name(request_header_finished_event));
        return IB_EINVAL;
    }

    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_STARTED)) {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_event_name(request_started_event));
        rc = ib_state_notify_request_started(ib, tx, tx->request_line);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Mark the time. */
    tx->t.request_header = ib_clock_get_time();

    /// @todo Seems this gets there too late.
    rc = ib_fctl_meta_add(tx->fctl, IB_STREAM_EOH);
    if (rc != IB_OK) {
        return rc;
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_SEENHEADER);

    rc = ib_state_notify_tx(ib, request_header_finished_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Select the transaction context to use. */
    rc = ib_ctxsel_select_context(ib, tx->conn, tx, &tx->ctx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, handle_context_tx_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_tx(ib, handle_request_header_event, tx);
    return rc;
}

ib_status_t ib_state_notify_request_body_data(ib_engine_t *ib,
                                              ib_tx_t *tx,
                                              ib_txdata_t *txdata)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(txdata != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_event_name(request_body_data_event));
        return IB_OK;
    }

    /* We should never get NULL data */
    if ( (txdata->data == NULL) || (txdata->dlen == 0) ) {
        ib_log_debug_tx(tx, "Request body data with no data.  Ignoring.");
        return IB_OK;
    }

    /* Validate. */
    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_SEENHEADER)) {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_event_name(request_header_finished_event));
        ib_state_notify_request_header_finished(ib, tx);
    }

    /* On the first call, record the time and mark that there is a body. */
    if (tx->t.request_body == 0) {
        tx->t.request_body = ib_clock_get_time();
        ib_tx_flags_set(tx, IB_TX_FREQ_SEENBODY);
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_txdata(ib, tx, request_body_data_event, txdata);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

ib_status_t ib_state_notify_request_finished(ib_engine_t *ib,
                                             ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_event_name(request_finished_event));
        return IB_OK;
    }

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FREQ_FINISHED)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified event: %s",
                        ib_state_event_name(request_finished_event));
        return IB_EINVAL;
    }

    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_SEENHEADER)) {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_event_name(request_header_finished_event));
        ib_state_notify_request_header_finished(ib, tx);
    }

    /* Mark the time. */
    tx->t.request_finished = ib_clock_get_time();

    /* Notify filters of the end-of-body (EOB) if there was a body. */
    if (ib_tx_flags_isset(tx, IB_TX_FREQ_SEENBODY) != 0) {
        rc = ib_fctl_meta_add(tx->fctl, IB_STREAM_EOB);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Notify filters of the end-of-stream (EOS). */
    rc = ib_fctl_meta_add(tx->fctl, IB_STREAM_EOS);
    if (rc != IB_OK) {
        return rc;
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_FINISHED);

    rc = ib_state_notify_tx(ib, request_finished_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, handle_request_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, tx_process_event, tx);
    return rc;
}

ib_status_t ib_state_notify_response_started(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_parsed_resp_line_t *line)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_event_name(response_started_event));
        return IB_OK;
    }

    tx->t.response_started = ib_clock_get_time();

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FRES_STARTED)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified event: %s",
                        ib_state_event_name(response_started_event));
        return IB_EINVAL;
    }

    /* If the request was started, but not finished, notify it now */
    if (    ib_tx_flags_isset(tx, IB_TX_FREQ_STARTED)
        && !ib_tx_flags_isset(tx, IB_TX_FREQ_FINISHED))
    {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_event_name(request_finished_event));
        ib_state_notify_request_finished(ib, tx);
    }

    /* Mark the time. */
    tx->t.response_started = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_STARTED);

    /* Notify the world about it */
    rc = ib_state_notify_resp_line(ib, tx, response_started_event, line);
    if (rc != IB_OK) {
        return rc;
    }

    /* Record if we saw a line. */
    if ( (line != NULL) &&
         (line->raw != NULL) &&
         (ib_bytestr_const_ptr(line->raw) != NULL) )
    {
        ib_tx_flags_set(tx, IB_TX_FRES_HAS_DATA);
        ib_tx_flags_set(tx, IB_TX_FRES_SEENLINE);
    }

    return IB_OK;
}

ib_status_t ib_state_notify_response_header_data(ib_engine_t *ib,
                                                 ib_tx_t *tx,
                                                 ib_parsed_header_wrapper_t *header)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(header != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_event_name(response_header_data_event));
        return IB_OK;
    }

    /* Mark the time. */
    if (tx->t.response_started == 0) {
        tx->t.response_started = ib_clock_get_time();
    }

    if ( tx->response_header == NULL ) {
        tx->response_header = header;
    }
    else {
        rc = ib_parsed_name_value_pair_list_append(tx->response_header,
                                                   header);

        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_header_data(ib, tx, response_header_data_event, header);
    return rc;
}

ib_status_t ib_state_notify_response_header_finished(ib_engine_t *ib,
                                                     ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_event_name(response_header_finished_event));
        return IB_OK;
    }

    /* Generate the response line event if it hasn't been seen */
    if (! ib_tx_flags_isset(tx, IB_TX_FRES_STARTED)) {
        /* For HTTP/0.9 there is no response line, so this is normal, but
         * for others this is not normal and should be logged. */
        if (!ib_tx_flags_isset(tx, IB_TX_FHTTP09)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_event_name(response_started_event));
        }
        rc = ib_state_notify_response_started(ib, tx, NULL);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FRES_SEENHEADER)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified event: %s",
                        ib_state_event_name(response_header_finished_event));
        return IB_EINVAL;
    }

    if (!ib_tx_flags_isset(tx, IB_TX_FHTTP09|IB_TX_FRES_STARTED)) {
        if (tx->response_line == NULL) {
            ib_log_notice_tx(tx,
                             "Attempted to notify response header finished"
                             " before response started.");
            return IB_EINVAL;
        }
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_event_name(response_started_event));
        ib_state_notify_response_started(ib, tx, tx->response_line);
    }

    /* Mark the time. */
    tx->t.response_header = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_SEENHEADER);

    rc = ib_state_notify_tx(ib, response_header_finished_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_tx(ib, handle_response_header_event, tx);
    return rc;
}

ib_status_t ib_state_notify_response_body_data(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_txdata_t *txdata)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(txdata != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_event_name(response_body_data_event));
        return IB_OK;
    }

    /* We should never get empty data */
    if ( (txdata->data == NULL) || (txdata->dlen == 0) ) {
        ib_log_debug_tx(tx, "Response body data with no data.  Ignoring.");
        return IB_OK;
    }

    /* Validate the header has already been seen. */
    if (! ib_tx_flags_isset(tx, IB_TX_FRES_SEENHEADER)) {
        /* For HTTP/0.9 there are no headers, so this is normal, but
         * for others this is not normal and should be logged.
         */
        if (!ib_tx_flags_isset(tx, IB_TX_FHTTP09)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_event_name(response_header_finished_event));
            if (tx->response_line == NULL) {
                ib_log_notice_tx(tx,
                                 "Attempted to notify response body data"
                                 " before response started.");
                return IB_EINVAL;
            }
        }
        rc = ib_state_notify_response_header_finished(ib, tx);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* On the first call, record the time and mark that there is a body. */
    if (tx->t.response_body == 0) {
        tx->t.response_body = ib_clock_get_time();
        ib_tx_flags_set(tx, IB_TX_FRES_HAS_DATA);
        ib_tx_flags_set(tx, IB_TX_FRES_SEENBODY);
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_txdata(ib, tx, response_body_data_event, txdata);

    return rc;
}

ib_status_t ib_state_notify_response_finished(ib_engine_t *ib,
                                              ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FRES_FINISHED)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified event: %s",
                        ib_state_event_name(response_finished_event));
        return IB_EINVAL;
    }

    if (!ib_tx_flags_isset(tx, IB_TX_FRES_SEENHEADER)) {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_event_name(response_header_finished_event));
        ib_state_notify_response_header_finished(ib, tx);
    }

    /* Mark the time. */
    tx->t.response_finished = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_FINISHED);

    rc = ib_state_notify_tx(ib, response_finished_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, handle_response_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    if (! ib_tx_flags_isset(tx, IB_TX_FPOSTPROCESS)) {
        rc = ib_state_notify_postprocess(ib, tx);
        if (rc != IB_OK) {
            return rc;
        }
    }

    if (! ib_tx_flags_isset(tx, IB_TX_FLOGGING)) {
        rc = ib_state_notify_logging(ib, tx);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Mark the time. */
    tx->t.finished = ib_clock_get_time();

    rc = ib_state_notify_tx(ib, tx_finished_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    return rc;
}

ib_status_t ib_state_notify_postprocess(ib_engine_t *ib,
                                        ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FPOSTPROCESS)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified event: %s",
                        ib_state_event_name(handle_postprocess_event));
        return IB_EINVAL;
    }

    /* Mark time. */
    tx->t.postprocess = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FPOSTPROCESS);

    rc = ib_state_notify_tx(ib, handle_postprocess_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_logging(ib_engine_t *ib,
                                    ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FLOGGING)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified event: %s",
                        ib_state_event_name(handle_logging_event));
        return IB_EINVAL;
    }

    ib_tx_flags_set(tx, IB_TX_FLOGGING);

    rc = ib_state_notify_tx(ib, handle_logging_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_logevent(ib_engine_t *ib,
                                     ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    rc = ib_state_notify_tx(ib, handle_logevent_event, tx);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_context_open(ib_engine_t *ib,
                                         ib_context_t *ctx)
{
    assert(ib != NULL);
    assert(ctx != NULL);

    ib_status_t rc;

    rc = ib_state_notify_context(ib, ctx, context_open_event);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_context_close(ib_engine_t *ib,
                                          ib_context_t *ctx)
{
    assert(ib != NULL);
    assert(ctx != NULL);

    ib_status_t rc;

    rc = ib_state_notify_context(ib, ctx, context_close_event);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_context_destroy(ib_engine_t *ib,
                                            ib_context_t *ctx)
{
    assert(ib != NULL);
    assert(ctx != NULL);

    ib_status_t rc;

    rc = ib_state_notify_context(ib, ctx, context_destroy_event);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_engine_shutdown_initiated(ib_engine_t *ib)
{
    assert(ib != NULL);

    ib_status_t rc;

    ib_log_info(ib, "IronBee Engine Shutdown Requested.");

    rc = ib_state_notify_null(ib, engine_shutdown_initiated_event);

    return rc;
}
