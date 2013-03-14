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
 * @brief IronBee --- Development logging
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "moddevel_private.h"

#include <ironbee/bytestr.h>
#include <ironbee/cfgmap.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/list.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/state_notify.h>
#include <ironbee/string.h>
#include <ironbee/util.h>
#include <ironbee/escape.h>

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <errno.h>

/**
 * Several max constants
 */
static const size_t max_fmt = 128;          /**< Format buffer size */
static const size_t max_path_element = 64;  /**< Max size of a path element */

/**
 * TxDump enable flags
 */
#define MODDEVEL_TXDUMP_ENABLED (1 <<  0) /**< Enabled? */
#define MODDEVEL_TXDUMP_BASIC   (1 <<  1) /**< Dump basic TX info? */
#define MODDEVEL_TXDUMP_CONN    (1 <<  2) /**< Dump connection info? */
#define MODDEVEL_TXDUMP_REQLINE (1 <<  3) /**< Dump request line? */
#define MODDEVEL_TXDUMP_REQHDR  (1 <<  4) /**< Dump request header? */
#define MODDEVEL_TXDUMP_RSPLINE (1 <<  5) /**< Dump response line? */
#define MODDEVEL_TXDUMP_RSPHDR  (1 <<  6) /**< Dump response header? */
#define MODDEVEL_TXDUMP_FLAGS   (1 <<  7) /**< Dump TX flags? */
#define MODDEVEL_TXDUMP_ARGS    (1 <<  8) /**< Dump request args? */
#define MODDEVEL_TXDUMP_DATA    (1 <<  9) /**< Dump TX Data? */
/** Default enable flags */
#define MODDEVEL_TXDUMP_DEFAULT                      \
    (                                                \
        MODDEVEL_TXDUMP_ENABLED |                    \
        MODDEVEL_TXDUMP_BASIC   |                    \
        MODDEVEL_TXDUMP_REQLINE |                    \
        MODDEVEL_TXDUMP_RSPLINE                      \
    )
/** Headers enable flags */
#define MODDEVEL_TXDUMP_HEADERS                      \
    (                                                \
        MODDEVEL_TXDUMP_ENABLED |                    \
        MODDEVEL_TXDUMP_BASIC   |                    \
        MODDEVEL_TXDUMP_REQLINE |                    \
        MODDEVEL_TXDUMP_REQHDR  |                    \
        MODDEVEL_TXDUMP_RSPLINE |                    \
        MODDEVEL_TXDUMP_RSPHDR                       \
    )
/** All enable flags */
#define MODDEVEL_TXDUMP_ALL                          \
    (                                                \
        MODDEVEL_TXDUMP_ENABLED |                    \
        MODDEVEL_TXDUMP_BASIC   |                    \
        MODDEVEL_TXDUMP_CONN    |                    \
        MODDEVEL_TXDUMP_REQLINE |                    \
        MODDEVEL_TXDUMP_REQHDR  |                    \
        MODDEVEL_TXDUMP_RSPLINE |                    \
        MODDEVEL_TXDUMP_RSPHDR  |                    \
        MODDEVEL_TXDUMP_FLAGS   |                    \
        MODDEVEL_TXDUMP_ARGS    |                    \
        MODDEVEL_TXDUMP_DATA                         \
    )

/**
 * Per-TxDump configuration
 */
typedef struct {
    ib_state_event_type_t        event;  /**< Event type */
    const char                  *name;   /**< Event name */
    ib_flags_t                   flags;  /**< Flags defining what to txdump */
    ib_log_level_t               level;  /**< IB Log level */
    FILE                        *fp;     /**< File pointer (or NULL) */
    ib_moddevel_txdump_config_t *config; /**< TxDump configuration data */
} ib_moddevel_txdump_t;

/**
 * Log configuration
 */
struct ib_moddevel_txdump_config_t {
    ib_list_t    *txdump_list;       /**< List of TxDump pointers */
    ib_mpool_t   *mp;                /**< Memory pool for allocations */
};

/**
 * Mapping of valid rule logging names to flag values.
 */
static IB_STRVAL_MAP(tx_flags_map) = {
    IB_STRVAL_PAIR("Error", IB_TX_FERROR),
    IB_STRVAL_PAIR("HTTP/0.9", IB_TX_FHTTP09),
    IB_STRVAL_PAIR("Pipelined", IB_TX_FPIPELINED),
    IB_STRVAL_PAIR("Request Started", IB_TX_FREQ_STARTED),
    IB_STRVAL_PAIR("Seen Request Header", IB_TX_FREQ_SEENHEADER),
    IB_STRVAL_PAIR("No Request Body", IB_TX_FREQ_NOBODY),
    IB_STRVAL_PAIR("Seen Request Body", IB_TX_FREQ_SEENBODY),
    IB_STRVAL_PAIR("Seen Request Trailer", IB_TX_FREQ_SEENTRAILER),
    IB_STRVAL_PAIR("Request Finished", IB_TX_FREQ_FINISHED),
    IB_STRVAL_PAIR("Response Started", IB_TX_FRES_STARTED),
    IB_STRVAL_PAIR("Seen Response Header", IB_TX_FRES_SEENHEADER),
    IB_STRVAL_PAIR("Seen Response Body", IB_TX_FRES_SEENBODY),
    IB_STRVAL_PAIR("Seen Response Trailer", IB_TX_FRES_SEENTRAILER),
    IB_STRVAL_PAIR("Response Finished", IB_TX_FRES_FINISHED),
    IB_STRVAL_PAIR("Suspicious", IB_TX_FSUSPICIOUS),
    IB_STRVAL_PAIR("Block: Advisory", IB_TX_BLOCK_ADVISORY),
    IB_STRVAL_PAIR("Block: Phase", IB_TX_BLOCK_PHASE),
    IB_STRVAL_PAIR("Block: Immediate", IB_TX_BLOCK_IMMEDIATE),
    IB_STRVAL_PAIR("Allow: Phase", IB_TX_ALLOW_PHASE),
    IB_STRVAL_PAIR("Allow: Request", IB_TX_ALLOW_REQUEST),
    IB_STRVAL_PAIR("Allow: All", IB_TX_ALLOW_ALL),
    IB_STRVAL_PAIR("Post-Process", IB_TX_FPOSTPROCESS),
    IB_STRVAL_PAIR("Inspect Request Header", IB_TX_FINSPECT_REQHDR),
    IB_STRVAL_PAIR("Inspect Request Body", IB_TX_FINSPECT_REQBODY),
    IB_STRVAL_PAIR("Inspect Response Header", IB_TX_FINSPECT_RSPHDR),
    IB_STRVAL_PAIR("Inspect Response Body", IB_TX_FINSPECT_RSPBODY),

    /* End */
    IB_STRVAL_PAIR_LAST
};

/**
 * Dump an item (varaiable args version)
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump Log parameters
 * @param[in] nspaces Number of leading spaces
 * @param[in] fmt printf-style format string
 * @param[in] ap Variable args list
 */
static void moddevel_txdump_va(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump,
    size_t                      nspaces,
    const char                 *fmt,
    va_list                     ap)
    VPRINTF_ATTRIBUTE(4);
static void moddevel_txdump_va(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump,
    size_t                      nspaces,
    const char                 *fmt,
    va_list                     ap)
{
    char fmtbuf[max_fmt+1];

    /* Limit # of leading spaces */
    if (nspaces > 32) {
        nspaces = 32;
    }

    /* Initialize the space buffer */
    for (size_t n = 0;  n < nspaces;  ++n) {
        fmtbuf[n] = ' ';
    }
    fmtbuf[nspaces] = '\0';
    strcat(fmtbuf, fmt);

    if (txdump->fp != NULL) {
        vfprintf(txdump->fp, fmtbuf, ap);
        fputs("\n", txdump->fp);
    }
    else {
        ib_log_tx_vex(tx, txdump->level, NULL, 0, fmtbuf, ap);
    }
}

/**
 * Dump an item
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump Log data
 * @param[in] nspaces Number of leading spaces to insert
 * @param[in] fmt printf-style format string
 */
static void moddevel_txdump(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump,
    size_t                      nspaces,
    const char                 *fmt, ...)
    PRINTF_ATTRIBUTE(4, 5);
static void moddevel_txdump(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump,
    size_t                      nspaces,
    const char                 *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    moddevel_txdump_va(tx, txdump, nspaces, fmt, ap);
    va_end(ap);
}

/**
 * Escape and format a bytestr
 *
 * @param[in] txdump Log data
 * @param[in] bs Byte string to log
 * @param[in] quotes Add surrounding quotes?
 * @param[in] maxlen Maximum string length (min = 6)
 *
 * @returns Formatted buffer
 */
static const char *moddevel_format_bs(
    const ib_moddevel_txdump_t *txdump,
    const ib_bytestr_t         *bs,
    bool                        quotes,
    size_t                      maxlen)
{
    assert(txdump != NULL);
    assert(bs != NULL);
    assert( (maxlen == 0) || (maxlen > 6) );

    ib_status_t    rc;
    const uint8_t *bsptr = NULL;
    char          *escaped;
    size_t         len;
    size_t         size;
    const char    *empty = quotes ? "\"\"" : "";
    char          *cur;

    /* Handle NULL bytestring */
    if (bs != NULL) {
        bsptr = ib_bytestr_const_ptr(bs);
    }

    /* If the data is NULL, no need to escape */
    if (bsptr == NULL) {
        return "<None>";
    }

    /* Escape the string */
    rc = ib_util_hex_escape_alloc(txdump->config->mp,
                                  ib_bytestr_length(bs), 5,
                                  &escaped, &size);
    if (rc != IB_OK) {
        return empty;
    }
    cur = escaped;
    if (quotes) {
        *escaped = '\"';
        ++cur;
        *cur = '\0';
    }
    len = ib_util_hex_escape_buf(bsptr, ib_bytestr_length(bs), cur, size-5);
    cur += len;

    /* Handle zero length case */
    if (len == 0) {
        return empty;
    }

    /* Add '...' if we need to crop the buffer */
    if ( (maxlen > 0) && (len > maxlen) ) {
        cur = escaped + (maxlen - 3);
        strcpy(cur, "...");
    }
    if (quotes) {
        strcat(cur, "\"");
    }

    return escaped;
}
/**
 * Log a bytestr
 *
 * @param[in] tx IronBee transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces
 * @param[in] label Label string
 * @param[in] bs Byte string to log
 * @param[in] maxlen Maximum string length
 *
 * @returns void
 */
static void moddevel_txdump_bs(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump,
    size_t                      nspaces,
    const char                 *label,
    const ib_bytestr_t         *bs,
    size_t                      maxlen)
{
    assert(tx != NULL);
    assert(txdump != NULL);
    assert(label != NULL);
    assert(bs != NULL);

    const char *buf;

    buf = moddevel_format_bs(txdump, bs, true, maxlen);
    if (buf != NULL) {
        moddevel_txdump(tx, txdump, nspaces, "%s = %s", label, buf);
    }
}

/**
 * Log a field.
 *
 * Logs a field name and value; handles various field types.
 *
 * @param[in] tx IronBee transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces
 * @param[in] label Label string
 * @param[in] field Field to log
 * @param[in] maxlen Maximum string length
 *
 * @returns void
 */
static void moddevel_txdump_field(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump,
    size_t                      nspaces,
    const char                 *label,
    const ib_field_t           *field,
    size_t                      maxlen)
{
    /* Check the field name
     * Note: field->name is not always a null ('\0') terminated string */
    if (field == NULL) {
        moddevel_txdump(tx, txdump, nspaces, "%s = <NULL>", label);
        return;
    }

    switch (field->type) {

    case IB_FTYPE_GENERIC :      /**< Generic data */
    {
        void *v;
        ib_field_value(field, ib_ftype_generic_out(&v));
        moddevel_txdump(tx, txdump, nspaces, "%s = %p", label, v);
        break;
    }

    case IB_FTYPE_NUM :          /**< Numeric value */
    {
        ib_num_t n;
        ib_field_value(field, ib_ftype_num_out(&n));
        moddevel_txdump(tx, txdump, nspaces, "%s = %"PRId64"", label, n);
        break;
    }

    case IB_FTYPE_NULSTR :       /**< NUL terminated string value */
    {
        const char *s;
        ib_field_value(field, ib_ftype_nulstr_out(&s));
        if (maxlen > 0) {
            moddevel_txdump(tx, txdump, nspaces,
                            "%s = \"%.*s...\"", label, (int)maxlen, s);
        }
        else {
            moddevel_txdump(tx, txdump, nspaces,
                            "%s = \"%s\"", label, s);
        }
        break;
    }

    case IB_FTYPE_BYTESTR :      /**< Byte string value */
    {
        const ib_bytestr_t *bs;
        ib_field_value(field, ib_ftype_bytestr_out(&bs));
        moddevel_txdump_bs(tx, txdump, nspaces, label, bs, maxlen);
        break;
    }

    case IB_FTYPE_LIST :         /**< List */
    {
        const ib_list_t *lst;
        ib_field_value(field, ib_ftype_list_out(&lst));
        size_t len = IB_LIST_ELEMENTS(lst);
        moddevel_txdump(tx, txdump, nspaces,
                        "%s = [%zd]", label, len);
        break;
    }

    case IB_FTYPE_SBUFFER :
        moddevel_txdump(tx, txdump, nspaces, "%s = sbuffer", label);
        break;

    default:
        moddevel_txdump(tx, txdump, nspaces, "Unknown field type.");
    }
}

/**
 * Log a header
 *
 * @param[in] tx IronBee transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces
 * @param[in] label Label string
 * @param[in] header Header to log
 *
 * @returns void
 */
static void moddevel_txdump_header(
    const ib_tx_t                    *tx,
    const ib_moddevel_txdump_t       *txdump,
    size_t                            nspaces,
    const char                       *label,
    const ib_parsed_header_wrapper_t *header)
{
    assert(tx != NULL);
    assert(txdump != NULL);
    assert(label != NULL);

    const ib_parsed_name_value_pair_list_t *node;

    if (header == NULL) {
        moddevel_txdump(tx, txdump, nspaces, "%s unavailable", label);
        return;
    }

    moddevel_txdump(tx, txdump, nspaces, "%s", label);
    for (node = header->head; node != NULL; node = node->next) {
        const char *name  = moddevel_format_bs(txdump, node->name, false, 24);
        const char *value = moddevel_format_bs(txdump, node->value, true, 64);
        moddevel_txdump(tx, txdump, nspaces+2, "%s = %s", name, value);
    }
}

/**
 * Build a path by appending the field name to an existing path.
 *
 * @param[in] tx IronBee transaction
 * @param[in] path Base path
 * @param[in] field Field whose name to append
 *
 * @returns Pointer to newly allocated path string
 */
static const char *moddevel_build_path(
    const ib_tx_t      *tx,
    const char         *path,
    const ib_field_t   *field)
{
    size_t pathlen;
    size_t fullpath_len;
    size_t tmplen;
    char *fullpath;
    ssize_t nlen = (ssize_t)field->nlen;
    bool truncated = false;

    if ( (nlen <= 0) || (field->name == NULL) ) {
        nlen = 0;
    }
    else if (nlen > (ssize_t)max_path_element) {
        size_t i;
        const char *p;
        for (i = 0, p=field->name; isprint(*p) && (i < max_path_element); ++i) {
            /* Do nothing */
        }
        nlen = i;
        truncated = true;
    }

    /* Special case */
    if ( (nlen == 0) || (field->name == NULL) ) {
        return path;
    }

    /* Allocate a path buffer */
    pathlen = strlen(path);
    fullpath_len = pathlen + (pathlen > 0 ? 2 : 1) + nlen + (truncated ? 3 : 0);
    fullpath = (char *)ib_mpool_alloc(tx->mp, fullpath_len);
    assert(fullpath != NULL);

    /* Copy in the base path */
    strcpy(fullpath, path);
    if (pathlen > 0) {
        strcat(fullpath, ":");
    }

    /* Append the field's name */
    tmplen = pathlen+(pathlen > 0 ? 1 : 0);
    memcpy(fullpath+tmplen, field->name, nlen);
    if (truncated) {
        strcpy(fullpath+tmplen+nlen, "...");
    }
    else {
        fullpath[fullpath_len-1] = '\0';
    }
    return fullpath;
}

/**
 * Log transaction details.
 *
 * Extract the address & ports from the transaction & log them.
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces
 * @param[in] path Base path
 * @param[in] lst List to log
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_list(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump,
    size_t                      nspaces,
    const char                 *path,
    const ib_list_t            *lst)
{
    ib_status_t rc;
    const ib_list_node_t *node = NULL;

    /* Loop through the list & log everything */
    IB_LIST_LOOP_CONST(lst, node) {
        const ib_field_t *field = (const ib_field_t *)node->data;
        const char *fullpath = NULL;

        switch (field->type) {
        case IB_FTYPE_GENERIC:
        case IB_FTYPE_NUM:
        case IB_FTYPE_NULSTR:
        case IB_FTYPE_BYTESTR:
            fullpath = moddevel_build_path(tx, path, field);
            moddevel_txdump_field(tx, txdump, nspaces, fullpath, field, 0);
            break;

        case IB_FTYPE_LIST:
        {
            const ib_list_t *v;
            // @todo Remove mutable once list is const correct.
            rc = ib_field_value(field, ib_ftype_list_out(&v));
            if (rc != IB_OK) {
                return rc;
            }

            fullpath = moddevel_build_path(tx, path, field);
            moddevel_txdump_field(tx, txdump, nspaces, fullpath, field, 0);
            moddevel_txdump_list(tx, txdump, nspaces+2, fullpath, v);
            break;
        }

        default :
            break;
        }
    }

    /* Done */
    return IB_OK;
}

/**
 * Log transaction details.
 *
 * Extract the address & ports from the transaction & log them.
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] event Event type
 * @param[in] cbdata Callback data (configuration data)
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_tx(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    void                  *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(cbdata != NULL);

    const ib_moddevel_txdump_t *txdump = (const ib_moddevel_txdump_t *)cbdata;
    assert(txdump->event == event);
    ib_status_t rc;

    /* No flags set: do nothing */
    if (!ib_flags_any(txdump->flags, MODDEVEL_TXDUMP_ALL) ) {
        return IB_OK;
    }

    moddevel_txdump(tx, txdump, 0, "[TX %s @ %s]", tx->id, txdump->name);

    /* Basic */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_BASIC) ) {
        char buf[30];
        ib_timeval_t tv;
        IB_CLOCK_TIMEVAL(tv, tx->t.started);
        ib_clock_timestamp(buf, &tv);
        moddevel_txdump(tx, txdump, 2, "Started: %s", buf);
        moddevel_txdump(tx, txdump, 2, "Hostname: %s", tx->hostname);
        moddevel_txdump(tx, txdump, 2, "Effective IP: %s", tx->er_ipstr);
        moddevel_txdump(tx, txdump, 2, "Path: %s", tx->path);
        moddevel_txdump(tx, txdump, 2, "Block Status: %d", tx->block_status);
    }

    /* Connection */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_CONN) ) {
        char buf[30];
        ib_clock_timestamp(buf, &tx->conn->tv_created);
        moddevel_txdump(tx, txdump, 2, "Connection");
        moddevel_txdump(tx, txdump, 4, "Created: %s", buf);
        moddevel_txdump(tx, txdump, 4, "Remote: %s:%d",
                      tx->conn->remote_ipstr, tx->conn->remote_port);
        moddevel_txdump(tx, txdump, 4, "Local: %s:%d",
                      tx->conn->local_ipstr, tx->conn->local_port);
    }

    /* Request Line */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_REQLINE) ) {
        if (tx->request_line == NULL) {
            moddevel_txdump(tx, txdump, 2, "Request line unavailable");
        }
        else {
            moddevel_txdump(tx, txdump, 2, "Request line:");
            moddevel_txdump_bs(tx, txdump, 4,
                             "Raw", tx->request_line->raw, 256);
            moddevel_txdump_bs(tx, txdump, 4,
                             "Method", tx->request_line->method, 32);
            moddevel_txdump_bs(tx, txdump, 4,
                             "URI", tx->request_line->uri, 256);
            moddevel_txdump_bs(tx, txdump, 4,
                             "Protocol", tx->request_line->protocol, 32);
        }
    }

    /* Request Header */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_REQHDR) ) {
        moddevel_txdump_header(tx, txdump, 2,
                               "Request Header", tx->request_header);
    }

    /* Response Line */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_RSPLINE) ) {
        if (tx->response_line == NULL) {
            moddevel_txdump(tx, txdump, 2, "Response line unavailable");
        }
        else {
            moddevel_txdump(tx, txdump, 2, "Response line:");
            moddevel_txdump_bs(tx, txdump, 4,
                               "Raw", tx->response_line->raw, 256);
            moddevel_txdump_bs(tx, txdump, 4,
                               "Protocol", tx->response_line->protocol, 32);
            moddevel_txdump_bs(tx, txdump, 4,
                               "Status", tx->response_line->status, 32);
            moddevel_txdump_bs(tx, txdump, 4,
                               "Message", tx->response_line->msg, 256);
        }
    }

    /* Response Header */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_REQHDR) ) {
        moddevel_txdump_header(tx, txdump, 2,
                               "Response Header", tx->response_header);
    }

    /* Flags */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_FLAGS) ) {
        const ib_strval_t *rec;

        moddevel_txdump(tx, txdump, 2,
                      "Flags: %08lx", (unsigned long)tx->flags);
        for (rec = tx_flags_map; rec->str != NULL; ++rec) {
            bool on = ib_tx_flags_isset(tx, rec->val);
            moddevel_txdump(tx, txdump, 4, "%08lx \"%s\": %s",
                          (unsigned long)rec->val, rec->str, on ? "On" : "Off");
        }
    }

    /* ARGS */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_ARGS) ) {
        const ib_list_t *lst;
        ib_field_t *field;
        moddevel_txdump(tx, txdump, 2, "ARGS:");
        rc = ib_data_get(tx->data, "ARGS", &field);
        if (rc == IB_OK) {
            moddevel_txdump_field(tx, txdump, 4, "ARGS", field, 0);

            rc = ib_field_value(field, ib_ftype_list_out(&lst));
            if ( (rc != IB_OK) || (lst == NULL) ) {
                return rc;
            }
            moddevel_txdump_list(tx, txdump, 4, "ARGS", lst);
        }
        else {
            ib_log_debug_tx(tx, "log_tx: Failed to get ARGS: %s",
                            ib_status_to_string(rc));
        }
    }

    /* All data fields */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_DATA) ) {
        ib_list_t *lst;
        moddevel_txdump(tx, txdump, 2, "Data:");

        /* Build the list */
        rc = ib_list_create(&lst, tx->mp);
        if (rc != IB_OK) {
            ib_log_debug_tx(tx, "log_tx: Failed to create tx list: %s",
                            ib_status_to_string(rc));
            return IB_EUNKNOWN;
        }

        /* Extract the request headers field from the provider instance */
        rc = ib_data_get_all(tx->data, lst);
        if (rc != IB_OK) {
            ib_log_debug_tx(tx, "log_tx: Failed to get all headers: %s",
                            ib_status_to_string(rc));
            return rc;
        }

        /* Log it all */
        rc = moddevel_txdump_list(tx, txdump, 4, "", lst);
        if (rc != IB_OK) {
            ib_log_debug_tx(tx, "log_tx: Failed logging headers: %s",
                            ib_status_to_string(rc));
            return rc;
        }
    }

    /* Done */
    return IB_OK;
}

/**
 * Handle the TxDump directive
 *
 * @param cp Config parser
 * @param directive Directive name
 * @param params List of directive parameters
 * @param cbdata Callback data (from directive registration)
 *
 * usage: TxDump &lt;event&gt; &lt;dest&gt; [&lt;enable&gt]
 * &lt;event&gt; is one of:
 *   "PostProcess", "RequestHeader", "Request", "ResponseHeader", "Response"
 * &lt;dest&gt; is of the form (stderr|stdout|ib|file://[+]&lt;path&gt;)
 * &lt;Enable is of the form &lt;flagname [[+-]&lt;flagname&gt;]&gt;
 * Valid flag names:
 *   Basic: Dump basic TX info
 *   Connection: Dump connection info
 *   ReqLine: Dump request line
 *   ReqHdr: Dump request header
 *   RspLine: Dump response line
 *   RspHdr: Dump response header
 *   Flags: Dump TX flags
 *   Args: Dump request args
 *   Data: Dump TX Data
 *   Default: Default flags (Basic, ReqLine, RspLine)
 *   Headers: Header information (Basic, ReqLine, ReqHdr, RspLine, RspHdr)
 *   All: Dump all TX information
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_handler(
    ib_cfgparser_t  *cp,
    const char      *directive,
    const ib_list_t *params,
    void            *cbdata)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(directive != NULL);
    assert(params != NULL);
    assert(cbdata != NULL);

    ib_status_t                  rc;
    ib_moddevel_txdump_config_t *config = (ib_moddevel_txdump_config_t *)cbdata;
    ib_moddevel_txdump_t         txdump;
    ib_moddevel_txdump_t        *ptxdump;
    const ib_list_node_t        *node;
    const char                  *param;

    /* Initialize the txdump object */
    memset(&txdump, 0, sizeof(txdump));

    /* First parameter is event type */
    node = ib_list_first_const(params);
    if ( (node == NULL) || (node->data == NULL) ) {
        ib_cfg_log_error(cp,
                         "Missing event type for \"%s\" directive", directive);
        return IB_EINVAL;
    }
    param = (const char *)node->data;
    if (strcasecmp(param, "PostProcess") == 0) {
        txdump.event = handle_postprocess_event;
    }
    else if (strcasecmp(param, "RequestHeader") == 0) {
        txdump.event = handle_request_header_event;
    }
    else if (strcasecmp(param, "Request") == 0) {
        txdump.event = handle_request_event;
    }
    else if (strcasecmp(param, "ResponseHeader") == 0) {
        txdump.event = handle_response_header_event;
    }
    else if (strcasecmp(param, "Response") == 0) {
        txdump.event = handle_response_event;
    }
    else {
        ib_cfg_log_error(cp, "Invalid parameter \"%s\" to \"%s\" directive",
                         param, directive);
        return IB_EINVAL;
    }
    txdump.name = ib_state_event_name(txdump.event);

    /* Second parameter is the destination */
    node = ib_list_node_next_const(node);
    if ( (node == NULL) || (node->data == NULL) ) {
        ib_cfg_log_error(cp,
                         "Missing destination for \"%s\" directive", directive);
        return IB_EINVAL;
    }
    param = (const char *)node->data;
    if (strcasecmp(param, "StdOut") == 0) {
        txdump.fp = ib_util_fdup(stdout, "a");
        if (txdump.fp == NULL) {
            return IB_EUNKNOWN;
        }
    }
    else if (strcasecmp(param, "StdErr") == 0) {
        txdump.fp = ib_util_fdup(stderr, "a");
        if (txdump.fp == NULL) {
            return IB_EUNKNOWN;
        }
    }
    else if (strncasecmp(param, "file://", 5) == 0) {
        const char *fname = param+7;
        const char *mode = "w";
        if (*fname == '+') {
            mode = "a";
            ++fname;
        }
        if (*fname == '\0') {
            ib_cfg_log_error(cp,
                             "Missing file name for \"%s\" directive",
                             directive);
            return IB_EINVAL;
        }
        txdump.fp = fopen(fname, mode);
        if (txdump.fp == NULL) {
            ib_cfg_log_error(cp,
                             "Failed to open \"%s\" for \"%s\" directive: %s",
                             fname, directive, strerror(errno));
            return IB_EINVAL;
        }
    }
    else if (strcasecmp(param, "ib") == 0) {
        txdump.level = IB_LOG_DEBUG;
    }
    else {
        ib_cfg_log_error(cp, "Invalid destination \"%s\" for \"%s\" directive",
                         param, directive);
        return IB_EINVAL;
    }

    /* Parse the remainder of the parameters a enables / disables */
    txdump.flags = MODDEVEL_TXDUMP_DEFAULT;
    while( (node = ib_list_node_next_const(node)) != NULL) {
        assert(node->data != NULL);
        param = (const char *)node->data;
        char mod = '\0';
        ib_flags_t flag = 0;
        if ( (*param == '+') || (*param == '-') ) {
            mod = *param;
            ++param;
        }
        if (strcasecmp(param, "default") == 0) {
            flag = MODDEVEL_TXDUMP_DEFAULT;
        }
        else if (strcasecmp(param, "basic") == 0) {
            flag = MODDEVEL_TXDUMP_BASIC;
        }
        else if (strcasecmp(param, "conn") == 0) {
            flag = MODDEVEL_TXDUMP_CONN;
        }
        else if (strcasecmp(param, "reqline") == 0) {
            flag = MODDEVEL_TXDUMP_REQLINE;
        }
        else if (strcasecmp(param, "reqhdr") == 0) {
            flag = MODDEVEL_TXDUMP_REQHDR;
        }
        else if (strcasecmp(param, "rspline") == 0) {
            flag = MODDEVEL_TXDUMP_RSPLINE;
        }
        else if (strcasecmp(param, "rsphdr") == 0) {
            flag = MODDEVEL_TXDUMP_RSPHDR;
        }
        else if (strcasecmp(param, "headers") == 0) {
            flag = MODDEVEL_TXDUMP_HEADERS;
        }
        else if (strcasecmp(param, "flags") == 0) {
            flag = MODDEVEL_TXDUMP_FLAGS;
        }
        else if (strcasecmp(param, "args") == 0) {
            flag = MODDEVEL_TXDUMP_ARGS;
        }
        else if (strcasecmp(param, "data") == 0) {
            flag = MODDEVEL_TXDUMP_DATA;
        }
        else if (strcasecmp(param, "all") == 0 ) {
            flag = MODDEVEL_TXDUMP_ALL;
        }
       else {
            ib_cfg_log_error(cp, "Invalid enable \"%s\" for \"%s\" directive",
                             param, directive);
            return IB_EINVAL;
        }
        switch(mod) {
        case '+':
            ib_flags_set(txdump.flags, flag);
            break;
        case '-':
            ib_flags_clear(txdump.flags, flag);
            break;
        default:
            txdump.flags = flag;
        }
    }

    /* Create the txdump entry */
    ptxdump = ib_mpool_memdup(config->mp, &txdump, sizeof(txdump));
    if (ptxdump == NULL) {
        return IB_EALLOC;
    }

    /* Add it to the list */
    ptxdump->config = config;
    rc = ib_list_push(config->txdump_list, ptxdump);
    if (rc != IB_OK) {
        return rc;
    }

    /* Finally, register the callback */
    rc = ib_hook_tx_register(
        cp->ib,
        txdump.event,
        moddevel_txdump_tx,
        ptxdump
    );

    return rc;
}

static ib_dirmap_init_t moddevel_txdump_directive_map[] = {
    IB_DIRMAP_INIT_LIST(
        "TxDump",
        moddevel_txdump_handler,
        NULL
    ),

    /* signal the end of the list */
    IB_DIRMAP_INIT_LAST
};

ib_status_t ib_moddevel_txdump_init(
    ib_engine_t                  *ib,
    ib_module_t                  *mod,
    ib_mpool_t                   *mp,
    ib_moddevel_txdump_config_t **pconfig)
{
    assert(ib != NULL);
    assert(mod != NULL);
    assert(mp != NULL);
    assert(pconfig != NULL);

    ib_moddevel_txdump_config_t *config;
    ib_status_t                  rc;

    config = ib_mpool_calloc(mp, sizeof(*config), 1);
    if (config == NULL) {
        return IB_EALLOC;
    }
    config->mp = mp;
    rc = ib_list_create(&config->txdump_list, mp);
    if (rc != IB_OK) {
        return rc;
    }

    /* Set the directive callback data to be our configuration object */
    moddevel_txdump_directive_map[0].cbdata_cb = config;
    rc = ib_config_register_directives(ib, moddevel_txdump_directive_map);
    if (rc != IB_OK) {
        return rc;
    }

    *pconfig = config;
    return IB_OK;
}

ib_status_t ib_moddevel_txdump_fini(
    ib_engine_t                 *ib,
    ib_module_t                 *mod,
    ib_moddevel_txdump_config_t *config)
{
    ib_list_node_t *node;

    IB_LIST_LOOP(config->txdump_list, node) {
        ib_moddevel_txdump_t *txdump = node->data;
        if (txdump->fp != NULL) {
            fclose(txdump->fp);
            txdump->fp = NULL;
        }
        ib_hook_tx_unregister(ib, txdump->event, moddevel_txdump_tx);
    }
    return IB_OK;
}
