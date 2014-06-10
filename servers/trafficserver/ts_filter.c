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
 * Flush buffered data in ATS.
 *
 * This doesn't actively flush data, but indicates the amount of data
 * available NOW to be consumed on the output buf.  It is up to TS
 * (or to another filter continuation module if present) whether
 * actually to consume the data at this point.
 *
 * Note: http://mail-archives.apache.org/mod_mbox/trafficserver-dev/201110.mbox/%3CCAFKFyq5gWqzi=LuxdVMTJzSbJcuZDR8hYV89khUssPxnnxbk_Q@mail.gmail.com%3E
 *
 * @param[in,out] contp Pointer to the continuation
 * @param[in,out] ibd filter continuation ctx
 */
static void flush_data(TSCont contp, ibd_ctx *ibd)
{
    TSVConn output_conn;
    if (ibd->data->buffering != IOBUF_DISCARD
        && ibd->data->buffering != IOBUF_NOBUF) {
        if (ibd->data->output_vio == NULL) {
            /* Get the output (downstream) vconnection where we'll write data to. */
            output_conn = TSTransformOutputVConnGet(contp);
            ibd->data->output_vio = TSVConnWrite(output_conn, contp, ibd->data->output_reader, TSIOBufferReaderAvail(ibd->data->output_reader));
        }
        TSVIONBytesSet(ibd->data->output_vio, ibd->data->buffered);
        TSVIOReenable(ibd->data->output_vio);
    }
}
/**
 * Process data from ATS.
 *
 * Process data from one of the ATS events.
 *
 * @param[in,out] contp Pointer to the continuation
 * @param[in,out] ibd unknown
 */
static void process_data(TSCont contp, ibd_ctx *ibd)
{
    TSVConn output_conn;
    TSIOBuffer buf_test;
    TSVIO input_vio;
    ib_txn_ctx *data;
    int64_t towrite;
    int64_t avail;
    int first_time = 0;
    char *bufp = NULL;

    TSDebug("ironbee", "Entering process_data()");

    /* Get the write VIO for the write operation that was performed on
     * ourself. This VIO contains the buffer that we are to read from
     * as well as the continuation we are to call when the buffer is
     * empty. This is the input VIO (the write VIO for the upstream
     * vconnection).
     */
    input_vio = TSVConnWriteVIOGet(contp);

    data = TSContDataGet(contp);
    if (IB_HTTP_CODE(data->status)) {  /* We're going to an error document,
                                        * so we discard all this data
                                        */
        TSDebug("ironbee", "Status is %d, discarding", data->status);
        ibd->data->buffering = IOBUF_DISCARD;
    }

    if (!ibd->data->output_buffer) {
        first_time = 1;

        ibd->data->output_buffer = TSIOBufferCreate();
        ib_mm_register_cleanup(data->tx->mm,
                               (ib_mm_cleanup_fn_t) TSIOBufferDestroy,
                               (void*) ibd->data->output_buffer);
        ibd->data->output_reader = TSIOBufferReaderAlloc(ibd->data->output_buffer);

        /* Is buffering configured? */
        if (!IB_HTTP_CODE(data->status)) {
            ib_core_cfg_t *corecfg = NULL;
            ib_status_t rc;

            if (data->tx == NULL) {
                ibd->data->buffering = IOBUF_NOBUF;
            }
            else {
                rc = ib_core_context_config(ib_context_main(data->tx->ib),
                                            &corecfg);
                if (rc != IB_OK) {
                    TSError ("Error determining buffering configuration.");
                }
                else {
                    if (ibd->ibd->dir == IBD_REQ) {
                        ibd->data->buffering = (corecfg->buffer_req == 0)
                            ? IOBUF_NOBUF :
                            (corecfg->limits.request_body_buffer_limit < 0)
                                ? IOBUF_BUFFER_ALL :
                                (corecfg->limits.request_body_buffer_limit_action == IB_BUFFER_LIMIT_ACTION_FLUSH_ALL)
                                    ? IOBUF_BUFFER_FLUSHALL
                                    : IOBUF_BUFFER_FLUSHPART;
                        ibd->data->buf_limit = (size_t) corecfg->limits.request_body_buffer_limit;
                    }
                    else {
                        ibd->data->buffering = (corecfg->buffer_res == 0)
                            ? IOBUF_NOBUF :
                            (corecfg->limits.response_body_buffer_limit < 0)
                                ? IOBUF_BUFFER_ALL :
                                (corecfg->limits.response_body_buffer_limit_action == IB_BUFFER_LIMIT_ACTION_FLUSH_ALL)
                                    ? IOBUF_BUFFER_FLUSHALL
                                    : IOBUF_BUFFER_FLUSHPART;
                        ibd->data->buf_limit = (size_t) corecfg->limits.response_body_buffer_limit;
                    }
                }
            }

            /* Override buffering based on flags */
            if (ibd->data->buffering != IOBUF_NOBUF) {
                if (ibd->ibd->dir == IBD_REQ) {
                    if (ib_flags_any(data->tx->flags, IB_TX_FALLOW_ALL | IB_TX_FALLOW_REQUEST) ||
                        (!ib_flags_all(data->tx->flags, IB_TX_FINSPECT_REQBODY) &&
                         !ib_flags_all(data->tx->flags, IB_TX_FINSPECT_REQHDR)) )
                    {
                        ibd->data->buffering = IOBUF_NOBUF;
                        TSDebug("ironbee", "\tDisable request buffering");
                    }
                } else if (ibd->ibd->dir == IBD_RESP) {
                    if (ib_flags_any(data->tx->flags, IB_TX_FALLOW_ALL) ||
                        (!ib_flags_all(data->tx->flags, IB_TX_FINSPECT_RESBODY) &&
                         !ib_flags_all(data->tx->flags, IB_TX_FINSPECT_RESHDR)) )
                    {
                        ibd->data->buffering = IOBUF_NOBUF;
                        TSDebug("ironbee", "\tDisable response buffering");
                    }
                }
            }
        }

        if (ibd->data->buffering == IOBUF_NOBUF) {
            int64_t n = TSVIONBytesGet(input_vio);
            TSDebug("ironbee", "\tBuffering: off");
            TSDebug("ironbee", "\tWriting %"PRId64" bytes on VConn", n);
            /* Get the output (downstream) vconnection where we'll write data to. */
            output_conn = TSTransformOutputVConnGet(contp);
            ibd->data->output_vio = TSVConnWrite(output_conn, contp, ibd->data->output_reader, n);
        } else {
            TSDebug("ironbee", "\tBuffering: on, flush %d", (int)ibd->data->buffering);
        }
    }
    if (ibd->data->buf) {
        /* this is the second call to us, and we have data buffered.
         * Feed buffered data to ironbee
         */
        if (ibd->data->buflen != 0) {
            const char *buf = ibd->data->buf;
            size_t buf_length = ibd->data->buflen;
            TSDebug("ironbee",
                    "process_data: calling ib_state_notify_%s_body() %s:%d",
                    ibd->ibd->dir_label, __FILE__, __LINE__);
            (*ibd->ibd->ib_notify_body)(data->tx->ib, data->tx, buf, buf_length);
        }
        //TSfree(ibd->data->buf);
        ibd->data->buf = NULL;
        ibd->data->buflen = 0;
        if (IB_HTTP_CODE(data->status)) {  /* We're going to an error document,
                                            * so we discard all this data
                                            */
            TSDebug("ironbee", "Status is %d, discarding", data->status);
            ibd->data->buffering = IOBUF_DISCARD;
        }
    }

    /* test for input data */
    buf_test = TSVIOBufferGet(input_vio);

    if (!buf_test) {
        TSDebug("ironbee", "No more data, finishing");
        if (ibd->data->buffering != IOBUF_DISCARD) {
            if (ibd->data->output_vio == NULL) {
                /* Get the output (downstream) vconnection where we'll write data to. */
                output_conn = TSTransformOutputVConnGet(contp);
                ibd->data->output_vio = TSVConnWrite(output_conn, contp, ibd->data->output_reader, TSIOBufferReaderAvail(ibd->data->output_reader));
            }
            else {
                TSVIONBytesSet(ibd->data->output_vio, TSVIONDoneGet(input_vio));
            }
            TSVIOReenable(ibd->data->output_vio);
        }
        //ibd->data->output_buffer = NULL;
        //ibd->data->output_reader = NULL;
        //ibd->data->output_vio = NULL;
        return;
    }

    /* Determine how much data we have left to read. For this null
     * transform plugin this is also the amount of data we have left
     * to write to the output connection.
     */
    towrite = TSVIONTodoGet(input_vio);
    TSDebug("ironbee", "\ttoWrite is %" PRId64 "", towrite);

    if (towrite > 0) {
        /* The amount of data left to read needs to be truncated by
         * the amount of data actually in the read buffer.
         */

        avail = TSIOBufferReaderAvail(TSVIOReaderGet(input_vio));
        TSDebug("ironbee", "\tavail is %" PRId64 "", avail);
        if (towrite > avail) {
            towrite = avail;
        }

        if (towrite > 0) {
            int btowrite = towrite;
            /* Copy the data from the read buffer to the output buffer. */
            switch (ibd->data->buffering) {
              case IOBUF_NOBUF:
                TSIOBufferCopy(TSVIOBufferGet(ibd->data->output_vio), TSVIOReaderGet(input_vio), towrite, 0);
                break;
              case IOBUF_BUFFER_ALL:
                ibd->data->buffered += towrite;
                TSIOBufferCopy(ibd->data->output_buffer, TSVIOReaderGet(input_vio), towrite, 0);
                break;
              case IOBUF_BUFFER_FLUSHALL:
                /* Append data to buffer, flush it all if over limit */
                TSIOBufferCopy(ibd->data->output_buffer, TSVIOReaderGet(input_vio), towrite, 0);
                ibd->data->buffered += towrite;
                if (ibd->data->buffered >= ibd->data->buf_limit) {
                    flush_data(contp, ibd);
                }
                break;
              case IOBUF_BUFFER_FLUSHPART:
                /* If data will bring buffer over the limit, flush existing
                 * data before appending new data, so new data still buffered
                 */
                if (ibd->data->buffered + towrite >= ibd->data->buf_limit) {
                    flush_data(contp, ibd);
                }
                TSIOBufferCopy(ibd->data->output_buffer, TSVIOReaderGet(input_vio), towrite, 0);
                ibd->data->buffered += towrite;
                break;
              case IOBUF_DISCARD:
                break;
              default: // bug
                assert(0==1);
            }

            /* first time through, we have to buffer the data until
             * after the headers have been sent.  Ugh!
             * At this point, we know the size to alloc.
             */
            if (first_time) {
                ibd->data->buflen = towrite;
                bufp = ibd->data->buf = ib_mm_alloc(data->tx->mm,
                                                    ibd->data->buflen);
            }

            /* feed the data to ironbee, and consume them */
            while (btowrite > 0) {
                //ib_conndata_t icdata;
                int64_t ilength;
                TSIOBufferReader input_reader = TSVIOReaderGet(input_vio);
                TSIOBufferBlock blkp = TSIOBufferReaderStart(input_reader);
                const char *ibuf = TSIOBufferBlockReadStart(blkp, input_reader, &ilength);

                /* feed it to ironbee or to buffer */
                if (first_time) {
                    memcpy(bufp, ibuf, ilength);
                    bufp += ilength;
                }
                else {
                    if (ibd->data->buflen > 0) {
                        const char *buf = ibd->data->buf;
                        size_t buf_length = ibd->data->buflen;
                        TSDebug("ironbee",
                                "process_data: calling ib_state_notify_%s_body() "
                                "%s:%d",
                                ((ibd->ibd->dir == IBD_REQ)?"request":"response"),
                                __FILE__, __LINE__);
                        (*ibd->ibd->ib_notify_body)(data->tx->ib, data->tx,
                                                    (ilength!=0) ? buf : NULL, buf_length);
                    }
                    if (IB_HTTP_CODE(data->status)) {  /* We're going to an error document,
                                                        * so we discard all this data
                                                        */
                        ibd->data->buffering = IOBUF_DISCARD;
                    }
                }

                /* and mark it as all consumed */
                btowrite -= ilength;
                TSIOBufferReaderConsume(input_reader, ilength);
                TSVIONDoneSet(input_vio, TSVIONDoneGet(input_vio) + ilength);
            }
        }
    }

    /* Now we check the input VIO to see if there is data left to
     * read.
     */
    if (TSVIONTodoGet(input_vio) > 0) {
        if (towrite > 0) {
            /* If there is data left to read, then we re-enable the output
             * connection by re-enabling the output VIO. This will wake up
             * the output connection and allow it to consume data from the
             * output buffer.
             */
            if (ibd->data->buffering == IOBUF_NOBUF) {
                TSVIOReenable(ibd->data->output_vio);
            }

            /* Call back the input VIO continuation to let it know that we
             * are ready for more data.
             */
            TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_READY, input_vio);
        }
    }
    else {
        /* If there is no data left to read, then we modify the output
         * VIO to reflect how much data the output connection should
         * expect. This allows the output connection to know when it
         * is done reading. We then re-enable the output connection so
         * that it can consume the data we just gave it.
         */
        if (ibd->data->buffering == IOBUF_NOBUF) {
            TSVIONBytesSet(ibd->data->output_vio, TSVIONDoneGet(input_vio));
            TSVIOReenable(ibd->data->output_vio);
        }
        else if (ibd->data->buffering == IOBUF_DISCARD) {
            TSVIONBytesSet(ibd->data->output_vio, 0);
            TSVIOReenable(ibd->data->output_vio);
        }
        else {
            flush_data(contp, ibd);
        }

        /* Call back the input VIO continuation to let it know that we
         * have completed the write operation.
         */
        TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_COMPLETE, input_vio);
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
        TSContDataSet(contp, NULL);
        TSContDestroy(contp);    /* from null-transform, ???? */

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
    ib_txn_ctx *data = TSContDataGet(contp);

    if ( (data == NULL) || (data->tx == NULL) ) {
        TSDebug("ironbee", "\tout_data_event: tx == NULL");
        return 0;
    }
    /* Protect against https://issues.apache.org/jira/browse/TS-922 */
    else if (data->out.buflen == (unsigned int)-1) {
        TSDebug("ironbee", "\tout_data_event: buflen == -1");
        return 0;
    }
    ibd_ctx direction;
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
    ib_txn_ctx *data;
    TSDebug("ironbee-in-data", "in_data_event: contp=%p", contp);
    data = TSContDataGet(contp);

    if ( (data == NULL) || (data->tx == NULL) ) {
        TSDebug("ironbee", "\tin_data_event: tx == NULL");
        return 0;
    }
    else if (data->out.buflen == (unsigned int)-1) {
        TSDebug("ironbee", "\tin_data_event: buflen == -1");
        return 0;
    }
    ibd_ctx direction;
    direction.ibd = &ib_direction_client_req;
    direction.data = &data->in;
    return data_event(contp, event, &direction);
}
