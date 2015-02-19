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
#include <ironbee/dso.h>
#include <ironbee/engine.h>
#include <ironbee/engine_state.h>
#include <ironbee/field.h>
#include <ironbee/flags.h>
#include <ironbee/log.h>
#include <ironbee/mm_mpool_lite.h>
#include <ironbee/stream_pump.h>

#include <assert.h>

/**
 * Generate and log a message about a hook function returning an error.
 *
 * An error is any return value that is not IB_OK or IB_DECLINED.
 *
 * @param[in] ib The engine to log through.
 * @param[in] state The state being processed.
 * @param[in] hook_rc The return code signaling the failure.
 * @param[in] hook_fn A pointer to the callback hook. This will
 *            be resolved to a symbol, if possible.
 *
 */
static
void log_hook_failure(
    ib_engine_t *ib,
    ib_state_t   state,
    ib_status_t  hook_rc,
    void        *hook_fn
)
{
    const char *hook_file   = NULL;
    const char *hook_symbol = NULL;

    ib_status_t      rc;
    ib_mpool_lite_t *mp = NULL;
    ib_mm_t          mm;

    /* Construct memory pool. */
    rc = ib_mpool_lite_create(&mp);
    if (rc != IB_OK) {
        goto no_mm_log;
    }

    mm = ib_mm_mpool_lite(mp);

    rc = ib_dso_sym_name_find(&hook_file, &hook_symbol, mm, hook_fn);
    if (rc != IB_OK) {
        hook_file = "[unavailable]";
        hook_symbol = "[unavailable]";
    }

    if (hook_file == NULL) {
        hook_file = "";
    }

    if (hook_symbol == NULL) {
        hook_symbol = "";
    }

    ib_log_notice(
        ib,
        "Hook %s from %s failed during state %s: %s",
        hook_symbol,
        hook_file,
        ib_state_name(state),
        ib_status_to_string(rc)
    );

    ib_mpool_lite_destroy(mp);

    return;

no_mm_log:
    ib_log_notice(
        ib,
        "Hook failed during state %s: %s",
        ib_state_name(state),
        ib_status_to_string(rc)
    );
}

static ib_status_t ib_state_notify_null(
    ib_engine_t *ib,
    ib_state_t state
)
{
    assert(ib != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, state, IB_STATE_HOOK_NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error checking hook for \"%s\": %s",
                     ib_state_name(state),
                     ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug3(ib, "NULL EVENT: %s", ib_state_name(state));

    IB_LIST_LOOP_CONST(ib->hooks[state], node) {
        const ib_hook_t *hook =
            (const ib_hook_t *)ib_list_node_data_const(node);
        rc = hook->callback.null(ib, state, hook->cbdata);
        if (rc == IB_DECLINED) {
            ib_log_debug(ib, "Hook declined: %s", ib_state_name(state));
        }
        else if (rc != IB_OK) {
            log_hook_failure(ib, state, rc, hook->callback.null);
            return rc;
        }
    }

    return IB_OK;
}

static ib_status_t ib_state_notify_context(
    ib_engine_t *ib,
    ib_context_t *ctx,
    ib_state_t state
)
{
    assert(ib != NULL);
    assert(ctx != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, state, IB_STATE_HOOK_CTX);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error checking hook for \"%s\": %s",
                     ib_state_name(state),
                     ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug3(ib, "CTX EVENT: %s", ib_state_name(state));

    IB_LIST_LOOP_CONST(ib->hooks[state], node) {
        const ib_hook_t *hook =
            (const ib_hook_t *)ib_list_node_data_const(node);
        rc = hook->callback.ctx(ib, ctx, state, hook->cbdata);
        if (rc == IB_DECLINED) {
            ib_log_debug(ib, "Hook declined: %s", ib_state_name(state));
        }
        else if (rc != IB_OK) {
            log_hook_failure(ib, state, rc, hook->callback.ctx);
            return rc;
        }
    }

    return IB_OK;
}

static ib_status_t ib_state_notify_conn(
    ib_engine_t *ib,
    ib_conn_t *conn,
    ib_state_t state
)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(conn != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, state, IB_STATE_HOOK_CONN);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error checking hook for \"%s\": %s",
                     ib_state_name(state),
                     ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug3(ib, "CONN EVENT: %s", ib_state_name(state));

    if (conn->ctx == NULL) {
        ib_log_notice(ib, "Connection context is null.");
    }

    IB_LIST_LOOP_CONST(ib->hooks[state], node) {
        const ib_hook_t *hook =
            (const ib_hook_t *)ib_list_node_data_const(node);
        rc = hook->callback.conn(ib, conn, state, hook->cbdata);
        if (rc == IB_DECLINED) {
            ib_log_debug(ib, "Hook declined: %s", ib_state_name(state));
        }
        else if (rc != IB_OK) {
            log_hook_failure(ib, state, rc, hook->callback.conn);
        }
    }

    return IB_OK;
}

static ib_status_t ib_state_notify_req_line(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_state_t state,
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

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_REQLINE);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error checking hook for \"%s\": %s",
                        ib_state_name(state),
                        ib_status_to_string(rc));
        return rc;
    }

    /* Is this a HTTP/0.9 request (has no protocol specification)? */
    if (ib_bytestr_length(line->protocol) == 0) {
        ib_tx_flags_set(tx, IB_TX_FHTTP09);
    }

    tx->request_line = line;
    tx->request_header_len = ib_bytestr_length(line->raw);

    if (tx->ctx == NULL) {
        ib_log_notice_tx(tx, "Connection context is null.");
    }

    IB_LIST_LOOP_CONST(ib->hooks[state], node) {
        const ib_hook_t *hook =
            (const ib_hook_t *)ib_list_node_data_const(node);
        rc = hook->callback.requestline(ib, tx, state, line, hook->cbdata);
        if (rc == IB_DECLINED) {
            ib_log_debug_tx(tx, "Hook declined: %s",
                            ib_state_name(state));
        }
        else if (rc != IB_OK) {
            log_hook_failure(ib, state, rc, hook->callback.requestline);
        }
    }

    return IB_OK;
}

static ib_status_t ib_state_notify_resp_line(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_state_t state,
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

    rc = ib_hook_check(ib, state, IB_STATE_HOOK_RESPLINE);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error checking hook for \"%s\": %s",
                        ib_state_name(state),
                        ib_status_to_string(rc));
        return rc;
    }

    /* Validate response line data.
     *
     * The response line may be NULL only for HTTP/0.9 requests
     * which contain neither a line nor headers.
     */
    if ((line == NULL) && !ib_flags_all(tx->flags, IB_TX_FHTTP09)) {
        if (! ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)) {
            ib_log_debug_tx(tx, "No request data: Ignoring %s",
                            ib_state_name(state));
        }
        else {
            ib_log_notice_tx(tx, "Invalid response line.");
        }
        return IB_OK;
    }

    tx->response_line = line;
    if (line != NULL) {
        tx->response_header_len = ib_bytestr_length(line->raw);
    }

    if (tx->ctx == NULL) {
        ib_log_notice_tx(tx, "Connection context is null.");
    }

    IB_LIST_LOOP_CONST(ib->hooks[state], node) {
        const ib_hook_t *hook =
            (const ib_hook_t *)ib_list_node_data_const(node);
        rc = hook->callback.responseline(ib, tx, state, line, hook->cbdata);
        if (rc == IB_DECLINED) {
            ib_log_debug_tx(tx, "Hook declined: %s",
                            ib_state_name(state));
        }
        else if (rc != IB_OK) {
            log_hook_failure(ib, state, rc, hook->callback.responseline);
        }
    }

    return IB_OK;
}

static ib_status_t ib_state_notify_tx(ib_engine_t *ib,
                                      ib_state_t state,
                                      ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, state, IB_STATE_HOOK_TX);
    if (rc != IB_OK) {
        return rc;
    }

    ib_log_debug3_tx(tx, "TX EVENT: %s", ib_state_name(state));

    /* This transaction is now the current (for pipelined). */
    tx->conn->tx = tx;

    if (tx->ctx == NULL) {
        ib_log_notice_tx(tx, "Connection context is null.");
    }

    IB_LIST_LOOP_CONST(ib->hooks[state], node) {
        const ib_hook_t *hook =
            (const ib_hook_t *)ib_list_node_data_const(node);
        rc = hook->callback.tx(ib, tx, state, hook->cbdata);
        if (rc == IB_DECLINED) {
            ib_log_debug_tx(tx, "Hook declined: %s",
                            ib_state_name(state));
        }
        else if (rc != IB_OK) {
            log_hook_failure(ib, state, rc, hook->callback.tx);
        }
    }

    return IB_OK;
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
    if (ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified state: %s",
                        ib_state_name(request_started_state));
        return IB_EINVAL;
    }

    /* Mark the time. */
    tx->t.request_started = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FREQ_STARTED);

    /* Notify everybody */
    rc = ib_state_notify_tx(ib, tx_started_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Notify the request line if it's present */
    if (line == NULL) {
        ib_log_info_tx(tx, "Request started with no line.");
    }
    else if (
        (line->raw == NULL) ||
        (line->method == NULL) ||
        (line->uri == NULL) ||
        (line->protocol == NULL)
    ) {
        ib_log_error_tx(tx, "Request started with malformed line.");
        return IB_EINVAL;
    }
    else {
        ib_tx_flags_set(tx, IB_TX_FREQ_HAS_DATA);
        rc = ib_state_notify_req_line(ib, tx, request_started_state, line);
        if (rc != IB_OK) {
            return rc;
        }
        ib_tx_flags_set(tx, IB_TX_FREQ_LINE);
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

    /* Validate. */
    if (ib_flags_all(conn->flags, IB_CONN_FOPENED)) {
        ib_log_error(ib, "Attempted to notify previously notified state: %s",
                     ib_state_name(conn_opened_state));
        return IB_EINVAL;
    }

    ib_flags_set(conn->flags, IB_CONN_FOPENED);

    rc = ib_state_notify_conn(ib, conn, conn_started_state);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, conn_opened_state);
    if (rc != IB_OK) {
        return rc;
    }

    /* Select the connection context to use. */
    rc = ib_ctxsel_select_context(ib, conn, NULL, &conn->ctx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, handle_context_conn_state);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, handle_connect_state);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_conn_closed(ib_engine_t *ib,
                                        ib_conn_t *conn)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(conn != NULL);

    ib_status_t rc;

    /* Validate. */
    if (! ib_flags_all(conn->flags, IB_CONN_FOPENED)) {
        ib_log_error(ib, "No connection opened: Ignoring %s",
                     ib_state_name(conn_closed_state));
        return IB_EINVAL;
    }
    if (ib_flags_all(conn->flags, IB_CONN_FCLOSED)) {
        ib_log_error(ib, "Attempted to notify previously notified state: %s",
                     ib_state_name(conn_closed_state));
        return IB_EINVAL;
    }

    /* Notify any pending transaction states on connection close state. */
    if (conn->tx != NULL) {
        ib_tx_t *tx = conn->tx;

        if (    ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)
            && !ib_flags_all(tx->flags, IB_TX_FREQ_FINISHED))
        {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_name(request_finished_state));
            ib_state_notify_request_finished(ib, tx);
        }

        if (    ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)
            && !ib_flags_all(tx->flags, IB_TX_FRES_STARTED))
        {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_name(response_started_state));
            ib_state_notify_response_started(ib, tx, NULL);
        }

        if (    ib_flags_all(tx->flags, IB_TX_FRES_STARTED)
            && !ib_flags_all(tx->flags, IB_TX_FRES_FINISHED))
        {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_name(response_finished_state));
            ib_state_notify_response_finished(ib, tx);
        }

        if (!ib_flags_all(tx->flags, IB_TX_FPOSTPROCESS)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_name(handle_postprocess_state));
            ib_state_notify_postprocess(ib, tx);
        }

        if (!ib_flags_all(tx->flags, IB_TX_FLOGGING)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_name(handle_logging_state));
            ib_state_notify_logging(ib, tx);
        }
    }

    /* Mark the time. */
    conn->t.finished = ib_clock_get_time();

    ib_flags_set(conn->flags, IB_CONN_FCLOSED);

    rc = ib_state_notify_conn(ib, conn, conn_closed_state);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, handle_disconnect_state);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_conn(ib, conn, conn_finished_state);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

static ib_status_t ib_state_notify_header_data(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_state_t state,
                                               ib_parsed_headers_t *header)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(header != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, state, IB_STATE_HOOK_HEADER);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error checking hook for \"%s\": %s",
                        ib_state_name(state),
                        ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug3_tx(tx, "HEADER EVENT: %s", ib_state_name(state));

    if (tx->ctx == NULL) {
        ib_log_notice_tx(tx, "Connection context is null.");
    }

    IB_LIST_LOOP_CONST(ib->hooks[state], node) {
        const ib_hook_t *hook =
            (const ib_hook_t *)ib_list_node_data_const(node);
        rc = hook->callback.headerdata(ib, tx, state,
                                       header->head, hook->cbdata);
        if (rc == IB_DECLINED) {
            ib_log_debug_tx(tx, "Hook declined: %s",
                            ib_state_name(state));
        }
        else if (rc != IB_OK) {
            log_hook_failure(ib, state, rc, hook->callback.headerdata);
        }
    }

    return IB_OK;
}

static ib_status_t ib_state_notify_txdata(ib_engine_t *ib,
                                          ib_tx_t *tx,
                                          ib_state_t state,
                                          const char *data,
                                          size_t data_length)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(data != NULL);

    const ib_list_node_t *node;
    ib_status_t rc = ib_hook_check(ib, state, IB_STATE_HOOK_TXDATA);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Error checking hook for \"%s\": %s",
                        ib_state_name(state),
                        ib_status_to_string(rc));
        return rc;
    }

    if (ib_logger_level_get(ib_engine_logger_get(ib)) >= 9) {
        ib_log_debug3_tx(tx, "TX DATA EVENT: %s", ib_state_name(state));
    }

    /* This transaction is now the current (for pipelined). */
    tx->conn->tx = tx;

    if (tx->ctx == NULL) {
        ib_log_notice_tx(tx, "Connection context is null.");
    }

    IB_LIST_LOOP_CONST(ib->hooks[state], node) {
        const ib_hook_t *hook =
            (const ib_hook_t *)ib_list_node_data_const(node);
        rc = hook->callback.txdata(ib, tx, state, data, data_length, hook->cbdata);
        if (rc == IB_DECLINED) {
            ib_log_debug_tx(tx, "Hook declined: %s",
                            ib_state_name(state));
        }
        else if (rc != IB_OK) {
            log_hook_failure(ib, state, rc, hook->callback.txdata);
        }
    }

    return IB_OK;
}

ib_status_t ib_state_notify_request_header_data(ib_engine_t *ib,
                                                ib_tx_t *tx,
                                                ib_parsed_headers_t *header)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(header != NULL);

    ib_status_t rc;

    /* Validate. */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)) {
        ib_log_debug_tx(tx, "No request started: Ignoring %s",
                        ib_state_name(request_header_data_state));
        return IB_OK;
    }
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_name(request_header_data_state));
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
        rc = ib_parsed_headers_append(tx->request_header, header);

        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Track length of header data. */
    const ib_parsed_header_t *node;
    for (node = header->head; node != NULL; node = node->next) {
        tx->request_header_len += ib_bytestr_length(node->name);
        tx->request_header_len += ib_bytestr_length(node->value);
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_header_data(ib, tx, request_header_data_state, header);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_request_header_finished(ib_engine_t *ib,
                                                    ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    /* Validate. */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)) {
        ib_log_debug_tx(tx, "No request started: Ignoring %s",
                        ib_state_name(request_header_finished_state));
        return IB_OK;
    }
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_name(request_header_finished_state));
        return IB_OK;
    }
    if (ib_flags_all(tx->flags, IB_TX_FREQ_HEADER)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified state: %s",
                        ib_state_name(request_header_finished_state));
        return IB_EINVAL;
    }

    if (!ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)) {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_name(request_started_state));
        rc = ib_state_notify_request_started(ib, tx, tx->request_line);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Mark the time. */
    tx->t.request_header = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FREQ_HEADER);

    rc = ib_state_notify_tx(ib, request_header_process_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Select the transaction context to use. */
    rc = ib_ctxsel_select_context(ib, tx->conn, tx, &tx->ctx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, handle_context_tx_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, request_header_finished_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_tx(ib, handle_request_header_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_request_body_data(ib_engine_t *ib,
                                              ib_tx_t *tx,
                                              const char *data,
                                              size_t data_length)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(data != NULL);

    ib_status_t rc;

    /* Validate. */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)) {
        ib_log_debug_tx(tx, "No request started: Ignoring %s",
                        ib_state_name(request_body_data_state));
        return IB_OK;
    }
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_name(request_body_data_state));
        return IB_OK;
    }

    /* We should never get NULL data. */
    if ( (data == NULL) || (data_length == 0) ) {
        ib_log_debug_tx(tx, "Request body data with no data.  Ignoring.");
        return IB_OK;
    }

    if (! ib_flags_all(tx->flags, IB_TX_FREQ_LINE)) {
        if (tx->request_line == NULL) {
            ib_log_error_tx(tx, "Request has no request line.");
            return IB_EINVAL;
        }

        rc = ib_state_notify_request_started(ib, tx, tx->request_line);
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_state_notify_request_header_finished(ib, tx);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Note that we have request data. */
    ib_tx_flags_set(tx, IB_TX_FREQ_HAS_DATA);

    /* Validate. */
    if (!ib_flags_all(tx->flags, IB_TX_FREQ_HEADER)) {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_name(request_header_finished_state));
        ib_state_notify_request_header_finished(ib, tx);
    }

    /* On the first call, record the time and mark that there is a body. */
    if (tx->t.request_body == 0) {
        tx->t.request_body = ib_clock_get_time();
        ib_tx_flags_set(tx, IB_TX_FREQ_BODY);
        tx->request_body_len = data_length;
    }
    else {
        tx->request_body_len += data_length;
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_txdata(ib, tx, request_body_data_state, data, data_length);
    if (rc != IB_OK) {
        return rc;
    }

    /* Pass data through streaming system. */
    rc = ib_stream_pump_process(
        ib_tx_request_body_pump(tx),
        (const uint8_t *)data,
        data_length);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_request_finished(ib_engine_t *ib,
                                             ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;


    /* Validate. */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)) {
        ib_log_debug_tx(tx, "No request started: Ignoring %s",
                        ib_state_name(request_finished_state));
        return IB_OK;
    }
    if (! ib_flags_any(tx->flags, IB_TX_FREQ_STARTED)) {
        ib_log_debug_tx(tx, "No request started: Ignoring %s",
                        ib_state_name(request_finished_state));
        return IB_OK;
    }
    if (ib_flags_all(tx->flags, IB_TX_FREQ_FINISHED)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified state: %s",
                        ib_state_name(request_finished_state));
        return IB_EINVAL;
    }

    if (!ib_flags_all(tx->flags, IB_TX_FREQ_HEADER)) {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_name(request_header_finished_state));
        ib_state_notify_request_header_finished(ib, tx);
    }

    /* Mark the time. */
    tx->t.request_finished = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FREQ_FINISHED);

    rc = ib_state_notify_tx(ib, request_finished_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, handle_request_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, tx_process_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Signal that all data should leave the pipeline. */
    rc = ib_stream_pump_flush(ib_tx_request_body_pump(tx));
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_response_started(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_parsed_resp_line_t *line)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    /* Validate. */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)) {
        ib_log_debug_tx(tx, "No request started: Ignoring %s",
                        ib_state_name(response_started_state));
        return IB_OK;
    }
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_name(response_started_state));
        return IB_OK;
    }

    tx->t.response_started = ib_clock_get_time();

    /* Validate. */
    if (ib_flags_all(tx->flags, IB_TX_FRES_STARTED)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified state: %s",
                        ib_state_name(response_started_state));
        return IB_EINVAL;
    }

    /* If the request was started, but not finished, notify it now */
    if (    ib_flags_all(tx->flags, IB_TX_FREQ_STARTED)
        && !ib_flags_all(tx->flags, IB_TX_FREQ_FINISHED))
    {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_name(request_finished_state));
        ib_state_notify_request_finished(ib, tx);
    }

    /* Mark the time. */
    tx->t.response_started = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_STARTED);

    /* Notify the world about it */
    rc = ib_state_notify_resp_line(ib, tx, response_started_state, line);
    if (rc != IB_OK) {
        return rc;
    }

    /* Record if we saw a line. */
    if ( (line != NULL) &&
         (line->raw != NULL) &&
         (ib_bytestr_const_ptr(line->raw) != NULL) )
    {
        ib_tx_flags_set(tx, IB_TX_FRES_HAS_DATA);
        ib_tx_flags_set(tx, IB_TX_FRES_LINE);
    }

    return IB_OK;
}

ib_status_t ib_state_notify_response_header_data(ib_engine_t *ib,
                                                 ib_tx_t *tx,
                                                 ib_parsed_headers_t *header)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(header != NULL);

    ib_status_t rc;

    /* Validate. */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_name(response_header_data_state));
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
        rc = ib_parsed_headers_append(tx->response_header, header);

        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Track length of header data. */
    const ib_parsed_header_t *node;
    for (node = header->head; node != NULL; node = node->next) {
        tx->response_header_len += ib_bytestr_length(node->name);
        tx->response_header_len += ib_bytestr_length(node->value);
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_header_data(
        ib, tx, response_header_data_state, header);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_response_header_finished(ib_engine_t *ib,
                                                     ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (!ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_name(response_header_finished_state));
        return IB_OK;
    }

    /* Generate the response line state if it hasn't been seen */
    if (! ib_flags_all(tx->flags, IB_TX_FRES_STARTED)) {
        /* For HTTP/0.9 there is no response line, so this is normal, but
         * for others this is not normal and should be logged. */
        if (!ib_flags_all(tx->flags, IB_TX_FHTTP09)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_name(response_started_state));
        }
        rc = ib_state_notify_response_started(ib, tx, NULL);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Validate. */
    if (ib_flags_all(tx->flags, IB_TX_FRES_HEADER)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified state: %s",
                        ib_state_name(response_header_finished_state));
        return IB_EINVAL;
    }

    if (!ib_flags_all(tx->flags, IB_TX_FRES_STARTED)) {
        /* For HTTP/0.9 there are no response headers, so this is normal, but
         * for others this is not normal and should be logged.
         */
        if (!ib_flags_all(tx->flags, IB_TX_FHTTP09)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_name(response_started_state));
            if (tx->response_line == NULL) {
                ib_log_notice_tx(tx,
                                 "Attempted to notify response header finished"
                                 " before response started.");
                return IB_EINVAL;
            }
        }
        rc = ib_state_notify_response_started(ib, tx, tx->response_line);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Mark the time. */
    tx->t.response_header = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_HEADER);

    rc = ib_state_notify_tx(ib, response_header_finished_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_tx(ib, handle_response_header_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_response_body_data(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               const char *data,
                                               size_t data_length)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);
    assert(data != NULL);

    ib_status_t rc;

    /* Check for data first. */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_name(response_body_data_state));
        return IB_OK;
    }

    /* We should never get empty data */
    if ( (data == NULL) || (data_length == 0) ) {
        ib_log_debug_tx(tx, "Response body data with no data.  Ignoring.");
        return IB_OK;
    }

    /* Validate the header has already been seen. */
    if (! ib_flags_all(tx->flags, IB_TX_FRES_HEADER)) {
        /* For HTTP/0.9 there are no response headers, so this is normal, but
         * for others this is not normal and should be logged.
         */
        if (!ib_flags_all(tx->flags, IB_TX_FHTTP09)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_name(response_header_finished_state));
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
        ib_tx_flags_set(tx, IB_TX_FRES_BODY);
        tx->response_body_len = data_length;
    }
    else {
        tx->response_body_len += data_length;
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_txdata(ib, tx, response_body_data_state, data, data_length);
    if (rc != IB_OK) {
        return rc;
    }

    /* Pass data through streaming system. */
    rc = ib_stream_pump_process(
        ib_tx_response_body_pump(tx),
        (const uint8_t *)data,
        data_length);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_response_finished(ib_engine_t *ib,
                                              ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    /* Check for response started first. */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_HAS_DATA)) {
        ib_log_debug_tx(tx, "No request data: Ignoring %s",
                        ib_state_name(response_finished_state));
        return IB_OK;
    }
    if (!ib_flags_any(tx->flags, IB_TX_FRES_STARTED)) {
        ib_log_debug_tx(tx, "No response started: Ignoring %s",
                        ib_state_name(response_finished_state));
        return IB_OK;
    }

    if (ib_flags_all(tx->flags, IB_TX_FRES_FINISHED)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified state: %s",
                        ib_state_name(response_finished_state));
        return IB_EINVAL;
    }

    if (!ib_flags_all(tx->flags, IB_TX_FRES_HEADER)) {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_name(response_header_finished_state));
        ib_state_notify_response_header_finished(ib, tx);
    }

    /* Mark the time. */
    tx->t.response_finished = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_FINISHED);

    rc = ib_state_notify_tx(ib, response_finished_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, handle_response_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    if (! ib_flags_all(tx->flags, IB_TX_FPOSTPROCESS)) {
        rc = ib_state_notify_postprocess(ib, tx);
        if (rc != IB_OK) {
            return rc;
        }
    }

    if (! ib_flags_all(tx->flags, IB_TX_FLOGGING)) {
        rc = ib_state_notify_logging(ib, tx);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Mark the time. */
    tx->t.finished = ib_clock_get_time();

    /* Signal that all data should leave the pipeline. */
    rc = ib_stream_pump_flush(ib_tx_response_body_pump(tx));
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_state_notify_tx(ib, tx_finished_state, tx);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_postprocess(ib_engine_t *ib,
                                        ib_tx_t *tx)
{
    assert(ib != NULL);
    assert(ib->cfg_state == CFG_FINISHED);
    assert(tx != NULL);

    ib_status_t rc;

    if (ib_flags_all(tx->flags, IB_TX_FPOSTPROCESS)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified state: %s",
                        ib_state_name(handle_postprocess_state));
        return IB_EINVAL;
    }

    /* Mark time. */
    tx->t.postprocess = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FPOSTPROCESS);

    rc = ib_state_notify_tx(ib, handle_postprocess_state, tx);
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

    if (ib_flags_all(tx->flags, IB_TX_FLOGGING)) {
        ib_log_error_tx(tx,
                        "Attempted to notify previously notified state: %s",
                        ib_state_name(handle_logging_state));
        return IB_EINVAL;
    }

    ib_tx_flags_set(tx, IB_TX_FLOGGING);

    rc = ib_state_notify_tx(ib, handle_logging_state, tx);
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

    rc = ib_state_notify_tx(ib, handle_logevent_state, tx);
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

    rc = ib_state_notify_context(ib, ctx, context_open_state);
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

    rc = ib_state_notify_context(ib, ctx, context_close_state);
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

    rc = ib_state_notify_context(ib, ctx, context_destroy_state);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_state_notify_engine_shutdown_initiated(ib_engine_t *ib)
{
    assert(ib != NULL);

    ib_status_t rc;

    ib_log_info(ib, "IronBee engine shutdown requested.");

    rc = ib_state_notify_null(ib, engine_shutdown_initiated_state);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}
