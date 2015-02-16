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
 * @brief IronBee --- Ironbee notification events.
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <assert.h>
#include <ts/ts.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif

#include <ironbee/core.h>
#include <ironbee/flags.h>
#include <ironbee/state_notify.h>

#include "ts_ib.h"

/* Manage a thread pool for notifications
 *
 * Requirements:
 * Event notifications dispatched via queue
 * Maintain pool of threads to do the work
 *
 * Issues:
 * Potential dangling pointers?
 * Race conditions - possibly structure event queue to be tx-aware,
 * or use tx->mutex.  But tx mutex alone isn't enough to enforce order.
 * Make queue-pop a mutexed event that checks on tx too.
 * Rendezvous, for when processing must wait on completion of a notification
 *   ... or else use state and EAGAIN?
 *
 * This is going to work with data events, but phase events may imply
 * unacceptable blocking.  See how much we can do.
 *
 * Queue: fifo, or more structured?
 *
 * No, we can move a busy flag to our existing tx record and just look that up
 * 
 *
 * Cleanups: we're going to have to run tx and conn cleanups async.
 * as scheduled events or block in SSN_CLOSE and TXN_CLOSE.
 * To avoid blocking we need to tie everything to Ironbee's tx and conn,
 * and ensure nothing gets cleaned up in TS's SSN/TXN_CLOSE events.
 * Actually there are bigger memory issues, because we have the conflict
 * between freeing data once streamed, and delaying until we've notified
 * asynchronously.  I *think* we can work this by moving TS Consume to
 * the notify thread, but maybe we'll need separate counters for processed
 * and notified data and only mark consumed up to the lesser one.
 */

/* Notification queue and its mutex */
static ib_queue_t *notifications;
static ib_lock_t *mutex;

/* Thread management */
static pthread_cond_t cond;
static pthread_mutex_t cond_mutex;

typedef struct notif_t {
    /* Notifications have 2-4 args.  All are pointers except the 4th
     * which is a size_t.  First arg is ib_engine; args 2-3 vary.
     */
    enum { args2, args3, args4 } calltype;
    enum { EVENT_TX, EVENT_CONN } event_type;
    union {
        void *p;
        ib_status_t (*f2)(ib_engine_t*, void*);
        ib_status_t (*f3)(ib_engine_t*, void*, void*);
        ib_status_t (*f4)(ib_engine_t*, void*, void*, size_t);
    } call;
    union {
        void *p;
        ib_tx_t *tx;
        ib_conn_t *conn;
    } event;
    void *arg3;
    size_t arg4;
} notif_t;

typedef struct txqueue_elem {
    notif_t *notif;
} txqueue_elem;

ib_status_t tsib_notify_tx(ib_tx_t *tx, void *call, void *arg3, size_t arg4)
{
    ib_status_t rv = IB_OK;
    notif_t *notif = ib_mm_alloc(tx->mm, sizeof(notif_t));
    //tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;

    if (arg4 == (size_t)-1) {
        notif->calltype = args2;
    }
    else if (arg4 == (size_t)-2) {
        notif->calltype = args3;
    }   
    else {
        notif->calltype = args4;
    }
    notif->call.p = call;
    notif->arg3 = arg3;
    notif->arg4 = arg4;
    /* fill in fields */
    notif->event_type = EVENT_TX;
    notif->event.tx = tx;
    ib_lock_lock(mutex);
    ib_queue_push_back(notifications, notif);
    ib_lock_unlock(mutex);

    /* In case all notification threads are asleep */
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&cond_mutex);
    return rv;
}
ib_status_t tsib_notify_conn(ib_conn_t *conn, void *call)
{
    /* A conn can't be marked busy
     * We can revert to synchronous notification for conns if necessary
     */
    ib_status_t rv = IB_OK;
    notif_t *notif = ib_mm_alloc(conn->mm, sizeof(notif_t));
    notif->calltype = args2;
    notif->call.p = call;
    notif->event.conn = conn;
    notif->event_type = EVENT_CONN;
    ib_lock_lock(mutex);
    ib_queue_push_back(notifications, notif);
    ib_lock_unlock(mutex);

    /* In case all notification threads are asleep */
    pthread_mutex_lock(&cond_mutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&cond_mutex);
    return rv;
}
static notif_t *tx_queue_pop_ex(void)
{
    notif_t *notif = NULL;
    if (ib_queue_pop_front(notifications, &notif) != IB_OK) {
        return NULL;
    }
    if (notif->event_type == EVENT_TX) {
        tsib_txn_ctx *txndata = (tsib_txn_ctx *)notif->event.tx->sctx;
        if (txndata->busy) {
            /* get the next element and return this one to front of queue */
            notif_t *busy = notif;
            notif = tx_queue_pop_ex();
            ib_queue_push_front(notifications, busy);
        }
        else {
            txndata->busy = 1;
        }
    }  /* If it's not a tx, we don't have a concept of busy */
    return notif;
}
static notif_t *tx_queue_pop(void)
{
    notif_t *ret;// = NULL;
    ib_lock_lock(mutex);
    ret = tx_queue_pop_ex();
    ib_lock_unlock(mutex);
    return ret;
}


static ib_status_t notify(notif_t *item)
{
    ib_status_t ret;
    ib_engine_t *ib = NULL;  /* init to shut up compiler error */
    
    switch (item->event_type) {
      case EVENT_TX:
        ib = item->event.tx->ib;
        break;
      case EVENT_CONN:
        ib = item->event.conn->ib;
        break;
    }
    switch (item->calltype) {
      case args2:
        ret = (*item->call.f2)(ib, item->event.p);
        break;
      case args3:
        ret = (*item->call.f3)(ib, item->event.p, item->arg3);
        break;
      case args4:
        ret = (*item->call.f4)(ib, item->event.p, item->arg3, item->arg4);
        break;
    }
    return ret;
}
static void *tsib_notification_thread(void* arg)
{
    /* get queue mutex
     * pop front of queue
     * free queue mutex
     * get tx mutex
     * run notify function
     * free tx mutex
     *
     * This doesn't work.  As above there's a race for get tx mutex.
     * If we do that while holding queue mutex, we'll block.
     * And ironbee's blocking decisions may be waiting on us!
     *
     * Hmmm.  Maybe create a notify thread per-tx rather than a threadpool?
     */

    for (;;) {
        notif_t* item = tx_queue_pop();
        if (item != NULL) {
           ib_status_t status = notify(item);
           /* TODO: can we do anything sensible with an error? */
           if (status != IB_OK) {
               TSDebug("[ironbee]", "notification returned %d", status);
           }
           if (item->event_type == EVENT_TX) {
              tsib_txn_ctx *txndata = (tsib_txn_ctx *)item->event.tx->sctx;
              txndata->busy = 0;
           }
        }
        else {
            /* Nothing to do now; sleep until woken */
            pthread_mutex_lock(&cond_mutex);
            pthread_cond_wait(&cond, &cond_mutex);
            pthread_mutex_unlock(&cond_mutex);
        }
    }
    return NULL;
}
ib_status_t tsib_notification_init(ib_engine_t *ib, int nthreads)
{
    int i;
    ib_status_t ret;
    ib_mm_t mm = ib_engine_mm_main_get(ib);
    /* Initialise notifications and mutexes */
    pthread_cond_init(&cond, NULL);
    ib_lock_create(&mutex, mm);
    ib_queue_create(&notifications, mm, IB_QUEUE_NEVER_SHRINK);

    /* Launch worker threads */
    for (i = 0; i < nthreads; ++i) {
        char *thread_name = (char *) ib_mm_alloc(mm, 32);
        sprintf(thread_name, "TS_IB Notification [%d]", i);
        if (!TSThreadCreate(tsib_notification_thread, thread_name)) {
            TSError("[TSPluginInit] Error while creating threads");
            ret = IB_EOTHER;
        }
    }
    return ret;
}

/* notify calls.  We can't use macros because we use most of these as struct members */
#define NOTIFY2(fn,tx) tsib_notify_tx(tx,(void*)fn,NULL,(size_t)-1)
#define NOTIFY3(fn,tx,arg) tsib_notify_tx(tx,(void*)fn,(void*)arg,(size_t)-2)
#define NOTIFY4(fn,tx,arg,sz) tsib_notify_tx(tx,(void*)fn,(void*)arg,sz)


ib_status_t tsib_state_notify_request_header_data(ib_engine_t *ib, ib_tx_t *tx,ib_parsed_headers_t *hdr)
{
    return NOTIFY3(ib_state_notify_request_header_data,tx,hdr);
}
ib_status_t tsib_state_notify_request_header_finished(ib_engine_t *ib, ib_tx_t *tx)
{
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;
    ib_status_t ret = NOTIFY2(ib_state_notify_request_header_finished,tx);
    /* this event may be used in a rendezvous */
    if (tx->flags & IB_TX_FBLOCKING_MODE) {
        pthread_mutex_lock(&txndata->rendezvous.mutex);
        pthread_cond_signal(&txndata->rendezvous.cond);
        pthread_mutex_unlock(&txndata->rendezvous.mutex);
    }
    return ret;
}
ib_status_t tsib_state_notify_request_body_data(ib_engine_t *ib, ib_tx_t *tx,const char *data,size_t len)
{
    ib_status_t ret = NOTIFY4(ib_state_notify_request_body_data,tx,data,len);
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;
    txndata->in.bytes_notified += len;
    return ret;
}
ib_status_t tsib_state_notify_request_finished(ib_engine_t *ib, ib_tx_t *tx)
{
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;
    ib_status_t ret = NOTIFY2(ib_state_notify_request_finished,tx);
    /* this event may be used in a rendezvous */
    if (tx->flags & IB_TX_FBLOCKING_MODE) {
        pthread_mutex_lock(&txndata->rendezvous.mutex);
        pthread_cond_signal(&txndata->rendezvous.cond);
        pthread_mutex_unlock(&txndata->rendezvous.mutex);
    }
    return ret;
}
ib_status_t tsib_state_notify_response_header_data(ib_engine_t *ib, ib_tx_t *tx,ib_parsed_headers_t *hdr)
{
    return NOTIFY3(ib_state_notify_response_header_data,tx,hdr);
}
ib_status_t tsib_state_notify_response_header_finished(ib_engine_t *ib, ib_tx_t *tx)
{
    return NOTIFY2(ib_state_notify_response_header_finished,tx);
}
ib_status_t tsib_state_notify_response_body_data(ib_engine_t *ib, ib_tx_t *tx,const char *data,size_t len)
{
    ib_status_t ret = NOTIFY4(ib_state_notify_response_body_data,tx,data,len);
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;
    txndata->out.bytes_notified += len;
    return ret;
}
ib_status_t tsib_state_notify_response_finished(ib_engine_t *ib, ib_tx_t *tx)
{
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;
    ib_status_t ret = NOTIFY2(ib_state_notify_response_finished,tx);
    /* this event will be used in a rendezvous */
    pthread_mutex_lock(&txndata->rendezvous.mutex);
    pthread_cond_signal(&txndata->rendezvous.cond);
    pthread_mutex_unlock(&txndata->rendezvous.mutex);
    return ret;
}
ib_status_t tsib_state_notify_postprocess(ib_engine_t *ib, ib_tx_t *tx)
{
    return NOTIFY2(ib_state_notify_postprocess,tx);
}
ib_status_t tsib_state_notify_logging(ib_engine_t *ib, ib_tx_t *tx)
{
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;
    ib_status_t ret = NOTIFY2(ib_state_notify_logging,tx);
    /* this event will be used in a rendezvous */
    pthread_mutex_lock(&txndata->rendezvous.mutex);
    pthread_cond_signal(&txndata->rendezvous.cond);
    pthread_mutex_unlock(&txndata->rendezvous.mutex);
    return ret;
}
ib_status_t tsib_state_notify_request_started(ib_engine_t *ib, ib_tx_t *tx,void *x)
{
    return NOTIFY3(ib_state_notify_request_started,tx,x);
}
ib_status_t tsib_state_notify_response_started(ib_engine_t *ib, ib_tx_t *tx,void *x)
{
    return NOTIFY3(ib_state_notify_response_started,tx,x);
}
ib_status_t tsib_state_notify_conn_opened(ib_engine_t *ib,ib_conn_t *conn)
{
    return tsib_notify_conn(conn, (void*)ib_state_notify_conn_opened);
}
ib_status_t tsib_state_notify_conn_closed(ib_engine_t *ib,ib_conn_t *conn)
{
    return tsib_notify_conn(conn, (void*)ib_state_notify_conn_closed);
}

void tsib_rendezvous(tsib_txn_ctx *txndata, unsigned int event, int mode)
{
    /* If ironbee blocking is disabled, we only need to wait if always is set */
    if ((mode == 0) && !(txndata->tx->flags & IB_TX_FBLOCKING_MODE)) {
        return;
    }

    /* suspend until we get notification and rendezvous reached */
    pthread_mutex_lock(&txndata->rendezvous.mutex);

    if (mode == -1) {
        /* Wait for data (rendezvous for a filter).  Event is a direction. */
        tsib_filter_ctx *fctx = (event == IBD_RESP) ? &txndata->out : &txndata->in;
        /* Wait for an data notification to complete */
        while (fctx->bytes_notified < fctx->bytes_done + fctx->buffered) {
            /* cond_wait unlocks the mutex while suspended */
            pthread_cond_wait(&txndata->rendezvous.cond, &txndata->rendezvous.mutex);
        }
    }
    else {
        /* Wait for an event to complete */
        while (!(txndata->tx->flags & event)) {
            /* cond_wait unlocks the mutex while suspended */
            pthread_cond_wait(&txndata->rendezvous.cond, &txndata->rendezvous.mutex);
        }
    }
    pthread_mutex_unlock(&txndata->rendezvous.mutex);
}
