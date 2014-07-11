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

struct ibd_ctx {
    const ib_direction_data_t *ibd;
    ib_filter_ctx *data;
};

/**
 * Comparison function for qsort to order edits
 * Sort in reverse so apr_array_pop discards "first" elt for us
 *
 * @param[in] a - first edit
 * @param[in] b - second edit
 * @return difference in edits respective positions in stream
 */
static int qcompare(const void *a, const void *b)
{
    return ((edit_t*)b)->start - ((edit_t*)a)->start;
}

/**
 * Function to flush buffered data and apply edits in-stream
 *
 * @param[in] fctx - the filter data
 * @param[in] nbytes - number of bytes to flush (-1 to flush all data)
 * @param[in] last - final flush indicator (no more data to come)
 * @return success or error status
 */
static ib_status_t flush_data(ib_filter_ctx *fctx, int64_t nbytes, int last)
{
    /* copy logic from mod_range_filter.
     *
     *
     * It's a push logic, so that'll be range_filter's output filter
     *
     * Note: We're not buffering anything here.  We only see data
     *       when they're flushed from the buffer!
     */
    ib_status_t rc = IB_OK;
    int nedits, i;
    size_t n, start;
    if (nbytes == -1) {
        /* just output all we have */
        nbytes = fctx->buffered;
    }

    if ((fctx->edits != NULL) && (fctx->edits->len > 0)) {
        /* Sort to reverse order, so we can pop elements simply by
         * decrementing len
         */
        nedits = fctx->edits->len/sizeof(edit_t);
        qsort(fctx->edits->data, nedits, sizeof(edit_t), qcompare);
        for (i = nedits-1; i >= 0; --i) {
            edit_t *edit = &((edit_t*) fctx->edits->data)[i];

            /* sanity-check that edit is in range */
            if (edit->start < fctx->bytes_done) {
                /* Edit applies to data already gone.  This probably means
                 * someone fed us overlapping edits
                 */
                // TODO: log an error
                rc = IB_EBADVAL;

                /* Abandon this edit.  Continue loop (next edit may be fine) */
                fctx->edits->len -= sizeof(edit_t);
                continue;
            }
            else if (edit->start + edit->bytes > fctx->bytes_done + nbytes) {
                /* Edit goes beyond data we're dealing with.
                 * So leave it for next time.
                 * This could affect buffering behaviour, but in a good cause
                 * If this is out-of-range then so are other edits,
                 * but they'll be in range when we have more data.
                 *
                 * Best we can do now is to flush data before this edit.
                 * by setting nbytes.
                 *
                 * Exception: if it's the last call, this edit is out-of-range
                 * so we just abandon it.
                 */
                if (!last) {
                    nbytes = edit->start - fctx->bytes_done;
                    rc = IB_EAGAIN;
                    break;
                }
                else {
                    fctx->edits->len -= sizeof(edit_t);
                    // TODO: log an error
                    rc = IB_EBADVAL;
                    continue;
                }
            }

            /* copy data up to start-of-edit */
            start = edit->start - fctx->bytes_done;
            while (start > 0) {
                n = TSIOBufferCopy(fctx->output_buffer, fctx->reader, start, 0);
                assert (n > 0);  // FIXME - handle error
                TSIOBufferReaderConsume(fctx->reader, n);
                fctx->buffered -= n;
                fctx->bytes_done += n;
                nbytes -= n;
                start -= n;
            }

            /* Discard anything that's being deleted */
            TSIOBufferReaderConsume(fctx->reader, edit->bytes);
            nbytes -= edit->bytes;
            fctx->buffered -= edit->bytes;
            fctx->bytes_done += edit->bytes;

            /* Insert replacement string */
            n = TSIOBufferWrite(fctx->output_buffer, edit->repl, edit->repl_len);
            assert(n == edit->repl_len);  // FIXME (if this ever happens)!

            /* Record change to data size */
            fctx->offs += edit->repl_len - edit->bytes;

            /* We're done with this edit. */
            fctx->edits->len -= sizeof(edit_t);
        }
    }

    /* There's no (more) editing to do, so we can just move data to output
     * using TS native refcounted pointer ops
     */
    while (nbytes > 0) {
        n = TSIOBufferCopy(fctx->output_buffer, fctx->reader, nbytes, 0);
        assert (n > 0);  // FIXME - handle error
        TSIOBufferReaderConsume(fctx->reader, n);
        fctx->buffered -= n;
        fctx->bytes_done += n;
        nbytes -= n;
    }
    if (last) {
        /* Now we can tell downstream exactly how much data it has */
        TSVIONBytesSet(fctx->output_vio, fctx->bytes_done + fctx->offs);
    }
    TSVIOReenable(fctx->output_vio);
    return rc;
}

/**
 * Function to buffer data, and flush buffer according to buffering rules.
 *
 * @param[in] fctx - the filter data
 * @param[in] reader - the data reader
 * @param[in] nbytes - number of bytes to buffer
 * @return success or error status
 */
static ib_status_t buffer_data_chunk(ib_filter_ctx *fctx, TSIOBufferReader reader, int64_t nbytes)
{
    ib_status_t rc = IB_OK;
    int64_t copied;

    if (fctx->buffering == IOBUF_DISCARD) {
        /* discard anything we have buffered */
        if (fctx->buffered > 0) {
            TSIOBufferReaderConsume(fctx->reader, fctx->buffered);
            fctx->buffered = 0;
        }
        /* caller will mark input consumed, so do-nothing == discard */
        return rc;
    }


    if ((fctx->buffering == IOBUF_BUFFER_FLUSHALL)
         && (fctx->buffered + nbytes > fctx->buf_limit)) {
        /* flush all old data before buffering new data */
        rc = flush_data(fctx, -1, 0);
    }

    /* If buffering enabled, copy the chunk to our buffer */
    /* It's only a refcount here */
    copied = TSIOBufferCopy(fctx->buffer, reader, nbytes, 0);
    fctx->buffered += copied;

    if (fctx->buffering == IOBUF_NOBUF) {
        /* consume it all right now */
        rc = flush_data(fctx, -1, 0);
    }

    else if ((fctx->buffering == IOBUF_BUFFER_FLUSHPART)
           && (fctx->buffered > fctx->buf_limit)) {
        /* flush just enough data to bring us within the limit */
        rc = flush_data(fctx, fctx->buffered - fctx->buf_limit, 0);
    }

    /* Check buffer size vs policy in operation */
    /* If we're over the limit, flush by passing to edit  */
    return rc;
}


/**
 * Determine buffering policy from config settings
 *
 * @param[in] ibd - the filter descriptor
 * @param[in] tx - the transaction
 */
static void buffer_init(ibd_ctx *ibd, ib_tx_t *tx)
{
    ib_core_cfg_t *corecfg = NULL;
    ib_status_t rc;

    ib_filter_ctx *fctx = ibd->data;
    ib_server_direction_t dir = ibd->ibd->dir;

    if (tx == NULL) {
        fctx->buffering = IOBUF_NOBUF;
        return;
    }
    rc = ib_core_context_config(ib_context_main(tx->ib), &corecfg);
    if (rc != IB_OK) {
        TSError ("Error determining buffering configuration.");
    }
    else {
        if (dir == IBD_REQ) {
            fctx->buffering = (corecfg->buffer_req == 0)
                ? IOBUF_NOBUF :
                (corecfg->limits.request_body_buffer_limit < 0)
                    ? IOBUF_BUFFER_ALL :
                    (corecfg->limits.request_body_buffer_limit_action == IB_BUFFER_LIMIT_ACTION_FLUSH_ALL)
                        ? IOBUF_BUFFER_FLUSHALL
                        : IOBUF_BUFFER_FLUSHPART;
            fctx->buf_limit = (size_t) corecfg->limits.request_body_buffer_limit;
        }
        else {
            fctx->buffering = (corecfg->buffer_res == 0)
                ? IOBUF_NOBUF :
                (corecfg->limits.response_body_buffer_limit < 0)
                    ? IOBUF_BUFFER_ALL :
                    (corecfg->limits.response_body_buffer_limit_action == IB_BUFFER_LIMIT_ACTION_FLUSH_ALL)
                        ? IOBUF_BUFFER_FLUSHALL
                        : IOBUF_BUFFER_FLUSHPART;
            fctx->buf_limit = (size_t) corecfg->limits.response_body_buffer_limit;
        }
    }

    /* Override buffering based on flags */
    if (fctx->buffering != IOBUF_NOBUF) {
        if (dir == IBD_REQ) {
            if (ib_flags_any(tx->flags, IB_TX_FALLOW_ALL | IB_TX_FALLOW_REQUEST) ||
                (!ib_flags_all(tx->flags, IB_TX_FINSPECT_REQBODY) 
&&
                 !ib_flags_all(tx->flags, IB_TX_FINSPECT_REQHDR)) 
)
            {
                fctx->buffering = IOBUF_NOBUF;
                TSDebug("ironbee", "\tDisable request buffering");
            }
        } else if (dir == IBD_RESP) {
            if (ib_flags_any(tx->flags, IB_TX_FALLOW_ALL) ||
                (!ib_flags_all(tx->flags, IB_TX_FINSPECT_RESBODY) &&
                 !ib_flags_all(tx->flags, IB_TX_FINSPECT_RESHDR)) )
            {
                fctx->buffering = IOBUF_NOBUF;
                TSDebug("ironbee", "\tDisable response buffering");
            }
        }
    }
}
/**
 * Process data from ATS.
 *
 * Process data from one of the ATS events.
 *
 * @param[in,out] contp - the continuation
 * @param[in,out] ibd - the filter descriptor
 */
static void process_data(TSCont contp, ibd_ctx *ibd)
{
    int64_t ntodo;
    int64_t navail;
    TSIOBufferReader input_reader, output_reader;
    TSIOBufferBlock block;
    const char *buf;
    int64_t nbytes;
    ib_status_t rc;

    ib_filter_ctx *fctx = ibd->data;

    ib_txn_ctx *data = TSContDataGet(contp);
    TSVIO  input_vio = TSVConnWriteVIOGet(contp);
    TSIOBuffer in_buf = TSVIOBufferGet(input_vio);

    /* Test whether we're going into an errordoc */
    if (IB_HTTP_CODE(data->status)) {  /* We're going to an error document,
                                        * so we discard all this data
                                        */
        TSDebug("ironbee", "Status is %d, discarding", data->status);
        ibd->data->buffering = IOBUF_DISCARD;
    }

    /* Test for EOS */
    if (in_buf == NULL) {
        /* flush anything we have buffered.  This is final! */
        flush_data(fctx, -1, 1);
        return;
    }

    ntodo = TSVIONTodoGet(input_vio);

    /* Test for first time, and initialise.  */
    if (!fctx->output_buffer) {
        fctx->output_buffer = TSIOBufferCreate();
        ib_mm_register_cleanup(data->tx->mm,
                               (ib_mm_cleanup_fn_t) TSIOBufferDestroy,
                               (void*) fctx->output_buffer);
        output_reader = TSIOBufferReaderAlloc(fctx->output_buffer);
        fctx->output_vio = TSVConnWrite(TSTransformOutputVConnGet(contp), contp, output_reader, INT64_MAX);

        fctx->buffer = TSIOBufferCreate();
        ib_mm_register_cleanup(data->tx->mm,
                               (ib_mm_cleanup_fn_t) TSIOBufferDestroy,
                               (void*) fctx->buffer);
        fctx->reader = TSIOBufferReaderAlloc(fctx->buffer);

        /* Get the buffering config */
        if (!IB_HTTP_CODE(data->status)) {
            buffer_init(ibd, data->tx);
        }

/* Do we still have to delay feeding the first data to Ironbee
 * to keep the IB events in their proper order?
 *
 * Appears maybe not, so let's do nothing until it shows signs of breakage.
 */
#if BUFFER_FIRST
        /* First time through we can only buffer data until headers are sent. */
        fctx->first_time = 1;
        input_reader = TSVIOReaderGet(input_vio);
        fctx->buffered = TSIOBufferCopy(fctx->buffer, TSVIOReaderGet(input_vio),
                                        ntodo, 0);
        TSIOBufferReaderConsume(input_reader, fctx->buffered);

        /* Do we need to request more input or just continue? */
        TSVIONDoneSet(input_vio, fctx->buffu`ered + fctx->bytes_done);

        TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_READY, input_vio);
        return;
#endif
    }

    /* second time through we have to feed already-buffered data through
     * ironbee while retaining it in buffer.  Regardless of what else happens.
     */
#if BUFFER_FIRST
    if (fctx->first_time) {
        fctx->first_time = 0;
        for (block = TSIOBufferStart(fctx->buffer);
	     block != NULL;
             block = TSIOBufferBlockNext(block)) {

            //nbytes = TSIOBufferBlockDataSizeGet(block);
            /* FIXME - do this without a reader ? */
            buf = TSIOBufferBlockReadStart(block, fctx->reader, &nbytes);
            //rc = examine_data_chunk(ibd->ibd, data->tx, buf, nbytes);
            rc = (*ibd->ibd->ib_notify_body)(data->tx->ib, data->tx, buf, nbytes);
            if (rc != IB_OK) {
                // FIXME ???
            }
        }
    }
#endif

    /* Test for EOS */
    if (ntodo == 0) {
        TSDebug("[ironbee]", "ntodo zero before consuming data");
        /* Call back the input VIO continuation to let it know that we
         * have completed the write operation.
         */
        TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_COMPLETE, input_vio);
        return;
    }

    /* OK, there's some input awaiting our attention */
    input_reader = TSVIOReaderGet(input_vio);
    while (navail = TSIOBufferReaderAvail(input_reader), navail > 0) {
        block = TSIOBufferReaderStart(input_reader);
        buf = TSIOBufferBlockReadStart(block, input_reader, &nbytes);
        rc = (*ibd->ibd->ib_notify_body)(data->tx->ib, data->tx, buf, nbytes);
        if (rc != IB_OK) {
            // FIXME ???
        }
        rc = buffer_data_chunk(fctx, input_reader, nbytes);
        if (rc != IB_OK) {
            // FIXME ???
        }
        TSIOBufferReaderConsume(input_reader, nbytes);
        TSVIONDoneSet(input_vio, TSVIONDoneGet(input_vio) + nbytes);
    }

    ntodo = TSVIONTodoGet(input_vio);
    if (ntodo == 0) {
        TSDebug("[ironbee]", "ntodo zero after consuming data");
        /* Call back the input VIO continuation to let it know that we
         * have completed the write operation.
         */
        TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_COMPLETE, input_vio);
    }
    else {
        /* Call back the input VIO continuation to let it know that we
         * are ready for more data.
         */
        TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_READY, input_vio);
    }
}

/**
 * Handle a data event from ATS.
 *
 * Handles all data events from ATS, uses process_data to handle the data
 * itself.
 *
 * @param[in,out] contp Pointer to the continuation
 * @param[in,out] event Event from ATS
 * @param[in,out] ibd unknown
 *
 * @returns status
 */
static int data_event(TSCont contp, TSEvent event, ibd_ctx *ibd)
{
    /* Check to see if the transformation has been closed by a call to
     * TSVConnClose.
     */
    ib_txn_ctx *data;
    TSDebug("ironbee", "Entering out_data for %s", ibd->ibd->dir_label);

    if (TSVConnClosedGet(contp)) {
        TSDebug("ironbee", "\tVConn is closed");
        return 0;
    }

    switch (event) {
        case TS_EVENT_ERROR:
        {
            TSVIO input_vio;

            TSDebug("ironbee", "\tEvent is TS_EVENT_ERROR");
            /* Get the write VIO for the write operation that was
             * performed on ourself. This VIO contains the continuation of
             * our parent transformation. This is the input VIO.
             */
            input_vio = TSVConnWriteVIOGet(contp);

            /* Call back the write VIO continuation to let it know that we
             * have completed the write operation.
             */
            TSContCall(TSVIOContGet(input_vio), TS_EVENT_ERROR, input_vio);
        }
        break;
        case TS_EVENT_VCONN_WRITE_COMPLETE:
            TSDebug("ironbee", "\tEvent is TS_EVENT_VCONN_WRITE_COMPLETE");
            /* When our output connection says that it has finished
             * reading all the data we've written to it then we should
             * shutdown the write portion of its connection to
             * indicate that we don't want to hear about it anymore.
             */
            TSVConnShutdown(TSTransformOutputVConnGet(contp), 0, 1);

            data = TSContDataGet(contp);
            TSDebug("ironbee", "data_event: calling ib_state_notify_%s_finished()", ((ibd->ibd->dir == IBD_REQ)?"request":"response"));
            (*ibd->ibd->ib_notify_end)(data->tx->ib, data->tx);
            if ( (ibd->ibd->ib_notify_post != NULL) &&
                 (!ib_flags_all(data->tx->flags, IB_TX_FPOSTPROCESS)) )
            {
                (*ibd->ibd->ib_notify_post)(data->tx->ib, data->tx);
            }
            if ( (ibd->ibd->ib_notify_log != NULL) &&
                 (!ib_flags_all(data->tx->flags, IB_TX_FLOGGING)) )
            {
                (*ibd->ibd->ib_notify_log)(data->tx->ib, data->tx);
            }
            break;
        case TS_EVENT_VCONN_WRITE_READY:
            TSDebug("ironbee", "\tEvent is TS_EVENT_VCONN_WRITE_READY");
            /* fall through */
        default:
            TSDebug("ironbee", "\t(event is %d)", event);
            /* If we get a WRITE_READY event or any other type of
             * event (sent, perhaps, because we were re-enabled) then
             * we'll attempt to transform more data.
             */
            process_data(contp, ibd);
            break;
    }

    return 0;
}

/**
 * Handle a outgoing data event from ATS.
 *
 * Handles all outgoing data events from ATS, uses process_data to handle the
 * data itself.
 *
 * @param[in,out] contp Pointer to the continuation
 * @param[in,out] event Event from ATS
 * @param[in,out] edata Event data
 *
 * @returns status
 */
int out_data_event(TSCont contp, TSEvent event, void *edata)
{
    ibd_ctx direction;
    ib_txn_ctx *data = TSContDataGet(contp);

    if ( (data == NULL) || (data->tx == NULL) ) {
        TSDebug("ironbee", "\tout_data_event: tx == NULL");
        return 0;
    }
    direction.ibd = &ib_direction_server_resp;
    direction.data = &data->out;
    return data_event(contp, event, &direction);
}

/**
 * Handle a incoming data event from ATS.
 *
 * Handles all incoming data events from ATS, uses process_data to handle the
 * data itself.
 *
 * @param[in,out] contp Pointer to the continuation
 * @param[in,out] event Event from ATS
 * @param[in,out] edata Event data
 *
 * @returns status
 */
int in_data_event(TSCont contp, TSEvent event, void *edata)
{
    ibd_ctx direction;
    ib_txn_ctx *data;
    TSDebug("ironbee-in-data", "in_data_event: contp=%p", contp);
    data = TSContDataGet(contp);

    if ( (data == NULL) || (data->tx == NULL) ) {
        TSDebug("ironbee", "\tin_data_event: tx == NULL");
        return 0;
    }
    direction.ibd = &ib_direction_client_req;
    direction.data = &data->in;
    return data_event(contp, event, &direction);
}
