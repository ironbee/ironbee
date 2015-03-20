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
    ib_queue_t   *queue;
    ib_lock_t    *lock;
    tsib_txn_ctx *txndata;
};

typedef struct job_rec_t job_rec_t;
struct job_rec_t {
    job_type_t    type;
    tsib_txn_ctx *txndata;
    TSCont        contp;
    void         *data;
};

ib_status_t ts_jobqueue_create(
    tsib_txn_ctx   *txndata,
    ib_mm_t         mm
)
{
    ts_jobqueue_t *jq = ib_mm_alloc(mm, sizeof(*jq));
    ib_status_t    rc;

    if (jq == NULL) {
        return IB_EALLOC;
    }

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
    jq->txndata->process_contp = TSContCreate(process_handler, NULL);
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

    if (rc != IB_ENOENT) {
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

static void do_work(int i){
    assert(0 && "Somehow we got to do_work(). We should not.");
}

static int process_handler(TSCont contp, TSEvent event, void *edata) {
    TSDebug("ironbee", "Entering process_handler() event=%d edata=%p", event, edata);

    ts_jobqueue_t *jobqueue = TSContDataGet(contp);
    tsib_txn_ctx *txndata = jobqueue->txndata;
    tsib_ssn_ctx *ssndata = txndata->ssn;

    job_rec_t *rec = job_queue_out(txndata);
    while (rec != NULL) {
        TSDebug("ironbee",
                "Process job type=%d txndata=%p contp=%p data=%p",
                rec->type, rec->txndata, rec->contp, rec->data);

        TSHttpTxn txnp = (TSHttpTxn) rec->data;
        TSCont new_cont;

        switch (rec->type) {
            case JOB_CONN_STARTED:
                TSDebug("ironbee", "Processing JOB_CONN_STARTED.");
                TSDebug("ironbee", "Done processing JOB_CONN_STARTED.");
                break;
            case JOB_TX_STARTED:
                TSDebug("ironbee", "Processing JOB_TX_STARTED.");

                // NOTE: Create connection object
                // NOTE: Create transaction here
                do_work(16);

                TSDebug("ironbee",
                        "Transaction %d Start: %s:%d -> %s:%d",
                        ssndata->txn_count,
                        ssndata->remote_ip, ssndata->iconn->remote_port,
                        ssndata->local_ip, ssndata->iconn->local_port);

                // NOTE: Lock as we are using data in another thread.
                //TSMutexLock(ssndata->ts_mutex);

                /* New continuation is created with same mutex as session. */
                new_cont = TSContCreate(ironbee_plugin, ssndata->ts_mutex);
                //new_cont = TSContCreate(fast_plugin, NULL);
                TSContDataSet(new_cont, txndata);

                TSHttpTxnHookAdd(txnp, TS_HTTP_TXN_CLOSE_HOOK, new_cont);
                TSHttpTxnHookAdd(txnp, TS_HTTP_READ_RESPONSE_HDR_HOOK, new_cont);
                TSHttpTxnHookAdd(txnp, TS_HTTP_PRE_REMAP_HOOK, new_cont);

                txndata->in_data_cont = TSTransformCreate(in_data_event, txnp);
                TSContDataSet(txndata->in_data_cont, txndata);

                txndata->out_data_cont = TSTransformCreate(out_data_event, txnp);
                TSContDataSet(txndata->out_data_cont, txndata);

                //TSMutexUnlock(ssndata->ts_mutex);

                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);

                TSDebug("ironbee", "Done processing JOB_TX_STARTED.");
                break;
            case JOB_REQ_HEADER:
                TSDebug("ironbee", "Processing JOB_REQ_HEADER.");

                // TODO: For some reason this crashes if requests DO NOT have bodies.

                // NOTE: Lock as we are using data in another thread.
                //TSMutexLock(ssndata->ts_mutex);

                TSHttpTxnHookAdd(txnp, TS_HTTP_REQUEST_TRANSFORM_HOOK, txndata->in_data_cont);

                // NOTE: Notify request started
                // NOTE: Notify request headers
                do_work(25);

                // NOTE: After this we can no longer prevent header going to origin.

                //TSMutexUnlock(ssndata->ts_mutex);

                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);

                TSDebug("ironbee", "Done processing JOB_REQ_HEADER.");
                break;
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
                TSDebug("ironbee", "Processing JOB_RES_HEADER.");

                // NOTE: Lock as we are using data in another thread.
                //TSMutexLock(ssndata->ts_mutex);

                TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK, txndata->out_data_cont);

                // NOTE: Notify response started
                // NOTE: Notify response headers
                do_work(22);

                // NOTE: After this we can no longer prevent header going to client.


                //TSMutexUnlock(ssndata->ts_mutex);

                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);

                TSDebug("ironbee", "Done processing JOB_RES_HEADER.");
                break;
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
                TSDebug("ironbee", "Processing JOB_TX_FINISHED.");
                do_work(13);
                TSDebug("ironbee", "Done processing JOB_TX_FINISHED.");
                break;
            case JOB_CONN_FINISHED:
                TSDebug("ironbee", "Processing JOB_CONN_FINISHED.");
                TSDebug("ironbee", "Done processing JOB_CONN_FINISHED.");
                break;
            default:
                TSError("ironbee" ": *** BUG: Invalid job type=%d", rec->type);
                abort();
        }
        job_rec_destroy(rec);
        rec = job_queue_out(txndata);
    }

    return 0;
}