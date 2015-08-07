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
 * @brief IronBee --- TxDump module
 *
 * This module defines the <tt>TxDump</tt> directive, and the <tt>txDump</tt>
 * action.
 *
 * @par The TxDump directive
 *
 * <tt>usage: TxDump @<state@> @<dest@> [@<enable@>]</tt>
 * - @<state@> is one of:
 *  - <tt>TxStarted</tt>
 *  - <tt>RequestStarted</tt>
 *  - <tt>RequestHeaderProcess</tt>
 *  - <tt>TxContext</tt>
 *  - <tt>RequestHeaderFinished</tt>
 *  - <tt>RequestHeader</tt>
 *  - <tt>RequestFinished</tt>
 *  - <tt>Request</tt>
 *  - <tt>TxProcess</tt>
 *  - <tt>ResponseStarted</tt>
 *  - <tt>ResponseHeaderFinished</tt>
 *  - <tt>ResponseHeader</tt>
 *  - <tt>ResponseFinished</tt>
 *  - <tt>Response</tt>
 *  - <tt>LogEvent</tt>
 *  - <tt>PostProcess</tt>
 *  - <tt>Logging</tt>
 *  - <tt>TxFinished</tt>
 * - @<dest@> is of the form (stderr|stdout|ib|file://@<path@>[+])
 *  - The '+' flag means append (file only)
 * - @<enable@> is of the form @<flag@> [[+-]@<flag@>]>
 *  - @<flag@> is one of:
 *   - <tt>Basic</tt>: Dump basic TX info
 *   - <tt>Context</tt>: Dump context info
 *   - <tt>Connection</tt>: Dump connection info
 *   - <tt>ReqLine</tt>: Dump request line
 *   - <tt>ReqHdr</tt>: Dump request header
 *   - <tt>ResLine</tt>: Dump response line
 *   - <tt>ResHdr</tt>: Dump response header
 *   - <tt>Flags</tt>: Dump TX flags
 *   - <tt>Args</tt>: Dump request args
 *   - <tt>Vars</tt>: Dump TX Vars
 *   - <tt>Default</tt>: Default flags (Basic, ReqLine, ResLine)
 *   - <tt>Headers</tt>: All headers (Basic, ReqLine, ReqHdr, ResLine, ResHdr)
 *   - <tt>All</tt>: Dump all TX information
 *
 * @par TxDump directive Examples:
 *
 * - <tt>TxDump TxContext ib Basic +Context</tt>
 * - <tt>TxDump PostProcess file:///tmp/tx.txt All</tt>
 * - <tt>TxDump Logging file:///var/log/ib/all.txt+ All</tt>
 * - <tt>TxDump PostProcess StdOut All</tt>
 *
 * @par The txDump action
 *
 * <tt>usage: txDump:@<dest@>,[@<enable@>]</tt>
 * - @<dest@> is of the form (stderr|stdout|ib|file://@<path@>[+])
 *  - The '+' flag means append (file only)
 * - @<enable@> is of the form @<flag@>[,[+-]@<flag@>]>
 *  - @<flag@> is one of:
 *   - <tt>Basic</tt>: Dump basic TX info
 *   - <tt>Context</tt>: Dump context info
 *   - <tt>Connection</tt>: Dump connection info
 *   - <tt>ReqLine</tt>: Dump request line
 *   - <tt>ReqHdr</tt>: Dump request header
 *   - <tt>ResLine</tt>: Dump response line
 *   - <tt>ResHdr</tt>: Dump response header
 *   - <tt>Flags</tt>: Dump TX flags
 *   - <tt>Args</tt>: Dump request args
 *   - <tt>Vars</tt>: Dump TX Vars
 *   - <tt>Default</tt>: Default flags (Basic, ReqLine, ResLine)
 *   - <tt>Headers</tt>: All headers (Basic, ReqLine, ReqHdr, ResLine, ResHdr)
 *   - <tt>All</tt>: Dump all TX information
 *
 * @par txDump action Examples:
 *
 * - <tt>rule x \@eq 4 id:1 txDump:ib,Basic,+Context</tt>
 * - <tt>rule y \@eq 1 id:2 txDump:file:///tmp/tx.txt,All</tt>
 * - <tt>rule z \@eq 2 id:3 txDump:file:///var/log/ib/all.txt+,All</tt>
 * - <tt>rule n \@eq 5 id:4 txDump:StdOut,All</tt>
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include <ironbee/bytestr.h>
#include <ironbee/cfgmap.h>
#include <ironbee/context.h>
#include <ironbee/engine_state.h>
#include <ironbee/escape.h>
#include <ironbee/field.h>
#include <ironbee/flags.h>
#include <ironbee/list.h>
#include <ironbee/mm.h>
#include <ironbee/module.h>
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
#include <sys/time.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        txdump
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Declare the public module symbol. */
IB_MODULE_DECLARE();

/**
 * Several max constants
 */
static const size_t MAX_LEADING_SPACES = 16;/**< Max # of leading spaces */
static const size_t MAX_METHOD = 32;        /**< Max HTTP method */
static const size_t MAX_PROTOCOL = 32;      /**< Max HTTP protocol */
static const size_t MAX_STATUS = 32;        /**< Max response status */
static const size_t MAX_PATH_ELEMENT = 32;  /**< Max size of a path element */
static const size_t MAX_FIELD_NAME = 48;    /**< Max field name for printing */
static const size_t MAX_FIELD_SIZE = 256;   /**< Max field value for printing */
static const size_t MAX_BS_LEN = 1024;      /**< Max escaped byte string */
static const size_t MIN_BS_LEN = 5;         /**< Min escaped byte string */

/**
 * TxDump bytestring format result flags
 */
#define TXDUMP_BS_NULL    (1 <<  0) /* NULL bytestring? */
#define TXDUMP_BS_CROPPED (1 <<  1) /* Final string cropped? */
#define TXDUMP_BS_ESCAPED (1 <<  2) /* Was escaping required? */

/**
 * Flag -> string format.
 *
 * @note If you add to this list, make sure that FLAGBUF_SIZE (below)
 * is sufficiently large to hold the maximum combination of these.
 */
static IB_STRVAL_MAP(bytestring_flags_map) = {
    IB_STRVAL_PAIR("NULL", TXDUMP_BS_NULL),
    IB_STRVAL_PAIR("CROPPED", TXDUMP_BS_CROPPED),
    IB_STRVAL_PAIR("JSON", TXDUMP_BS_ESCAPED),
    IB_STRVAL_PAIR_LAST,
};
static const size_t FLAGBUF_SIZE = 24;  /* See note above */

/**
 * TxDump quote mode
 */
enum txdump_qmode_t {
    QUOTE_ALWAYS,         /**< Always quote the result string */
    QUOTE_NEVER,          /**< Never quote the result string */
    QUOTE_AUTO,           /**< Quote only if escaping required */
};
typedef enum txdump_qmode_t txdump_qmode_t;

/**
 * TxDump enable flags
 */
#define TXDUMP_ENABLED (1 <<  0) /* Enabled? */
#define TXDUMP_BASIC   (1 <<  1) /* Dump basic TX info? */
#define TXDUMP_CONN    (1 <<  2) /* Dump connection info? */
#define TXDUMP_CONTEXT (1 <<  3) /* Dump context info? */
#define TXDUMP_REQLINE (1 <<  4) /* Dump request line? */
#define TXDUMP_REQHDR  (1 <<  5) /* Dump request header? */
#define TXDUMP_RESLINE (1 <<  6) /* Dump response line? */
#define TXDUMP_RESHDR  (1 <<  7) /* Dump response header? */
#define TXDUMP_FLAGS   (1 <<  8) /* Dump TX flags? */
#define TXDUMP_ARGS    (1 <<  9) /* Dump request args? */
#define TXDUMP_VARS    (1 << 10) /* Dump TX vars? */
/* Default enable flags */
#define TXDUMP_DEFAULT                      \
    (                                       \
        TXDUMP_ENABLED |                    \
        TXDUMP_BASIC   |                    \
        TXDUMP_REQLINE |                    \
        TXDUMP_RESLINE                      \
    )
/* Headers enable flags */
#define TXDUMP_HEADERS                      \
    (                                       \
        TXDUMP_ENABLED |                    \
        TXDUMP_BASIC   |                    \
        TXDUMP_REQLINE |                    \
        TXDUMP_REQHDR  |                    \
        TXDUMP_RESLINE |                    \
        TXDUMP_RESHDR                       \
    )
/* All enable flags */
#define TXDUMP_ALL                          \
    (                                       \
        TXDUMP_ENABLED |                    \
        TXDUMP_BASIC   |                    \
        TXDUMP_CONTEXT |                    \
        TXDUMP_CONN    |                    \
        TXDUMP_REQLINE |                    \
        TXDUMP_REQHDR  |                    \
        TXDUMP_RESLINE |                    \
        TXDUMP_RESHDR  |                    \
        TXDUMP_FLAGS   |                    \
        TXDUMP_ARGS    |                    \
        TXDUMP_VARS                         \
    )

/** Transaction block flags */
#define TX_BLOCKED                                    \
    (                                                 \
        IB_TX_FBLOCK_ADVISORY |                       \
        IB_TX_FBLOCK_PHASE |                          \
        IB_TX_FBLOCK_IMMEDIATE                        \
    )

/** TxDump configuration */
typedef struct txdump_config_t txdump_config_t;

/**
 * Per-TxDump directive configuration
 */
struct txdump_t {
    ib_state_t             state;     /**< State */
    ib_state_hook_type_t   hook_type; /**< Hook type */
    const char            *name;      /**< Event name */
    ib_flags_t             flags;     /**< Flags defining what to txdump */
    ib_logger_level_t      level;     /**< IB Log level */
    FILE                  *fp;        /**< File pointer (or NULL) */
    const char            *dest;      /**< Copy of the destination string */
    txdump_config_t       *config;    /**< TxDump configuration data */
    const ib_module_t     *module;    /**< Pointer to module object */
};
typedef struct txdump_t txdump_t;

/**
 * TxDump module instance data
 */
struct txdump_moddata_t {
    ib_list_t *fp_list;               /**< List of all file pointers */
};
typedef struct txdump_moddata_t txdump_moddata_t;

/**
 * TxDump configuration
 */
struct txdump_config_t {
    ib_list_t *txdump_list;           /**< List of @ref txdump_t pointers */
};

/**
 * TxDump global configuration
 */
static txdump_config_t txdump_config = {
    .txdump_list = NULL
};


/**
 * Dump an item (variable args version)
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump Log parameters
 * @param[in] nspaces Number of leading spaces (max = MAX_LEADING_SPACES)
 * @param[in] fmt printf-style format string
 * @param[in] ap Variable args list
 */
static void txdump_va(
    const ib_tx_t  *tx,
    const txdump_t *txdump,
    size_t          nspaces,
    const char     *fmt,
    va_list         ap
) VPRINTF_ATTRIBUTE(4);
static void txdump_va(
    const ib_tx_t  *tx,
    const txdump_t *txdump,
    size_t          nspaces,
    const char     *fmt,
    va_list         ap
)
{
    assert(tx != NULL);
    assert(txdump != NULL);
    assert(fmt != NULL);

    /* Limit # of leading spaces */
    if (nspaces > MAX_LEADING_SPACES) {
        nspaces = MAX_LEADING_SPACES;
    }

    /* Prefix fmt with spaces if required. Replaces fmt. */
    if (nspaces != 0) {
        char *fmttmp = ib_mm_alloc(tx->mm, strlen(fmt) + nspaces + 1);
        if (fmttmp != NULL) {
            memset(fmttmp, ' ', nspaces); /* Write prefix. */
            strcpy(fmttmp+nspaces, fmt);  /* Copy fmt string. */
            fmt = fmttmp;                 /* Replace fmt. */
        }
    }

    if (txdump->fp != NULL) {
        vfprintf(txdump->fp, fmt, ap);
        fputs("\n", txdump->fp);
    }
    else {
        ib_log_tx_vex(tx, txdump->level, NULL, NULL, 0, fmt, ap);
    }
}

/**
 * Dump an item
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump Log data
 * @param[in] nspaces Number of leading spaces to insert (max = MAX_LEADING_SPACES)
 * @param[in] fmt printf-style format string
 */
static void txdump_v(
    const ib_tx_t  *tx,
    const txdump_t *txdump,
    size_t          nspaces,
    const char     *fmt, ...
) PRINTF_ATTRIBUTE(4, 5);
static void txdump_v(
    const ib_tx_t  *tx,
    const txdump_t *txdump,
    size_t          nspaces,
    const char     *fmt, ...
)
{
    assert(tx != NULL);
    assert(txdump != NULL);
    assert(fmt != NULL);

    va_list ap;

    va_start(ap, fmt);
    txdump_va(tx, txdump, nspaces, fmt, ap);
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
static ib_status_t txdump_flush(
    const ib_tx_t  *tx,
    const txdump_t *txdump)
{
    assert(tx != NULL);
    assert(txdump != NULL);

    if (txdump->fp != NULL) {
        fflush(txdump->fp);
    }
    return IB_OK;
}

/**
 * Get string of bytestring flags
 *
 * @param[in] mm Memory manager to use for allocations
 * @param[in] rc Error status
 * @param[in] flags Bytestring format flags
 *
 * @returns Pointer to flags string
 */
static const char *format_flags(
    ib_mm_t      mm,
    ib_status_t  rc,
    ib_flags_t   flags
)
{
    size_t             count = 0;
    const ib_strval_t *rec;
    char              *buf;

    /* If there's nothing to report, do nothing */
    if ( (rc == IB_OK) && (flags == 0) ) {
        return "";
    }
    buf = ib_mm_alloc(mm, FLAGBUF_SIZE+1);
    if (buf == NULL) {
        return " [?]";
    }
    if (rc != IB_OK) {
        snprintf(buf, FLAGBUF_SIZE, " [%s]", ib_status_to_string(rc));
        return buf;
    }

    /* Build up the string */
    IB_STRVAL_LOOP(bytestring_flags_map, rec) {
        if (ib_flags_all(flags, rec->val)) {
            if (count == 0) {
                strcpy(buf, " [");
            }
            else {
                strcat(buf, ",");
            }

            /* Copy in the string itself */
            strcat(buf, rec->str);
            ++count;
        }
    }

    /* Terminate the buffer */
    if (count != 0) {
        strcat(buf, "]");
    }
    else {
        *buf = '\0';
    }
    return buf;
}

/**
 * Escape and format a bytestring, extended version
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump Log data
 * @param[in] bsptr Pointer to bytestring data (can be NULL)
 * @param[in] bslen Length of data in @a bsptr
 * @param[in] qmode Quoting mode
 * @param[in] maxlen Maximum string length (clipped at MIN_BS_LEN to MAX_BS_LEN)
 * @param[out] pflagbuf Pointer to buffer for flags (or NULL)
 * @param[out] pescaped Pointer to escaped buffer
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure.
 */
static ib_status_t format_bs_ex(
    const ib_tx_t   *tx,
    const txdump_t  *txdump,
    const uint8_t   *bsptr,
    size_t           bslen,
    txdump_qmode_t   qmode,
    size_t           maxlen,
    const char     **pflagbuf,
    const char     **pescaped
)
{
    assert(tx != NULL);
    assert(txdump != NULL);
    assert(pescaped != NULL);

    ib_status_t  rc = IB_OK;
    char        *buf;
    size_t       slen;
    size_t       offset;
    size_t       size;
    const char  *empty = (qmode == QUOTE_ALWAYS) ? "\"\"" : "";
    bool         is_printable = true;
    bool         crop = false;
    bool         quotes;
    ib_flags_t   flags = 0;

    /* If the data is NULL, no need to escape */
    if (bsptr == NULL) {
        flags |= TXDUMP_BS_NULL;
        *pescaped = empty;
        goto done;
    }

    /* Make sure that maxlen is sane */
    if (maxlen > MAX_BS_LEN) {
        maxlen = MAX_BS_LEN;
    }
    else if (maxlen < MIN_BS_LEN) {
        maxlen = MIN_BS_LEN;
    }

    /* See if all of the characters are printable */
    for (offset = 0;  offset < bslen;  ++offset) {
        uint8_t c = *(bsptr + offset);
        if (iscntrl(c) ||  !isprint(c) ) {
            is_printable = false;
            break;
        }
    }

    /* Handle the case in which is all printable */
    if (is_printable) {
        quotes = (qmode == QUOTE_ALWAYS);
        crop = bslen > maxlen;
        slen = (crop ? maxlen : bslen);
        size = slen + (quotes ? 2 : 0);

        /* Allocate buffer, Leave room for \0 */
        buf = ib_mm_alloc(tx->mm, size+1);
        if (buf == NULL) {
            *pescaped = empty;
            rc = IB_EALLOC;
            goto done;
        }

        /* Build the final string in the escaped buffer */
        if (quotes) {
            *buf = '\"';
            memcpy(buf+1, bsptr, slen);
            *(buf+slen+1) = '\"';
            *(buf+slen+2) = '\0';
        }
        else {
            strncpy(buf, (const char *)bsptr, slen);
            *(buf+slen) = '\0';
        }
        if (crop) {
            flags |= TXDUMP_BS_CROPPED;
        }
        *pescaped = buf;
        goto done;
    }

    /* Escape the string */
    buf = ib_mm_alloc(tx->mm, bslen * 2 + 3);
    if (buf == NULL) {
        rc = IB_EALLOC;
        goto done;
    }
    /* ib_string_escape_json_buf() always quotes. */
    rc = ib_string_escape_json_buf(bsptr, bslen,
                                   buf, bslen * 2 + 3,
                                   &size);
    if (rc != IB_OK) {
        *pescaped = empty;
        goto done;
    }
    flags |= TXDUMP_BS_ESCAPED;

    /* Crop if required */
    slen = size - 2;
    crop = slen > maxlen;
    if (crop) {
        flags |= TXDUMP_BS_CROPPED;
        *(buf+maxlen+1) = '\"';
        *(buf+maxlen+2) = '\0';
    }
    *pescaped = buf;

done:
    if (pflagbuf != NULL) {
        *pflagbuf = format_flags(tx->mm, rc, flags);
    }
    return rc;
}

/**
 * Escape and format a bytestring
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump Log data
 * @param[in] bs Byte string to log
 * @param[in] qmode Quoting mode
 * @param[in] maxlen Maximum string length (clipped at MIN_BS_LEN to MAX_BS_LEN)
 * @param[out] pflagbuf Pointer to buffer for flags (or NULL)
 * @param[out] pescaped Pointer to escaped buffer
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure of format_bs_ex().
 */
static ib_status_t format_bs(
    const ib_tx_t       *tx,
    const txdump_t      *txdump,
    const ib_bytestr_t  *bs,
    txdump_qmode_t       qmode,
    size_t               maxlen,
    const char         **pflagbuf,
    const char         **pescaped
)
{
    assert(tx != NULL);
    assert(txdump != NULL);
    assert(pescaped != NULL);

    ib_status_t rc;

    rc = format_bs_ex(tx, txdump,
                      (bs == NULL) ? NULL : ib_bytestr_const_ptr(bs),
                      (bs == NULL) ? 0    : ib_bytestr_length(bs),
                      qmode, maxlen,
                      pflagbuf, pescaped);
    return rc;
}

/**
 * Log a bytestr
 *
 * @param[in] tx IronBee transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces (max = MAX_LEADING_SPACES)
 * @param[in] label Label string
 * @param[in] bs Byte string to log
 * @param[in] maxlen Maximum string length (clipped at MIN_BS_LEN to MAX_BS_LEN)
 *
 * @returns void
 */
static void txdump_bs(
    const ib_tx_t      *tx,
    const txdump_t     *txdump,
    size_t              nspaces,
    const char         *label,
    const ib_bytestr_t *bs,
    size_t              maxlen
)
{
    assert(tx != NULL);
    assert(txdump != NULL);
    assert(label != NULL);
    assert(bs != NULL);

    const char *buf;
    const char *flagbuf;

    format_bs(tx, txdump, bs, QUOTE_ALWAYS, maxlen, &flagbuf, &buf);
    if (buf != NULL) {
        txdump_v(tx, txdump, nspaces, "%s = %s%s", label, buf, flagbuf);
    }
}

/**
 * Log a field.
 *
 * Logs a field name and value; handles various field types.
 *
 * @param[in] tx IronBee transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces (max = MAX_LEADING_SPACES)
 * @param[in] label Label string
 * @param[in] field Field to log
 * @param[in] maxlen Maximum string length (clipped at MIN_BS_LEN to MAX_BS_LEN)
 *
 * @returns void
 */
static void txdump_field(
    const ib_tx_t    *tx,
    const txdump_t   *txdump,
    size_t            nspaces,
    const char       *label,
    const ib_field_t *field,
    size_t            maxlen
)
{
    ib_status_t rc;

    /* Check the field name
     * Note: field->name is not always a null ('\0') terminated string */
    if (field == NULL) {
        txdump_v(tx, txdump, nspaces, "%s = <NULL>", label);
        return;
    }

    /* Dump the field based on it's type */
    switch (field->type) {
    case IB_FTYPE_GENERIC :      /* Generic data */
    {
        void *v;
        rc = ib_field_value(field, ib_ftype_generic_out(&v));
        if (rc == IB_OK) {
            txdump_v(tx, txdump, nspaces, "%s = %p", label, v);
        }
        break;
    }

    case IB_FTYPE_NUM :          /* Numeric (Integer) value */
    {
        ib_num_t n;
        rc = ib_field_value(field, ib_ftype_num_out(&n));
        if (rc == IB_OK) {
            txdump_v(tx, txdump, nspaces, "%s = %"PRId64"", label, n);
        }
        break;
    }

    case IB_FTYPE_TIME :         /* Time value */
    {
        ib_time_t t;

        rc = ib_field_value(field, ib_ftype_time_out(&t));
        if (rc == IB_OK) {
            ib_timeval_t tv;
            char         buf[30];

            IB_CLOCK_TIMEVAL(tv, t);
            ib_clock_timestamp(buf, &tv);
            txdump_v(tx, txdump, nspaces, "%s = %s", label, buf);
        }
        break;
    }

    case IB_FTYPE_FLOAT :        /* Floating point value */
    {
        ib_float_t  v;
        rc = ib_field_value(field, ib_ftype_float_out(&v));
        if (rc == IB_OK) {
            txdump_v(tx, txdump, nspaces, "%s = %Lg", label, (long double)v);
        }
        break;
    }

    case IB_FTYPE_NULSTR :       /**< NUL terminated string value */
        assert(0 && "NULSTR var detected!");
        break;

    case IB_FTYPE_BYTESTR :      /* Byte string value */
    {
        const ib_bytestr_t *bs;
        rc = ib_field_value(field, ib_ftype_bytestr_out(&bs));
        if (rc == IB_OK) {
            txdump_bs(tx, txdump, nspaces, label, bs, maxlen);
        }
        break;
    }

    case IB_FTYPE_LIST :         /* List */
    {
        const ib_list_t *lst;
        rc = ib_field_value(field, ib_ftype_list_out(&lst));
        if (rc == IB_OK) {
            size_t len = ib_list_elements(lst);
            txdump_v(tx, txdump, nspaces,
                     "%s = [%zd]", label, len);
        }
        break;
    }

    case IB_FTYPE_SBUFFER :      /* Stream buffer */
        txdump_v(tx, txdump, nspaces, "%s = sbuffer", label);
        break;

    default:                     /* Other */
        txdump_v(tx, txdump, nspaces,
                 "Unknown field type (%d)", field->type);
    }
}

/**
 * Log a header
 *
 * @param[in] tx IronBee transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces (max = MAX_LEADING_SPACES-2)
 * @param[in] label Label string
 * @param[in] header Header to log. May be NULL.
 *
 * @returns void
 */
static void txdump_header(
    const ib_tx_t             *tx,
    const txdump_t            *txdump,
    size_t                     nspaces,
    const char                *label,
    const ib_parsed_headers_t *header
)
{
    assert(tx != NULL);
    assert(txdump != NULL);
    assert(label != NULL);

    const ib_parsed_header_t *node;

    if (header == NULL) {
        txdump_v(tx, txdump, nspaces, "%s unavailable", label);
        return;
    }

    txdump_v(tx, txdump, nspaces, "%s", label);
    for (node = header->head; node != NULL; node = node->next) {
        const char *nbuf;
        const char *nflags;
        const char *vbuf;
        const char *vflags;
        format_bs(tx, txdump, node->name, QUOTE_AUTO, MAX_FIELD_NAME,
                  &nflags, &nbuf);
        format_bs(tx, txdump, node->value, QUOTE_ALWAYS, MAX_BS_LEN,
                  &vflags, &vbuf);
        txdump_v(tx, txdump, nspaces+2,
                 "%s%s = %s%s", nbuf, nflags, vbuf, vflags);
    }
}

/**
 * Build a path by appending the field name to an existing path.
 *
 * @param[in] tx IronBee transaction
 * @param[in] txdump TxDump data
 * @param[in] path Base path
 * @param[in] field Field whose name to append
 *
 * @returns Pointer to newly allocated path string
 */
static const char *build_path(
    const ib_tx_t    *tx,
    const txdump_t   *txdump,
    const char       *path,
    const ib_field_t *field
)
{
    size_t   pathlen;
    size_t   fullpath_len;
    size_t   tmplen;
    ssize_t  nlen = (ssize_t)field->nlen;
    bool     truncated = false;
    char    *fullpath;

    if ( (nlen <= 0) || (field->name == NULL) ) {
        nlen = 0;
    }
    else if (nlen > (ssize_t)MAX_PATH_ELEMENT) {
        size_t i;
        const char *p;
        for (i = 0, p=field->name; isprint(*p) && (i < MAX_PATH_ELEMENT); ++i) {
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
    fullpath = (char *)ib_mm_alloc(tx->mm, fullpath_len);
    if (fullpath == NULL) {
        return "";
    }

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
 * @param[in] nspaces Number of leading spaces (max = 30)
 * @param[in] path Base path
 * @param[in] lst List to log
 *
 * @returns
 * - IB_OK On success.
 * - Other on failure of ib_field_value().
 */
static ib_status_t txdump_list(
    const ib_tx_t    *tx,
    const txdump_t   *txdump,
    size_t            nspaces,
    const char       *path,
    const ib_list_t  *lst
)
{
    ib_status_t rc;
    const ib_list_node_t *node = NULL;

    /* Loop through the list & log everything */
    IB_LIST_LOOP_CONST(lst, node) {
        const ib_field_t *field =
            (const ib_field_t *)ib_list_node_data_const(node);
        const char *fullpath;
        const char *escaped;

        /* Build the path, escape it */
        fullpath = build_path(tx, txdump, path, field);
        format_bs_ex(tx, txdump,
                     (const uint8_t *)fullpath, strlen(fullpath),
                     QUOTE_AUTO, MAX_FIELD_NAME,
                     NULL, &escaped);

        switch (field->type) {
        case IB_FTYPE_GENERIC:
        case IB_FTYPE_NUM:
        case IB_FTYPE_FLOAT:
        case IB_FTYPE_TIME:
        case IB_FTYPE_BYTESTR:
            txdump_field(tx, txdump, nspaces, escaped, field, MAX_FIELD_SIZE);
            break;

        case IB_FTYPE_NULSTR:
            assert(0 && "NULSTR var detected!");
            break;

        case IB_FTYPE_LIST:
        {
            const ib_list_t *v;

            rc = ib_field_value(field, ib_ftype_list_out(&v));
            if (rc != IB_OK) {
                return rc;
            }

            txdump_field(tx, txdump, nspaces, escaped, field, MAX_FIELD_SIZE);
            txdump_list(tx, txdump, nspaces+2, fullpath, v);
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
 * @param[in] nspaces Number of leading spaces (max = 30)
 * @param[in] context Context to dump
 */
static void txdump_context(
    const ib_tx_t      *tx,
    const txdump_t     *txdump,
    size_t              nspaces,
    const ib_context_t *context
)
{
    const ib_site_t          *site = NULL;
    const ib_site_location_t *location = NULL;

    txdump_v(tx, txdump, nspaces, "Context");
    txdump_v(tx, txdump, nspaces+2, "Name = %s",
             ib_context_full_get(context) );

    ib_context_site_get(context, &site);
    if (site != NULL) {
        txdump_v(tx, txdump, nspaces+2, "Site Name = %s", site->name);
        txdump_v(tx, txdump, nspaces+2, "Site ID = %s", site->id);
    }
    ib_context_location_get(context, &location);
    if (location != NULL) {
        txdump_v(tx, txdump, nspaces+2, "Location Path = %s",
                 location->path);
    }
}

/**
 * Dump a request line
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump TxDump data
 * @param[in] nspaces Number of leading spaces
 * @param[in] line Request line to dump
 */
static void txdump_reqline(
    const ib_tx_t              *tx,
    const txdump_t             *txdump,
    size_t                      nspaces,
    const ib_parsed_req_line_t *line
)
{
    assert(tx != NULL);
    assert(txdump != NULL);

    if (line == NULL) {
        txdump_v(tx, txdump, nspaces, "Request Line unavailable");
        return;
    }
    txdump_v(tx, txdump, nspaces, "Request Line:");
    txdump_bs(tx, txdump, nspaces+2, "Raw", line->raw, MAX_FIELD_SIZE);
    txdump_bs(tx, txdump, nspaces+2, "Method", line->method, MAX_METHOD);
    txdump_bs(tx, txdump, nspaces+2, "URI", line->uri, MAX_FIELD_SIZE);
    txdump_bs(tx, txdump, nspaces+2, "Protocol", line->protocol, MAX_PROTOCOL);
}

/**
 * Dump a response line.
 *
 * @param[in] tx IronBee Transaction.
 * @param[in] txdump TxDump data.
 * @param[in] nspaces Number of leading spaces.
 * @param[in] line Response line to dump. May be NULL.
 */
static void txdump_resline(
    const ib_tx_t               *tx,
    const txdump_t              *txdump,
    size_t                       nspaces,
    const ib_parsed_resp_line_t *line
)
{
    assert(tx != NULL);
    assert(txdump != NULL);

    if (line == NULL) {
        txdump_v(tx, txdump, nspaces, "Response Line unavailable");
        return;
    }

    txdump_v(tx, txdump, nspaces, "Response Line:");
    txdump_bs(tx, txdump, nspaces+2, "Raw", line->raw, MAX_FIELD_SIZE);
    txdump_bs(tx, txdump, nspaces+2, "Protocol", line->protocol, MAX_PROTOCOL);
    txdump_bs(tx, txdump, nspaces+2, "Status", line->status, MAX_STATUS);
    txdump_bs(tx, txdump, nspaces+2, "Message", line->msg, MAX_FIELD_SIZE);
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
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 * - Other on var failures.
 */
static ib_status_t txdump_tx(
    const ib_engine_t *ib,
    const ib_tx_t     *tx,
    const txdump_t    *txdump)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(txdump != NULL);

    ib_status_t rc;

    /* No flags set: do nothing */
    if (!ib_flags_any(txdump->flags, TXDUMP_ENABLED) ) {
        return IB_OK;
    }

    /* Basic */
    if (ib_flags_all(txdump->flags, TXDUMP_BASIC) ) {
        char        buf[30];
        const char *id;

        ib_clock_timestamp(buf, &tx->tv_created);
        txdump_v(tx, txdump, 2, "IronBee Version = %s", IB_VERSION);

        /* Dump the engine's instance and sensor IDs */
        id = ib_engine_instance_id(ib);
        if (id != NULL) {
            txdump_v(tx, txdump, 2, "IronBee Instance ID = %s", id);
        }
        id = ib_engine_sensor_id(ib);
        if (id != NULL) {
            txdump_v(tx, txdump, 2, "Sensor ID = %s", id);
        }
        txdump_v(tx, txdump, 2, "Started = %s", buf);
        txdump_v(tx, txdump, 2, "Hostname = %s", tx->hostname);
        txdump_v(tx, txdump, 2, "Effective IP = %s", tx->remote_ipstr);
        txdump_v(tx, txdump, 2, "Path = %s", tx->path);
        txdump_v(tx, txdump, 2, "Blocking Mode = %s",
                 ib_flags_any(tx->flags, IB_TX_FBLOCKING_MODE) ? "On" : "Off");

        if (ib_tx_is_blocked(tx)) {
            txdump_v(tx, txdump, 2, "IsBlocked");
        }
        if (ib_tx_is_allowed(tx)) {
            txdump_v(tx, txdump, 2, "IsAllowed");
        }
        if (ib_tx_block_applied(tx)) {
            txdump_v(tx, txdump, 2, "BlockApplied");
        }
        if (ib_flags_any(tx->flags, TX_BLOCKED)) {
            if (ib_flags_any(tx->flags, IB_TX_FBLOCK_ADVISORY) ) {
                txdump_v(tx, txdump, 2, "Block: Advisory");
            }
            if (ib_flags_any(tx->flags, IB_TX_FBLOCK_PHASE) ) {
                txdump_v(tx, txdump, 2, " Block: Phase");
            }
            if (ib_flags_any(tx->flags, IB_TX_FBLOCK_IMMEDIATE) ) {
                txdump_v(tx, txdump, 2, "Block: Immediate");
            }
        }
    }

    /* Context info */
    if (ib_flags_all(txdump->flags, TXDUMP_CONTEXT) ) {
        txdump_context(tx, txdump, 2, tx->ctx);
    }

    /* Connection */
    if (ib_flags_all(txdump->flags, TXDUMP_CONN) ) {
        char buf[30];
        ib_clock_timestamp(buf, &tx->conn->tv_created);
        txdump_v(tx, txdump, 2, "Connection");
        txdump_v(tx, txdump, 4, "ID = %s", tx->conn->id);
        txdump_v(tx, txdump, 4, "Created = %s", buf);
        txdump_v(tx, txdump, 4, "Remote = %s:%d",
                 tx->conn->remote_ipstr, tx->conn->remote_port);
        txdump_v(tx, txdump, 4, "Local = %s:%d",
                 tx->conn->local_ipstr, tx->conn->local_port);
        if (ib_flags_all(txdump->flags, TXDUMP_CONTEXT) ) {
            txdump_context(tx, txdump, 4, tx->conn->ctx);
        }
    }

    /* Request Line */
    if (ib_flags_all(txdump->flags, TXDUMP_REQLINE) ) {
        txdump_reqline(tx, txdump, 2, tx->request_line);
    }

    /* Request Header */
    if (ib_flags_all(txdump->flags, TXDUMP_REQHDR) ) {
        txdump_header(tx, txdump, 2,
                      "Request Header", tx->request_header);
    }

    /* Response Line */
    if (ib_flags_all(txdump->flags, TXDUMP_RESLINE) ) {
        txdump_resline(tx, txdump, 2, tx->response_line);
    }

    /* Response Header */
    if (ib_flags_all(txdump->flags, TXDUMP_RESHDR) ) {
        txdump_header(tx, txdump, 2,
                      "Response Header", tx->response_header);
    }

    /* Flags */
    if (ib_flags_all(txdump->flags, TXDUMP_FLAGS) ) {
        const ib_strval_t *rec;

        txdump_v(tx, txdump, 2,
                 "Flags = %010lx", (unsigned long)tx->flags);
        IB_STRVAL_LOOP(ib_tx_flags_strval_first(), rec) {
            bool on = ib_flags_any(tx->flags, rec->val);
            txdump_v(tx, txdump, 4, "%010lx \"%s\" = %s",
                     (unsigned long)rec->val, rec->str,
                     on ? "On" : "Off");
        }
    }

    /* If the transaction never started, do nothing */
    if (! ib_flags_all(tx->flags, IB_TX_FREQ_STARTED) ) {
        return IB_OK;
    }

    /* ARGS */
    if (ib_flags_all(txdump->flags, TXDUMP_ARGS) ) {
        const ib_list_t *lst;
        ib_field_t *field;
        ib_var_source_t *source;
        txdump_v(tx, txdump, 2, "ARGS:");
        rc = ib_var_source_acquire(
            &source,
            tx->mm,
            ib_engine_var_config_get_const(ib),
            IB_S2SL("ARGS")
        );
        if (rc == IB_OK) {
            rc = ib_var_source_get(source, &field, tx->var_store);
        }
        if (rc == IB_OK) {
            txdump_field(tx, txdump, 4, "ARGS", field, MAX_FIELD_SIZE);

            rc = ib_field_value(field, ib_ftype_list_out(&lst));
            if ( (rc != IB_OK) || (lst == NULL) ) {
                return rc;
            }
            txdump_list(tx, txdump, 4, "ARGS", lst);
        }
        else {
            ib_log_debug_tx(tx, "log_tx: Failed to get ARGS: %s",
                            ib_status_to_string(rc));
        }
    }

    /* All vars */
    if (ib_flags_all(txdump->flags, TXDUMP_VARS) ) {
        ib_list_t *lst;
        txdump_v(tx, txdump, 2, "Vars:");

        /* Build the list */
        rc = ib_list_create(&lst, tx->mm);
        if (rc != IB_OK) {
            ib_log_debug_tx(tx, "log_tx: Failed to create tx list: %s",
                            ib_status_to_string(rc));
            return rc;
        }

        /* Extract the request headers field from the provider instance */
        ib_var_store_export(tx->var_store, lst);

        /* Log it all */
        rc = txdump_list(tx, txdump, 4, "", lst);
        if (rc != IB_OK) {
            ib_log_debug_tx(tx, "log_tx: Failed logging headers: %s",
                            ib_status_to_string(rc));
            return rc;
        }
    }

    /* Done */
    txdump_flush(tx, txdump);
    return IB_OK;
}

/**
 * Check if this TX should be dumped by this TxDump
 *
 * @param[in] tx IronBee Transaction
 * @param[in] txdump TxDump data
 *
 * @returns True if this TX should be dumped
 */

static bool txdump_check_tx(
    const ib_tx_t  *tx,
    const txdump_t *txdump
)
{
    ib_status_t           rc;
    const ib_list_node_t *node;
    txdump_config_t      *config;

    /* Get my module configuration */
    rc = ib_context_module_config(tx->ctx, txdump->module, (void *)&config);
    if (rc != IB_OK) {
        ib_log_error_tx(tx, "Failed to get %s module configuration: %s",
                        txdump->module->name, ib_status_to_string(rc));
        return false;
    }

    /* Loop through the TX's context configuration, see if this TxDump
     * is in the list.  Do nothing if there is no list or it's empty. */
    if ( (config->txdump_list == NULL) ||
         (ib_list_elements(config->txdump_list) == 0) )
    {
        return false;
    }
    IB_LIST_LOOP_CONST(config->txdump_list, node) {
        const txdump_t *tmp = ib_list_node_data_const(node);
        if (tmp == txdump) {
            return true;
        }
    }

    /* This TxDump is not in the context configuration's list */
    return false;
}


/**
 * Handle a TX state for TxDump
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] state State
 * @param[in] cbdata Callback data (TxDump object)
 *
 * @returns
 * - IB_OK If @a tx does not need to be dumped or @a tx was dumped successfully.
 * - Other on failure of txdump_tx().
 */
static
ib_status_t txdump_tx_state(
    ib_engine_t *ib,
    ib_tx_t     *tx,
    ib_state_t   state,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(cbdata != NULL);

    const txdump_t *txdump = (const txdump_t *)cbdata;
    ib_status_t     rc;

    assert(txdump->state == state);
    if (!txdump_check_tx(tx, txdump)) {
        return IB_OK;
    }

    txdump_v(tx, txdump, 0, "[TX %s @ %s]", tx->id, txdump->name);

    rc = txdump_tx(ib, tx, txdump);
    txdump_flush(tx, txdump);
    return rc;
}

/**
 * Handle a Request Line state for TxDump
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] state State
 * @param[in] line Parsed request line
 * @param[in] cbdata Callback data (TxDump object)
 *
 * @returns This always returns IB_OK.
 */
static
ib_status_t txdump_reqline_state(
    ib_engine_t          *ib,
    ib_tx_t              *tx,
    ib_state_t            state,
    ib_parsed_req_line_t *line,
    void                 *cbdata
)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(cbdata != NULL);

    const txdump_t *txdump = (const txdump_t *)cbdata;

    assert(txdump->state == state);
    if (!txdump_check_tx(tx, txdump)) {
        return IB_OK;
    }

    txdump_v(tx, txdump, 0, "[TX %s @ %s]", tx->id, txdump->name);
    txdump_reqline(tx, txdump, 2, line);
    txdump_flush(tx, txdump);
    return IB_OK;
}

/**
 * Handle a TX state for TxDump
 *
 * @param[in] ib IronBee object
 * @param[in] tx Transaction object
 * @param[in] state State
 * @param[in] line Parsed response line
 * @param[in] cbdata Callback data (TxDump object)
 *
 * @returns This always returns IB_OK.
 */
static
ib_status_t txdump_resline_state(
    ib_engine_t           *ib,
    ib_tx_t               *tx,
    ib_state_t             state,
    ib_parsed_resp_line_t *line,
    void                  *cbdata
)
{
    assert(ib != NULL);
    assert(tx != NULL);
    assert(cbdata != NULL);

    const txdump_t *txdump = (const txdump_t *)cbdata;

    assert(txdump->state == state);
    if (!txdump_check_tx(tx, txdump)) {
        return IB_OK;
    }

    txdump_v(tx, txdump, 0, "[TX %s @ %s]", tx->id, txdump->name);
    txdump_resline(tx, txdump, 2, line);
    txdump_flush(tx, txdump);
    return IB_OK;
}

/**
 * Execute function for the "TxDump" action
 *
 * @param[in] rule_exec The rule execution object
 * @param[in] data C-style string to log
 * @param[in] cbdata Callback data (TxDump object)
 *
 * @returns
 * - IB_OK On success.
 * - Other if txdump_tx() failed to dump @a tx.
 */
static ib_status_t txdump_act_execute(
    const ib_rule_exec_t *rule_exec,
    void                 *data,
    void                 *cbdata
)
{
    assert(rule_exec != NULL);
    assert(rule_exec->ib != NULL);
    assert(rule_exec->rule != NULL);
    assert(rule_exec->tx != NULL);
    assert(rule_exec->tx->id != NULL);
    assert(data != NULL);

    const txdump_t *txdump = (const txdump_t *)data;
    ib_status_t     rc;
    const ib_tx_t  *tx = rule_exec->tx;

    txdump_v(tx, txdump, 0, "[TX %s @ Rule %s]",
             tx->id, ib_rule_id(rule_exec->rule));

    rc = txdump_tx(rule_exec->ib, tx, txdump);
    txdump_flush(tx, txdump);
    return rc;
}

/**
 * TxDump state data
 */
struct txdump_state_t {
    ib_state_t state;     /**< Event type */
    ib_state_hook_type_t  hook_type; /**< Hook type */
};
typedef struct txdump_state_t txdump_state_t;

/**
 * TxDump state parsing mapping data
 */
struct txdump_strval_state_t {
    const char           *str;  /**< String "key" */
    const txdump_state_t  data; /**< Data portion */
};
typedef struct txdump_strval_state_t txdump_strval_state_t;

static IB_STRVAL_DATA_MAP(txdump_strval_state_t, state_map) = {
    IB_STRVAL_DATA_PAIR("TxStarted",
                        tx_started_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("RequestStarted",
                        request_started_state,
                        IB_STATE_HOOK_REQLINE),
    IB_STRVAL_DATA_PAIR("RequestHeaderProcess",
                        request_header_process_state,
                        IB_STATE_HOOK_REQLINE),
    IB_STRVAL_DATA_PAIR("TxContext",
                        handle_context_tx_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("RequestHeaderFinished",
                        request_header_finished_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("RequestHeader",
                        handle_request_header_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("RequestFinished",
                        request_finished_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("Request",
                        handle_request_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("TxProcess",
                        tx_process_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("ResponseStarted",
                        response_started_state,
                        IB_STATE_HOOK_RESPLINE),
    IB_STRVAL_DATA_PAIR("ResponseHeaderFinished",
                        response_header_finished_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("ResponseHeader",
                        response_header_finished_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("ResponseFinished",
                        response_finished_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("Response",
                        handle_response_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("PostProcess",
                        handle_postprocess_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("Logging",
                        handle_logging_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR("TxFinished",
                        tx_finished_state,
                        IB_STATE_HOOK_TX),
    IB_STRVAL_DATA_PAIR_LAST((ib_state_t)-1,
                             (ib_state_hook_type_t)-1),
};

/**
 * Parse the state for a TxDump directive
 *
 * @param[in] ib IronBee engine
 * @param[in] label Label for logging
 * @param[in] param Parameter string
 * @param[in,out] txdump TxDump object to set the state in
 *
 * @returns: Status code
 */
static ib_status_t txdump_parse_state(
    ib_engine_t  *ib,
    const char   *label,
    const char   *param,
    txdump_t     *txdump
)
{
    assert(ib != NULL);
    assert(label != NULL);
    assert(param != NULL);
    assert(txdump != NULL);

    ib_status_t           rc;
    const txdump_state_t *value = NULL;

    rc = IB_STRVAL_DATA_LOOKUP(state_map, txdump_strval_state_t, param, &value);
    if (rc != IB_OK) {
        ib_log_error(ib, "Invalid state parameter \"%s\" for %s.", param, label);
        return rc;
    }

#ifndef __clang_analyzer__
    txdump->state = value->state;
    txdump->hook_type = value->hook_type;
#endif
    txdump->name = ib_state_name(txdump->state);

    return IB_OK;
}

/**
 * Parse the destination for a TxDump directive or action
 *
 * @param[in] ib IronBee engine
 * @param[in] mm Memory manager to use for allocations
 * @param[in] module Module
 * @param[in] label Label for logging
 * @param[in] param Parameter string
 * @param[in,out] txdump TxDump object to set the state in
 *
 * @returns
 * - IB_OK On success.
 * - IB_EUNKNOWN if @c stdout or @c stderr cannot be used as output streams
 *   as requested (@a param equals "StdOut" or "StdErr").
 * - IB_EALLOC On memory allocation errors.
 * - IB_EINVAL No supported dump destination was requested or
 *             the file destination requested failed to be opened.
 */
static ib_status_t txdump_parse_dest(
    ib_engine_t       *ib,
    ib_mm_t            mm,
    const ib_module_t *module,
    const char        *label,
    const char        *param,
    txdump_t          *txdump
)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(module->data != NULL);
    assert(label != NULL);
    assert(param != NULL);
    assert(txdump != NULL);

    ib_status_t             rc;
    const txdump_moddata_t *moddata = (const txdump_moddata_t *)module->data;

    assert(moddata->fp_list != NULL);

    txdump->dest = ib_mm_strdup(mm, param);
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
        fname = ib_mm_strdup(mm, param + 7);
        if (fname == NULL) {
            return IB_EALLOC;
        }
        len = strlen(fname);
        if (len <= 1) {
            ib_log_error(ib, "Missing file name for %s.", label);
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
        ib_log_error(ib, "Invalid destination \"%s\" for %s.", param, label);
        return IB_EINVAL;
    }

    /* Store the file pointer so that we can close it later */
    if (txdump->fp != NULL) {
        rc = ib_list_push(moddata->fp_list, txdump->fp);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

static IB_STRVAL_MAP(flags_map) = {
    IB_STRVAL_PAIR("default", TXDUMP_DEFAULT),
    IB_STRVAL_PAIR("basic", TXDUMP_BASIC),
    IB_STRVAL_PAIR("context", TXDUMP_CONTEXT),
    IB_STRVAL_PAIR("connection", TXDUMP_CONN),
    IB_STRVAL_PAIR("reqline", TXDUMP_REQLINE),
    IB_STRVAL_PAIR("reqhdr", TXDUMP_REQHDR),
    IB_STRVAL_PAIR("resline", TXDUMP_RESLINE),
    IB_STRVAL_PAIR("reshdr", TXDUMP_RESHDR),
    IB_STRVAL_PAIR("headers", TXDUMP_HEADERS),
    IB_STRVAL_PAIR("flags", TXDUMP_FLAGS),
    IB_STRVAL_PAIR("args", TXDUMP_ARGS),
    IB_STRVAL_PAIR("vars", TXDUMP_VARS),
    IB_STRVAL_PAIR("all", TXDUMP_ALL),
    IB_STRVAL_PAIR_LAST,
};

/**
 * Handle the TxDump directive
 *
 * @param[in] cp Config parser
 * @param[in] directive Directive name
 * @param[in] params List of directive parameters
 * @param[in] cbdata Callback data (module)
 *
 * @returns
 * - IB_OK On success.
 * - IB_EINVAL If a parameter is omitted or if an unsupported state
 *   is passed as the first parameter.
 * - IB_EALLOC On allocation errors.
 * - Other on API failures.
 */
static ib_status_t txdump_handler(
    ib_cfgparser_t  *cp,
    const char      *directive,
    const ib_list_t *params,
    void            *cbdata
)
{
    assert(cp != NULL);
    assert(cp->ib != NULL);
    assert(directive != NULL);
    assert(params != NULL);
    assert(cbdata != NULL);

    ib_status_t           rc;
    ib_mm_t               mm = ib_engine_mm_main_get(cp->ib);
    const ib_module_t    *module = cbdata;
    ib_context_t         *context;
    txdump_config_t      *config;
    txdump_t              txdump;
    txdump_t             *ptxdump;
    const ib_list_node_t *node;
    const char           *param;
    static const char    *label = "TxDump directive";
    int                   flagno = 0;
    ib_flags_t            flags = 0;
    ib_flags_t            mask = 0;

    /* Get my configuration context */
    rc = ib_cfgparser_context_current(cp, &context);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Txdump: Failed to get current context: %s",
                         ib_status_to_string(rc));
        return rc;
    }

    /* Get my module configuration */
    rc = ib_context_module_config(context, module, (void *)&config);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to get %s module configuration: %s",
                         module->name, ib_status_to_string(rc));
        return rc;
    }
    assert(config->txdump_list != NULL);

    /* Initialize the txdump object */
    memset(&txdump, 0, sizeof(txdump));
    txdump.config = config;
    txdump.module = module;

    /* First parameter is state type */
    node = ib_list_first_const(params);
    if ( (node == NULL) || (ib_list_node_data_const(node) == NULL) ) {
        ib_cfg_log_error(cp,
                         "Missing state type for %s.", label);
        return IB_EINVAL;
    }
    param = (const char *)ib_list_node_data_const(node);
    rc = txdump_parse_state(cp->ib, label, param, &txdump);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing state for %s.", label);
        return rc;
    }

    /* Second parameter is the destination */
    node = ib_list_node_next_const(node);
    if ( (node == NULL) || (ib_list_node_data_const(node) == NULL) ) {
        ib_cfg_log_error(cp, "Missing destination for %s.", label);
        return IB_EINVAL;
    }
    param = (const char *)ib_list_node_data_const(node);
    rc = txdump_parse_dest(cp->ib, mm, module, label, param, &txdump);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error parsing destination for %s: %s",
                         label, ib_status_to_string(rc));
        return rc;
    }

    /* Parse the remainder of the parameters a enables / disables */
    while( (node = ib_list_node_next_const(node)) != NULL) {
        param = (const char *)ib_list_node_data_const(node);
        assert(param != NULL);
        rc = ib_flags_string(flags_map, param, flagno++, &flags, &mask);
        if (rc != IB_OK) {
            ib_cfg_log_error(cp, "Error parsing enable for %s: %s",
                             label, ib_status_to_string(rc));
            return rc;
        }
    }
    txdump.flags = ib_flags_merge(TXDUMP_DEFAULT, flags, mask);
    if (txdump.flags != 0) {
        txdump.flags |= TXDUMP_ENABLED;
    }

    /* Create the txdump entry */
    ptxdump = ib_mm_memdup(mm, &txdump, sizeof(txdump));
    if (ptxdump == NULL) {
        return IB_EALLOC;
    }

    /* Add it to the list */
    rc = ib_list_push(config->txdump_list, ptxdump);
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Error adding TxDump object to list for %s: %s",
                         label, ib_status_to_string(rc));
        return rc;
    }

    /* Finally, register the callback */
    switch(txdump.hook_type) {
    case IB_STATE_HOOK_TX:
        rc = ib_hook_tx_register(
            cp->ib,
            txdump.state,
            txdump_tx_state,
            ptxdump);
        break;
    case IB_STATE_HOOK_REQLINE:
        rc = ib_hook_parsed_req_line_register(
            cp->ib,
            txdump.state,
            txdump_reqline_state,
            ptxdump);
        break;
    case IB_STATE_HOOK_RESPLINE:
        rc = ib_hook_parsed_resp_line_register(
            cp->ib,
            txdump.state,
            txdump_resline_state,
            ptxdump);
        break;
    default:
        ib_cfg_log_error(cp, "No handler for hook type %d.", txdump.hook_type);
        return IB_EINVAL;
    }
    if (rc != IB_OK) {
        ib_cfg_log_error(cp, "Failed to register handler for hook type %d: %s",
                         txdump.hook_type, ib_status_to_string(rc));
    }

    return IB_OK;
}

/**
 * Create function for the txDump action.
 *
 * @param[in]  mm            Memory manager.
 * @param[in]  ctx           Context
 * @param[in]  parameters    Parameters
 * @param[out] instance_data Instance data to pass to execute.
 * @param[in]  cbdata        Callback data (module).
 *
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On allocation errors.
 * - IB_EINVAL If no destination could be parsed from @a parameters.
 * - Other on API failures.
 */
static ib_status_t txdump_act_create(
    ib_mm_t       mm,
    ib_context_t *ctx,
    const char   *parameters,
    void         *instance_data,
    void         *cbdata
)
{
    assert(ctx != NULL);
    assert(cbdata != NULL);

    ib_engine_t       *ib = ib_context_get_engine(ctx);
    ib_status_t        rc;
    const ib_module_t *module = cbdata;
    txdump_t           txdump;
    txdump_t          *ptxdump;
    char              *pcopy;
    char              *param;
    static const char *label = "txDump action";
    int                flagno = 0;
    ib_flags_t         flags = 0;
    ib_flags_t         mask = 0;

    if (parameters == NULL) {
        return IB_EINVAL;
    }

    /* Initialize the txdump object */
    memset(&txdump, 0, sizeof(txdump));
    txdump.name = "Action";

    /* Make a copy of the parameters that we can use for strtok() */
    pcopy = ib_mm_strdup(ib_engine_mm_temp_get(ib), parameters);
    if (pcopy == NULL) {
        return IB_EALLOC;
    }

    /* First parameter is the destination */
    param = strtok(pcopy, ",");
    if (param == NULL) {
        ib_log_error(ib, "Missing destination for %s.", label);
        return IB_EINVAL;
    }
    rc = txdump_parse_dest(ib, mm, module, label, param, &txdump);
    if (rc != IB_OK) {
        ib_log_error(ib, "Error parsing destination for %s.", label);
        return rc;
    }

    /* Parse the remainder of the parameters a enables / disables */
    while ((param = strtok(NULL, ",")) != NULL) {
        rc = ib_flags_string(flags_map, param, flagno++, &flags, &mask);
        if (rc != IB_OK) {
            ib_log_error(ib, "Error parsing enable for %s.", label);
            return rc;
        }
    }
    txdump.flags = ib_flags_merge(TXDUMP_DEFAULT, flags, mask);
    if (txdump.flags != 0) {
        txdump.flags |= TXDUMP_ENABLED;
    }

    /* Create the txdump entry */
    ptxdump = ib_mm_memdup(mm, &txdump, sizeof(txdump));
    if (ptxdump == NULL) {
        ib_log_error(ib, "Error allocating TxDump object for %s.", label);
        return IB_EALLOC;
    }

    /* Done */
    *(void **)instance_data = ptxdump;
    return IB_OK;
}

/**
 * Handle copying configuration data for the TxDump module
 *
 * @param[in] ib     Engine handle
 * @param[in] module Module
 * @param[in] dst    Destination of data.
 * @param[in] src    Source of data.
 * @param[in] length Length of data.
 * @param[in] cbdata Callback data
 *
 * @returns
 * - IB_OK On success.
 * - Other on ib_list_copy() or ib_list_copy() failures.
 */
static ib_status_t txdump_config_copy(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *dst,
    const void  *src,
    size_t       length,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(dst != NULL);
    assert(src != NULL);
    assert(length == sizeof(txdump_config));

    ib_status_t            rc;
    txdump_config_t       *dst_config = dst;
    const txdump_config_t *src_config = src;
    ib_mm_t                mm = ib_engine_mm_main_get(ib);

    /*
     * If there is no source list, create an empty list.  Otherwise, copy
     * nodes from the source list.
     */
    if (src_config->txdump_list == NULL) {
        rc = ib_list_create(&dst_config->txdump_list, mm);
    }
    else {
        rc = ib_list_copy(src_config->txdump_list, mm, &dst_config->txdump_list);
    }
    return rc;
}

/**
 * Initialize the txdump module.
 *
 * @param[in] ib IronBee Engine.
 * @param[in] module Module data.
 * @param[in] cbdata Callback data (unused).
 *
 * @returns
 * - IB_OK On success.
 * - Other if actions or directives cannot be registered.
 */
static ib_status_t txdump_init(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(module != NULL);

    ib_status_t        rc;
    size_t             flagbuf_len = 3;
    const ib_strval_t *rec;
    ib_mm_t            mm = ib_engine_mm_main_get(ib);
    txdump_moddata_t  *moddata;

    /* Sanity check that FLAGBUF_SIZE is sufficiently large */
    IB_STRVAL_LOOP(bytestring_flags_map, rec) {
        flagbuf_len += (strlen(rec->str) + 1);
    }
    assert(FLAGBUF_SIZE > flagbuf_len);

    /* Register the TxDump directive */
    rc = ib_config_register_directive(ib,
                                      "TxDump",
                                      IB_DIRTYPE_LIST,
                                      (ib_void_fn_t)txdump_handler,
                                      NULL,
                                      module,
                                      NULL,
                                      NULL);
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to register TxDump directive: %s",
                     ib_status_to_string(rc));
        return rc;
    }

    /* Register the TxDump action */
    rc = ib_action_create_and_register(
        NULL, ib,
        "txDump",
        txdump_act_create, module,
        NULL, NULL, /* no destroy function */
        txdump_act_execute, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    /* Allocate the module instance data */
    moddata = ib_mm_alloc(mm, sizeof(*moddata));
    if (moddata == NULL) {
        ib_log_error(ib, "Failed to allocate TxDump module instance data");
        return IB_EALLOC;
    }

    /* Create the file pointer list */
    rc = ib_list_create(&(moddata->fp_list), ib_engine_mm_main_get(ib));
    if (rc != IB_OK) {
        ib_log_error(ib, "Failed to create TxDump file pointer list: %s",
                     ib_status_to_string(rc));
        return rc;
    }
    module->data = (void *)moddata;

    return IB_OK;
}

/**
 * Finish the txdump module.
 *
 * @param[in] ib IronBee Engine.
 * @param[in] module Module data.
 * @param[in] cbdata Callback data (unused).
 *
 * @returns
 * - IB_OK On success.
 */
static ib_status_t txdump_finish(
    ib_engine_t *ib,
    ib_module_t *module,
    void        *cbdata
)
{
    assert(ib != NULL);
    assert(module != NULL);
    assert(module->data != NULL);

    txdump_moddata_t *moddata = (txdump_moddata_t *)module->data;
    ib_list_node_t   *node = NULL;

    assert(moddata->fp_list != NULL);

    /* Loop through the list & log everything */
    IB_LIST_LOOP(moddata->fp_list, node) {
        FILE *fp = (FILE *)node->data;
        fclose( fp );
    }

    return IB_OK;
}

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,               /* Default metadata */
    MODULE_NAME_STR,                         /* Module name */
    &txdump_config, sizeof(txdump_config),   /* Global config data */
    txdump_config_copy, NULL,                /* Config copy function */
    NULL,                                    /* Module config map */
    NULL,                                    /* Module directive map */
    txdump_init,                             /* Initialize function */
    NULL,                                    /* Callback data */
    txdump_finish,                           /* Finish function */
    NULL,                                    /* Callback data */
);
