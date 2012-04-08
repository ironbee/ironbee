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

/**
 * @file
 * @brief IronBee - Apache Traffic Server Plugin
 *
 * @author Nick Kew <nkew@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <ts/ts.h>

#include <sys/socket.h>
#include <netdb.h>

// This gets the PRI*64 types
# define __STDC_FORMAT_MACROS 1
# include <inttypes.h>

#include <ironbee/engine.h>
#include <ironbee/config.h>
#include <ironbee/module.h> /* Only needed while config is in here. */
#include <ironbee/provider.h>
#include <ironbee/server.h>
#include <ironbee/state_notify.h>
#include <ironbee/util.h>
#include <ironbee/debug.h>


static void addr2str(const struct sockaddr *addr, char *str, int *port);

#define ADDRSIZE 48 /* what's the longest IPV6 addr ? */

ib_engine_t DLL_LOCAL *ironbee = NULL;
TSTextLogObject ironbee_log;
#define DEFAULT_LOG "ts-ironbee"

typedef struct {
    ib_conn_t *iconn;

    /* store the IPs here so we can clean them up and not leak memory */
    char remote_ip[ADDRSIZE];
    char local_ip[ADDRSIZE];
    TSHttpTxn txnp; /* hack: conn data requires txnp to access */
} ib_ssn_ctx;

typedef struct {
    /* data filtering stuff */
    TSVIO output_vio;
    TSIOBuffer output_buffer;
    TSIOBufferReader output_reader;
    char *buf;
    unsigned int buflen;
} ib_filter_ctx;

#define IBD_REQ IB_SERVER_REQUEST
#define IBD_RESP IB_SERVER_RESPONSE
#define HDRS_IN IB_SERVER_REQUEST
#define HDRS_OUT IB_SERVER_RESPONSE
#define START_RESPONSE 0x04
#define DATA 0

typedef struct hdr_do {
    ib_server_header_action_t action;
    ib_server_direction_t dir;
    char *hdr;
    char *value;
    struct hdr_do *next;
} hdr_do;

typedef struct hdr_list {
    char *hdr;
    char *value;
    struct hdr_list *next;
} hdr_list;

typedef struct {
    const char *ctype;
    const char *redirect;
    const char *authn;
    const char *body;
} error_resp_t;

typedef struct {
    ib_ssn_ctx *ssn;
    TSHttpTxn txnp;
    ib_filter_ctx in;
    ib_filter_ctx out;
    int state;
    int status;
    hdr_do *hdr_actions;
    hdr_list *err_hdrs;
    char *err_body;    /* this one can't be const */
} ib_txn_ctx;

/* mod_ironbee uses ib_state_notify_conn_data_[in|out]
 * for both headers and data
 */
typedef struct {
    ib_server_direction_t dir;
    const char *word;
    TSReturnCode (*hdr_get)(TSHttpTxn, TSMBuffer *, TSMLoc *);
    ib_status_t (*ib_notify)(ib_engine_t *, ib_conndata_t *);
} ironbee_direction_t;

static ironbee_direction_t ironbee_direction_req = {
    IBD_REQ,
    "request",
    TSHttpTxnClientReqGet,
    ib_state_notify_conn_data_in
};
static ironbee_direction_t ironbee_direction_resp = {
    IBD_RESP,
    "response",
    TSHttpTxnClientRespGet,
    ib_state_notify_conn_data_out
};

typedef struct {
    ironbee_direction_t *ibd;
    ib_filter_ctx *data;
} ibd_ctx;

/**
 * Callback functions for Ironbee to signal to us
 */
static ib_server_header_action_t ib_header_callback
                                          (void *x, ib_server_direction_t dir,
                                           ib_server_header_action_t action,
                                           const char *hdr, const char *value)
{
    ib_txn_ctx *ctx = x;
    hdr_do *header;
    /* Logic for whether we're in time for the requested action */
    /* Output headers can change any time before they're sent */
    /* Input headers can only be touched during their read */

    if (ctx->state & HDRS_OUT ||
        (ctx->state & HDRS_IN && dir == IB_SERVER_REQUEST))
        return IB_HDR_ERROR;  /* too late for requested op */

    header = TSmalloc(sizeof(hdr_do));
    header->next = ctx->hdr_actions;
    ctx->hdr_actions = header;
    header->dir = dir;
    /* FIXME: deferring merge support - implementing append instead */
    header->action = action = action == IB_HDR_MERGE ? IB_HDR_APPEND : action;
    header->hdr = TSstrdup(hdr);
    header->value = TSstrdup(value);

    return action;
}

/**
 * Handler function to generate an error response
 */
static void error_response(TSHttpTxn txnp, ib_txn_ctx *txndata)
{
    const char *reason = TSHttpHdrReasonLookup(txndata->status);
    TSMBuffer bufp;
    TSMLoc hdr_loc, field_loc;
    hdr_list *hdrs;
    TSReturnCode rv;

    if (TSHttpTxnClientRespGet(txnp, &bufp, &hdr_loc) != TS_SUCCESS) {
        TSError("couldn't retrieve client response header\n");
        TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
        return;
    }
    TSHttpHdrStatusSet(bufp, hdr_loc, txndata->status);
    TSHttpHdrReasonSet(bufp, hdr_loc, reason, strlen(reason));

    /* FIXME - generalize this lot */
    while (hdrs = txndata->err_hdrs, hdrs != 0) {
        txndata->err_hdrs = hdrs->next;
        rv = TSMimeHdrFieldCreate(bufp, hdr_loc, &field_loc);
        if (rv != TS_SUCCESS) {
            // TODO
        }
        rv = TSMimeHdrFieldNameSet(bufp, hdr_loc, field_loc,
                                   hdrs->hdr, strlen(hdrs->hdr));
        if (rv != TS_SUCCESS) {
            // TODO
        }
        TSMimeHdrFieldValueStringInsert(bufp, hdr_loc, field_loc, -1,
                                        hdrs->value, strlen(hdrs->value));
        TSMimeHdrFieldAppend(bufp, hdr_loc, field_loc);
        TSHandleMLocRelease(bufp, hdr_loc, field_loc);
        TSfree(hdrs->hdr);
        TSfree(hdrs->value);
        TSfree(hdrs);
        hdrs = txndata->err_hdrs;
    }
    if (txndata->err_body) {
        /* this will free the body, so copy it first! */
        TSHttpTxnErrorBodySet(txnp, txndata->err_body, strlen(txndata->err_body), NULL);
    }
    TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
}

static ib_status_t ib_error_callback(void *x, int status)
{
    ib_txn_ctx *ctx = x;
    if (status >= 200 && status < 600) {
        /* We can't return an error after the response has started */
        if (ctx->state & START_RESPONSE)
            return IB_DECLINED;
        /* ironbee wants to return an HTTP status.  We'll oblige */
        /* FIXME: would the semantics work for 1xx?  Do we care? */
        ctx->status = status;
        return IB_OK;
    }
    return IB_ENOTIMPL;
}
static ib_status_t ib_errhdr_callback(void *x, const char *hdr, const char *val)
{
    ib_txn_ctx *ctx = x;
    hdr_list *hdrs;
    /* We can't return an error after the response has started */
    if (ctx->state & START_RESPONSE)
        return IB_DECLINED;
    if (!hdr || !val)
        return IB_EINVAL;
    hdrs = TSmalloc(sizeof(hdr_list));
    hdrs->hdr = TSstrdup(hdr);
    hdrs->value = TSstrdup(val);
    hdrs->next = ctx->err_hdrs;
    ctx->err_hdrs = hdrs;
    return IB_OK;
}
static ib_status_t ib_errdata_callback(void *x, const char *data)
{
    ib_txn_ctx *ctx = x;
    /* We can't return an error after the response has started */
    if (ctx->state & START_RESPONSE)
        return IB_DECLINED;
    if (!data)
        return IB_EINVAL;
    ctx->err_body = TSstrdup(data);
    return IB_OK;
}

/* Plugin Structure */
ib_server_t DLL_LOCAL ibplugin = {
    IB_SERVER_HEADER_DEFAULTS,
    "ts-ironbee",
    ib_header_callback,
    ib_error_callback,
    ib_errhdr_callback,
    ib_errdata_callback,
};

/**
 * @internal
 * Handle transaction context destroy.
 *
 * Handles TS_EVENT_HTTP_TXN_CLOSE (transaction close) close event from the
 * ATS.
 *
 * @param[in,out] data Transaction context data
 */
static void ib_txn_ctx_destroy(ib_txn_ctx * data)
{
    if (data) {
        hdr_do *x;
        if (data->out.output_buffer) {
            TSIOBufferDestroy(data->out.output_buffer);
            data->out.output_buffer = NULL;
        }
        if (data->in.output_buffer) {
            TSIOBufferDestroy(data->in.output_buffer);
            data->in.output_buffer = NULL;
        }
        while (x=data->hdr_actions, x != NULL) {
            data->hdr_actions = x->next;
            TSfree(x->hdr);
            TSfree(x->value);
            TSfree(x);
        }
        TSfree(data);
    }
}

/**
 * @internal
 * Handle session context destroy.
 *
 * Handles TS_EVENT_HTTP_SSN_CLOSE (session close) close event from the
 * ATS.
 *
 * @param[in,out] data session context data
 */
static void ib_ssn_ctx_destroy(ib_ssn_ctx * data)
{
    if (data) {
        if (data->iconn)
            ib_state_notify_conn_closed(ironbee, data->iconn);
        TSfree(data);
    }
}

/**
 * @internal
 * Process data from ATS.
 *
 * Process data from one of the ATS events.
 *
 * @param[in,out] contp Pointer to the continuation
 * @param[in,out] ibd unknown
 */
static void process_data(TSCont contp, ibd_ctx* ibd)
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
    /* Get the output (downstream) vconnection where we'll write data to. */

    output_conn = TSTransformOutputVConnGet(contp);

    /* Get the write VIO for the write operation that was performed on
     * ourself. This VIO contains the buffer that we are to read from
     * as well as the continuation we are to call when the buffer is
     * empty. This is the input VIO (the write VIO for the upstream
     * vconnection).
     */
    input_vio = TSVConnWriteVIOGet(contp);

    data = TSContDataGet(contp);
    if (!ibd->data->output_buffer) {
        first_time = 1;

        ibd->data->output_buffer = TSIOBufferCreate();
        ibd->data->output_reader = TSIOBufferReaderAlloc(ibd->data->output_buffer);
        TSDebug("ironbee", "\tWriting %"PRId64" bytes on VConn", TSVIONBytesGet(input_vio));
        ibd->data->output_vio = TSVConnWrite(output_conn, contp, ibd->data->output_reader, INT64_MAX);
    }
    if (ibd->data->buf) {
        /* this is the second call to us, and we have data buffered.
         * Feed buffered data to ironbee
         */
        ib_conndata_t icdata;
        icdata.conn = data->ssn->iconn;
        icdata.dlen = ibd->data->buflen;
        icdata.data = (uint8_t *)ibd->data->buf;
        (*ibd->ibd->ib_notify)(ironbee, &icdata);
        TSfree(ibd->data->buf);
        ibd->data->buf = NULL;
        ibd->data->buflen = 0;
    }

    /* test for input data */
    buf_test = TSVIOBufferGet(input_vio);

    if (!buf_test) {
        TSDebug("ironbee", "No more data, finishing");
        TSVIONBytesSet(ibd->data->output_vio, TSVIONDoneGet(input_vio));
        TSVIOReenable(ibd->data->output_vio);
        /* FIXME - is this right here - can conn data be kept across reqs? */
        ibd->data->output_buffer = NULL;
        ibd->data->output_reader = NULL;
        ibd->data->output_vio = NULL;
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
            TSIOBufferCopy(TSVIOBufferGet(ibd->data->output_vio), TSVIOReaderGet(input_vio), towrite, 0);

            /* first time through, we have to buffer the data until
             * after the headers have been sent.  Ugh!
             * At this point, we know the size to alloc.
             */
            if (first_time) {
                bufp = ibd->data->buf = TSmalloc(towrite);
                ibd->data->buflen = towrite;
            }

            /* feed the data to ironbee, and consume them */
            while (btowrite > 0) {
                ib_conndata_t icdata;
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
                    icdata.conn = data->ssn->iconn;
                    icdata.dlen = ilength;
                    icdata.data = (uint8_t *)ibuf;
                    (*ibd->ibd->ib_notify)(ironbee, &icdata);
                }
                //"response", TSHttpTxnClientRespGet, ib_state_notify_conn_data_out
                //      ib_state_notify_conn_data_out(ironbee, &icdata);

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
            TSVIOReenable(ibd->data->output_vio);

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
        TSVIONBytesSet(ibd->data->output_vio, TSVIONDoneGet(input_vio));
        TSVIOReenable(ibd->data->output_vio);

        /* Call back the input VIO continuation to let it know that we
         * have completed the write operation.
         */
        TSContCall(TSVIOContGet(input_vio), TS_EVENT_VCONN_WRITE_COMPLETE, input_vio);
    }
}

/**
 * @internal
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
    TSDebug("ironbee", "Entering out_data for %s\n", ibd->ibd->word);

    if (TSVConnClosedGet(contp)) {
        TSDebug("ironbee", "\tVConn is closed");
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
 * @internal
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
static int out_data_event(TSCont contp, TSEvent event, void *edata)
{
    ib_txn_ctx *data = TSContDataGet(contp);
    if (data->out.buflen == (unsigned int)-1) {
    TSDebug("ironbee", "\tout_data_event: buflen = -1");
    ib_log_debug3(ironbee,
                     "ironbee/out_data_event(): buflen = -1");
    return 0;
    }
    ibd_ctx direction;
    direction.ibd = &ironbee_direction_resp;
    direction.data = &data->out;
    return data_event(contp, event, &direction);
}

/**
 * @internal
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
static int in_data_event(TSCont contp, TSEvent event, void *edata)
{
    ib_txn_ctx *data = TSContDataGet(contp);
    if (data->out.buflen == (unsigned int)-1) {
    TSDebug("ironbee", "\tin_data_event: buflen = -1");
    ib_log_debug3(ironbee,
                 "ironbee/in_data_event(): buflen = -1");
    return 0;
    }
    ibd_ctx direction;
    direction.ibd = &ironbee_direction_req;
    direction.data = &data->in;
    return data_event(contp, event, &direction);
}

/**
 * @internal
 * Process an HTTP header from ATS.
 *
 * Handles an HTTP header, called from ironbee_plugin.
 *
 * @param[in,out] data Transaction context
 * @param[in,out] txnp ATS transaction pointer
 * @param[in,out] ibd unknown
 */
static int process_hdr(ib_txn_ctx *data, TSHttpTxn txnp,
                       ironbee_direction_t *ibd)
{
    ib_conndata_t icdata;
    int rv;
    TSMBuffer bufp;
    TSMLoc hdr_loc;
    TSIOBuffer iobufp;
    TSIOBufferReader readerp;
    TSIOBufferBlock blockp;
    int64_t len;
    hdr_do *hdr;

    TSDebug("ironbee", "process %s headers\n", ibd->word);

    icdata.conn = data->ssn->iconn;

    /* Use alternative simpler path to get the un-doctored request
     * if we have the fix for TS-998
     *
     * This check will want expanding/fine-tuning according to what released
     * versions incorporate the fix
     */
#if (TS_VERSION_MAJOR >= 3) &&  ( \
    ((TS_VERSION_MINOR >= 1) && (TS_VERSION_MICRO >= 3)) ||  \
    (TS_VERSION_MINOR >= 2))
    if (ibd->dir == IBD_RESP) {
        char *head_buf;

        /* before the HTTP headers comes the request line / response code */
        rv = (*ibd->hdr_get)(txnp, &bufp, &hdr_loc);
        if (rv) {
            TSError ("couldn't retrieve %s header: %d\n", ibd->word, rv);
            return 0;
        }

        /* Get the data into an IOBuffer so we can access them! */
        //iobufp = TSIOBufferSizedCreate(...);
        iobufp = TSIOBufferCreate();
        TSHttpHdrPrint(bufp, hdr_loc, iobufp);

        readerp = TSIOBufferReaderAlloc(iobufp);
        blockp = TSIOBufferReaderStart(readerp);

        len = TSIOBufferBlockReadAvail(blockp, readerp);
        head_buf = (void *)TSIOBufferBlockReadStart(blockp, readerp, &len);

        icdata.data = (void *)head_buf;
        icdata.dlen = len;

        (*ibd->ib_notify)(ironbee, &icdata, NULL);

        TSIOBufferDestroy(iobufp);
        TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
    }
    else {
        void *head_buf;
        char *head_ptr;

        rv = TSHttpTxnClientDataGet(txnp, &head_buf, &icdata.dlen);
        if (rv) {
            TSError ("couldn't retrieve %s header: %d\n", ibd->word, rv);
            return 0;
        }

        /* Workaround:
         * Search for and remove the extra "http://" in the path by
         * advancing the bytes preceding the extra string forward.
         *
         * EX: Here 1 would become 2 (x are removed bytes)
         *  1) "GET http:///foo HTTP/1.0"
         *  2) "xxxxxxxGET /foo HTTP/1.0"
         */
        head_ptr = (char *)memchr(head_buf, ' ', icdata.dlen);
        if ((head_ptr != NULL) &&
            (icdata.dlen - (head_ptr - (char *)head_buf) >= 9) &&
            (head_ptr[1] = 'h') && (head_ptr[2] = 't') &&
            (head_ptr[3] = 't') && (head_ptr[4] = 'p') &&
            (head_ptr[5] = ':') && (head_ptr[6] = '/') &&
            (head_ptr[7] = '/') && (head_ptr[8] = '/'))
        {
            TSError("ATS Workaround - Removing extra http:// from request line: %.*s\n", 50, (char *)head_buf);
            while (head_ptr >= (char *)head_buf) {
                *(head_ptr + 7) = *head_ptr;
                --head_ptr;
            }
            icdata.data = (void *)(((char *)head_buf) + 7);
            icdata.dlen -= 7;
            TSError("ATS Workaround - DATA[%d]: %.*s ...\n", (int)icdata.dlen, 25, (char *)icdata.data);
        }
        else {
            icdata.data = head_buf;
        }

        (*ibd->ib_notify)(ironbee, &icdata, NULL);
    }

#else
    /* We'll get a bogus URL from TS-998 */

    char *head_buf;
    char *head_ptr;
    void *head_start;
    int first_time = 1;
    unsigned char *dptr;

    /* before the HTTP headers comes the request line / response code */
    rv = (*ibd->hdr_get)(txnp, &bufp, &hdr_loc);
    if (rv) {
        TSError ("couldn't retrieve %s header: %d\n", ibd->word, rv);
        return 0;
    }

    /* Get the data into an IOBuffer so we can access them! */
    //iobufp = TSIOBufferSizedCreate(...);
    iobufp = TSIOBufferCreate();
    TSHttpHdrPrint(bufp, hdr_loc, iobufp);

    readerp = TSIOBufferReaderAlloc(iobufp);
    blockp = TSIOBufferReaderStart(readerp);

    len = TSIOBufferBlockReadAvail(blockp, readerp);
    ib_log_debug3(ironbee,
                 "ts/ironbee/process_header: len=%ld", len );

    /* if we're going to enable manipulation of headers, we need a copy */
    icdata.dlen = len;
    icdata.data = dptr = TSmalloc(len);

    for (head_buf = (void *)TSIOBufferBlockReadStart(blockp, readerp, &len);
         len > 0;
         head_buf = (void *)TSIOBufferBlockReadStart(
                        TSIOBufferReaderStart(readerp), readerp, &len)) {

        if (first_time && (ibd->dir == IBD_REQ)) {
            /* Workaround:
             * Search for and remove the extra "http://" in the path by
             * advancing the bytes preceding the extra string forward.
             *
             * EX: Here 1 would become 2 (x are removed bytes)
             *  1) "GET http:///foo HTTP/1.0"
             *  2) "xxxxxxxGET /foo HTTP/1.0"
             */
            head_ptr = (char *)memchr(head_buf, ' ', len);
            if ((head_ptr != NULL) &&
                (len - (head_ptr - head_buf) >= 9) &&
                (head_ptr[1] = 'h') && (head_ptr[2] = 't') &&
                (head_ptr[3] = 't') && (head_ptr[4] = 'p') &&
                (head_ptr[5] = ':') && (head_ptr[6] = '/') &&
                (head_ptr[7] = '/') && (head_ptr[8] = '/'))
            {
                TSError("ATS Workaround - Removing extra http:// from request line: %.*s\n", 50, (char *)head_buf);
                while (head_ptr >= head_buf) {
                    *(head_ptr + 7) = *head_ptr;
                    --head_ptr;
                }
                head_start = head_buf + 7;
                len -= 7;
                icdata.dlen -= 7;
                TSError("ATS Workaround - DATA[%d]: %.*s ...\n", (int)len, 25, (char *)head_start);
            }
            else {
                head_start = head_buf;
            }

            memcpy(dptr, head_start, len);
            dptr += len;
        }
        else {
            memcpy(dptr, head_buf, len);
            dptr += len;
        }
        first_time = 0;


        /* if there's more to come, go round again ... */
        TSIOBufferReaderConsume(readerp, len);
    }
    (*ibd->ib_notify)(ironbee, &icdata);

    /* Now manipulate headers as requested by ironbee */
    for (hdr = data->hdr_actions; hdr != NULL; hdr = hdr->next) {
        TSMLoc field_loc;
        if (hdr->dir != ibd->dir)
            continue;    /* it's not for us */
        switch (hdr->action) {
            case IB_HDR_SET:  /* replace any previous instance == unset + add */
            case IB_HDR_UNSET:  /* unset it */
                /* Use a while loop in case there are multiple instances */
                while (field_loc = TSMimeHdrFieldFind(bufp, hdr_loc, hdr->hdr,
                                                      strlen(hdr->hdr)),
                           field_loc != TS_NULL_MLOC) {
                    TSMimeHdrFieldDestroy(bufp, hdr_loc, field_loc);
                    TSHandleMLocRelease(bufp, hdr_loc, field_loc);
                }
                if (hdr->action == IB_HDR_UNSET)
                    break;
                /* else fallthrough to ADD */
            case IB_HDR_ADD:  /* add it in, regardless of whether it exists */
add_hdr:
                rv = TSMimeHdrFieldCreate(bufp, hdr_loc, &field_loc);
                if (rv != TS_SUCCESS)
                     TSError("Failed to add MIME header field");
                rv = TSMimeHdrFieldNameSet(bufp, hdr_loc, field_loc,
                                           hdr->hdr, strlen(hdr->hdr));
                rv = TSMimeHdrFieldValueStringSet(bufp, hdr_loc, field_loc, -1,
                                                  hdr->value, strlen(hdr->value));
                rv = TSMimeHdrFieldAppend(bufp, hdr_loc, field_loc);
                TSHandleMLocRelease(bufp, hdr_loc, field_loc);
                break;
            case IB_HDR_MERGE:  /* append UNLESS value already appears */
                /* FIXME: implement this in full */
                /* treat this as APPEND */
            case IB_HDR_APPEND: /* append it to any existing instance */
                field_loc = TSMimeHdrFieldFind(bufp, hdr_loc, hdr->hdr,
                                               strlen(hdr->hdr));
                if (field_loc == TS_NULL_MLOC) {
                    /* this is identical to IB_HDR_ADD */
                    goto add_hdr;
                }
                /* This header exists, so append to it
                 * (the function is called Insert but actually appends
                 */
                rv = TSMimeHdrFieldValueStringInsert(bufp, hdr_loc, field_loc, -1,
                                               hdr->value, strlen(hdr->value));
                TSHandleMLocRelease(bufp, hdr_loc, field_loc);
                break;
            default:  /* bug !! */
                TSDebug("ironbee", "Bogus header action %d", hdr->action);
                break;
        }
    }

    TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
    TSIOBufferReaderFree(readerp);
    TSIOBufferDestroy(iobufp);
    TSfree(icdata.data);

    return data->status;
#endif
}

/**
 * @internal
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
static int ironbee_plugin(TSCont contp, TSEvent event, void *edata)
{
    TSVConn connp;
    TSCont mycont;
    TSHttpTxn txnp = (TSHttpTxn) edata;
    TSHttpSsn ssnp = (TSHttpSsn) edata;
    ib_txn_ctx *txndata;
    ib_ssn_ctx *ssndata;
    int status = 0;

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
            TSContDataSet(mycont, NULL);

            TSHttpSsnHookAdd (ssnp, TS_HTTP_SSN_CLOSE_HOOK, mycont);

            TSHttpSsnReenable (ssnp, TS_EVENT_HTTP_CONTINUE);
            break;
        case TS_EVENT_HTTP_TXN_START:
            /* start of Request */
            /* First req on a connection, we set up conn stuff */
            ssndata = TSContDataGet(contp);
            if (ssndata == NULL) {
                ib_conn_t *iconn = NULL;
                ib_status_t rc;
                rc = ib_conn_create(ironbee, &iconn, contp);
                if (rc != IB_OK) {
                    TSError("ironbee: ib_conn_create: %d\n", rc);
                    return rc; // FIXME - figure out what to do
                }
                ssndata = TSmalloc(sizeof(ib_ssn_ctx));
                memset(ssndata, 0, sizeof(ib_ssn_ctx));
                ssndata->iconn = iconn;
                ssndata->txnp = txnp;
                TSContDataSet(contp, ssndata);
                ib_state_notify_conn_opened(ironbee, iconn);
            }

            /* create a txn cont (request ctx) */
            mycont = TSContCreate(ironbee_plugin, NULL);
            txndata = TSmalloc(sizeof(ib_txn_ctx));
            memset(txndata, 0, sizeof(ib_txn_ctx));
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

            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;

            /* HTTP RESPONSE */
        case TS_EVENT_HTTP_READ_RESPONSE_HDR:
            txndata = TSContDataGet(contp);

            /* hook to examine output headers */
            /* Not sure why we can't do it right now, but it seems headers
             * are not yet available.
             * Can we use another case switch in this function?
             */
            TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);

            /* hook an output filter to watch data */
            connp = TSTransformCreate(out_data_event, txnp);
            TSContDataSet(connp, txndata);
            TSHttpTxnHookAdd(txnp, TS_HTTP_RESPONSE_TRANSFORM_HOOK, connp);

            txndata->state |= START_RESPONSE;
            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;

            /* hook for processing response headers */
            /* If ironbee has sent us into an error response then
             * we came here in our error path, with nonzero status
             * FIXME: tests
             */
        case TS_EVENT_HTTP_SEND_RESPONSE_HDR:
            txndata = TSContDataGet(contp);
            if (txndata->status == 0) {
                status = process_hdr(txndata, txnp, &ironbee_direction_resp);
                txndata->state |= HDRS_OUT;
                if (status >= 200 && status < 600)
                    TSError("IB Error response %d ignored (too late)", status);
            }
            else {
                error_response(txnp, txndata);
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
            TSContDataSet(connp, txndata);
            TSHttpTxnHookAdd(txnp, TS_HTTP_REQUEST_TRANSFORM_HOOK, connp);

            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;

            /* hook for processing incoming request/headers */
        case TS_EVENT_HTTP_PRE_REMAP:
            txndata = TSContDataGet(contp);
            status = process_hdr(txndata, txnp, &ironbee_direction_req);
            txndata->state |= HDRS_IN;
            if (status >= 200 && status < 600) {
                TSHttpTxnHookAdd(txnp, TS_HTTP_SEND_RESPONSE_HDR_HOOK, contp);
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_ERROR);
            }
            else {
                /* Other nonzero statuses not supported */
                TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            }
            break;


            /* CLEANUP EVENTS */
        case TS_EVENT_HTTP_TXN_CLOSE:
            TSDebug("ironbee", "TXN Close: %lx\n", (long unsigned int)contp);
            ib_txn_ctx_destroy(TSContDataGet(contp));
            TSContDataSet(contp, NULL);
            TSContDestroy(contp);
            TSHttpTxnReenable(txnp, TS_EVENT_HTTP_CONTINUE);
            break;

        case TS_EVENT_HTTP_SSN_CLOSE:
            TSDebug("ironbee", "SSN Close: %lx\n", (long unsigned int)contp);
            ib_ssn_ctx_destroy(TSContDataGet(contp));
            TSContDestroy(contp);
            TSHttpSsnReenable(ssnp, TS_EVENT_HTTP_CONTINUE);
            break;

            /* if we get here we've got a bug */
        default:
            TSError("BUG: unhandled event %d in ironbee_plugin\n", event);
            break;
    }

    return 0;
}

static int check_ts_version(void)
{

    const char *ts_version = TSTrafficServerVersionGet();
    int result = 0;

    if (ts_version) {
        int major_ts_version = 0;
        int minor_ts_version = 0;
        int patch_ts_version = 0;

        if (sscanf(ts_version, "%d.%d.%d", &major_ts_version, &minor_ts_version, &patch_ts_version) != 3) {
            return 0;
        }

        /* Need at least TS 2.0 */
        if (major_ts_version >= 2) {
            result = 1;
        }

    }

    return result;
}

/**
 * @internal
 * IronBee ATS logger.
 *
 * Performs IronBee logging for the ATS plugin.
 *
 * @param[in] dummy Dummy pointer
 * @param[in] level Debug level
 * @param[in] file File name
 * @param[in] line Line number
 * @param[in] fmt Format string
 * @param[in] ap Var args list to match the format
 */
static void ironbee_logger(void *dummy, int level,
                           const char *prefix, const char *file, int line,
                           const char *fmt, va_list ap)
{
    char buf[8192 + 1];
    int limit = 7000;
    int ec;
    TSReturnCode rc;
    const char *errmsg = NULL;

    /* Buffer the log line. */
    ec = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (ec >= limit) {
        /* Mark as truncated, with a " ...". */
        memcpy(buf + (limit - 5), " ...", 5);
        errmsg = "Data truncated in log";
    }

    /* Write it to the ironbee log. */
    /* FIXME: why is the format arg's prototype not const char* ? */
    rc = prefix ? TSTextLogObjectWrite(ironbee_log, (char *)"%s: %s", prefix, buf)
        : TSTextLogObjectWrite(ironbee_log, (char *)"%s", buf);
    if (rc != TS_SUCCESS) {
        errmsg = "Data logging failed!";
    }
    if (errmsg != NULL)
        TSError("[ts-ironbee] %s\n", errmsg);
}

/**
 * @internal
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
        TSError("[ts-ironbee] getnameinfo: %d\n", rv);
    }
    *port = atoi(serv);
}

/**
 * @internal
 * Initialize the IB connection.
 *
 * Initializes an IronBee connection from a ATS continuation
 *
 * @param[in,out] ib IronBee engine
 * @param[in] iconn IB connection
 * @param[in] cbdata Callback data
 *
 * @returns status
 */
static ib_status_t ironbee_conn_init(ib_engine_t *ib,
                                     ib_state_event_type_t event,
                                     ib_conn_t *iconn,
                                     void *cbdata)
{
    /* when does this happen? */
    ib_status_t rc;
    const struct sockaddr *addr;
    int port;

    TSCont contp = iconn->pctx;
    ib_ssn_ctx* data = TSContDataGet(contp);
//  ib_clog_debug(....);

    /* remote ip */
    addr = TSHttpTxnClientAddrGet(data->txnp);

    addr2str(addr, data->remote_ip, &port);

    iconn->remote_ipstr = data->remote_ip;
    rc = ib_data_add_bytestr(iconn->dpi,
                             "remote_ip",
                             (uint8_t *)iconn->remote_ipstr,
                             strlen(data->remote_ip),
                             NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* remote port */
    iconn->remote_port = port;
    rc = ib_data_add_num(iconn->dpi, "remote_port", port, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* local end */
    addr = TSHttpTxnIncomingAddrGet(data->txnp);

    addr2str(addr, data->local_ip, &port);

    iconn->local_ipstr = data->local_ip;
    rc = ib_data_add_bytestr(iconn->dpi,
                             "local_ip",
                             (uint8_t *)iconn->local_ipstr,
                             strlen(data->local_ip),
                             NULL);
    if (rc != IB_OK) {
        return rc;
    }

    /* local_port */
    iconn->local_port = port;
    rc = ib_data_add_num(iconn->dpi, "local_port", port, NULL);
    if (rc != IB_OK) {
        return rc;
    }
    return IB_OK;
}

static IB_PROVIDER_IFACE_TYPE(logger) ironbee_logger_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,
    (ib_log_logger_fn_t)ironbee_logger
};


/* this can presumably be global since it's only setup on init */
//static ironbee_config_t ibconfig;
//#define TRACEFILE "/tmp/ironbee-trace"
#define TRACEFILE NULL

/**
 * @internal
 * Handle ATS shutdown for IronBee plugin.
 *
 * Registered via atexit() during initialization, destroys the IB engine,
 * etc.
 *
 */
static void ibexit(void)
{
    TSTextLogObjectDestroy(ironbee_log);
    ib_engine_destroy(ironbee);
}

/**
 * @internal
 * Initialize IronBee for ATS.
 *
 * Performs IB initializations for the ATS plugin.
 *
 * @param[in] configfile Configuration file
 * @param[in] logfile Log file
 *
 * @returns status
 */
static int ironbee_init(const char *configfile, const char *logfile)
{
    /* grab from httpd module's post-config */
    ib_status_t rc;
//  ib_provider_t *lpr;
    ib_cfgparser_t *cp;
    ib_context_t *ctx;
    int rv;

    rc = ib_initialize();
    if (rc != IB_OK) {
        return rc;
    }

    ib_util_log_level(9);

    ib_trace_init(TRACEFILE);

    rc = ib_engine_create(&ironbee, &ibplugin);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_provider_register(ironbee, IB_PROVIDER_TYPE_LOGGER, "ironbee-ts",
                              NULL, &ironbee_logger_iface, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    ib_context_set_string(ib_context_engine(ironbee),
                          IB_PROVIDER_TYPE_LOGGER, "ironbee-ts");
    ib_context_set_num(ib_context_engine(ironbee),
                       IB_PROVIDER_TYPE_LOGGER ".log_level", 4);

    rc = ib_engine_init(ironbee);
    if (rc != IB_OK) {
        return rc;
    }

    /* success is documented as TS_LOG_ERROR_NO_ERROR but that's undefined.
     * It's actually a TS_SUCCESS (proxy/InkAPI.cc line 6641).
     */
    rv = TSTextLogObjectCreate(logfile, TS_LOG_MODE_ADD_TIMESTAMP, &ironbee_log);
    if (rv != TS_SUCCESS) {
        return IB_OK + rv;
    }

    rc = atexit(ibexit);
    if (rc != 0) {
        return IB_OK + rv;
    }

    ib_hook_conn_register(ironbee, conn_opened_event,
                          ironbee_conn_init, NULL);


    ib_state_notify_cfg_started(ironbee);
    ctx = ib_context_main(ironbee);

    ib_context_set_string(ctx, IB_PROVIDER_TYPE_LOGGER, "ironbee-ts");
    ib_context_set_num(ctx, "logger.log_level", 4);

    rc = ib_cfgparser_create(&cp, ironbee);
    if (rc != IB_OK) {
        return rc;
    }
    if (cp != NULL) {  // huh?
        ib_cfgparser_parse(cp, configfile);
        ib_cfgparser_destroy(cp);
    }
    ib_state_notify_cfg_finished(ironbee);


    return IB_OK;
}

/**
 * @internal
 * Initialize the IronBee ATS plugin.
 *
 * Performs initializations required by ATS.
 *
 * @param[in] argc Command-line argument count
 * @param[in] argv Command-line argument list
 */
void TSPluginInit(int argc, const char *argv[])
{
    int rv;
    TSPluginRegistrationInfo info;
    TSCont cont;

    /* FIXME - check why these are char*, not const char* */
    info.plugin_name = (char *)"ironbee";
    info.vendor_name = (char *)"Qualys, Inc";
    info.support_email = (char *)"ironbee-users@lists.sourceforge.com";

    if (TSPluginRegister(TS_SDK_VERSION_3_0, &info) != TS_SUCCESS) {
        TSError("[ironbee] Plugin registration failed.\n");
        goto Lerror;
    }

    if (!check_ts_version()) {
        TSError("[ironbee] Plugin requires Traffic Server 3.0 or later\n");
        goto Lerror;
    }

    cont = TSContCreate(ironbee_plugin, NULL);

    /* connection initialization & cleanup */
    TSHttpHookAdd(TS_HTTP_SSN_START_HOOK, cont);


    if (argc < 2) {
        TSError("[ironbee] configuration file name required\n");
        goto Lerror;
    }
    rv = ironbee_init(argv[1], argc >= 3 ? argv[2] : DEFAULT_LOG);
    if (rv != IB_OK) {
        TSError("[ironbee] initialization failed with %d\n", rv);
    }
    return;

Lerror:
    TSError("[ironbee] Unable to initialize plugin (disabled).\n");
}
