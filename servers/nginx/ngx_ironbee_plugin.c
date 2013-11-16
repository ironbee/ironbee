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
 * @brief IronBee --- nginx 1.3 module
 *
 * @author Nick Kew <nkew@qualys.com> - ironbee plugin and callbacks
 */

#include "ngx_ironbee.h"
#include <assert.h>

/**
 * Function to add a new header to a list.  Any existing entry
 * of the same name is ignored and remains intact.  Note that this
 * only affects headers transmitted to a backend or client: where
 * a 'real' header affects nginx internally, this will do nothing.
 *
 * @param[in] list   the list to add to
 * @param[in] entry  the name of the entry to add
 * @param[in] val    the value to set it to
 */
static void list_add(ngx_list_t *list, const char *entry, const char *val)
{
    ngx_table_elt_t *elt = ngx_list_push(list);
    assert(elt != NULL);
    elt->key.len = strlen(entry);
    elt->key.data = ngx_palloc(list->pool, elt->key.len);
    memcpy(elt->key.data, entry, elt->key.len);
    elt->value.len = strlen(val);
    elt->value.data = ngx_palloc(list->pool, elt->value.len);
    memcpy(elt->value.data, val, elt->value.len);
}
/**
 * Function to unset a header in a list.  This will not remove the
 * entry altogether, but will instead set the value to empty.
 *
 * @param[in] list   the list to add to
 * @param[in] entry  the name of the entry to empty
 */
static void list_unset(ngx_list_t *list, const char *entry)
{
    ngx_list_part_t *part;
    ngx_table_elt_t *elt;
    unsigned int i;
    for (part = &list->part; part; part = part->next) {
        elt = part->elts;
        for (i = 0; i < part->nelts; ++i) {
#if 1
            if (elt[i].key.len == strlen(entry)
                && !strncasecmp((const char*)elt[i].key.data, entry, elt[i].key.len)) {
                /* This is a match.  Remove it */
                elt[i].value.len = 0;  /* just clobber the value? */
#else
            while (elt[i].key.len == strlen(entry) && !strcasecmp(elt[i].key.data, entry, elt[i].key.len)) {
                /* This is a match.  Remove it */
                /* Remove altogether, and do lots of housekeeping */
                memcpy(elt[--part->nelts], elt[i], sizeof(...));
                /* This is useless: it'll leave dangling pointers elsewhere */
#endif
            }
        }
        if (part == list->last)
            break;
    }
}
/**
 * Function to set a header in a list.  Any existing entry
 * of the same name will be overwritten, causing the new value to
 * be used if the header affects nginx internally.
 *
 * @param[in] list   the list to add to
 * @param[in] entry  the name of the entry to add
 * @param[in] val    the value to set it to
 */
static void list_set(ngx_list_t *list, const char *entry, const char *val)
{
    ngx_list_part_t *part;
    ngx_table_elt_t *elt;
    unsigned int i;
    int found = 0;
    unsigned int vlen = strlen(val);
    for (part = &list->part; part; part = part->next) {
        elt = part->elts;
        for (i = 0; i < part->nelts; ++i) {
            if (elt[i].key.len == strlen(entry)
                && !strncasecmp((const char*)elt[i].key.data, entry, elt[i].key.len)) {
                /* This is a match.  Remove it */
                if (elt[i].value.len > vlen) {
                    elt[i].value.data = ngx_palloc(list->pool, vlen);
                }
                elt[i].value.len = vlen;  /* just clobber the value? */
                memcpy(elt[i].value.data, val, vlen);
                ++found;
            }
        }
        if (part == list->last)
            break;
    }
    if (!found)
        list_add(list, entry, val);
}
/**
 * Function to append a header in a list.  Any existing entry
 * of the same name will be overwritten by appending the new value
 * to the old-value in a comma-separated list.
 *
 * @param[in] list   the list to add to
 * @param[in] entry  the name of the entry to add
 * @param[in] val    the value to set it to
 */
static void list_append(ngx_list_t *list, const char *entry, const char *val)
{
    ngx_list_part_t *part;
    ngx_table_elt_t *elt;
    unsigned int i;
    int found = 0;
    unsigned int vlen = strlen(val);
    for (part = &list->part; part && !found; part = part->next) {
        elt = part->elts;
        for (i = 0; i < part->nelts && !found; ++i) {
            if (elt[i].key.len == strlen(entry)
                && !strncasecmp((const char*)elt[i].key.data, entry, elt[i].key.len)) {
                unsigned int oldlen = elt[i].value.len;
                const unsigned char *oldval = elt[i].value.data;
                elt[i].value.len += vlen+1;
                elt[i].value.data = ngx_palloc(list->pool, elt[i].value.len);
                elt[i].value.len = vlen;  /* just clobber the value? */
                memcpy(elt[i].value.data, oldval, oldlen);
                elt[i].value.data[oldlen] = ',';
                memcpy(elt[i].value.data+oldlen+1, val, vlen);
                ++found;
            }
        }
        if (part == list->last)
            break;
    }
    if (!found)
        list_add(list, entry, val);
}
/**
 * Function to apply regexp-based edit to a header in a list.
 *
 * @param[in] list   the list to add to
 * @param[in] entry  the name of the entry to edit
 * @param[in] val    string value of the regexp, if rx is null
 * @param[in] tx     The ironbee tx rec
 * @param[in] rx     The compiled regex operation, or NULL to use val
 */
static ib_status_t list_edit(ngx_list_t *list, const char *entry,
                             const char *val, ib_tx_t *tx, ib_rx_t *rx)
{
    ngx_list_part_t *part;
    ngx_table_elt_t *elt;
    unsigned int i;
    char *repl;
    char *oldval;
    ngxib_req_ctx *ctx = tx->sctx;

    ngx_regex_malloc_init(ctx->r->pool);

    /* Check we were passed something valid */
    if (rx == NULL) {
        if (rx = ib_rx_compile(tx->mp, val), rx == NULL) {
            ib_log_error_tx(ctx->tx, "Failed to compile %s as regexp", val);
            cleanup_return IB_EINVAL;
        }
    }
    for (part = &list->part; part; part = part->next) {
        elt = part->elts;
        for (i = 0; i < part->nelts; ++i) {
            if (elt[i].key.len == strlen(entry)
                && !strncasecmp((const char*)elt[i].key.data, entry, elt[i].key.len)) {
                /* we need a null-terminated string for ib_rx_exec */
                oldval = strndup((const char*)elt[i].value.data, elt[i].value.len);
                ib_rx_exec(tx->mp, rx, oldval, &repl, NULL);
                free(oldval);
                if (repl != NULL) {
                    elt[i].value.data = (unsigned char*)repl;
                    elt[i].value.len = strlen(repl);
                }
            }
        }
        if (part == list->last)
            break;
    }
    cleanup_return IB_OK;
}
/**
 * IronBee callback function to manipulate an HTTP header
 *
 * @param[in] tx - IronBee transaction
 * @param[in] dir - Request/Response
 * @param[in] action - Requested header manipulation
 * @param[in] hdr - Header
 * @param[in] value - Header Value
 * @param[in] rx - Compiled regexp of value (if applicable)
 * @return status (OK, Declined if called too late, Error if called with
 *                 invalid data).  NOTIMPL should never happen.
 */
static ib_status_t ib_header_callback(ib_tx_t *tx, ib_server_direction_t dir,
                                      ib_server_header_action_t action,
                                      const char *hdr, const char *value,
                                      ib_rx_t *rx, void *cbdata)
{
    /* This is more complex for nginx than for other servers because
     * headers_in and headers_out are different structs, and there
     * are lots of enumerated headers to watch out for.
     *
     * It appears the enumerated headers are in fact just pointers
     * into the generic lists.  So with luck it should be sufficient
     * to deal with just the lists.  Revisit if we seem to get
     * unexpected failures in manipulating headers.
     *
     * That won't work for setting/unsetting a header altogether.
     * It's no use if we set the list but leave the enumerated
     * pointers uninitialized or dangling.
     */
    ngxib_req_ctx *ctx = tx->sctx;

    /* the headers list is common between request and response */
    ngx_list_t *headers = (dir == IB_SERVER_REQUEST)
                          ? &ctx->r->headers_in.headers
                          : &ctx->r->headers_out.headers;

    if (ctx->hdrs_out || (ctx->hdrs_in && (dir == IB_SERVER_REQUEST)))
        return IB_DECLINED;  /* too late for requested op */

    switch (action) {
      case IB_HDR_SET:
        list_set(headers, hdr, value);
        return IB_OK;
      case IB_HDR_UNSET:
        list_unset(headers, hdr);
        return IB_OK;
      case IB_HDR_ADD:
        list_add(headers, hdr, value);
        return IB_OK;
      case IB_HDR_MERGE:
      case IB_HDR_APPEND:
        list_append(headers, hdr, value);
        return IB_OK;
      case IB_HDR_EDIT:
        return list_edit(headers, hdr, value, tx, rx);
    }
    return IB_ENOTIMPL;
}
/**
 * IronBee callback function to set an HTTP error status.
 * This will divert processing into an ErrorDocument for the status.
 *
 * @param[in] tx - IronBee transaction
 * @param[in] status - Status to set
 * @return OK, or Declined if called too late.  NOTIMPL should never happen.
 */
static ib_status_t ib_error_callback(ib_tx_t *tx, int status, void *cbdata)
{
    ngxib_req_ctx *ctx = tx->sctx;
    if (status >= 200 && status < 600) {
        if (ctx->status >= 200 && ctx->status < 600) {
            ib_log_notice_tx(tx, "Ignoring: status already set to %d", ctx->status);
            return IB_OK;
        }
        if (ctx->start_response) {
            ib_log_notice_tx(tx, "Too late to change status=%d", status);
            return IB_DECLINED;
        }
        ib_log_info_tx(tx, "Setting status: %d -> %d", ctx->status, status);
        ctx->status = status;
        return IB_OK;
    }
    return IB_ENOTIMPL;
}
/**
 * IronBee callback function to set an HTTP header for an ErrorDocument.
 *
 * @param[in] tx - IronBee transaction
 * @param[in] hdr - Header to set
 * @param[in] val - Value to set
 * @return Not Implemented, or error.
 */
static ib_status_t ib_errhdr_callback(ib_tx_t *tx, const char *hdr, const char *val, void *cbdata)
{
    ngxib_req_ctx *ctx = tx->sctx;
    if (ctx->start_response)
        return IB_DECLINED;
    if (!hdr || !val)
        return IB_EINVAL;

    return IB_ENOTIMPL;
}

/**
 * IronBee callback function to set an ErrorDocument
 * Since httpd has its own internal ErrorDocument mechanism,
 * we use that for the time being and leave this NOTIMPL
 *
 * TODO: think about something along the lines of mod_choice's errordoc.
 *
 * @param[in] tx IronBee transaction
 * @param[in] data Data to set
 * @param[in] dlen Length of @a data to set.
 * @return NOTIMPL, or Declined if called too late, or EINVAL.
 */
static ib_status_t ib_errdata_callback(
    ib_tx_t *tx,
    const uint8_t *data,
    size_t dlen,
    void *cbdata)
{
    ngxib_req_ctx *ctx = tx->sctx;
    if (ctx->start_response)
        return IB_DECLINED;
    if (!data)
        return IB_EINVAL;

/* Maybe implement something here?
    ctx->errdata = apr_pstrdup(ctx->r->pool, data);
    return IB_OK;
*/
    return IB_ENOTIMPL;
}

static ib_status_t ib_errclose_callback(
    ib_conn_t *conn,
    ib_tx_t *tx,
    void *cbdata)
{
    ib_log_error(conn->ib, "BLOCK BY CLOSE NOT IMPLEMENTED.");
    return IB_ENOTIMPL;
}

/**
 * IronBee callback function to return the ib_server instance for nginx
 *
 * @return pointer to the ib_server instance for nginx
 */
ib_server_t *ib_plugin(void)
{
    static ib_server_t ibplugin = {
        IB_SERVER_HEADER_DEFAULTS,
        "nginx-ironbee",
        ib_header_callback,
        NULL,
        ib_error_callback,
        NULL,
        ib_errhdr_callback,
        NULL,
        ib_errdata_callback,
        NULL,
        ib_errclose_callback,
        NULL
    };
    return &ibplugin;
}
