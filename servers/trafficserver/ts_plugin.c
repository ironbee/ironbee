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

#include <assert.h>
#include <ts/ts.h>

#if defined(__cplusplus) && !defined(__STDC_FORMAT_MACROS)
/* C99 requires that inttypes.h only exposes PRI* macros
 * for C++ implementations if this is defined: */
#define __STDC_FORMAT_MACROS
#endif

#include <ironbee/core.h>
#include <ironbee/flags.h>

#include "ts_ib.h"

static bool is_error_status(int status)
{
    return ( (status >= 200) && (status < 600) );
}

/**
 * Callback functions for IronBee to signal to us
 */
static
ib_status_t ib_header_callback(
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    ib_server_header_action_t  action,
    const char                *name,
    size_t                     name_length,
    const char                *value,
    size_t                     value_length,
    void                      *cbdata
)
{
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;
    hdr_action_t *header;
    /* Logic for whether we're in time for the requested action */
    /* Output headers can change any time before they're sent */
    /* Input headers can only be touched during their read */

    if (ib_flags_all(tx->flags, IB_TX_FCLIENTRES_STARTED) ||
        (ib_flags_all(tx->flags, IB_TX_FSERVERREQ_STARTED)
                  && dir == IB_SERVER_REQUEST))
    {
        ib_log_debug_tx(tx, "Too late to change headers.");
        return IB_DECLINED;  /* too late for requested op */
    }

    header = ib_mm_alloc(tx->mm, sizeof(*header));
    header->next = txndata->hdr_actions;
    txndata->hdr_actions = header;
    header->dir = dir;
    /* FIXME: deferring merge support - implementing append instead */
    header->action = action = action == IB_HDR_MERGE ? IB_HDR_APPEND : action;
    header->hdr = ib_mm_memdup_to_str(tx->mm, name, name_length);
    header->value = ib_mm_memdup_to_str(tx->mm, value, value_length);

    return IB_OK;
}
static ib_status_t ib_error_callback(ib_tx_t *tx, int status, void *cbdata)
{
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;
    ib_log_debug_tx(tx, "ib_error_callback with status=%d", status);
    if ( is_error_status(status) ) {
        if (is_error_status(txndata->status) ) {
            ib_log_debug_tx(tx, "Ignoring: status already set to %d", txndata->status);
            return IB_OK;
        }
        /* We can't return an error after the response has started */
        if (ib_flags_all(tx->flags, IB_TX_FCLIENTRES_STARTED)) {
            ib_log_debug_tx(tx, "Too late to change status=%d", status);
            return IB_DECLINED;
        }
        /* ironbee wants to return an HTTP status.  We'll oblige */
        /* FIXME: would the semantics work for 1xx?  Do we care? */
        /* No, we don't care unless a use case arises for the proxy
         * to initiate a 1xx response independently of the backend.
         */
        txndata->status = status;
        return IB_OK;
    }
    return IB_ENOTIMPL;
}

static
ib_status_t ib_errhdr_callback(
    ib_tx_t    *tx,
    const char *name,
    size_t      name_length,
    const char *value,
    size_t      value_length,
    void       *cbdata
)
{
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;
    hdr_list *hdrs;
    /* We can't return an error after the response has started */
    if (ib_flags_all(tx->flags, IB_TX_FCLIENTRES_STARTED))
        return IB_DECLINED;
    if (!name || !value)
        return IB_EINVAL;
    hdrs = ib_mm_alloc(tx->mm, sizeof(*hdrs));
    hdrs->hdr = ib_mm_memdup_to_str(tx->mm, name, name_length);
    hdrs->value = ib_mm_memdup_to_str(tx->mm, value, value_length);
    hdrs->next = txndata->err_hdrs;
    txndata->err_hdrs = hdrs;
    return IB_OK;
}

static ib_status_t ib_errbody_callback(
    ib_tx_t *tx,
    const char *data,
    size_t dlen,
    void *cbdata)
{
    uint8_t *err_body;
    tsib_txn_ctx *txndata = (tsib_txn_ctx *)tx->sctx;

    /* Handle No Data as zero length data. */
    if (data == NULL || dlen == 0) {
        return IB_OK;
    }

    /* We can't return an error after the response has started */
    if (ib_flags_all(tx->flags, IB_TX_FCLIENTRES_STARTED)) {
        return IB_DECLINED;
    }

    /* This alloc will be freed within TSHttpTxnErrorBodySet
     * so we have to use TSmalloc for it.
     */
    err_body = TSmalloc(dlen);
    if (err_body == NULL) {
        return IB_EALLOC;
    }

    txndata->err_body = memcpy(err_body, data, dlen);
    txndata->err_body_len = dlen;
    return IB_OK;
}

/**
 * Called by IronBee when a connection should be blocked by closing the conn.
 *
 * If this returns not-IB_OK a block by status code will be attempted.
 *
 * @param[in] conn The connection to close.
 * @param[in] tx The transaction, if available. If a transaction is
 *            not available, this will be NULL.
 * @param[in] cbdata Callback data.
 *
 * @returns
 *   - IB_OK on success.
 */
static ib_status_t ib_errclose_callback(
    ib_conn_t *conn,
    ib_tx_t *tx,
    void *cbdata)
{
    ib_log_error(conn->ib, "Block by close not implemented; returning BAD_REQUEST.");
    return ib_error_callback(tx, 400, cbdata);
    //return IB_ENOTIMPL;
}

static ib_status_t ib_streamedit_callback(
    ib_tx_t                   *tx,
    ib_server_direction_t      dir,
    off_t                      start,
    size_t                     bytes,
    const char                *repl,
    size_t                     repl_len,
    void                      *dummy
)
{
    ib_status_t rc;
    edit_t edit;
    tsib_txn_ctx *txndata = tx->sctx;
    tsib_filter_ctx *fctx = (dir == tsib_direction_client_req.dir)
                                 ? &txndata->in
                                 : &txndata->out;
    /* Check we're in time to edit this stream */
    if (fctx->bytes_done > (size_t)start) {
        ib_log_error_tx(tx, "Tried to edit data that's already been forwarded");
        rc = IB_EINVAL;
    }
    else { /* All's well */
        if (!fctx->edits) {
            rc = ib_vector_create(&fctx->edits, tx->mm, 0);
            assert((rc == IB_OK) && (fctx->edits != NULL));
        }
        edit.start = start;
        edit.bytes = bytes;
        edit.repl = repl;
        edit.repl_len = repl_len;

        rc = ib_vector_append(fctx->edits, &edit, sizeof(edit_t));
        assert(rc == IB_OK);
    }

    return rc;
}

static ib_status_t ib_edit_init_callback(ib_tx_t *tx, int flags, void *x)
{
    tsib_txn_ctx *txndata = tx->sctx;
    if (flags & IB_SERVER_REQUEST) {
        txndata->in.have_edits = 1;
    }
    if (flags & IB_SERVER_RESPONSE) {
        txndata->out.have_edits = 1;
    }
    return IB_OK;
}

/* Plugin Structure */
ib_server_t ibplugin = {
    IB_SERVER_HEADER_DEFAULTS,
    "ts-ironbee",
    ib_header_callback,
    NULL,
    ib_error_callback,
    NULL,
    ib_errhdr_callback,
    NULL,
    ib_errbody_callback,
    NULL,
    ib_errclose_callback,
    NULL,
    ib_streamedit_callback,
    NULL,
    ib_edit_init_callback,
    NULL
};
