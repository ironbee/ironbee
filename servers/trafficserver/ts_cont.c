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
 * @author Sam Baskinger <sbaskinger@qualys.com>
 *
 * This file holds code that should run on separate scheduled calls
 * into traffic server's NET threads. That is, the default behavior
 * is for `TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE)` to
 * recurse into the continuation which continues to block the network
 * thread. This causes latching delay in TrafficServer.
 *
 * Rather, continutaions in this file are schedule for execution on
 * the `TS_THREAD_POOL_NET` after a delay of 0 seconds.
 */

#include <ironbee/core.h>
#include <ironbee/queue.h>
#include <ironbee/flags.h>
#include <ironbee/queue.h>
#include <ironbee/lock.h>
#include <ironbee/state_notify.h>
#include <ironbee/mpool_lite.h>
#include <ironbee/mm_mpool_lite.h>

#include <assert.h>
#include <ts/ts.h>
#include "ts_ib.h"
#include "ts_cont.h"

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif

static int process_handler(TSCont contp, TSEvent event, void *edata);

struct ts_jobqueue_t {
    ib_mm_t          mm;
    ib_mpool_lite_t *mp;
    ib_queue_t      *queue;
    ib_lock_t       *lock;
    tsib_txn_ctx    *txndata;
};

typedef struct job_rec_t job_rec_t;
struct job_rec_t {
    job_type_t    type;
    tsib_txn_ctx *txndata;
    TSCont        contp;
    void         *data;
};

void ts_jobqueue_destroy(ts_jobqueue_t *jq)
{
    if (jq != NULL && jq->mp != NULL) {
        ib_mpool_lite_destroy(jq->mp);
    }
}

ib_status_t ts_jobqueue_create(
    tsib_txn_ctx   *txndata,
    TSMutex         mutex
)
{
    assert(txndata != NULL);

    ib_mm_t          mm;
    ib_mpool_lite_t *mp;
    ts_jobqueue_t   *jq;
    ib_status_t      rc;

    rc = ib_mpool_lite_create(&mp);
    if (rc != IB_OK) {
        return IB_EALLOC;
    }

    mm = ib_mm_mpool_lite(mp);

    jq = ib_mm_alloc(mm, sizeof(*jq));
    if (jq == NULL) {
        return IB_EALLOC;
    }

    jq->mm = mm;
    jq->mp = mp;

    rc = ib_queue_create(&jq->queue, mm, 0);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_lock_create(&jq->lock, mm);
    if (rc != IB_OK) {
        return rc;
    }

    txndata->jobqueue          = jq;
    jq->txndata                = txndata;
    jq->txndata->process_contp = TSContCreate(process_handler, mutex);
    TSContDataSet(jq->txndata->process_contp, (void *)jq);

    return IB_OK;
}

static void job_rec_destroy(job_rec_t *rec)
{
    if (rec != NULL) {
        TSfree(rec);
    }
}

static job_rec_t * job_rec_create(
    tsib_txn_ctx *txndata,
    job_type_t    type,
    TSCont        contp,
    void         *edata
)
{
    assert(txndata != NULL);

    job_rec_t *rec = TSmalloc(sizeof(*rec));

    // FIXME - fail more gracefully incase of malloc failures.
    if (rec != NULL) {
        rec->type    = type;
        rec->txndata = txndata;
        rec->contp   = contp;
        rec->data    = edata;
    }

    return rec;
}

void ts_jobqueue_in(
    tsib_txn_ctx *txndata,
    job_type_t    type,
    TSCont        contp,
    void         *edata
)
{
    assert(txndata != NULL);
    assert(txndata->jobqueue != NULL);
    assert(txndata->jobqueue->txndata != NULL);
    assert(txndata == txndata->jobqueue->txndata);

    ts_jobqueue_t *jobqueue = txndata->jobqueue;

    // FIXME - fail more gracefully incase of malloc failures.
    job_rec_t *rec = job_rec_create(
        txndata,
        type,
        contp,
        edata
    );

    TSDebug("ironbee",
            "Queue job type=%d txndata=%p contp=%p data=%p",
            rec->type, rec->txndata, rec->contp, rec->data);

    // FIXME - capture and respond to rc status.
    ib_lock_lock(jobqueue->lock);
    ib_queue_enqueue(jobqueue->queue, rec);
    ib_lock_unlock(jobqueue->lock);
}

static job_rec_t * job_queue_out(tsib_txn_ctx *txndata)
{
    assert(txndata != NULL);
    assert(txndata->jobqueue != NULL);

    ib_status_t rc;
    job_rec_t *rec;

    rc = ib_lock_lock(txndata->jobqueue->lock);
    if (rc != IB_OK) {
        return NULL;
    }

    rc = ib_queue_dequeue(txndata->jobqueue->queue, (void *)&rec);

    ib_lock_unlock(txndata->jobqueue->lock);

    /* Queue is empty case. */
    if (rc == IB_ENOENT) {
        return NULL;
    }
    /* Error path. Result is the same as the empty case. */
    else if (rc != IB_OK) {
        return NULL;
    }

    return rec;
}


void ts_jobqueue_schedule(tsib_txn_ctx *txndata)
{
    assert(txndata != NULL);
    assert(txndata->process_contp != NULL);

    TSContSchedule(txndata->process_contp, 0, TS_THREAD_POOL_NET);
}

static int process_handler(TSCont contp, TSEvent event, void *edata) {
    TSDebug("ironbee", "Entering process_handler() event=%d edata=%p", event, edata);

    ts_jobqueue_t *jobqueue = TSContDataGet(contp);
    tsib_txn_ctx  *txndata  = jobqueue->txndata;
    tsib_ssn_ctx  *ssndata  = txndata->ssn;

    job_rec_t *rec = job_queue_out(txndata);
    while (rec != NULL) {
        TSDebug("ironbee",
                "Process job type=%d txndata=%p contp=%p data=%p",
                rec->type, rec->txndata, rec->contp, rec->data);

        TSHttpTxn txnp = rec->data;

        switch (rec->type) {
            case JOB_CONN_STARTED:
                TSDebug("ironbee", "Processing JOB_CONN_STARTED.");
                TSDebug("ironbee", "Done processing JOB_CONN_STARTED.");
                break;
            case JOB_TX_STARTED:
            {
                TSCont mycont = NULL;
                TSDebug("ironbee", "Processing JOB_TX_STARTED.");

                ib_log_debug_tx(txndata->tx,
                                "TX CREATE: conn=%p tx=%p id=%s txn_count=%d",
                                ssndata->iconn, txndata->tx, txndata->tx->id,
                                txndata->ssn->txn_count);

                mycont = TSContCreate(ironbee_plugin, ssndata->ts_mutex);
                TSContDataSet(mycont, txndata);

                TSHttpTxnHookAdd(txnp, TS_HTTP_TXN_CLOSE_HOOK, mycont);

                /* Hook to process responses */
                TSHttpTxnHookAdd(txnp, TS_HTTP_READ_RESPONSE_HDR_HOOK, mycont);

                /* Hook to process requests */
                TSHttpTxnHookAdd(txnp, TS_HTTP_PRE_REMAP_HOOK, mycont);

                /* Create continuations for input and output filtering
                 * to give them txn lifetime.
                 */
                txndata->in_data_cont = TSTransformCreate(in_data_event, txnp);
                TSContDataSet(txndata->in_data_cont, txndata);

                txndata->out_data_cont = TSTransformCreate(out_data_event, txnp);
                TSContDataSet(txndata->out_data_cont, txndata);

                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);

                TSDebug("ironbee", "Done processing JOB_TX_STARTED.");
                break;
            }
            case JOB_REQ_HEADER:
            {
                TSDebug("ironbee", "Processing JOB_REQ_HEADER.");
                tsib_hdr_outcome status;
                int request_inspection_finished = 0;
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
                        break;  /* All's well */
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
                TSDebug("ironbee", "Done processing JOB_REQ_HEADER.");
                break;
            }
            case JOB_REQ_DATA:
                TSDebug("ironbee", "Processing JOB_REQ_DATA.");

                // NOTE: Lock as we are using data in another thread.
                //TSMutexLock(ssndata->ts_mutex);

                // tsib_filter_ctx *fctx = (tsib_filter_ctx *) rec->data;
                // ibd_ctx ctx = {
                //     .data = fctx,
                //     .ibd = &tsib_direction_client_req
                // };
                // FIXME TODO - srb - process_data(rec->contp, fctx);
                assert(0 && "TODO");

                //TSMutexUnlock(ssndata->ts_mutex);

                TSDebug("ironbee", "Done processing JOB_REQ_DATA.");
                break;
            case JOB_RES_HEADER:
            {
                tsib_hdr_outcome status;

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
            }
            case JOB_RES_DATA:
                TSDebug("ironbee", "Processing JOB_RES_DATA.");

                // TODO: This may crash for responses that DO NOT have bodies.

                // NOTE: Lock as we are using data in another thread.
                //TSMutexLock(ssndata->ts_mutex);

                // tsib_filter_ctx *fctx = (tsib_filter_ctx *) rec->data;
                // ibd_ctx ctx = {
                //     .data = fctx,
                //     .ibd = &tsib_direction_server_resp
                // };
                // FIXME TODO - srb - process_data(rec->contp, fctx);
                assert(0 && "TODO");

                //TSMutexUnlock(ssndata->ts_mutex);

                TSDebug("ironbee", "Done processing JOB_RES_DATA.");
                break;
            case JOB_TX_FINISHED:
            {
                TSDebug("ironbee", "Processing JOB_TX_FINISHED.");
                if (txndata != NULL) {
                    jobqueue = txndata->jobqueue;
                    TSContDestroy(txndata->out_data_cont);
                    TSContDestroy(txndata->in_data_cont);
                }
                TSContDataSet(contp, NULL);
                TSContDestroy(contp);
                if ( (txndata != NULL) && (txndata->tx != NULL) ) {
                    ib_log_debug_tx(txndata->tx,
                                    "TXN Close: %p", (void *)contp);
                    ts_tsib_txn_ctx_destroy(txndata);
                }
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);

                TSDebug("ironbee", "Done processing JOB_TX_FINISHED on %p.", contp);

                /* Setup the loop for exit. */
                ts_jobqueue_destroy(jobqueue);
                job_rec_destroy(rec);
                rec = NULL;

                /* Skip the code after this case statement that touches
                 * the queue. The queue has been destroyed. */
                continue;
            }
            case JOB_CONN_FINISHED:
                TSDebug("ironbee", "Processing JOB_CONN_FINISHED.");
                TSDebug("ironbee", "Done processing JOB_CONN_FINISHED.");
                break;
            default:
                TSError("ironbee" ": *** BUG: Invalid job type=%d", rec->type);
                abort();
        }

        /* Advance the queue. This code is skipped when a JOB_TX_FINISHED
         * destroys the queue. */
        job_rec_destroy(rec);
        rec = job_queue_out(txndata);
    }

    return 0;
}