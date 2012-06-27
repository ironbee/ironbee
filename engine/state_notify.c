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
 *****************************************************************************/

#include "ironbee_config_auto.h"

#include <ironbee/state_notify.h>

#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/debug.h>
#include <ironbee/provider.h>

#include <assert.h>

#include "state_notify_private.h"
#include "engine_private.h"

#define CALL_HOOKS(out_rc, first_hook, event, whicb, ib, tx, param) \
    do { \
        *(out_rc) = IB_OK; \
        for (ib_hook_t* hook_ = (first_hook); hook_ != NULL; hook_ = hook_->next ) { \
            ib_status_t rc_ = hook_->callback.whicb((ib), (tx), (event), (param), hook_->cdata); \
            if (rc_ != IB_OK) { \
                ib_log_error_tx((tx),  "Hook returned error: %s=%s", \
                             ib_state_event_name((event)), ib_status_to_string(rc_)); \
                (*out_rc) = rc_; \
                break; \
             } \
        } \
    } while(0)

#define CALL_NOTX_HOOKS(out_rc, first_hook, event, whicb, ib, param) \
    do { \
        *(out_rc) = IB_OK; \
        for (ib_hook_t* hook_ = (first_hook); hook_ != NULL; hook_ = hook_->next ) { \
            ib_status_t rc_ = hook_->callback.whicb((ib), (event), (param), hook_->cdata); \
            if (rc_ != IB_OK) { \
                ib_log_error((ib),  "Hook returned error: %s=%s", \
                             ib_state_event_name((event)), ib_status_to_string(rc_)); \
                (*out_rc) = rc_; \
                break; \
             } \
        } \
    } while(0)

#define CALL_TX_HOOKS(out_rc, first_hook, event, whicb, ib, tx) \
    do { \
        *(out_rc) = IB_OK; \
        for (ib_hook_t* hook_ = (first_hook); hook_ != NULL; hook_ = hook_->next ) { \
            ib_status_t rc_ = hook_->callback.whicb((ib), (tx), (event), hook_->cdata); \
            if (rc_ != IB_OK) { \
                ib_log_error_tx((tx),  "Hook returned error: %s=%s", \
                             ib_state_event_name((event)), ib_status_to_string(rc_)); \
                (*out_rc) = rc_; \
                break; \
             } \
        } \
    } while(0)

#define CALL_NULL_HOOKS(out_rc, first_hook, event, ib) \
    do { \
        *(out_rc) = IB_OK; \
        for (ib_hook_t* hook_ = (first_hook); hook_ != NULL; hook_ = hook_->next ) { \
            ib_status_t rc_ = hook_->callback.null((ib), (event), hook_->cdata); \
            if (rc_ != IB_OK) { \
                ib_log_error((ib),  "Hook returned error: %s=%s", \
                             ib_state_event_name((event)), ib_status_to_string(rc_)); \
                (*out_rc) = rc_; \
                break; \
             } \
        } \
    } while(0)


static ib_status_t ib_state_notify_conn(ib_engine_t *ib,
                                        ib_state_event_type_t event,
                                        ib_conn_t *conn)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(conn != NULL);

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_CONN);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug3(ib, "CONN EVENT: %s", ib_state_event_name(event));

    CALL_NOTX_HOOKS(&rc, ib->hook[event], event, conn, ib, conn);

    if ((rc != IB_OK) || (conn->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_state_notify_conn_data(ib_engine_t *ib,
                                             ib_state_event_type_t event,
                                             ib_conndata_t *conndata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(conndata != NULL);

    ib_conn_t *conn = conndata->conn;

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_CONNDATA);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug3(ib, "CONN DATA EVENT: %s", ib_state_event_name(event));

    CALL_NOTX_HOOKS(&rc, ib->hook[event], event, conndata, ib, conndata);

    if ((rc != IB_OK) || (conn->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_state_notify_req_line(ib_engine_t *ib,
                                            ib_tx_t *tx,
                                            ib_state_event_type_t event,
                                            ib_parsed_req_line_t *line)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(line != NULL);
    assert(line->raw != NULL);
    assert(line->method != NULL);
    assert(line->uri != NULL);
    assert(line->protocol != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_REQLINE);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "ib_check_hook() failed: %s",
                        ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Is this a HTTP/0.9 request (has no protocol specification)? */
    if (ib_bytestr_length(line->protocol) == 0) {
        ib_tx_flags_set(tx, IB_TX_FHTTP09);
    }

    tx->request_line = line;

    /* Notify the parser of the request line. */
    if (iface->request_line != NULL) {
        rc = iface->request_line(pi, tx, line);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    CALL_HOOKS(&rc, ib->hook[event], event, requestline, ib, tx, line);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_state_notify_resp_line(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_state_event_type_t event,
                                             ib_parsed_resp_line_t *line)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert((line == NULL) || (line->raw != NULL));
    assert((line == NULL) || (line->protocol != NULL));
    assert((line == NULL) || (line->status != NULL));
    assert((line == NULL) || (line->msg != NULL));

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    rc = ib_check_hook(ib, event, IB_STATE_HOOK_RESPLINE);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "ib_check_hook() failed: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Validate response line data.
     *
     * The response line may be NULL only for HTTP/0.9 requests
     * which contain neither a line nor headers.
     */
    if ((line == NULL) && !ib_tx_flags_isset(tx, IB_TX_FHTTP09)) {
        ib_log_notice_tx(tx, "Invalid response line");
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    tx->response_line = line;

    /* Notify the parser of the response line. */
    if (iface->response_line != NULL) {
        rc = iface->response_line(pi, tx, line);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    CALL_HOOKS(&rc, ib->hook[event], event, responseline, ib, tx, line);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_state_notify_tx(ib_engine_t *ib,
                                      ib_state_event_type_t event,
                                      ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_TX);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug3_tx(tx, "TX EVENT: %s", ib_state_event_name(event));

    /* This transaction is now the current (for pipelined). */
    tx->conn->tx = tx;

    CALL_TX_HOOKS(&rc, ib->hook[event], event, tx, ib, tx);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_request_started(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_parsed_req_line_t *line)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(line != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FREQ_STARTED)) {
        ib_log_error_tx(tx, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_started_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Mark the time. */
    tx->t.request_started = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FREQ_STARTED);

    /* Notify the parser to initialize the transaction. */
    if (iface->tx_init != NULL) {
        rc = iface->tx_init(pi, tx);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Mark as a transaction that is receiving parsed data if
     * the request was started without seeing data from the
     * connection (conndata_in) event.
     */
    if (!ib_conn_flags_isset(tx->conn, IB_CONN_FSEENDATAIN)) {
        ib_tx_flags_set(tx, IB_TX_FPARSED_DATA);
    }

    rc = ib_state_notify_tx(ib, tx_started_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_req_line(ib, tx, request_started_event, line);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_cfg_started(ib_engine_t *ib)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);

    ib_status_t rc;

    /* Create and configure the main configuration context. */
    ib_engine_context_create_main(ib);

    rc = ib_context_open(ib->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /// @todo Create a temp mem pool???
    CALL_NULL_HOOKS(&rc, ib->hook[cfg_started_event], cfg_started_event, ib);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_cfg_finished(ib_engine_t *ib)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);

    ib_status_t rc;

    /* Initialize (and close) the main configuration context. */
    rc = ib_context_close(ib->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Run the hooks. */
    CALL_NULL_HOOKS(&rc, ib->hook[cfg_finished_event], cfg_finished_event, ib);

    /* Destroy the temporary memory pool. */
    ib_engine_pool_temp_destroy(ib);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_context_get_ex(
    ib_engine_t *ib,
    ib_ctype_t type,
    void *data,
    ib_context_t **pctx
) {
    IB_FTRACE_INIT();
    ib_context_t *ctx;
    ib_status_t rc;
    size_t nctx, i;

    *pctx = NULL;

    /* Run through the config context functions to select the context. */
    IB_ARRAY_LOOP(ib->contexts, nctx, i, ctx) {
        ib_log_debug3(ib, "Checking context %d=%p '%s'",
                      (int)i, ctx, ib_context_full_get(ctx));
        /* A NULL function is a null context, so skip it */
        if ((ctx == NULL) || (ctx->fn_ctx == NULL)) {
            continue;
        }

        rc = ctx->fn_ctx(ctx, type, data, ctx->fn_ctx_data);
        if (rc == IB_OK) {
            ib_site_t *site = ib_context_site_get(ctx);
            ib_log_debug2(ib, "Selected context %d=%p '%s' site=%s(%s)",
                          (int)i, ctx, ib_context_full_get(ctx),
                          (site?site->id_str:"none"),
                          (site?site->name:"none"));
            *pctx = ctx;
            break;
        }
        else if (rc != IB_DECLINED) {
            /// @todo Log the error???
        }
    }
    if (*pctx == NULL) {
        ib_log_debug3(ib, "Using \"main\" context");
        *pctx = ib_context_main(ib);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_state_notify_conn_opened(ib_engine_t *ib,
                                        ib_conn_t *conn)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(conn != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (ib_conn_flags_isset(conn, IB_CONN_FOPENED)) {
        ib_log_error(ib, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(conn_opened_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_conn_flags_set(conn, IB_CONN_FOPENED);

    /* Notify the parser to initialize the connection. */
    if (iface->conn_init != NULL) {
        rc = iface->conn_init(pi, conn);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    rc = ib_state_notify_conn(ib, conn_started_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_conn(ib, conn_opened_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Select the connection context to use. */
    rc = ib_context_get_ex(ib, IB_CTYPE_CONN, conn, &conn->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_conn(ib, handle_context_conn_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Notify the parser a connection was made. */
    if (iface->connect != NULL) {
        rc = iface->connect(pi, conn);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    rc = ib_state_notify_conn(ib, handle_connect_event, conn);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_conn_data_in(ib_engine_t *ib,
                                         ib_conndata_t *conndata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(conndata != NULL);

    ib_conn_t *conn = conndata->conn;
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (!ib_conn_flags_isset(conndata->conn, IB_CONN_FSEENDATAIN)) {
        ib_conn_flags_set(conndata->conn, IB_CONN_FSEENDATAIN);
    }

    /* Notify data handlers before the parser. */
    rc = ib_state_notify_conn_data(ib, conn_data_in_event, conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Notify the parser there is incoming data. */
    rc = iface->conn_data_in(pi, conndata);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_conn_data_out(ib_engine_t *ib,
                                          ib_conndata_t *conndata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(conndata != NULL);

    ib_conn_t *conn = conndata->conn;
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    if (!ib_conn_flags_isset(conndata->conn, IB_CONN_FSEENDATAOUT)) {
        ib_conn_flags_set(conndata->conn, IB_CONN_FSEENDATAOUT);
    }

    /* Notify data handlers before the parser. */
    rc = ib_state_notify_conn_data(ib, conn_data_out_event, conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Notify the parser there is outgoing data. */
    rc = iface->conn_data_out(pi, conndata);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_conn_closed(ib_engine_t *ib,
                                        ib_conn_t *conn)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(conn != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Validate. */
    if (ib_conn_flags_isset(conn, IB_CONN_FCLOSED)) {
        ib_log_error(ib, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(conn_closed_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
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
    }

    /* Mark the time. */
    conn->t.finished = ib_clock_get_time();

    ib_conn_flags_set(conn, IB_CONN_FCLOSED);

    rc = ib_state_notify_conn(ib, conn_closed_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_conn(ib, handle_disconnect_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_conn(ib, conn_finished_event, conn);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Notify the parser a disconnect was performed. */
    if (iface->disconnect != NULL) {
        rc = iface->disconnect(pi, conn);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t ib_state_notify_header_data(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_state_event_type_t event,
                                               ib_parsed_header_wrapper_t *header)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(header != NULL);

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_HEADER);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "ib_check_hook() failed: %s",
                     ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug3_tx(tx, "HEADER EVENT: %s", ib_state_event_name(event));

    CALL_HOOKS(&rc,
               ib->hook[event],
               event,
               headerdata,
               ib,
               tx,
               header->head);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t ib_state_notify_txdata(ib_engine_t *ib,
                                          ib_tx_t *tx,
                                          ib_state_event_type_t event,
                                          ib_txdata_t *txdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(txdata != NULL);

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_TXDATA);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (ib_log_get_level(ib) >= 9) {
        ib_log_debug3_tx(tx, "TX DATA EVENT: %s", ib_state_event_name(event));
    }

    /* This transaction is now the current (for pipelined). */
    tx->conn->tx = tx;

    CALL_HOOKS(&rc, ib->hook[event], event, txdata, ib, tx, txdata);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_request_header_data(ib_engine_t *ib,
                                                ib_tx_t *tx,
                                                ib_parsed_header_wrapper_t *header)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(header != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
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
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Notify the parser of request header data. */
    if (iface->request_header_data != NULL) {
        rc = iface->request_header_data(pi, tx, header);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_header_data(ib, tx, request_header_data_event, header);
    IB_FTRACE_RET_STATUS(rc);
}


ib_status_t ib_state_notify_request_header_finished(ib_engine_t *ib,
                                                    ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FREQ_SEENHEADER)) {
        ib_log_error_tx(tx, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_header_finished_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (!ib_tx_flags_isset(tx, IB_TX_FREQ_STARTED)) {
        if (tx->request_line == NULL) {
            ib_log_debug3_tx(tx,
                             "Attempted to notify request header finished"
                             " before request started.");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        ib_log_debug_tx(tx, "Automatically triggering %s",
                        ib_state_event_name(request_started_event));
        ib_state_notify_request_started(ib, tx, tx->request_line);
    }

    /* Mark the time. */
    tx->t.request_header = ib_clock_get_time();

    /// @todo Seems this gets there too late.
    rc = ib_fctl_meta_add(tx->fctl, IB_STREAM_EOH);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_SEENHEADER);

    rc = ib_state_notify_tx(ib, request_header_finished_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Notify the request header is finished. */
    rc = iface->request_header_finished(pi, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Select the transaction context to use. */
    rc = ib_context_get_ex(ib, IB_CTYPE_TX, tx, &tx->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_context_tx_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_tx(ib, handle_request_header_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_request_body_data(ib_engine_t *ib,
                                              ib_tx_t *tx,
                                              ib_txdata_t *txdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(txdata != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
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

    /* Notify the parser of request body data. */
    if (iface->request_body_data != NULL) {
        rc = iface->request_body_data(pi, tx, txdata);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_txdata(ib, tx, request_body_data_event, txdata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_request_finished(ib_engine_t *ib,
                                             ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FREQ_FINISHED)) {
        ib_log_error_tx(tx, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_finished_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
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
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Notify filters of the end-of-stream (EOS). */
    rc = ib_fctl_meta_add(tx->fctl, IB_STREAM_EOS);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_FINISHED);

    /* Notify the parser the request finished. */
    if (iface->request_finished != NULL) {
        rc = iface->request_finished(pi, tx);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    rc = ib_state_notify_tx(ib, request_finished_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_request_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, tx_process_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_started(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_parsed_resp_line_t *line)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);

    ib_status_t rc;

    tx->t.response_started = ib_clock_get_time();

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FRES_STARTED)) {
        ib_log_error_tx(tx, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_started_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Mark the time. */
    tx->t.response_started = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_STARTED);

    rc = ib_state_notify_resp_line(ib, tx, response_started_event, line);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_header_data(ib_engine_t *ib,
                                                 ib_tx_t *tx,
                                                 ib_parsed_header_wrapper_t *header)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(header != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
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
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Notify the parser of response header data. */
    if (iface->response_header_data != NULL) {
        rc = iface->response_header_data(pi, tx, header);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_header_data(ib, tx, response_header_data_event, header);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_header_finished(ib_engine_t *ib,
                                                     ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Validate. */
    if (ib_tx_flags_isset(tx, IB_TX_FRES_SEENHEADER)) {
        ib_log_error_tx(tx, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_header_finished_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (!ib_tx_flags_isset(tx, IB_TX_FRES_STARTED)) {
        if (tx->response_line == NULL) {
            ib_log_debug3_tx(tx,
                             "Attempted to notify response header finished"
                             " before response started.");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
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
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Notify the parser the response header finished. */
    if (iface->response_header_finished != NULL) {
        rc = iface->response_header_finished(pi, tx);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_tx(ib, handle_response_header_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_body_data(ib_engine_t *ib,
                                               ib_tx_t *tx,
                                               ib_txdata_t *txdata)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);
    assert(txdata != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (iface == NULL) {
        ib_log_alert(ib, "Failed to fetch parser interface.");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }

    /* Validate the header has already been seen. */
    if (! ib_tx_flags_isset(tx, IB_TX_FRES_SEENHEADER)) {
        /* For HTTP/0.9 there are no headers, so this is normal, but
         * for others this is not normal and should be logged.
         */
        if (!ib_tx_flags_isset(tx, IB_TX_FHTTP09)) {
            ib_log_debug_tx(tx, "Automatically triggering %s",
                            ib_state_event_name(response_header_finished_event));
        }
        else if (tx->response_line == NULL) {
            ib_log_debug3_tx(tx,
                             "Attempted to notify response body data"
                             " before response started.");
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
        rc = ib_state_notify_response_header_finished(ib, tx);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* On the first call, record the time and mark that there is a body. */
    if (tx->t.response_body == 0) {
        tx->t.response_body = ib_clock_get_time();
        ib_tx_flags_set(tx, IB_TX_FRES_SEENBODY);
    }

    /* Notify the parser of response body data. */
    if (iface->response_body_data != NULL) {
        rc = iface->response_body_data(pi, tx, txdata);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    /* Notify the engine and any callbacks of the data. */
    rc = ib_state_notify_txdata(ib, tx, response_body_data_event, txdata);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_finished(ib_engine_t *ib,
                                              ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    assert(ib != NULL);
    assert(tx != NULL);

    ib_provider_inst_t *pi = ib_parser_provider_get_instance(tx->conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FRES_FINISHED)) {
        ib_log_error_tx(tx, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_finished_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if (!ib_tx_flags_isset(tx, IB_TX_FRES_SEENHEADER)) {
        ib_log_debug_tx(tx, "Automatically triggering %s",
                     ib_state_event_name(response_header_finished_event));
        ib_state_notify_response_header_finished(ib, tx);
    }

    /* Mark the time. */
    tx->t.response_finished = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_FINISHED);

    /* Notify the parser the response finished. */
    if (iface->response_finished != NULL) {
        rc = iface->response_finished(pi, tx);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    rc = ib_state_notify_tx(ib, response_finished_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_response_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Mark time. */
    tx->t.postprocess = ib_clock_get_time();

    rc = ib_state_notify_tx(ib, handle_postprocess_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Mark the time. */
    tx->t.finished = ib_clock_get_time();

    rc = ib_state_notify_tx(ib, tx_finished_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Notify the parser to cleanup the transaction. */
    if (iface->tx_cleanup != NULL) {
        rc = iface->tx_cleanup(pi, tx);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    IB_FTRACE_RET_STATUS(rc);
}
