#include "ironbee_config_auto.h"

#include <ironbee/state_notify.h>

#include <assert.h>

#include <ironbee/engine.h>
#include <ironbee/field.h>

#include "ironbee_private.h"

#define CALL_HOOKS(out_rc, first_hook, event, whicb, ib, tx, param) \
    do { \
        *(out_rc) = IB_OK; \
        for (ib_hook_t* hook_ = (first_hook); hook_ != NULL; hook_ = hook_->next ) { \
            ib_status_t rc_ = hook_->callback.whicb((ib), (tx), (event), (param), hook_->cdata); \
            if (rc_ != IB_OK) { \
                ib_log_error((ib), 4, "Hook returned error: %s=%s", \
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
                ib_log_error((ib), 4, "Hook returned error: %s=%s", \
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
                ib_log_error((ib), 4, "Hook returned error: %s=%s", \
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
                ib_log_error((ib), 4, "Hook returned error: %s=%s", \
                             ib_state_event_name((event)), ib_status_to_string(rc_)); \
                (*out_rc) = rc_; \
                break; \
             } \
        } \
    } while(0)


/**
 * Notify the engine that a connection event has occurred.
 *
 * @param[in] ib Engine
 * @param[in] event Event
 * @param[in] conn Connection
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_conn(ib_engine_t *ib,
                                        ib_state_event_type_t event,
                                        ib_conn_t *conn)
{
    IB_FTRACE_INIT();

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_CONN);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 9, "CONN EVENT: %s", ib_state_event_name(event));

    CALL_NOTX_HOOKS(&rc, ib->ectx->hook[event], event, conn, ib, conn);

    if ((rc != IB_OK) || (conn->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (conn->ctx != ib->ctx) {
        CALL_NOTX_HOOKS(&rc, conn->ctx->hook[event], event, conn, ib, conn);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Notify the engine that a connection data event has occurred.
 *
 * @param[in] ib Engine
 * @param[in] event Event
 * @param[in] conndata Connection data
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_conn_data(ib_engine_t *ib,
                                             ib_state_event_type_t event,
                                             ib_conndata_t *conndata)
{
    IB_FTRACE_INIT();
    ib_conn_t *conn = conndata->conn;

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_CONNDATA);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 9, "CONN DATA EVENT: %s", ib_state_event_name(event));

    CALL_NOTX_HOOKS(&rc, ib->ectx->hook[event], event, conndata, ib, conndata);

    if ((rc != IB_OK) || (conn->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (conn->ctx != ib->ctx) {
        CALL_NOTX_HOOKS(&rc, conn->ctx->hook[event], event, conndata, ib, conndata);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Signal that the response line was received.
 *
 * @param[in] ib Engine
 * @param[in] event Event
 * @param[in] txdata Connection data
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_resp_line(ib_engine_t *ib,
                                             ib_state_event_type_t event,
                                             ib_parsed_resp_line_t *line)
{
    IB_FTRACE_INIT();

    ib_tx_t *tx = line->tx;

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_RESPLINE);
    assert(rc == IB_OK);

    ib_log_debug(ib, 9, "RESP LINE EVENT: %s", ib_state_event_name(event));

    CALL_HOOKS(&rc, ib->ectx->hook[event], event, responseline, ib, tx, line);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (tx->ctx != ib->ctx) {
        CALL_HOOKS(&rc, tx->ctx->hook[event], event, responseline, ib, tx, line);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Signal that the request line was received.
 *
 * @param[in] ib Engine
 * @param[in] event Event
 * @param[in] txdata Connection data
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_req_line(ib_engine_t *ib,
                                            ib_state_event_type_t event,
                                            ib_parsed_req_line_t *line)
{
    IB_FTRACE_INIT();

    ib_tx_t *tx = line->tx;

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_REQLINE);
    assert(rc == IB_OK);

    /* Request line stored for use when the context has been determined. */
    tx->request_line = line;

    ib_log_debug(ib, 9, "REQ LINE EVENT: %s", ib_state_event_name(event));

    CALL_HOOKS(&rc, ib->ectx->hook[event], event, requestline, ib, tx, line);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (tx->ctx != ib->ctx) {
        CALL_HOOKS(&rc, tx->ctx->hook[event], event, requestline, ib, tx, line);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Signal that the header data has been received.
 *
 * @internal
 *
 * @param ib Engine
 * @param event Event
 * @param txdata Connection data
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_headers(ib_engine_t *ib,
                                           ib_state_event_type_t event,
                                           ib_parsed_header_wrapper_t *headers)
{
    IB_FTRACE_INIT();
    ib_tx_t *tx = headers->tx;

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_HEADER);
    assert(rc == IB_OK);

    ib_log_debug(ib, 9, "HEADER EVENT: %s", ib_state_event_name(event));

    CALL_HOOKS(&rc,
               ib->ectx->hook[event],
               event,
               headersdata,
               ib,
               tx,
               headers->head);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (tx->ctx != ib->ctx) {
        CALL_HOOKS(&rc,
                   tx->ctx->hook[event],
                   event,
                   headersdata,
                   ib,
                   tx,
                   headers->head);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Signal that transaction data was received.
 *
 * @param[in] ib IronBee engine.
 * @param[in] tx Transaction.
 * @param[in] event The event type.
 * @param[in] txdata The transaction data chunk.
 *
 * @returns Status code.
 */
static ib_status_t ib_state_notify_txdata(ib_engine_t *ib,
                                          ib_tx_t *tx,
                                          ib_state_event_type_t event,
                                          ib_txdata_t *txdata)
{
    IB_FTRACE_INIT();

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_TXDATA);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 9, "TX DATA EVENT: %s (type %d)",
                 ib_state_event_name(event), txdata->dtype);

    /* This transaction is now the current (for pipelined). */
    tx->conn->tx = tx;

    CALL_HOOKS(&rc, ib->ectx->hook[event], event, txdata, ib, tx, txdata);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (tx->ctx != ib->ctx) {
        CALL_HOOKS(&rc, tx->ctx->hook[event], event, txdata, ib, tx, txdata);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Notify the engine that a transaction event has occurred.
 *
 * @param[in] ib Engine
 * @param[in] event Event
 * @param[in] tx Transaction
 *
 * @returns Status code
 */
static ib_status_t ib_state_notify_tx(ib_engine_t *ib,
                                      ib_state_event_type_t event,
                                      ib_tx_t *tx)
{
    IB_FTRACE_INIT();

    ib_status_t rc = ib_check_hook(ib, event, IB_STATE_HOOK_TX);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug(ib, 9, "TX EVENT: %s", ib_state_event_name(event));

    /* This transaction is now the current (for pipelined). */
    tx->conn->tx = tx;

    CALL_TX_HOOKS(&rc, ib->ectx->hook[event], event, tx, ib, tx);

    if ((rc != IB_OK) || (tx->ctx == NULL)) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if (tx->ctx != ib->ctx) {
        CALL_TX_HOOKS(&rc, tx->ctx->hook[event], event, tx, ib, tx);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Create a main context to operate in.
 *
 * @param[in] ib IronBee engine that contains the ectx that we will use
 *            in creating the main context. The main context
 *            will be assigned to ib->ctx if it is successfully created.
 *
 * @returns IB_OK or the result of
 *          ib_context_create(ctx, ib, ib->ectx, NULL, NULL, NULL).
 */
static ib_status_t ib_engine_context_create_main(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_context_t *ctx;
    ib_status_t rc;

    rc = ib_context_create(&ctx, ib, ib->ectx, NULL, NULL, NULL);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib->ctx = ctx;

    IB_FTRACE_RET_STATUS(IB_OK);
}


ib_status_t ib_state_notify_cfg_started(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Create and configure the main configuration context. */
    ib_engine_context_create_main(ib);

    rc = ib_context_open(ib->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /// @todo Create a temp mem pool???
    CALL_NULL_HOOKS(&rc, ib->ectx->hook[cfg_started_event], cfg_started_event, ib);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_cfg_finished(ib_engine_t *ib)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    /* Initialize (and close) the main configuration context. */
    rc = ib_context_close(ib->ctx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Run the hooks. */
    CALL_NULL_HOOKS(&rc, ib->ectx->hook[cfg_finished_event], cfg_finished_event, ib);

    /* Destroy the temporary memory pool. */
    ib_engine_pool_temp_destroy(ib);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * Find the config context by executing context functions.
 *
 * @param[in] ib Engine
 * @param[in] type Context type
 * @param[in] data Data (type based on context type)
 * @param[in] pctx Address which context is written
 *
 * @returns Status code
 */
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
        ib_log_debug(ib, 9, "Processing context %d=%p", (int)i, ctx);
        /* A NULL function is a null context, so skip it */
        if ((ctx == NULL) || (ctx->fn_ctx == NULL)) {
            continue;
        }

        rc = ctx->fn_ctx(ctx, type, data, ctx->fn_ctx_data);
        if (rc == IB_OK) {
            ib_site_t *site = ib_context_site_get(ctx);
            ib_log_debug(ib, 7, "Selected context %d=%p site=%s(%s)",
                    (int)i, ctx,
                    (site?site->id_str:"none"), (site?site->name:"none"));
            *pctx = ctx;
            break;
        }
        else if (rc != IB_DECLINED) {
            /// @todo Log the error???
        }
    }
    if (*pctx == NULL) {
        ib_log_debug(ib, 9, "Using engine context");
        *pctx = ib_context_main(ib);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/**
 * Notify engine of additional events when notification of a
 * conn_opened_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * prior to it:
 *
 *  - conn_started_event
 *
 * And immediately following it:
 *
 *  - handle_context_conn_event
 *  - handle_connect_event
 *
 * @param[in] ib IronBee Engine.
 * @param[in] conn Connection.
 * @returns Status code.
 */
ib_status_t ib_state_notify_conn_opened(ib_engine_t *ib,
                                        ib_conn_t *conn)
{
    IB_FTRACE_INIT();
    if (ib_conn_flags_isset(conn, IB_CONN_FOPENED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(conn_opened_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    ib_conn_flags_set(conn, IB_CONN_FOPENED);

    ib_status_t rc = ib_state_notify_conn(ib, conn_started_event, conn);
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

    rc = ib_state_notify_conn(ib, handle_connect_event, conn);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_conn_data_in(ib_engine_t *ib,
                                         ib_conndata_t *conndata,
                                         void *appdata)
{
    IB_FTRACE_INIT();
    ib_conn_t *conn = conndata->conn;
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if ((conndata->conn->flags & IB_CONN_FSEENDATAIN) == 0) {
        ib_conn_flags_set(conndata->conn, IB_CONN_FSEENDATAIN);
    }

    /* Notify data handlers before the parser. */
    rc = ib_state_notify_conn_data(ib, conn_data_in_event, conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Run the data through the parser. */
    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface on data in");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }
    rc = iface->data_in(pi, conndata);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_conn_data_out(ib_engine_t *ib,
                                          ib_conndata_t *conndata,
                                          void *appdata)
{
    IB_FTRACE_INIT();
    ib_conn_t *conn = conndata->conn;
    ib_provider_inst_t *pi = ib_parser_provider_get_instance(conn->ctx);
    IB_PROVIDER_IFACE_TYPE(parser) *iface = pi?(IB_PROVIDER_IFACE_TYPE(parser) *)pi->pr->iface:NULL;
    ib_status_t rc;

    if ((conndata->conn->flags & IB_CONN_FSEENDATAOUT) == 0) {
        ib_conn_flags_set(conndata->conn, IB_CONN_FSEENDATAOUT);
    }

    /* Notify data handlers before the parser. */
    rc = ib_state_notify_conn_data(ib, conn_data_out_event, conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Run the data through the parser. */
    if (iface == NULL) {
        /// @todo Probably should not need this check
        ib_log_error(ib, 0, "Failed to fetch parser interface on data out");
        IB_FTRACE_RET_STATUS(IB_EUNKNOWN);
    }
    rc = iface->data_out(pi, conndata);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * conn_closed_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * prior to it:
 *
 *  - handle_disconnect_event
 *  - conn_finished_event
 */
ib_status_t ib_state_notify_conn_closed(ib_engine_t *ib,
                                        ib_conn_t *conn)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (ib_conn_flags_isset(conn, IB_CONN_FCLOSED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(conn_closed_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Notify any pending transaction events on connection close event. */
    if (conn->tx != NULL) {
        ib_tx_t *tx = conn->tx;

        if ((tx->flags & IB_TX_FREQ_FINISHED) == 0) {
            ib_log_debug(ib, 9, "Automatically triggering %s",
                         ib_state_event_name(request_finished_event));
            ib_state_notify_request_finished(ib, tx);
        }

        if ((tx->flags & IB_TX_FRES_FINISHED) == 0) {
            ib_log_debug(ib, 9, "Automatically triggering %s",
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

    ib_log_debug(ib, 9, "Destroying connection structure");
    ib_conn_destroy(conn);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref tx_data_in_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * prior to it:
 *
 *  - @ref tx_started_event
 */
ib_status_t ib_state_notify_tx_data_in(ib_engine_t *ib,
                                       ib_tx_t *tx,
                                       ib_txdata_t *txdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if ((tx->flags & IB_TX_FSEENDATAIN) == 0) {
        ib_tx_flags_set(tx, IB_TX_FSEENDATAIN);
    }

    rc = ib_state_notify_txdata(ib, tx, tx_data_in_event, txdata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_fctl_data_add(tx->fctl,
                          txdata->dtype,
                          txdata->data,
                          txdata->dlen);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_tx_data_out(ib_engine_t *ib,
                                        ib_tx_t *tx,
                                        ib_txdata_t *txdata)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if ((tx->flags & IB_TX_FSEENDATAOUT) == 0) {
        ib_tx_flags_set(tx, IB_TX_FSEENDATAOUT);
    }

    rc = ib_state_notify_txdata(ib, tx, tx_data_out_event, txdata);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_request_started(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_parsed_req_line_t *req)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FREQ_STARTED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_started_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Mark the time. */
    tx->t.request_started = ib_clock_get_time();

    rc = ib_state_notify_tx(ib, tx_started_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_STARTED);

    if ( req != NULL ) {
        rc = ib_state_notify_req_line(ib, request_started_event, req);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_request_headers_data(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_parsed_header_wrapper_t *headers)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    /* Mark the time. */
    tx->t.request_started = ib_clock_get_time();

    /* Mark the time. */
    tx->t.request_started = ib_clock_get_time();

    /* Make sure the headers have the right tx. */
    headers->tx = tx;

    if ( tx->request_headers == NULL ) {
        tx->request_headers = headers;
    }

    else {
        rc = ib_parsed_name_value_pair_list_append(tx->request_headers,
                                                   headers);

        if ( rc != IB_OK ) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    rc = ib_state_notify_headers(ib, request_headers_data_event, headers);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_headers_data(
    ib_engine_t *ib,
    ib_tx_t *tx,
    ib_parsed_header_wrapper_t *headers)
{
    IB_FTRACE_INIT();

    ib_status_t rc;

    /* Mark the time. */
    tx->t.request_started = ib_clock_get_time();

    /* Make sure the headers have the right tx. */
    headers->tx = tx;

    rc = ib_state_notify_headers(ib, response_headers_data_event, headers);

    IB_FTRACE_RET_STATUS(rc);
}


/**
 * @ref request_headers_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * prior to it:
 *
 *  - @ref request_started_event (if not already notified)
 *
 * And immediately following it:
 *
 *  - @ref handle_context_tx_event
 *  - @ref handle_request_headers_event
 */
ib_status_t ib_state_notify_request_headers(
    ib_engine_t *ib,
    ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FREQ_SEENHEADERS)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_headers_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if ((tx->flags & IB_TX_FREQ_STARTED) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering optional %s",
                     ib_state_event_name(request_started_event));
        ib_state_notify_request_started(ib, tx, NULL);
    }

    /* Mark the time. */
    tx->t.request_headers = ib_clock_get_time();

    /// @todo Seems this gets there too late.
    rc = ib_fctl_meta_add(tx->fctl, IB_STREAM_EOH);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_SEENHEADERS);

    rc = ib_state_notify_tx(ib, request_headers_event, tx);
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

    rc = ib_state_notify_tx(ib, handle_request_headers_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref request_body_data_event occurs.
 *
 * When the event is notified, additional events are notified immediately
 * following it:
 *
 *  - @ref handle_request_event
 */
static ib_status_t ib_state_notify_request_body_ex(ib_engine_t *ib,
                                                   ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    rc = ib_fctl_meta_add(tx->fctl, IB_STREAM_EOB);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, request_body_data_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_request_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_request_body_data(ib_engine_t *ib,
                                              ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FREQ_SEENBODY)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_body_data_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if ((tx->flags & IB_TX_FREQ_SEENHEADERS) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering %s",
                     ib_state_event_name(request_headers_event));
        ib_state_notify_request_headers(ib, tx);
    }

    /* Mark the time. */
    tx->t.request_body = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FREQ_SEENBODY);

    rc = ib_state_notify_request_body_ex(ib, tx);
    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref request_finished_event occurs.
 *
 * When the event is notified, additional events are notified
 * immediately prior to it:
 *
 *  - @ref request_body_data_event (only if not already notified)
 *
 * And immediately following it:
 *
 *  - @ref tx_process_event
 */
ib_status_t ib_state_notify_request_finished(ib_engine_t *ib,
                                             ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FREQ_FINISHED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(request_finished_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if ((tx->flags & IB_TX_FREQ_SEENHEADERS) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering %s",
                     ib_state_event_name(request_headers_event));
        ib_state_notify_request_headers(ib, tx);
    }

    if (ib_tx_flags_isset(tx, IB_TX_FREQ_SEENBODY) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering %s",
                     ib_state_event_name(request_body_data_event));
        ib_state_notify_request_body_data(ib, tx);
    }

    /* Mark the time. */
    tx->t.request_finished = ib_clock_get_time();

    rc = ib_fctl_meta_add(tx->fctl, IB_STREAM_EOS);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_tx_flags_set(tx, IB_TX_FREQ_FINISHED);

    /* Still need to notify request_body_data_event, if it has not yet
     * been triggered, however, it is an error if it was not
     * triggered for a request that should have had a body.
     */
    if ((tx->flags & IB_TX_FREQ_SEENBODY) == 0) {
        if ((tx->flags & IB_TX_FREQ_NOBODY) == 0) {
            ib_tx_flags_set(tx, IB_TX_FERROR);
        }
        rc = ib_state_notify_request_body_ex(ib, tx);
        if (rc != IB_OK) {
            IB_FTRACE_RET_STATUS(rc);
        }
    }

    rc = ib_state_notify_tx(ib, request_finished_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, tx_process_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_started(ib_engine_t *ib,
                                             ib_tx_t *tx,
                                             ib_parsed_resp_line_t *resp)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    tx->t.response_started = ib_clock_get_time();

    if (ib_tx_flags_isset(tx, IB_TX_FRES_STARTED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_started_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    /* Mark the time. */
    tx->t.response_started = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_STARTED);

    rc = ib_state_notify_tx(ib, response_started_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if ( resp != NULL ) {
        rc = ib_state_notify_resp_line(ib, response_started_event, resp);
    }

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref response_headers_event occurs.
 *
 * When the event is notified, additional events are notified
 * immediately prior to it:
 *
 *  - @ref response_started_event (only if not already notified)
 *
 * And immediately following it:
 *
 *  - @ref handle_response_headers_event
 */
ib_status_t ib_state_notify_response_headers(ib_engine_t *ib,
                                             ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FRES_SEENHEADERS)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_headers_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if ((tx->flags & IB_TX_FRES_STARTED) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering optional %s",
                     ib_state_event_name(response_started_event));
        ib_state_notify_response_started(ib, tx, NULL);
    }

    /* Mark the time. */
    tx->t.response_headers = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_SEENHEADERS);

    rc = ib_state_notify_tx(ib, response_headers_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, response_headers_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_response_headers_event, tx);

    IB_FTRACE_RET_STATUS(rc);
}

/**
 * @ref response_body_data_event occurs.
 *
 * When the event is notified, additional events are notified
 * immediately following it:
 *
 *  - @ref handle_response_event
 */
ib_status_t ib_state_notify_response_body_data(ib_engine_t *ib,
                                               ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FRES_SEENBODY)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_body_data_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if ((tx->flags & IB_TX_FRES_SEENHEADERS) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering %s",
                     ib_state_event_name(response_headers_event));
        ib_state_notify_response_headers(ib, tx);
    }

    /* Mark the time. */
    tx->t.response_body = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_SEENBODY);

    rc = ib_state_notify_tx(ib, response_body_data_event, tx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    rc = ib_state_notify_tx(ib, handle_response_event, tx);
    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_state_notify_response_finished(ib_engine_t *ib,
                                              ib_tx_t *tx)
{
    IB_FTRACE_INIT();
    ib_status_t rc;

    if (ib_tx_flags_isset(tx, IB_TX_FRES_FINISHED)) {
        ib_log_error(ib, 4, "Attempted to notify previously notified event: %s",
                     ib_state_event_name(response_finished_event));
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    if ((tx->flags & IB_TX_FRES_SEENHEADERS) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering %s",
                     ib_state_event_name(response_headers_event));
        ib_state_notify_response_headers(ib, tx);
    }

    if (ib_tx_flags_isset(tx, IB_TX_FRES_SEENBODY) == 0) {
        ib_log_debug(ib, 9, "Automatically triggering %s",
                     ib_state_event_name(response_body_data_event));
        ib_state_notify_response_body_data(ib, tx);
    }

    /* Mark the time. */
    tx->t.response_finished = ib_clock_get_time();

    ib_tx_flags_set(tx, IB_TX_FRES_FINISHED);

    rc = ib_state_notify_tx(ib, response_finished_event, tx);
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
    IB_FTRACE_RET_STATUS(rc);
}


