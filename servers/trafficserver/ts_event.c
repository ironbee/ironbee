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
 * @brief IronBee --- Apache Traffic Server Plugin
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/flags.h>

#include <assert.h>
#include <ts/ts.h>

#include <sys/socket.h>
#include <netdb.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif

#include <ironbee/core.h>
#include <ironbee/state_notify.h>

#include "ts_ib.h"

struct tsib_ssn_ctx {
    ib_conn_t *iconn;
    /* store the IPs here so we can clean them up and not leak memory */
    char remote_ip[ADDRSIZE];
    char local_ip[ADDRSIZE];
    TSHttpTxn txnp; /* hack: conn data requires txnp to access */
    /* Keep track of whether this is open and has active transactions */
    int txn_count;
    int closing;
    /* include the contp, so we can delay destroying it from the event */
    TSCont contp;
};

/**
 * IronBee connection cleanup
 *
 * @param[in] data Callback data (IronBee engine)
 */
static void cleanup_ib_connection(void *data)
{
    assert(data != NULL);

    ib_engine_t *ib = (ib_engine_t *)data;

    /* Release the engine, but don't destroy it */
    tsib_manager_engine_release(ib);
}

/**
 * Convert an IP address into a string.
 *
 * @param[in,out] addr IP address structure
 * @param[in] str Buffer in which to store the address string
 * @param[in] port Pointer to port number (also filled in)
 */
static void addr2str(const struct sockaddr *addr, char *str, int *port)
{
    char serv[8]; /* port num */
    int rv = getnameinfo(addr, sizeof(*addr), str, ADDRSIZE, serv, 8,
                         NI_NUMERICHOST|NI_NUMERICSERV);
    if (rv != 0) {
        TSError("[ironbee] getnameinfo: %d", rv);
    }
    *port = atoi(serv);
}

/**
 * Initialize the IB connection.
 *
 * Initializes an IronBee connection from a ATS continuation
 *
 * @param[in] iconn IB connection
 * @param[in] ssn Session context data
 *
 * @returns status
 */
static ib_status_t ironbee_conn_init(
    tsib_ssn_ctx *ssndata)
{
    assert(ssndata != NULL);
    const struct sockaddr *addr;
    int                    port;
    ib_conn_t             *iconn = ssndata->iconn;

    /* remote ip */
    addr = TSHttpTxnClientAddrGet(ssndata->txnp);
    addr2str(addr, ssndata->remote_ip, &port);

    iconn->remote_ipstr = ssndata->remote_ip;

    /* remote port */
    iconn->remote_port = port;

    /* local end */
    addr = TSHttpTxnIncomingAddrGet(ssndata->txnp);

    addr2str(addr, ssndata->local_ip, &port);

    iconn->local_ipstr = ssndata->local_ip;

    /* local_port */
    iconn->local_port = port;

    return IB_OK;
}

static void tx_finish(ib_tx_t *tx)
{
    if (!ib_flags_all(tx->flags, IB_TX_FREQ_FINISHED) ) {
        ib_state_notify_request_finished(tx->ib, tx);
    }
    if (!ib_flags_all(tx->flags, IB_TX_FRES_FINISHED) ) {
        ib_state_notify_response_finished(tx->ib, tx);
    }
    if (!ib_flags_all(tx->flags, IB_TX_FPOSTPROCESS)) {
        ib_state_notify_postprocess(tx->ib, tx);
    }
    if (!ib_flags_all(tx->flags, IB_TX_FLOGGING)) {
        ib_state_notify_logging(tx->ib, tx);
    }
}

static void tx_list_destroy(ib_conn_t *conn)
{
    while (conn->tx_first != NULL) {
        tx_finish(conn->tx_first);
        ib_tx_destroy(conn->tx_first);
    }
}


/**
 * Handle session context destroy.
 *
 * Handles TS_EVENT_HTTP_SSN_CLOSE (session close) close event from the
 * ATS.
 *
 * @param[in,out] ctx session context
 */
static void tsib_ssn_ctx_destroy(tsib_ssn_ctx * ssndata)
{
    if (ssndata == NULL) {
        return;
    }

    /* To avoid the risk of sequencing issues with this coming before TXN_CLOSE,
     * we just mark the session as closing, but leave actually closing it
     * for the TXN_CLOSE if there's a TXN
     */
    if (ssndata->txn_count == 0) { /* No outstanding TXN_CLOSE to come. */
        if (ssndata->iconn != NULL) {
            ib_conn_t *conn = ssndata->iconn;
            ssndata->iconn = NULL;

            tx_list_destroy(conn);
            TSDebug("ironbee",
                    "tsib_ssn_ctx_destroy: calling ib_state_notify_conn_closed()");
            ib_state_notify_conn_closed(conn->ib, conn);
            TSDebug("ironbee", "CONN DESTROY: conn=%p", conn);
            ib_conn_destroy(conn);
        }

        /* Store off the continuation pointer */
        TSCont contp = ssndata->contp;
        TSContDataSet(contp, NULL);
        ssndata->contp = NULL;

        TSContDestroy(contp);
        TSfree(ssndata);
    }
    else {
        ssndata->closing = 1;
    }
}

/**
 * Handler function to generate an internal error response
 * when ironbee is unavailable to fill the fields or log errors.
 *
 * This may come from a TXN_START event, in which case we're
 * returning an HTTP/0.9 response and most of this is superfluous.
 */
static void internal_error_response(TSHttpTxn txnp)
{
    TSReturnCode rv;
    const char *reason = "Server Unavailable";
    TSMBuffer bufp;
    TSMLoc hdr_loc;
    TSMLoc field_loc;
    char *body;
    char clen[8];
    int i;
    const struct {
        const char *name;
        const char *val;
    } headers[2] = {
        { "Content-Type", "text/plain" },
        { "Content-Length", clen }
    };

    if (TSHttpTxnClientRespGet(txnp, &bufp, &hdr_loc) != TS_SUCCESS) {
        TSError("[ironbee] ErrorDoc: couldn't retrieve client response header.");
        return;
    }
    rv = TSHttpHdrStatusSet(bufp, hdr_loc, 503);
    if (rv != TS_SUCCESS) {
        TSError("[ironbee] ErrorDoc: TSHttpHdrStatusSet");
    }
    rv = TSHttpHdrReasonSet(bufp, hdr_loc, reason, strlen(reason));
    if (rv != TS_SUCCESS) {
        TSError("[ironbee] ErrorDoc: TSHttpHdrReasonSet");
    }

    /* this will free the body, so copy it first! */
    body = TSstrdup("Server unavailable or disabled.\n");

    snprintf(clen, sizeof(clen), "%zd", strlen(body));

    for (i = 0; i < 2; ++i) {
        rv = TSMimeHdrFieldCreate(bufp, hdr_loc, &field_loc);
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSMimeHdrFieldCreate");
            continue;
        }
        rv = TSMimeHdrFieldNameSet(bufp, hdr_loc, field_loc,
                                   headers[i].name, strlen(headers[i].name));
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSMimeHdrFieldNameSet");
            goto freehdr;
        }
        rv = TSMimeHdrFieldValueStringInsert(bufp, hdr_loc, field_loc, -1,
                                             headers[i].val, strlen(headers[i].val));
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSMimeHdrFieldValueStringInsert");
            goto freehdr;
        }
        rv = TSMimeHdrFieldAppend(bufp, hdr_loc, field_loc);
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSMimeHdrFieldAppend");
            goto freehdr;
        }

freehdr:
        rv = TSHandleMLocRelease(bufp, hdr_loc, field_loc);
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSHandleMLocRelease 3");
            continue;
        }
    }

    TSHttpTxnErrorBodySet(txnp, body, strlen(body), NULL);

    rv = TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
    if (rv != TS_SUCCESS) {
        TSError("[ironbee] ErrorDoc: TSHandleMLocRelease 4");
    }
}
/**
 * Handler function to generate an error response
 */
static void error_response(TSHttpTxn txnp, tsib_txn_ctx *txndata)
{
    const char *reason;
    TSMBuffer bufp;
    TSMLoc hdr_loc;
    TSMLoc field_loc;
    hdr_list *hdrs;
    TSReturnCode rv;

    /* make caller responsible for sanity checking */
    assert((txndata != NULL) && (txndata->tx != NULL));

    reason = TSHttpHdrReasonLookup(txndata->status);

    if (TSHttpTxnClientRespGet(txnp, &bufp, &hdr_loc) != TS_SUCCESS) {
        ib_log_error_tx(txndata->tx,
                        "ErrorDoc: couldn't retrieve client response header.");
        TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
        return;
    }
    rv = TSHttpHdrStatusSet(bufp, hdr_loc, txndata->status);
    if (rv != TS_SUCCESS) {
        ib_log_error_tx(txndata->tx,
                        "ErrorDoc: TSHttpHdrStatusSet");
    }
    if (reason == NULL) {
        reason = "Other";
    }
    rv = TSHttpHdrReasonSet(bufp, hdr_loc, reason, strlen(reason));
    if (rv != TS_SUCCESS) {
        ib_log_error_tx(txndata->tx,
                        "ErrorDoc: TSHttpHdrReasonSet");
    }

    while (hdrs = txndata->err_hdrs, hdrs != 0) {
        txndata->err_hdrs = hdrs->next;
        rv = TSMimeHdrFieldCreate(bufp, hdr_loc, &field_loc);
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(txndata->tx,
                            "ErrorDoc: TSMimeHdrFieldCreate");
            continue;
        }
        rv = TSMimeHdrFieldNameSet(bufp, hdr_loc, field_loc,
                                   hdrs->hdr, strlen(hdrs->hdr));
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(txndata->tx,
                            "ErrorDoc: TSMimeHdrFieldNameSet");
            goto errordoc_free1;
        }
        rv = TSMimeHdrFieldValueStringInsert(bufp, hdr_loc, field_loc, -1,
                                        hdrs->value, strlen(hdrs->value));
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(txndata->tx,
                            "ErrorDoc: TSMimeHdrFieldValueStringInsert");
            goto errordoc_free1;
        }
        rv = TSMimeHdrFieldAppend(bufp, hdr_loc, field_loc);
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(txndata->tx,
                            "ErrorDoc: TSMimeHdrFieldAppend");
            goto errordoc_free1;
        }

errordoc_free1:
        rv = TSHandleMLocRelease(bufp, hdr_loc, field_loc);
        if (rv != TS_SUCCESS) {
            ib_log_error_tx(txndata->tx,
                            "ErrorDoc: TSHandleMLocRelease 1");
            continue;
        }
    }

    if (txndata->err_body) {
        /* this will free the body, so copy it first! */
        TSHttpTxnErrorBodySet(txnp, txndata->err_body,
                              txndata->err_body_len, NULL);
    }
    rv = TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
    if (rv != TS_SUCCESS) {
        ib_log_error_tx(txndata->tx,
                        "ErrorDoc: TSHandleMLocRelease 2");
    }

    ib_log_debug_tx(txndata->tx,
                    "Sent error page %d \"%s\".", txndata->status, reason);
}

/**
 * Handle transaction context destroy.
 *
 * Handles TS_EVENT_HTTP_TXN_CLOSE (transaction close) close event from the
 * ATS.
 *
 * @param[in,out] ctx Transaction context
 */
static void tsib_txn_ctx_destroy(tsib_txn_ctx *txndata)
{
    if (txndata == NULL) {
        return;
    }

    ib_tx_t *tx = txndata->tx;
    tsib_ssn_ctx *ssndata = txndata->ssn;

    assert(tx != NULL);
    assert(ssndata != NULL);

    txndata->tx = NULL;
    ib_log_debug_tx(tx,
                    "TX DESTROY: conn=>%p tx_count=%zd tx=%p id=%s txn_count=%d",
                    tx->conn, tx->conn->tx_count, tx, tx->id, ssndata->txn_count);
    tx_finish(tx);

    ib_tx_destroy(tx);

    txndata->ssn = NULL;

    /* Decrement the txn count on the ssn, and destroy ssn if it's closing.
     * We trust TS not to create more TXNs after signalling SSN close!
     */
    if (ssndata->closing && ssndata->txn_count <= 1) {
        if (ssndata->iconn) {
            tx_list_destroy(ssndata->iconn);
            ib_conn_t *conn = ssndata->iconn;
            ib_engine_t *ib = conn->ib;

            ssndata->iconn = NULL;
            TSDebug("ironbee",
                    "tsib_txn_ctx_destroy: calling ib_state_notify_conn_closed()");
            ib_state_notify_conn_closed(ib, conn);
            TSDebug("ironbee",
                    "CONN DESTROY: conn=%p", conn);
            ib_conn_destroy(conn);
        }
        TSContDataSet(ssndata->contp, NULL);
        TSContDestroy(ssndata->contp);
        TSfree(ssndata);
    }
    else {
        --(ssndata->txn_count);
    }
    TSfree(txndata);
}

/**
 * Plugin for the IronBee ATS.
 *
 * Handles some ATS events.
 *
 * @param[in,out] contp Pointer to the continuation
 * @param[in,out] event Event from ATS
 * @param[in,out] edata Event data
 *
 * @returns status
 */
int ironbee_plugin(TSCont contp, TSEvent event, void *edata)
{
    TSCont mycont;
    TSHttpTxn txnp = (TSHttpTxn) edata;
    TSHttpSsn ssnp = (TSHttpSsn) edata;
    tsib_txn_ctx *txndata;
    tsib_ssn_ctx *ssndata;
    tsib_hdr_outcome status;

    TSDebug("ironbee", "Entering ironbee_plugin with %d", event);
    switch (event) {

        /* CONNECTION */
        case TS_EVENT_HTTP_SSN_START:
            /* start of connection */
            /* But we can't initialize conn stuff here, because there's
             * no API to get the connection stuff required by ironbee
             * at this point.  So instead, intercept the first TXN
             *
             * what we can and must do: create a new contp whose
             * lifetime is our ssn
             */
            mycont = TSContCreate(ironbee_plugin, NULL);
            TSHttpSsnHookAdd (ssnp, TS_HTTP_TXN_START_HOOK, mycont);
            ssndata = TSmalloc(sizeof(*ssndata));
            memset(ssndata, 0, sizeof(*ssndata));
            /* The only failure here is EALLOC, and if that happens
             * we're ****ed anyway
             */
            ssndata->contp = mycont;
            TSContDataSet(mycont, ssndata);

            TSHttpSsnHookAdd (ssnp, TS_HTTP_SSN_CLOSE_HOOK, mycont);

            TSHttpSsnReenable (ssnp, TS_EVENT_HTTP_CONTINUE);
            break;

        case TS_EVENT_HTTP_TXN_START:
        {
            /* start of Request */
            /* First req on a connection, we set up conn stuff */
            ib_status_t  rc;
            ib_engine_t *ib = NULL;

            ssndata = TSContDataGet(contp);

            if (ssndata->iconn == NULL) {
                rc = tsib_manager_engine_acquire(&ib);
                if (rc == IB_DECLINED) {
                    /* OK, this means the manager is disabled deliberately,
                     * but otherwise all's well.  So this TXN
                     * gets processed without intervention from Ironbee
                     * and is invisble when our SSN_CLOSE hook runs.
                     */
                    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
                    TSDebug("ironbee", "Decline from engine manager");
                    break;
                }
                else if (rc != IB_OK) {
                    TSError("[ironbee] Failed to acquire engine: %s",
                            ib_status_to_string(rc));
                    goto noib_error;
                }
                if (ib != NULL) {
                    rc = ib_conn_create(ib, &ssndata->iconn, contp);
                    if (rc != IB_OK) {
                        TSError("[ironbee] ib_conn_create: %s",
                                ib_status_to_string(rc));
                        tsib_manager_engine_release(ib);
                        goto noib_error;
                    }

                    /* In the normal case, release the engine when the
                     * connection's memory pool is destroyed */
                    rc = ib_mm_register_cleanup(ssndata->iconn->mm,
                                                cleanup_ib_connection,
                                                ib);
                    if (rc != IB_OK) {
                        TSError("[ironbee] ib_mm_register_cleanup: %s",
                                ib_status_to_string(rc));
                        tsib_manager_engine_release(ib);
                        goto noib_error;
                    }

                    TSDebug("ironbee", "CONN CREATE: conn=%p", ssndata->iconn);
                    ssndata->txnp = txnp;
                    ssndata->txn_count = ssndata->closing = 0;

                    rc = ironbee_conn_init(ssndata);
                    if (rc != IB_OK) {
                        TSError("[ironbee] ironbee_conn_init: %s",
                                ib_status_to_string(rc));
                        goto noib_error;
                    }

                    TSContDataSet(contp, ssndata);
                    TSDebug("ironbee",
                            "ironbee_plugin: ib_state_notify_conn_opened()");
                    rc = ib_state_notify_conn_opened(ib, ssndata->iconn);
                    if (rc != IB_OK) {
                        TSError("[ironbee] Failed to notify connection opened: %s",
                                ib_status_to_string(rc));
                    }
                }
                else {
                    /* Use TSError where there's no ib or tx */
                    TSError("Ironbee: No ironbee engine!");
                    goto noib_error;
                }
            }

            /* create a txn cont (request ctx) and tx */
            txndata = TSmalloc(sizeof(*txndata));
            memset(txndata, 0, sizeof(*txndata));
            txndata->ssn = ssndata;
            txndata->txnp = txnp;

            rc = ib_tx_create(&txndata->tx, ssndata->iconn, txndata);
            if (rc != IB_OK) {
                TSError("[ironbee] Failed to create tx: %d", rc);
                tsib_manager_engine_release(ib);
                TSfree(txndata);
                goto noib_error;
            }

            ++ssndata->txn_count;

            ib_log_debug_tx(txndata->tx,
                            "TX CREATE: conn=%p tx=%p id=%s txn_count=%d",
                            ssndata->iconn, txndata->tx, txndata->tx->id,
                            txndata->ssn->txn_count);

            mycont = TSContCreate(ironbee_plugin, TSContMutexGet(contp));
            TSContDataSet(mycont, txndata);

            TSHttpTxnHookAdd(txnp, TS_HTTP_TXN_CLOSE_HOOK, mycont);

            /* Hook to process responses */
            TSHttpTxnHookAdd(txnp, TS_HTTP_READ_RESPONSE_HDR_HOOK, mycont);

            /* Hook to process requests */
            TSHttpTxnHookAdd(txnp, TS_HTTP_PRE_REMAP_HOOK, mycont);

            /* Hook to process request headers when sent to the server. */
            TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_REQUEST_HDR_HOOK, mycont);

            /* Create continuations for input and output filtering
             * to give them txn lifetime.
             */
            txndata->in_data_cont = TSTransformCreate(in_data_event, txnp);
            TSContDataSet(txndata->in_data_cont, txndata);

            txndata->out_data_cont = TSTransformCreate(out_data_event, txnp);
            TSContDataSet(txndata->out_data_cont, txndata);

            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;

noib_error:

            /* NULL txndata signals this to SEND_RESPONSE */
            TSContDataSet(contp, NULL);

            TSError("[ironbee] Internal error initialising for transaction");
            TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);

            /* FIXME: check this.
             * Purpose is to ensure contp doesn't leak, but may not be right
             */
            TSHttpTxnHookAdd(txnp, TS_HTTP_TXN_CLOSE_HOOK, contp);

            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
            break;
        }

        /* HTTP RESPONSE */
        case TS_EVENT_HTTP_SEND_REQUEST_HDR:
        {
            txndata = TSContDataGet(contp);

            /* This event is about as late as we can possibly block
             * body attacks against the server.
             */

            /* If we are not yet blocked, ask IronBee if we should block. */
            if (!HTTP_CODE(txndata->status)) {
                if (!ib_flags_all(txndata->tx->flags, IB_TX_FREQ_FINISHED)) {
                    ib_log_debug_tx(
                        txndata->tx,
                        "data_event: calling ib_state_notify_request_finished()"
                    );
                    (tsib_direction_client_req.ib_notify_end)(txndata->tx->ib, txndata->tx);
                }

                /* If we were not blocking and now are blocking,
                 * then this is a very late block and, to protect the server,
                 * we need to error-out of the transaction ASAP.
                 */
                if (HTTP_CODE(txndata->status)) {
                    TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
                    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);

                    /* Leave the case statement. */
                    break;
                }
            }

            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;
        }
        case TS_EVENT_HTTP_READ_RESPONSE_HDR:
            txndata = TSContDataGet(contp);
            if (txndata->tx == NULL) {
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
                break;
            }

            /* Feed ironbee the headers if not done already. */
            if (!ib_flags_all(txndata->tx->flags, IB_TX_FRES_STARTED)) {
                status = process_hdr(txndata, txnp, &tsib_direction_server_resp);

                /* OK, if this was an HTTP 100 response, it's not the
                 * response we're interested in.  No headers have been
                 * sent yet, and no data will be sent until we've
                 * reached here again with the final response.
                 */
                if (status == HDR_HTTP_100) {
                    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
                    break;
                }
                // FIXME: Need to know if this fails as it (I think) means
                //        that the response did not come from the server and
                //        that ironbee should ignore it.
                /* I've not seen a fail here.  AFAICT if either the origin
                 * isn't responding or we're responding from cache. we
                 * never reach here in the first place.
                 */
            }

            /* If ironbee signalled an error while processing request body data,
             * this is the first opportunity to divert to an errordoc
             */
            if (HTTP_CODE(txndata->status)) {
                ib_log_debug_tx(txndata->tx,
                                "HTTP code %d contp=%p", txndata->status, contp);
                TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
                break;
            }

            /* If we're not going to inspect response body data
             * we can bring forward notification of response-end
             * so we're in time to respond with an errordoc if Ironbee
             * wants to block in the response phase.
             *
             * This currently fails.  However, that appears to be because I
             * can't unset IB_TX_FINSPECT_RESBODY with InspectionEngineOptions
             */
            if (!ib_flags_all(txndata->tx->flags, IB_TX_FINSPECT_RESBODY)) {
                if (!ib_flags_all(txndata->tx->flags, IB_TX_FRES_STARTED) ) {
                    ib_state_notify_response_started(txndata->tx->ib, txndata->tx, NULL);
                }
                if (!ib_flags_all(txndata->tx->flags, IB_TX_FRES_FINISHED) ) {
                    ib_state_notify_response_finished(txndata->tx->ib, txndata->tx);
                }
                /* Test again for Ironbee telling us to block */
                if (HTTP_CODE(txndata->status)) {
                    TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
                    TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
                    break;
                }
            }

            /* Flag that we're too late to divert to an error response */
            ib_tx_flags_set(txndata->tx, IB_TX_FCLIENTRES_STARTED);

            /* Normal execution.  Add output filter to inspect response. */
            TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK,
                             txndata->out_data_cont);
            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);

            break;
        /* Hook for processing response headers. */
        case TS_EVENT_HTTP_SEND_RESPONSE_HDR:
            txndata = TSContDataGet(contp);
            if (txndata == NULL) {
                /* Ironbee is unavailable to help with our response. */
                /* This contp is not ours, so we leave it. */
                internal_error_response(txnp);
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
                break;
            }

            /* If ironbee has sent us into an error response then
             * we came here in our error path, with nonzero status.
             */
            if (txndata->status != 0) {
                error_response(txnp, txndata);
            }

            /* Feed ironbee the headers if not done already. */
            if (!ib_flags_all(txndata->tx->flags, IB_TX_FRES_STARTED)) {
                if (process_hdr(txndata, txnp, &tsib_direction_client_resp) != HDR_OK) {
                    /* I think this is a shouldn't happen event, and that
                     * if it does we have an ironbee bug or misconfiguration.
                     * Log an error to catch if it happens in practice.
                     */
                    ib_log_error_tx(txndata->tx, "process_hdr returned error in send_response_hdr event");
                }
            }

            /* If there is an ironbee-generated response body, notify ironbee.
             *
             * NOTE: I do not see anywhere else to put this as the error body is
             *       just a buffer and not delivered via normal IO channels, so
             *       the error body will never get caught by an event.
             */
            if ((txndata->status != 0) && (txndata->err_body != NULL)) {
                const char *data = txndata->err_body;
                size_t data_length = txndata->err_body_len;
                ib_log_debug_tx(txndata->tx,
                        "error_response: calling ib_state_notify_response_body_data() %s:%d",
                        __FILE__, __LINE__);
                ib_state_notify_response_body_data(txndata->tx->ib,
                                                   txndata->tx,
                                                   data, data_length);
            }

            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;

        /* HTTP REQUEST */
        case TS_EVENT_HTTP_READ_REQUEST_HDR:
            /* We got here from the same cont used for ssnstart.
             * If it's NULL we have a noib_error or just uninitialised IronBee
             * so we need to kill off the request as an internal error
             */
            txndata = TSContDataGet(contp);
            if (txndata == NULL) {
                TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
            }

            else {
                /* All's well */
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            }
            break;

        /* hook for processing incoming request/headers
         * The OS_DNS hook is an alternative here.
         */
        case TS_EVENT_HTTP_PRE_REMAP:
        {
            int request_inspection_finished = 0;
            txndata = TSContDataGet(contp);
            assert ((txndata != NULL) && (txndata->tx != NULL));
            status = process_hdr(txndata, txnp, &tsib_direction_client_req);
            if (HDR_OUTCOME_IS_HTTP_OR_ERROR(status, txndata)) {
                if (status == HDR_HTTP_STATUS) {
                    ib_log_debug_tx(txndata->tx,
                                    "HTTP code %d contp=%p", txndata->status, contp);
                 }
                 else {
                    /* Ironbee set a status we don't handle.
                     * We returned EINVAL, but we also need housekeeping to
                     * avoid a crash in modhtp and log something bad.
                     */
                    ib_log_debug_tx(txndata->tx,
                                    "Internal error %d contp=%p", txndata->status, contp);
                    /* Ugly hack: notifications to stop modhtp bombing out */
                    request_inspection_finished = 1;
                }
            }
            else {
                /* Other nonzero statuses not supported */
                switch(status) {
                  case HDR_OK:
                    /* If we're not inspecting the Request body,
                     * we can bring forward notification of end-request
                     * so any header-only tests run on Request phase
                     * can abort the tx before opening a backend connection.
                     */
                    if (!ib_flags_all(txndata->tx->flags, IB_TX_FINSPECT_REQBODY)) {
                        request_inspection_finished = 1;
                    }
                    break;	/* All's well */
                  case HDR_HTTP_STATUS:
                    // FIXME: should we take the initiative here and return 500?
                    ib_log_error_tx(txndata->tx,
                                    "Internal error: ts-ironbee requested error but no error response set.");
                    break;
                  case HDR_HTTP_100:
                    /* This can't actually happen with current Trafficserver
                     * versions, as TS will generate a 400 error without
                     * reference to us.  But in case that changes in future ...
                     */
                    ib_log_error_tx(txndata->tx,
                                    "No request headers found.");
                    break;
                  default:
                    ib_log_error_tx(txndata->tx,
                                    "Unhandled state arose in handling request headers.");
                    break;
                }
            }
            if (request_inspection_finished) {
                if (!ib_flags_all(txndata->tx->flags, IB_TX_FREQ_STARTED) ) {
                    ib_state_notify_request_started(txndata->tx->ib, txndata->tx, NULL);
                }
                if (!ib_flags_all(txndata->tx->flags, IB_TX_FREQ_FINISHED) ) {
                    ib_state_notify_request_finished(txndata->tx->ib, txndata->tx);
                }
            }
            else {
                /* hook an input filter to watch data */
                TSHttpTxnHookAdd(txnp, TS_HTTP_REQUEST_TRANSFORM_HOOK,
                                 txndata->in_data_cont);
            }
            /* Flag that we can no longer prevent a request going to backend */
            ib_tx_flags_set(txndata->tx, IB_TX_FSERVERREQ_STARTED);

            /* Check whether Ironbee told us to block the request.
             * This could now come not just from process_hdr, but also
             * from a brought-forward notification if we aren't inspecting
             * a request body and notified request_finished.
             */
            if (HTTP_CODE(txndata->status)) {
                TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
            }
            else {
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            }
            break;
        }


        /* CLEANUP EVENTS */
        case TS_EVENT_HTTP_TXN_CLOSE:
        {
            txndata = TSContDataGet(contp);

            if (txndata != NULL) {
                TSContDestroy(txndata->out_data_cont);
                TSContDestroy(txndata->in_data_cont);
            }
            TSContDataSet(contp, NULL);
            TSContDestroy(contp);
            if ( (txndata != NULL) && (txndata->tx != NULL) ) {
                ib_log_debug_tx(txndata->tx,
                                "TXN Close: %p", (void *)contp);
                tsib_txn_ctx_destroy(txndata);
            }
            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;
        }

        case TS_EVENT_HTTP_SSN_CLOSE:
            TSDebug("ironbee", "SSN Close: %p", (void *)contp);
            tsib_ssn_ctx_destroy(TSContDataGet(contp));
            tsib_manager_engine_cleanup();
            TSHttpSsnReenable(ssnp, TS_EVENT_HTTP_CONTINUE);
            break;

        case TS_EVENT_MGMT_UPDATE:
        {
            TSDebug("ironbee", "Management update");
            ib_status_t  rc;
            rc = tsib_manager_engine_create();
            if (rc != IB_OK) {
                TSError("[ironbee] Error creating new engine: %s",
                        ib_status_to_string(rc));
            }
            break;
        }

        /* if we get here we've got a bug */
        default:
            TSError("[ironbee] *** Unhandled event %d in ironbee_plugin.", event);
            break;
    }

    return 0;
}
