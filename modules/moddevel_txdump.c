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
#include <ironbee/context.h>
#include <ironbee/engine_state.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/flags.h>
#include <ironbee/list.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/release.h>
#include <ironbee/rule_engine.h>
#include <ironbee/site.h>
#include <ironbee/state_notify.h>
#include <ironbee/string.h>
#include <ironbee/strval.h>
#include <ironbee/util.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

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
#define MODDEVEL_TXDUMP_CONTEXT (1 <<  3) /**< Dump context info? */
#define MODDEVEL_TXDUMP_REQLINE (1 <<  4) /**< Dump request line? */
#define MODDEVEL_TXDUMP_REQHDR  (1 <<  5) /**< Dump request header? */
#define MODDEVEL_TXDUMP_RSPLINE (1 <<  6) /**< Dump response line? */
#define MODDEVEL_TXDUMP_RSPHDR  (1 <<  7) /**< Dump response header? */
#define MODDEVEL_TXDUMP_FLAGS   (1 <<  8) /**< Dump TX flags? */
#define MODDEVEL_TXDUMP_ARGS    (1 <<  9) /**< Dump request args? */
#define MODDEVEL_TXDUMP_DATA    (1 << 10) /**< Dump TX Data? */
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
        MODDEVEL_TXDUMP_CONTEXT |                    \
        MODDEVEL_TXDUMP_CONN    |                    \
        MODDEVEL_TXDUMP_REQLINE |                    \
        MODDEVEL_TXDUMP_REQHDR  |                    \
        MODDEVEL_TXDUMP_RSPLINE |                    \
        MODDEVEL_TXDUMP_RSPHDR  |                    \
        MODDEVEL_TXDUMP_FLAGS   |                    \
        MODDEVEL_TXDUMP_ARGS    |                    \
        MODDEVEL_TXDUMP_DATA                         \
    )

/** Transaction block flags */
#define TX_BLOCKED                                   \
    (                                                \
        IB_TX_BLOCK_ADVISORY |                       \
        IB_TX_BLOCK_PHASE |                          \
        IB_TX_BLOCK_IMMEDIATE                        \
    )

/**
 * Per-TxDump configuration
 */
typedef struct {
    ib_state_event_type_t        event;     /**< Event type */
    ib_state_hook_type_t         hook_type; /**< Hook type */
    const char                  *name;   /**< Event name */
    ib_flags_t                   flags;  /**< Flags defining what to txdump */
    ib_log_level_t               level;  /**< IB Log level */
    FILE                        *fp;     /**< File pointer (or NULL) */
    const char                  *dest;   /**< Copy of the destination string */
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
    IB_STRVAL_PAIR("Blocking Mode", IB_TX_FBLOCKING_MODE),
    IB_STRVAL_PAIR("Allow: Phase", IB_TX_ALLOW_PHASE),
    IB_STRVAL_PAIR("Allow: Request", IB_TX_ALLOW_REQUEST),
    IB_STRVAL_PAIR("Allow: All", IB_TX_ALLOW_ALL),
    IB_STRVAL_PAIR("Post-Process", IB_TX_FPOSTPROCESS),
    IB_STRVAL_PAIR("Inspect Request Header", IB_TX_FINSPECT_REQHDR),
    IB_STRVAL_PAIR("Inspect Request URI", IB_TX_FINSPECT_REQURI),
    IB_STRVAL_PAIR("Inspect Request Parameters", IB_TX_FINSPECT_REQPARAMS),
    IB_STRVAL_PAIR("Inspect Request Body", IB_TX_FINSPECT_REQBODY),
    IB_STRVAL_PAIR("Inspect Response Header", IB_TX_FINSPECT_RSPHDR),
    IB_STRVAL_PAIR("Inspect Response Body", IB_TX_FINSPECT_RSPBODY),

    /* End */
    IB_STRVAL_PAIR_LAST
};

/**
 * Dump an item (variable args version)
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
 * Flush the file stream
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump TxDump data
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_flush(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump)
{
    assert(tx != NULL);
    assert(txdump != NULL);

    if (txdump->fp != NULL) {
        fflush(txdump->fp);
    }
    return IB_OK;
}

/**
 * Escape and format a bytestr
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump Log data
 * @param[in] bs Byte string to log
 * @param[in] quotes Add surrounding quotes?
 * @param[in] maxlen Maximum string length (min = 6)
 *
 * @returns Formatted buffer
 */
static const char *moddevel_format_bs(
    const ib_tx_t              *tx,
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
    rc = ib_util_hex_escape_alloc(tx->mp,
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

    buf = moddevel_format_bs(tx, txdump, bs, true, maxlen);
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
    ib_status_t rc;

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
        rc = ib_field_value(field, ib_ftype_generic_out(&v));
        if (rc == IB_OK) {
            moddevel_txdump(tx, txdump, nspaces, "%s = %p", label, v);
        }
        break;
    }

    case IB_FTYPE_NUM :          /**< Numeric value */
    {
        ib_num_t n;
        rc = ib_field_value(field, ib_ftype_num_out(&n));
        if (rc == IB_OK) {
            moddevel_txdump(tx, txdump, nspaces, "%s = %"PRId64"", label, n);
        }
        break;
    }

    case IB_FTYPE_NULSTR :       /**< NUL terminated string value */
    {
        const char *s;
        rc = ib_field_value(field, ib_ftype_nulstr_out(&s));
        if (rc == IB_OK) {
            if (maxlen > 0) {
                moddevel_txdump(tx, txdump, nspaces,
                                "%s = \"%.*s...\"", label, (int)maxlen, s);
            }
            else {
                moddevel_txdump(tx, txdump, nspaces,
                                "%s = \"%s\"", label, s);
            }
        }
        break;
    }

    case IB_FTYPE_BYTESTR :      /**< Byte string value */
    {
        const ib_bytestr_t *bs;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
        if (rc == IB_OK) {
            moddevel_txdump_bs(tx, txdump, nspaces, label, bs, maxlen);
        }
        break;
    }

    case IB_FTYPE_LIST :         /**< List */
    {
        const ib_list_t *lst;
        rc = ib_field_value(field, ib_ftype_list_out(&lst));
        if (rc == IB_OK) {
            size_t len = IB_LIST_ELEMENTS(lst);
            moddevel_txdump(tx, txdump, nspaces,
                            "%s = [%zd]", label, len);
        }
        break;
    }

    case IB_FTYPE_SBUFFER :
        moddevel_txdump(tx, txdump, nspaces, "%s = sbuffer", label);
        break;

    default:
        moddevel_txdump(tx, txdump, nspaces,
                        "Unknown field type (%d)", field->type);
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
        const char *name  = moddevel_format_bs(tx, txdump,
                                               node->name, false, 24);
        const char *value = moddevel_format_bs(tx, txdump,
                                               node->value, true, 64);
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
 * Dump a list
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
 * Dump a context
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces
 * @param[in] context Context to dump
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_context(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump,
    size_t                      nspaces,
    const ib_context_t         *context)
{
    const ib_site_t          *site = NULL;
    const ib_site_location_t *location = NULL;

    moddevel_txdump(tx, txdump, nspaces, "Context");
    moddevel_txdump(tx, txdump, nspaces+2, "Name = %s",
                    ib_context_full_get(context) );

    ib_context_site_get(context, &site);
    if (site != NULL) {
        moddevel_txdump(tx, txdump, nspaces+2, "Site name = %s", site->name);
        moddevel_txdump(tx, txdump, nspaces+2, "Site ID = %s", site->id_str);
    }
    ib_context_location_get(context, &location);
    if (location != NULL) {
        moddevel_txdump(tx, txdump, nspaces+2, "Location path = %s",
                        location->path);
    }

    return IB_OK;
}

/**
 * Dump a request line
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces
 * @param[in] line Request line to dump
 */
static void moddevel_txdump_reqline(
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump,
    size_t                      nspaces,
    const ib_parsed_req_line_t *line)
{
    assert(tx != NULL);
    assert(txdump != NULL);

    if (line == NULL) {
        moddevel_txdump(tx, txdump, nspaces, "Request line unavailable");
        return;
    }
    moddevel_txdump(tx, txdump, nspaces, "Request line:");
    moddevel_txdump_bs(tx, txdump, nspaces+2, "Raw", line->raw, 256);
    moddevel_txdump_bs(tx, txdump, nspaces+2, "Method", line->method, 32);
    moddevel_txdump_bs(tx, txdump, nspaces+2, "URI", line->uri, 256);
    moddevel_txdump_bs(tx, txdump, nspaces+2, "Protocol", line->protocol, 32);
}

/**
 * Dump a response line
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces
 * @param[in] line Response line to dump
 */
static void moddevel_txdump_rspline(
    const ib_tx_t               *tx,
    const ib_moddevel_txdump_t  *txdump,
    size_t                       nspaces,
    const ib_parsed_resp_line_t *line)
{
    assert(tx != NULL);
    assert(txdump != NULL);

    if (line == NULL) {
        moddevel_txdump(tx, txdump, nspaces, "Response line unavailable");
        return;
    }

    moddevel_txdump(tx, txdump, nspaces, "Response line:");
    moddevel_txdump_bs(tx, txdump, nspaces+2, "Raw", line->raw, 256);
    moddevel_txdump_bs(tx, txdump, nspaces+2, "Protocol", line->protocol, 32);
    moddevel_txdump_bs(tx, txdump, nspaces+2, "Status", line->status, 32);
    moddevel_txdump_bs(tx, txdump, nspaces+2, "Message", line->msg, 256);
}

/**
 * Log transaction details.
 *
 * Extract details from the transaction & dump them
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] txdump TxDump object
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_tx(
    const ib_engine_t          *ib,
    const ib_tx_t              *tx,
    const ib_moddevel_txdump_t *txdump)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(txdump != NULL);

    ib_status_t rc;

    /* No flags set: do nothing */
    if (!ib_flags_any(txdump->flags, MODDEVEL_TXDUMP_ENABLED) ) {
        return IB_OK;
    }

    /* Basic */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_BASIC) ) {
        char         buf[30];

        ib_clock_timestamp(buf, &tx->tv_created);
        moddevel_txdump(tx, txdump, 2, "IronBee Version = %s", IB_VERSION);
        moddevel_txdump(tx, txdump, 2, "IronBee Instance UUID = %s",
                        ib_engine_instance_uuid_str(ib));
        moddevel_txdump(tx, txdump, 2, "Started = %s", buf);
        moddevel_txdump(tx, txdump, 2, "Hostname = %s", tx->hostname);
        moddevel_txdump(tx, txdump, 2, "Effective IP = %s", tx->er_ipstr);
        moddevel_txdump(tx, txdump, 2, "Path = %s", tx->path);
        if (ib_tx_flags_isset(tx, TX_BLOCKED)) {
            moddevel_txdump(tx, txdump, 2, "Block Code = %" PRId64, tx->block_status);
            if (ib_tx_flags_isset(tx, IB_TX_BLOCK_ADVISORY) ) {
                moddevel_txdump(tx, txdump, 2, "Block: Advisory");
            }
            if (ib_tx_flags_isset(tx, IB_TX_BLOCK_PHASE) ) {
                moddevel_txdump(tx, txdump, 2, " Block: Phase");
            }
            if (ib_tx_flags_isset(tx, IB_TX_BLOCK_IMMEDIATE) ) {
                moddevel_txdump(tx, txdump, 2, "Block: Immediate");
            }
        }
    }

    /* Context info */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_CONTEXT) ) {
        moddevel_txdump_context(tx, txdump, 2, tx->ctx);
    }

    /* Connection */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_CONN) ) {
        char buf[30];
        ib_clock_timestamp(buf, &tx->conn->tv_created);
        moddevel_txdump(tx, txdump, 2, "Connection");
        moddevel_txdump(tx, txdump, 4, "Created = %s", buf);
        moddevel_txdump(tx, txdump, 4, "Remote = %s:%d",
                        tx->conn->remote_ipstr, tx->conn->remote_port);
        moddevel_txdump(tx, txdump, 4, "Local = %s:%d",
                        tx->conn->local_ipstr, tx->conn->local_port);
        if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_CONTEXT) ) {
            moddevel_txdump_context(tx, txdump, 4, tx->conn->ctx);
        }
    }

    /* Request Line */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_REQLINE) ) {
        moddevel_txdump_reqline(tx, txdump, 2, tx->request_line);
    }

    /* Request Header */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_REQHDR) ) {
        moddevel_txdump_header(tx, txdump, 2,
                               "Request Header", tx->request_header);
    }

    /* Response Line */
    if (ib_flags_all(txdump->flags, MODDEVEL_TXDUMP_RSPLINE) ) {
        moddevel_txdump_rspline(tx, txdump, 2, tx->response_line);
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
                        "Flags = %010lx", (unsigned long)tx->flags);
        for (rec = tx_flags_map; rec->str != NULL; ++rec) {
            bool on = ib_tx_flags_isset(tx, rec->val);
            moddevel_txdump(tx, txdump, 4, "%010lx \"%s\" = %s",
                            (unsigned long)rec->val, rec->str,
                            on ? "On" : "Off");
        }
    }

    /* If the transaction never started, do nothing */
    if (! ib_tx_flags_isset(tx, IB_TX_FREQ_STARTED) ) {
        return IB_OK;
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
    moddevel_txdump_flush(tx, txdump);
    return IB_OK;
}

/**
 * Handle a TX event for TxDump
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] event Event type
 * @param[in] cbdata Callback data (TxDump object)
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_tx_event(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    void                  *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(cbdata != NULL);

    const ib_moddevel_txdump_t *txdump = (const ib_moddevel_txdump_t *)cbdata;
    ib_status_t                 rc;

    assert(txdump->event == event);

    moddevel_txdump(tx, txdump, 0, "[TX %s @ %s]", tx->id, txdump->name);

    rc = moddevel_txdump_tx(ib, tx, txdump);
    moddevel_txdump_flush(tx, txdump);
    return rc;
}

/**
 * Handle a Request Line event for TxDump
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] event Event type
 * @param[in] line Parsed request line
 * @param[in] cbdata Callback data (TxDump object)
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_reqline_event(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    ib_parsed_req_line_t  *line,
    void                  *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(cbdata != NULL);

    const ib_moddevel_txdump_t *txdump = (const ib_moddevel_txdump_t *)cbdata;

    assert(txdump->event == event);

    moddevel_txdump(tx, txdump, 0, "[TX %s @ %s]", tx->id, txdump->name);
    moddevel_txdump_reqline(tx, txdump, 2, line);
    moddevel_txdump_flush(tx, txdump);
    return IB_OK;
}

/**
 * Handle a TX event for TxDump
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] event Event type
 * @param[in] line Parsed response line
 * @param[in] cbdata Callback data (TxDump object)
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_rspline_event(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_event_type_t  event,
    ib_parsed_resp_line_t *line,
    void                  *cbdata)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(cbdata != NULL);

    const ib_moddevel_txdump_t *txdump = (const ib_moddevel_txdump_t *)cbdata;

    assert(txdump->event == event);

    moddevel_txdump(tx, txdump, 0, "[TX %s @ %s]", tx->id, txdump->name);
    moddevel_txdump_rspline(tx, txdump, 2, line);
    moddevel_txdump_flush(tx, txdump);
    return IB_OK;
}

/**
 * Execute function for the "TxDump" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data C-style string to log
 * @param[in] flags Action instance flags
 * @param[in] cbdata Callback data (TxDump object)
 *
 * @returns Status code
 */
static ib_status_t moddevel_txdump_act_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    ib_flags_t            flags,
    void                 *cbdata)
{
    const ib_moddevel_txdump_t *txdump = (const ib_moddevel_txdump_t *)data;
    ib_status_t                 rc;
    const ib_tx_t              *tx = rule_exec->tx;

    moddevel_txdump(tx, txdump, 0, "[TX %s @ Rule %s]",
                    tx->id, ib_rule_id(rule_exec->rule));

    rc = moddevel_txdump_tx(rule_exec->ib, tx, txdump);
    moddevel_txdump_flush(tx, txdump);
    return rc;
}

/**
 * TxDump event data
 */
typedef struct {
    ib_state_event_type_t event;      /**< Event type */
    ib_state_hook_type_t  hook_type;  /**< Hook type */
} txdump_event_t;

/**
 * TxDump event parsing mapping data
 */
typedef struct {
    const char           *str;        /** String< "key" */
    const txdump_event_t  data;       /** Data portion */
} txdump_strval_event_t;

static IB_STRVAL_DATA_MAP(txdump_strval_event_t, event_map) = {
    IB_STRVAL_DATA_PAIR("PostProcess",
                        handle_postprocess_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("Logging",
                        handle_logging_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("RequestStart",
                        request_started_event,
                        IB_STATE_HOOK_REQLINE),
    IB_STRVAL_DATA_PAIR("RequestHeader",
                        handle_request_header_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("Request",
                        handle_request_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("ResponseStart",
                        response_started_event,
                        IB_STATE_HOOK_RESPLINE),
    IB_STRVAL_DATA_PAIR("ResponseHeader",
                        handle_response_header_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("Response",
                        handle_response_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("TxStarted",
                        tx_started_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("TxContext",
                        handle_context_tx_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("TxProcess",
                        tx_process_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("TxFinished",
                        tx_finished_event,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR_LAST((ib_state_event_type_t)-1,
                             (ib_state_hook_type_t)-1),
};

/**
 * Parse the event for a TxDump directive
 *
 * @param[in] ib IronBee engine
 * @param[in] label Label for logging
 * @param[in] param Parameter string
 * @param[in,out] txdump TxDump object to set the event in
 *
 * @returns: Status code
 */
static ib_status_t moddevel_txdump_parse_event(
    ib_engine_t            *ib,
    const char             *label,
    const char             *param,
    ib_moddevel_txdump_t   *txdump)
{
    assert(ib != NULL);
    assert(label != NULL);
    assert(param != NULL);
    assert(txdump != NULL);

    ib_status_t           rc;
    const txdump_event_t *value;

    rc = IB_STRVAL_DATA_LOOKUP(event_map, txdump_strval_event_t, param, &value);
    if (rc != IB_OK) {
        ib_log_error(ib, "Invalid event parameter \"%s\" for %s", param, label);
        return rc;
    }

    txdump->event = value->event;
    txdump->hook_type = value->hook_type;
    txdump->name = ib_state_event_name(txdump->event);
    return IB_OK;
}

/**
 * Parse the destination for a TxDump directive or action
 *
 * @param[in] ib IronBee engine
 * @param[in] label Label for logging
 * @param[in] param Parameter string
 * @param[in,out] txdump TxDump object to set the event in
 *
 * @returns: Status code
 */
static ib_status_t moddevel_txdump_parse_dest(
    ib_engine_t            *ib,
    const char             *label,
    const char             *param,
    ib_moddevel_txdump_t   *txdump)
{
    assert(ib != NULL);
    assert(label != NULL);
    assert(param != NULL);
    assert(txdump != NULL);

    txdump->dest = ib_mpool_strdup(ib_engine_pool_main_get(ib), param);
    if (strcasecmp(param, "StdOut") == 0) {
        txdump->fp = ib_util_fdup(stdout, "a");
        if (txdump->fp == NULL) {
            return IB_EUNKNOWN;
        }
    }
    else if (strcasecmp(param, "StdErr") == 0) {
        txdump->fp = ib_util_fdup(stderr, "a");
        if (txdump->fp == NULL) {
            return IB_EUNKNOWN;
        }
    }
    else if (strncasecmp(param, "file://", 7) == 0) {
        const char *mode;
        char       *fname;
        char       *end;
        size_t      len;

        /* Make a copy of the file name */
        fname = ib_mpool_strdup(txdump->config->mp, param + 7);
        if (fname == NULL) {
            return IB_EALLOC;
        }
        len = strlen(fname);
        if (len <= 1) {
            ib_log_error(ib, "Missing file name for %s", label);
            return IB_EINVAL;
        }

        /* If the last character is a '+', open in append mode */
        end = fname + len - 1;
        if (*end == '+') {
            mode = "a";
            *end = '\0';
        }
        else {
            mode = "w";
        }
        txdump->fp = fopen(fname, mode);
        if (txdump->fp == NULL) {
            ib_log_error(ib, "Failed to open \"%s\" for %s: %s",
                         fname, label, strerror(errno));
            return IB_EINVAL;
        }
    }
    else if (strcasecmp(param, "ib") == 0) {
        txdump->level = IB_LOG_DEBUG;
    }
    else {
        ib_log_error(ib, "Invalid destination \"%s\" for %s", param, label);
        return IB_EINVAL;
    }
    return IB_OK;
}

static IB_STRVAL_MAP(flags_map) = {
    IB_STRVAL_PAIR("default", MODDEVEL_TXDUMP_DEFAULT),
    IB_STRVAL_PAIR("basic", MODDEVEL_TXDUMP_BASIC),
    IB_STRVAL_PAIR("context", MODDEVEL_TXDUMP_CONTEXT),
    IB_STRVAL_PAIR("connection", MODDEVEL_TXDUMP_CONN),
    IB_STRVAL_PAIR("reqline", MODDEVEL_TXDUMP_REQLINE),
    IB_STRVAL_PAIR("reqhdr", MODDEVEL_TXDUMP_REQHDR),
    IB_STRVAL_PAIR("rspline", MODDEVEL_TXDUMP_RSPLINE),
    IB_STRVAL_PAIR("rsphdr", MODDEVEL_TXDUMP_RSPHDR),
    IB_STRVAL_PAIR("headers", MODDEVEL_TXDUMP_HEADERS),
    IB_STRVAL_PAIR("flags", MODDEVEL_TXDUMP_FLAGS),
    IB_STRVAL_PAIR("args", MODDEVEL_TXDUMP_ARGS),
    IB_STRVAL_PAIR("data", MODDEVEL_TXDUMP_DATA),
    IB_STRVAL_PAIR("all", MODDEVEL_TXDUMP_ALL),
    IB_STRVAL_PAIR_LAST,
};

/**
 * Handle the TxDump directive
 *
 * @param[in] cp Config parser
 * @param[in] directive Directive name
 * @param[in] params List of directive parameters
 * @param[in] cbdata Callback data (from directive registration)
 *
 * @returns Status code
 *
 * @par usage: TxDump @<event@> @<dest@> [@<enable@>]
 * @par @<event@> is one of:
 *  - <tt>TxStarted</tt>
 *  - <tt>TxProcess</tt>
 *  - <tt>TxContext</tt>
 *  - <tt>RequestStart</tt>
 *  - <tt>RequestHeader</tt>
 *  - <tt>Request</tt>
 *  - <tt>ResponseStart</tt>
 *  - <tt>ResponseHeader</tt>
 *  - <tt>Response</tt>
 *  - <tt>TxFinished</tt>
 *  - <tt>Logging</tt>
 *  - <tt>PostProcess</tt>
 * @par @<dest@> is of the form (stderr|stdout|ib|file://@<path@>[+])
 *  - The '+' flag means append
 * @par @<enable@> is of the form @<flag@> [[+-]@<flag@>]>
 * @par @<flag@> is one of:
 *  - <tt>Basic</tt>: Dump basic TX info
 *  - <tt>Context</tt>: Dump context info
 *  - <tt>Connection</tt>: Dump connection info
 *  - <tt>ReqLine</tt>: Dump request line
 *  - <tt>ReqHdr</tt>: Dump request header
 *  - <tt>RspLine</tt>: Dump response line
 *  - <tt>RspHdr</tt>: Dump response header
 *  - <tt>Flags</tt>: Dump TX flags
 *  - <tt>Args</tt>: Dump request args
 *  - <tt>Data</tt>: Dump TX Data
 *  - <tt>Default</tt>: Default flags (Basic, ReqLine, RspLine)
 *  - <tt>Headers</tt>: All headers (Basic, ReqLine, ReqHdr, RspLine, RspHdr)
 *  - <tt>All</tt>: Dump all TX information
 *
 * @par Examples:
 *  - <tt>TxDump TxContext ib Basic +Context</tt>
 *  - <tt>TxDump PostProcess file:///tmp/tx.txt All</tt>
 *  - <tt>TxDump Logging file:///var/log/ib/all.txt+ All</tt>
 *  - <tt>TxDump PostProcess StdOut All</tt>
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
    static const char           *label = "TxDump directive";
    int                          flagno = 0;
    ib_flags_t                   flags = 0;
    ib_flags_t                   mask = 0;

    /* Initialize the txdump object */
    memset(&txdump, 0, sizeof(txdump));
    txdump.config = config;

    /* First parameter is event type */
    node = ib_list_first_const(params);
    if ( (node == NULL) || (node->data == NULL) ) {
        ib_cfg_log_error(cp,
                         "Missing event type for %s", label);
        return IB_EINVAL;
    }
    param = (const char *)node->data;
    rc = moddevel_txdump_parse_event(cp->ib, label, param, &txdump);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing event for %s", label);
        return rc;
    }

    /* Second parameter is the destination */
    node = ib_list_node_next_const(node);
    if ( (node == NULL) || (node->data == NULL) ) {
        ib_cfg_log_error(cp, "Missing destination for %s", label);
        return IB_EINVAL;
    }
    param = (const char *)node->data;
    rc = moddevel_txdump_parse_dest(cp->ib, label, param, &txdump);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing destination for %s", label);
        return rc;
    }

    /* Parse the remainder of the parameters a enables / disables */
    while( (node = ib_list_node_next_const(node)) != NULL) {
        assert(node->data != NULL);
        param = (const char *)node->data;
        rc = ib_flags_string(flags_map, param, flagno++, &flags, &mask);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Error parsing enable for %s", label);
            return rc;
        }
    }
    txdump.flags = ib_flags_merge(MODDEVEL_TXDUMP_DEFAULT, flags, mask);
    if (txdump.flags != 0) {
        txdump.flags |= MODDEVEL_TXDUMP_ENABLED;
    }

    /* Create the txdump entry */
    ptxdump = ib_mpool_memdup(config->mp, &txdump, sizeof(txdump));
    if (ptxdump == NULL) {
        ib_cfg_log_error(cp, "Error allocating TxDump object for %s", label);
        return IB_EALLOC;
    }

    /* Add it to the list */
    rc = ib_list_push(config->txdump_list, ptxdump);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error adding TxDump object to list for %s",
                         label);
        return rc;
    }

    /* Finally, register the callback */
    switch(txdump.hook_type) {
    case IB_STATE_HOOK_TX:
        rc = ib_hook_tx_register(
            cp->ib,
            txdump.event,
            moddevel_txdump_tx_event,
            ptxdump);
        break;
    case IB_STATE_HOOK_REQLINE:
        rc = ib_hook_parsed_req_line_register(
            cp->ib,
            txdump.event,
            moddevel_txdump_reqline_event,
            ptxdump);
        break;
    case IB_STATE_HOOK_RESPLINE:
        rc = ib_hook_parsed_resp_line_register(
            cp->ib,
            txdump.event,
            moddevel_txdump_rspline_event,
            ptxdump);
        break;
    default:
        ib_cfg_log_error(cp, "No handler for hook type %d", txdump.hook_type);
        return IB_EINVAL;
    }
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to register handler for hook type %d",
                         txdump.hook_type);
    }

    return rc;
}

/**
 * Create function for the TxDump action.
 *
 * @param[in] ib IronBee engine (unused)
 * @param[in] parameters Constant parameters from the rule definition
 * @param[in,out] inst Action instance
 * @param[in] cbdata Callback data (unused)
 *
 * @returns Status code
 *
 * @par usage: TxDump:@<dest@>,[@<enable@>]
 * @par @<dest@> is of the form (stderr|stdout|ib|file://@<path@>[+])
 *  - The '+' flag means append
 * @par @<enable@> is of the form @<flag@> [[+-]@<flag@>]>
 * @par @<flag@> is one of:
 *  - <tt>Basic</tt>: Dump basic TX info
 *  - <tt>Context</tt>: Dump context info
 *  - <tt>Connection</tt>: Dump connection info
 *  - <tt>ReqLine</tt>: Dump request line
 *  - <tt>ReqHdr</tt>: Dump request header
 *  - <tt>RspLine</tt>: Dump response line
 *  - <tt>RspHdr</tt>: Dump response header
 *  - <tt>Flags</tt>: Dump TX flags
 *  - <tt>Args</tt>: Dump request args
 *  - <tt>Data</tt>: Dump TX Data
 *  - <tt>Default</tt>: Default flags (Basic, ReqLine, RspLine)
 *  - <tt>Headers</tt>: All headers (Basic, ReqLine, ReqHdr, RspLine, RspHdr)
 *  - <tt>All</tt>: Dump all TX information
 *
 * @par Examples:
 *  - <tt>TxDump:ib,Basic,+Context</tt>
 *  - <tt>TxDump:file:///tmp/tx.txt,All</tt>
 *  - <tt>TxDump:file:///var/log/ib/all.txt+,All</tt>
 *  - <tt>TxDump:StdOut,All</tt>
 */
static ib_status_t moddevel_txdump_act_create(ib_engine_t *ib,
                                              const char *parameters,
                                              ib_action_inst_t *inst,
                                              void *cbdata)
{
    assert(ib != NULL);
    assert(inst != NULL);

    ib_status_t           rc;
    ib_moddevel_txdump_t  txdump;
    ib_moddevel_txdump_t *ptxdump;
    char                 *pcopy;
    char                 *param;
    static const char    *label = "TxDump action";
    int                   flagno = 0;
    ib_flags_t            flags = 0;
    ib_flags_t            mask = 0;
    ib_mpool_t           *mp = ib_engine_pool_main_get(ib);

    assert(mp != NULL);

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    /* Initialize the txdump object */
    memset(&txdump, 0, sizeof(txdump));
    txdump.name = "Action";

    /* Make a copy of the parameters that we can use for strtok() */
    pcopy = ib_mpool_strdup(ib_engine_pool_temp_get(ib), parameters);
    if (pcopy == NULL) {
        return IB_EALLOC;
    }

    /* First parameter is the destination */
    param = strtok(pcopy, ",");
    if (param == NULL) {
        ib_log_error(ib, "Missing destination for %s", label);
        return IB_EINVAL;
    }
    rc = moddevel_txdump_parse_dest(ib, label, param, &txdump);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error parsing destination for %s", label);
        return rc;
    }

    /* Parse the remainder of the parameters a enables / disables */
    while ((param = strtok(NULL, ",")) != NULL) {
        rc = ib_flags_string(flags_map, param, flagno++, &flags, &mask);
        if (rc != IB_OK) {
            ib_log_error(ib, "Error parsing enable for %s", label);
            return rc;
        }
    }
    txdump.flags = ib_flags_merge(MODDEVEL_TXDUMP_DEFAULT, flags, mask);
    if (txdump.flags != 0) {
        txdump.flags |= MODDEVEL_TXDUMP_ENABLED;
    }

    /* Create the txdump entry */
    ptxdump = ib_mpool_memdup(mp, &txdump, sizeof(txdump));
    if (ptxdump == NULL) {
        ib_log_error(ib, "Error allocating TxDump object for %s", label);
        return IB_EALLOC;
    }

    /* Done */
    inst->data = ptxdump;
    return IB_OK;
}

static ib_dirmap_init_t moddevel_txdump_directive_map[] = {
    IB_DIRMAP_INIT_LIST(
        "TxDump",
        moddevel_txdump_handler,
        NULL                        /* Filled in by the init function */
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

    /* Register the TxDump action */
    rc = ib_action_register(ib,
                            "TxDump",
                            IB_ACT_FLAG_NONE,
                            moddevel_txdump_act_create, NULL,
                            NULL, NULL, /* no destroy function */
                            moddevel_txdump_act_execute, NULL);
    if (rc != IB_OK) {
        return rc;
    }

    *pconfig = config;
    return IB_OK;
}

ib_status_t ib_moddevel_txdump_cleanup(
    ib_engine_t                 *ib,
    ib_module_t                 *mod,
    ib_moddevel_txdump_config_t *config)
{
    /* Do nothing */
    return IB_OK;
}

ib_status_t ib_moddevel_txdump_fini(
    ib_engine_t                 *ib,
    ib_module_t                 *mod)
{
    /* Do nothing */
    return IB_OK;
}
