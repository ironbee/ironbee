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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include <ts/ts.h>

#include <sys/socket.h>
#include <netdb.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <ironbee/engine.h>
#include <ironbee/engine_manager.h>
#include <ironbee/engine_manager_control_channel.h>
#include <ironbee/config.h>
#include <ironbee/server.h>
#include <ironbee/context.h>
#include <ironbee/core.h>
#include <ironbee/logger.h>
#include <ironbee/site.h>
#include <ironbee/state_notify.h>
#include <ironbee/util.h>
#include <ironbee/string.h>

#include "ts_ib.h"

/**
 * IronBee connection cleanup
 *
 * @param[in] data Callback data (IronBee engine)
 */
static void cleanup_ib_connection(void *data)
{
    assert(data != NULL);
    assert(module_data.manager != NULL);

    ib_engine_t *ib = (ib_engine_t *)data;

    /* Release the engine, but don't destroy it */
    ib_manager_engine_release(module_data.manager, ib);
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
    ib_ssn_ctx *ssn)
{
    assert(ssn != NULL);
    const struct sockaddr *addr;
    int                    port;
    ib_conn_t             *iconn = ssn->iconn;

    /* remote ip */
    addr = TSHttpTxnClientAddrGet(ssn->txnp);
    addr2str(addr, ssn->remote_ip, &port);

    iconn->remote_ipstr = ssn->remote_ip;

    /* remote port */
    iconn->remote_port = port;

    /* local end */
    addr = TSHttpTxnIncomingAddrGet(ssn->txnp);

    addr2str(addr, ssn->local_ip, &port);

    iconn->local_ipstr = ssn->local_ip;

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
static void ib_ssn_ctx_destroy(ib_ssn_ctx * ctx)
{
    if (ctx == NULL) {
        return;
    }

    /* To avoid the risk of sequencing issues with this coming before TXN_CLOSE,
     * we just mark the session as closing, but leave actually closing it
     * for the TXN_CLOSE if there's a TXN
     */
    ib_lock_lock(&ctx->mutex);
    if (ctx->txn_count == 0) { /* TXN_CLOSE happened already */
        if (ctx->iconn) {
            ib_conn_t *conn = ctx->iconn;
            ctx->iconn = NULL;

            tx_list_destroy(conn);
            TSDebug("ironbee",
                    "ib_ssn_ctx_destroy: calling ib_state_notify_conn_closed()");
            ib_state_notify_conn_closed(conn->ib, conn);
            TSDebug("ironbee", "CONN DESTROY: conn=%p", conn);
            ib_conn_destroy(conn);
        }

        /* Store off the continuation pointer */
        TSCont contp = ctx->contp;
        TSContDataSet(contp, NULL);
        ctx->contp = NULL;

        /* Unlock has to come first 'cos ContDestroy destroys the mutex */
        TSContDestroy(contp);
        ib_lock_unlock(&ctx->mutex);
        ib_lock_destroy(&ctx->mutex);
        TSfree(ctx);
    }
    else {
        ctx->closing = 1;
        ib_lock_unlock(&ctx->mutex);
    }
}

/**
 * Handler function to generate an error response
 */
static void error_response(TSHttpTxn txnp, ib_txn_ctx *txndata)
{
    const char *reason = TSHttpHdrReasonLookup(txndata->status);
    TSMBuffer bufp;
    TSMLoc hdr_loc;
    TSMLoc field_loc;
    hdr_list *hdrs;
    TSReturnCode rv;

    if (TSHttpTxnClientRespGet(txnp, &bufp, &hdr_loc) != TS_SUCCESS) {
        TSError("[ironbee] ErrorDoc: couldn't retrieve client response header.");
        TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
        return;
    }
    rv = TSHttpHdrStatusSet(bufp, hdr_loc, txndata->status);
    if (rv != TS_SUCCESS) {
        TSError("[ironbee] ErrorDoc: TSHttpHdrStatusSet");
    }
    if (reason == NULL) {
        reason = "Other";
    }
    rv = TSHttpHdrReasonSet(bufp, hdr_loc, reason, strlen(reason));
    if (rv != TS_SUCCESS) {
        TSError("[ironbee] ErrorDoc: TSHttpHdrReasonSet");
    }

    while (hdrs = txndata->err_hdrs, hdrs != 0) {
        txndata->err_hdrs = hdrs->next;
        rv = TSMimeHdrFieldCreate(bufp, hdr_loc, &field_loc);
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSMimeHdrFieldCreate");
            continue;
        }
        rv = TSMimeHdrFieldNameSet(bufp, hdr_loc, field_loc,
                                   hdrs->hdr, strlen(hdrs->hdr));
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSMimeHdrFieldNameSet");
            goto errordoc_free1;
        }
        rv = TSMimeHdrFieldValueStringInsert(bufp, hdr_loc, field_loc, -1,
                                        hdrs->value, strlen(hdrs->value));
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSMimeHdrFieldValueStringInsert");
            goto errordoc_free1;
        }
        rv = TSMimeHdrFieldAppend(bufp, hdr_loc, field_loc);
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSMimeHdrFieldAppend");
            goto errordoc_free1;
        }

errordoc_free1:
        rv = TSHandleMLocRelease(bufp, hdr_loc, field_loc);
        if (rv != TS_SUCCESS) {
            TSError("[ironbee] ErrorDoc: TSHandleMLocRelease 1");
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
        TSError("[ironbee] ErrorDoc: TSHandleMLocRelease 2");
    }

    TSDebug("ironbee", "Sent error page %d \"%s\".", txndata->status, reason);
}

/**
 * Handle transaction context destroy.
 *
 * Handles TS_EVENT_HTTP_TXN_CLOSE (transaction close) close event from the
 * ATS.
 *
 * @param[in,out] ctx Transaction context
 */
static void ib_txn_ctx_destroy(ib_txn_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }

    //hdr_action_t *x;
    ib_tx_t *tx = ctx->tx;
    ib_ssn_ctx *ssn = ctx->ssn;

    assert(tx != NULL);
    assert(ssn != NULL);

    ib_lock_lock(&ssn->mutex);

    ctx->tx = NULL;
    TSDebug("ironbee",
            "TX DESTROY: conn=>%p tx_count=%zd tx=%p id=%s txn_count=%d",
            tx->conn, tx->conn->tx_count, tx, tx->id, ssn->txn_count);
    tx_finish(tx);
    ib_tx_destroy(tx);


    /* Decrement the txn count on the ssn, and destroy ssn if it's closing */
    ctx->ssn = NULL;

    /* If it's closing, the contp and with it the mutex are already gone.
     * Trust TS not to create more TXNs after signalling SSN close!
     */
    if (ssn->closing) {
        if (ssn->iconn) {
            tx_list_destroy(ssn->iconn);
            ib_conn_t *conn = ssn->iconn;
            ib_engine_t *ib = conn->ib;

            ssn->iconn = NULL;
            TSDebug("ironbee",
                    "ib_txn_ctx_destroy: calling ib_state_notify_conn_closed()");
            ib_state_notify_conn_closed(ib, conn);
            TSDebug("ironbee", "CONN DESTROY: conn=%p", conn);
            ib_conn_destroy(conn);
        }
        TSContDataSet(ssn->contp, NULL);
        TSContDestroy(ssn->contp);
        ib_lock_unlock(&ssn->mutex);
        ib_lock_destroy(&ssn->mutex);
        TSfree(ssn);
    }
    else {
        --(ssn->txn_count);
        ib_lock_unlock(&ssn->mutex);
    }
    TSfree(ctx);
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
    ib_status_t rc;
    TSVConn connp;
    TSCont mycont;
    TSHttpTxn txnp = (TSHttpTxn) edata;
    TSHttpSsn ssnp = (TSHttpSsn) edata;
    ib_txn_ctx *txndata;
    ib_ssn_ctx *ssndata;
    ib_hdr_outcome status;

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
            mycont = TSContCreate(ironbee_plugin, TSMutexCreate());
            TSHttpSsnHookAdd (ssnp, TS_HTTP_TXN_START_HOOK, mycont);
            ssndata = TSmalloc(sizeof(*ssndata));
            memset(ssndata, 0, sizeof(*ssndata));
            /* The only failure here is EALLOC, and if that happens
             * we're ****ed anyway
             */
            rc = ib_lock_init(&ssndata->mutex);
            assert(rc == IB_OK);
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
            ib_lock_lock(&ssndata->mutex);

            if (ssndata->iconn == NULL) {
                if (module_data.manager != NULL) {
                    rc = ib_manager_engine_acquire(module_data.manager, &ib);
                    if (rc == IB_DECLINED) {
                        TSError("[ironbee] Decline from engine manager");
                    }
                    else if (rc != IB_OK) {
                        TSError("[ironbee] Failed to acquire engine: %s",
                                ib_status_to_string(rc));
                    }
                }
            }

            if ( (ssndata->iconn == NULL) && (ib != NULL) ) {
                rc = ib_conn_create(ib, &ssndata->iconn, contp);
                if (rc != IB_OK) {
                    TSError("[ironbee] ib_conn_create: %s",
                            ib_status_to_string(rc));
                    ib_manager_engine_release(module_data.manager, ib);
                    return rc; // FIXME - figure out what to do
                }

                /* In the normal case, release the engine when the
                 * connection's memory pool is destroyed */
                rc = ib_mm_register_cleanup(ssndata->iconn->mm,
                                            cleanup_ib_connection,
                                            ib);
                if (rc != IB_OK) {
                    TSError("[ironbee] ib_mm_register_cleanup: %s",
                            ib_status_to_string(rc));
                    ib_manager_engine_release(module_data.manager, ib);
                    return rc; // FIXME - figure out what to do
                }

                TSDebug("ironbee", "CONN CREATE: conn=%p", ssndata->iconn);
                ssndata->txnp = txnp;
                ssndata->txn_count = ssndata->closing = 0;

                rc = ironbee_conn_init(ssndata);
                if (rc != IB_OK) {
                    TSError("[ironbee] ironbee_conn_init: %s",
                            ib_status_to_string(rc));
                    return rc; // FIXME - figure out what to do
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
            ++ssndata->txn_count;
            ib_lock_unlock(&ssndata->mutex);

            /* create a txn cont (request ctx) */
            mycont = TSContCreate(ironbee_plugin, TSMutexCreate());
            txndata = TSmalloc(sizeof(*txndata));
            memset(txndata, 0, sizeof(*txndata));
            txndata->ssn = ssndata;
            txndata->txnp = txnp;
            TSContDataSet(mycont, txndata);

            /* With both of these, SSN_CLOSE gets called first.
             * I must be misunderstanding SSN
             * So hook it all to TXN
             */
            TSHttpTxnHookAdd(txnp, TS_HTTP_TXN_CLOSE_HOOK, mycont);

            /* Hook to process responses */
            TSHttpTxnHookAdd(txnp, TS_HTTP_READ_RESPONSE_HDR_HOOK, mycont);

            /* Hook to process requests */
            TSHttpTxnHookAdd(txnp, TS_HTTP_READ_REQUEST_HDR_HOOK, mycont);

            if (ssndata->iconn == NULL) {
                txndata->tx = NULL;
            }
            else {
                ib_tx_create(&txndata->tx, ssndata->iconn, txndata);
                TSDebug("ironbee",
                        "TX CREATE: conn=%p tx=%p id=%s txn_count=%d",
                        ssndata->iconn, txndata->tx, txndata->tx->id,
                        txndata->ssn->txn_count);
            }

            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;
        }

        /* HTTP RESPONSE */
        case TS_EVENT_HTTP_READ_RESPONSE_HDR:
            txndata = TSContDataGet(contp);
            if (txndata->tx == NULL) {
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
                break;
            }

            /* Feed ironbee the headers if not done alread. */
            if (!ib_flags_all(txndata->tx->flags, IB_TX_FRES_STARTED)) {
                status = process_hdr(txndata, txnp, &ib_direction_server_resp);

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
                if (ib_flags_all(txndata->tx->flags, IB_TX_FRES_HEADER)) {
                    txndata->state |= HDRS_OUT;
                }
            }

            /* If ironbee signalled an error while processing request body data,
             * this is the first opportunity to divert to an errordoc
             */
            if (IB_HTTP_CODE(txndata->status)) {
                TSDebug("ironbee", "HTTP code %d contp=%p", txndata->status, contp);
                TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
                break;
            }

            /* hook an output filter to watch data */
            connp = TSTransformCreate(out_data_event, txnp);
            TSContDataSet(connp, txndata);
            TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK, connp);

            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;

        /* hook for processing response headers */
        /* If ironbee has sent us into an error response then
         * we came here in our error path, with nonzero status
         * FIXME: tests
         */
        case TS_EVENT_HTTP_SEND_RESPONSE_HDR:
            txndata = TSContDataGet(contp);

            txndata->state |= START_RESPONSE;

            if (txndata->status != 0) {
                error_response(txnp, txndata);
            }

            txndata->state |= START_RESPONSE;

            /* Feed ironbee the headers if not done already. */
            if (!ib_flags_all(txndata->tx->flags, IB_TX_FRES_STARTED)) {
                process_hdr(txndata, txnp, &ib_direction_client_resp);
            }

            /* If there is an error with a body, then notify ironbee.
             *
             * NOTE: I do not see anywhere else to put this as the error body is
             *       just a buffer and not delivered via normal IO channels, so
             *       the error body will never get caught by an event.
             */
            if ((txndata->status != 0) && (txndata->err_body != NULL)) {
                const char *data = txndata->err_body;
                size_t data_length = txndata->err_body_len;
                TSDebug("ironbee",
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
            txndata = TSContDataGet(contp);

            /* hook to examine output headers */
            /* Not sure why we can't do it right now, but it seems headers
             * are not yet available.
             * Can we use another case switch in this function?
             */
            //TSHttpTxnHookAdd(txnp, TS_HTTP_OS_DNS_HOOK, contp);
            TSHttpTxnHookAdd(txnp, TS_HTTP_PRE_REMAP_HOOK, contp);

            /* hook an input filter to watch data */
            connp = TSTransformCreate(in_data_event, txnp);
            TSDebug("ironbee-in-data", "TSTransformCreate contp=%p", connp);
            TSContDataSet(connp, txndata);
            TSHttpTxnHookAdd(txnp, TS_HTTP_REQUEST_TRANSFORM_HOOK, connp);

            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;

            /* hook for processing incoming request/headers */
        case TS_EVENT_HTTP_PRE_REMAP:
        case TS_EVENT_HTTP_OS_DNS:
            txndata = TSContDataGet(contp);
            status = process_hdr(txndata, txnp, &ib_direction_client_req);
            txndata->state |= HDRS_IN;
            if (IB_HDR_OUTCOME_IS_HTTP_OR_ERROR(status, txndata)) {
                if (status == HDR_HTTP_STATUS) {
                    TSDebug("ironbee", "HTTP code %d contp=%p", txndata->status, contp);
                 }
                 else {
                    TSDebug("ironbee", "Internal error %d contp=%p", txndata->status, contp);
                    /* Ugly hack: notifications to stop modhtp bombing out */
                    if (!ib_flags_all(txndata->tx->flags, IB_TX_FREQ_STARTED) ) {
                        ib_state_notify_request_started(txndata->tx->ib, txndata->tx, NULL);
                    }
                    if (!ib_flags_all(txndata->tx->flags, IB_TX_FREQ_FINISHED) ) {
                        ib_state_notify_request_finished(txndata->tx->ib, txndata->tx);
                    }
                 }
                TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
            }
            else {
                /* Other nonzero statuses not supported */
                switch(status) {
                  case HDR_OK:
                    break;	/* All's well */
                  case HDR_HTTP_STATUS:
                    // FIXME: should we take the initiative here and return 500?
                    TSError("[ironbee] Internal error: ts-ironbee requested error but no error response set.");
                    break;
                  case HDR_HTTP_100:
                    /* This can't actually happen with current Trafficserver
                     * versions, as TS will generate a 400 error without
                     * reference to us.  But in case that changes in future ...
                     */
                    TSError("[ironbee] No request headers found.");
                    break;
                  default:
                    TSError("[ironbee] Unhandled state arose in handling request headers.");
                    break;
                }
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            }
            break;


            /* CLEANUP EVENTS */
        case TS_EVENT_HTTP_TXN_CLOSE:
        {
            ib_txn_ctx *ctx = TSContDataGet(contp);

            TSContDataSet(contp, NULL);
            TSContDestroy(contp);
            if ( (ctx != NULL) && (ctx->tx != NULL) ) {
                TSDebug("ironbee", "TXN Close: %p", (void *)contp);
                ib_txn_ctx_destroy(ctx);
            }
            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;
        }

        case TS_EVENT_HTTP_SSN_CLOSE:
            TSDebug("ironbee", "SSN Close: %p", (void *)contp);
            ib_ssn_ctx_destroy(TSContDataGet(contp));
            if (module_data.manager != NULL) {
                ib_manager_engine_cleanup(module_data.manager);
            }
            //TSContDestroy(contp);
            TSHttpSsnReenable(ssnp, TS_EVENT_HTTP_CONTINUE);
            break;

        case TS_EVENT_MGMT_UPDATE:
        {
            TSDebug("ironbee", "Management update");
            if (module_data.manager != NULL) {
                ib_status_t  rc;
                rc = ib_manager_engine_create(module_data.manager,
                                              module_data.config_file);
                if (rc != IB_OK) {
                    TSError("[ironbee] Error creating new engine: %s",
                            ib_status_to_string(rc));
                }
            }
            break;
        }

        /* if we get here we've got a bug */
        default:
            TSError("[ironbee] BUG: unhandled event %d in ironbee_plugin.", event);
            break;
    }

    return 0;
}
