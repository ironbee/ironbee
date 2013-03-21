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
 * @brief IronBee --- HTP Module
 *
 * This module integrates libhtp.
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */
#include "ironbee_config_auto.h"

#include <ironbee/bytestr.h>
#include <ironbee/cfgmap.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/provider.h>
#include <ironbee/state_notify.h>
#include <ironbee/string.h>
#include <ironbee/util.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#endif
#include <htp.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* Define the module name as well as a string version of it. */
#define MODULE_NAME        htp
#define MODULE_NAME_STR    IB_XSTRINGIFY(MODULE_NAME)

/* Define the public module symbol. */
IB_MODULE_DECLARE();

/* Pre-declare types */
typedef struct modhtp_context_t modhtp_context_t;
typedef struct modhtp_cfg_t modhtp_cfg_t;
typedef struct modhtp_nameval_t modhtp_nameval_t;

typedef enum htp_data_source_t modhtp_param_source_t;

/**
 * Module Context Structure
 */
struct modhtp_context_t {
    ib_engine_t    *ib;           /**< Engine handle */
    ib_conn_t      *iconn;        /**< Connection structure */
    modhtp_cfg_t   *modcfg;       /**< Module config structure */
    htp_cfg_t      *htp_cfg;      /**< Parser config handle */
    htp_connp_t    *htp;          /**< Parser handle */
};

/**
 * Callback data for the param iterator callback
 */
typedef struct {
    ib_field_t	          *field_list;   /**< Field list to populate */
    modhtp_param_source_t  source;       /**< Desired source */
    size_t                 count;        /**< Count of matches */
} modhtp_param_iter_data_t;

/**
 * Module Configuration Structure
 */
struct modhtp_cfg_t {
    const char     *personality;  /**< libhtp personality */
};

/* Instantiate a module global configuration. */
static modhtp_cfg_t modhtp_global_cfg = {
    "generic" /* personality */
};

static ib_engine_t *modhtp_ib = NULL;

/* -- Define several function types for callbacks */

/**
 * Function used as a callback to modhtp_table_iterator()
 *
 * @param[in] tx IronBee transaction
 * @param[in] key Key of the key/value pair
 * @param[in] vptr Pointer to value of the key/value pair
 * @param[in] data Function specific data
 *
 * @returns Status code
 */
typedef ib_status_t (* modhtp_table_iterator_callback_fn_t)(
    const ib_tx_t             *tx,
    const bstr                *key,
    void                      *vptr,
    void                      *data);

/**
 * Function used as a callback to modhtp_set_generic()
 *
 * This matches the signature of many htp_tx_{req,res}_set_xxx_c() functions
 * from htp_transaction.h
 *
 * @param[in] tx IronBee transaction
 * @param[in] data Data to set
 * @param[in] dlen Length of @a data
 * @param[in] alloc Allocation strategy
 *
 * @returns Status code
 */
typedef htp_status_t (* modhtp_set_fn_t)(
    htp_tx_t                  *tx,
    const char                *data,
    size_t                     dlen,
    enum htp_alloc_strategy_t  alloc);

/**
 * Function used as a callback to modhtp_set_header()
 *
 * This matches the signature of htp_tx_{req,res}_set_header_c() functions
 * from htp_transaction.h
 *
 * @param[in] htx HTP transaction
 * @param[in] name Header name
 * @param[in] name_len Length of @a name
 * @param[in] value Header value
 * @param[in] value_len Length of @a value
 * @param[in] alloc Allocation strategy
 *
 * @returns HTP status code
 */
typedef htp_status_t (* modhtp_set_header_fn_t)(
    htp_tx_t                  *htx,
    const char                *name,
    size_t                     name_len,
    const char                *value,
    size_t                     value_len,
    enum htp_alloc_strategy_t  alloc);


/* -- libhtp Routines -- */

/* Define a name/val lookup record. */
struct modhtp_nameval_t {
    const char *name;
    int         val;
};

/* Text versions of personalities */
static const modhtp_nameval_t modhtp_personalities[] = {
    { "",           HTP_SERVER_IDS },
    { "minimal",    HTP_SERVER_MINIMAL },
    { "generic",    HTP_SERVER_GENERIC },
    { "ids",        HTP_SERVER_IDS },
    { "iis_4_0",    HTP_SERVER_IIS_4_0 },
    { "iis_5_0",    HTP_SERVER_IIS_5_0 },
    { "iis_5_1",    HTP_SERVER_IIS_5_1 },
    { "iis_6_0",    HTP_SERVER_IIS_6_0 },
    { "iis_7_0",    HTP_SERVER_IIS_7_0 },
    { "iis_7_5",    HTP_SERVER_IIS_7_5 },
    { "apache_2",   HTP_SERVER_APACHE_2 },
    { NULL, 0 }
};

/* Lookup a numeric personality from a name. */
static int modhtp_personality(
    const char *name)
{
    const modhtp_nameval_t *rec = modhtp_personalities;

    if (name == NULL) {
        return -1;
    }

    while (rec->name != NULL) {
        if (strcasecmp(name, rec->name) == 0) {
            return rec->val;
        }

        ++rec;
    }

    return -1;
}

/* Log htp data via ironbee logging. */
static int modhtp_callback_log(
    htp_log_t *log)
{
    modhtp_context_t *modctx =
        (modhtp_context_t *)htp_connp_get_user_data(log->connp);
    int level;

    /* Parsing issues are unusual but not IronBee failures. */
    switch(log->level) {
    case HTP_LOG_ERROR:
    case HTP_LOG_WARNING:
    case HTP_LOG_NOTICE:
    case HTP_LOG_INFO:
        level = IB_LOG_INFO;
        break;
    case HTP_LOG_DEBUG:
        level = IB_LOG_DEBUG;
        break;
    default:
        level = IB_LOG_DEBUG3;
    }

    if (log->code != 0) {
        ib_log_ex(modctx->ib, level,
                  log->file, log->line,
                  "LibHTP [error %d] %s",
                  log->code, log->msg);
    }
    else {
        ib_log_ex(modctx->ib, level,
                  log->file, log->line,
                  "LibHTP %s",
                  log->msg);
    }

    return 0;
}

/* -- Table iterator functions -- */

/**
 * modhtp_table_iterator() callback to add a field to an IronBee list
 *
 * @param[in] tx IronBee transaction
 * @param[in] key Key of the key/value pair
 * @param[in] vptr Pointer to value of the key/value pair
 * @param[in] data Function specific data (IB field/list pointer)
 *
 * @returns Status code (IB_OK)
 */
static ib_status_t modhtp_field_list_callback(
    const ib_tx_t *tx,
    const bstr    *key,
    void          *vptr,
    void          *data)
{
    assert(tx != NULL);
    assert(key != NULL);
    assert(vptr != NULL);
    assert(data != NULL);

    ib_field_t *flist = (ib_field_t *)data;
    const bstr *value = (const bstr *)vptr;
    ib_field_t *field;
    ib_status_t rc;

    /* Create a list field as an alias into htp memory. */
    rc = ib_field_create_bytestr_alias(&field,
                                       tx->mp,
                                       (const char *)bstr_ptr(key),
                                       bstr_len(key),
                                       (uint8_t *)bstr_ptr(value),
                                       bstr_len(value));
    if (rc != IB_OK) {
        ib_log_debug3_tx(tx, "Failed to create field: %s",
                         ib_status_to_string(rc));
        return IB_OK;
    }

    /* Add the field to the field list. */
    rc = ib_field_list_add(flist, field);
    if (rc != IB_OK) {
        ib_log_debug3_tx(tx, "Failed to add field: %s",
                         ib_status_to_string(rc));
        return IB_OK;
    }

    /* Always return IB_OK */
    return IB_OK;
}

/**
 * modhtp_table_iterator() callback to add a field to handle
 * request / response parameters
 *
 * @param[in] tx IronBee transaction
 * @param[in] key Key of the key/value pair
 * @param[in] value Pointer to value of the key/value pair
 * @param[in] data Function specific data (modhtp_param_iter_data_t *)
 *
 * @returns Status code (IB_OK)
 */
static ib_status_t modhtp_param_iter_callback(
    const ib_tx_t *tx,
    const bstr    *key,
    void          *value,
    void          *data)
{
    assert(tx != NULL);
    assert(key != NULL);
    assert(value != NULL);
    assert(data != NULL);

    modhtp_param_iter_data_t *idata = (modhtp_param_iter_data_t *)data;
    const htp_param_t *param = (const htp_param_t *)value;
    ib_field_t *field;
    ib_status_t rc;

    /* Ignore if from wrong source */
    if (param->source != idata->source) {
        return IB_OK;
    }

    /* Create a list field as an alias into htp memory. */
    rc = ib_field_create_bytestr_alias(&field,
                                       tx->mp,
                                       (const char *)bstr_ptr(key),
                                       bstr_len(key),
                                       (uint8_t *)bstr_ptr(param->value),
                                       bstr_len(param->value));
    if (rc != IB_OK) {
        ib_log_debug3_tx(tx, "Failed to create field: %s",
                         ib_status_to_string(rc));
        return IB_OK;
    }

    /* Add the field to the field list. */
    rc = ib_field_list_add(idata->field_list, field);
    if (rc != IB_OK) {
        ib_log_debug3_tx(tx, "Failed to add field: %s",
                         ib_status_to_string(rc));
        return IB_OK;
    }

    /* Always return IB_OK */
    ++(idata->count);
    return IB_OK;
}

/**
 * Generic HTP table iterator that takes a callback function
 *
 * The callback function @a fn is called for each iteration of @a table.
 *
 * @param[in] tx IronBee transaction passed to @a fn
 * @param[in] table The table to iterate
 * @param[in] fn The callback function
 * @param[in] data Generic data passed to @a fn
 *
 * @note If @a fn returns an error, it will cause an error to returned
 * immediately without completing table iteration.
 *
 * @returns Status code
 *  - IB_OK All OK
 *  - IB_EINVAL If either key or value of any iteration is NULL
 *  - Errors returned by @a fn
 */
static ib_status_t modhtp_table_iterator(
    const ib_tx_t                       *tx,
    const htp_table_t                   *table,
    modhtp_table_iterator_callback_fn_t  fn,
    void                                *data)
{
    assert(table != NULL);
    assert(fn != NULL);

    size_t index;
    size_t tsize = htp_table_size(table);

    for (index = 0;  index < tsize;  ++index) {
        bstr *key = NULL;
        void *value = NULL;
        ib_status_t rc;

        value = htp_table_get_index(table, index, &key);
        if (key == NULL) {
            return IB_EINVAL;
        }

        rc = fn(tx, key, value, data);
        if (rc != IB_OK) {
            return rc;
        }
    }

    return IB_OK;
}

/**
 * Set a generic request / response item for libhtp by creating a
 * c-style (nul-terminated) string from @a bstr and then calling
 * @a fn with the new c string.
 *
 * @param[in] itx IronBee transaction
 * @param[in] htx HTP transaction
 * @param[in] data Data to set
 * @param[in] dlen Length of @a data
 * @param[in] fn libhtp function to call
 *
 * @returns Status code
 */
static inline ib_status_t modhtp_set_data(
    const ib_tx_t      *itx,
    htp_tx_t           *htx,
    const char         *data,
    size_t              dlen,
    modhtp_set_fn_t     fn)
{
    htp_status_t  hrc;

    /* If there's no NULL, libhtp will return an error, so ignore it. */
    if (data == NULL) {
        return IB_OK;
    }

    /* Hand it off to libhtp */
    hrc = fn(htx, data, dlen, HTP_ALLOC_COPY);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    return IB_OK;
}

/**
 * Set a generic request / response item for libhtp by creating a
 * c-style (nul terminated) string from the non-terminated string @a data
 * of length @a dlen and then calling @a fn with the new c string.
 *
 * @param[in] itx IronBee transaction
 * @param[in] htx HTP transaction
 * @param[in] bstr ByteString to set
 * @param[in] fn libhtp function to call
 *
 * @returns Status code
 */
static inline ib_status_t modhtp_set_bstr(
    const ib_tx_t      *itx,
    htp_tx_t           *htx,
    const ib_bytestr_t *bstr,
    modhtp_set_fn_t     fn)
{
    ib_status_t rc;
    const char *ptr;

    ptr = (const char *)ib_bytestr_const_ptr(bstr);
    if (ptr == NULL) {
        ptr = "";
    }
    rc = modhtp_set_data(itx, htx, ptr, ib_bytestr_length(bstr), fn);
    return rc;
}

/**
 * Set headers to libhtp
 *
 * The callback function @a fn is called for each iteration of @a header.
 *
 * @param[in] itx IronBee transaction
 * @param[in] htx HTP transaction passed to @a fn
 * @param[in] header The header to iterate
 * @param[in] fn The callback function
 *
 * @returns Status code
 *  - IB_OK All OK
 *  - IB_EINVAL If either key or value of any iteration is NULL
 */
static ib_status_t modhtp_set_header(
    const ib_tx_t                    *itx,
    htp_tx_t                         *htx,
    const ib_parsed_header_wrapper_t *header,
    modhtp_set_header_fn_t            fn)
{
    assert(htx != NULL);
    assert(header != NULL);
    assert(fn != NULL);

    const ib_parsed_name_value_pair_list_t *node;

    for (node = header->head;  node != NULL;  node = node->next) {
        htp_status_t hrc;
        const char *value = (const char *)ib_bytestr_const_ptr(node->value);
        size_t vlen = ib_bytestr_length(node->value);

        if (value == NULL) {
            value = "";
            vlen = 0;
        }
        hrc = fn(htx,
                 (const char *)ib_bytestr_const_ptr(node->name),
                 ib_bytestr_length(node->name),
                 value, vlen,
                 HTP_ALLOC_COPY);
        if (hrc != HTP_OK) {
            return IB_EUNKNOWN;
        }
    }

    return IB_OK;
}

/**
 * Set a IronBee bytestring if it's NULL or empty from a libhtp bstr.
 *
 * @param[in] itx IronBee transaction
 * @param[in] label Label for logging
 * @param[in] force Set even if value already set
 * @param[in] htp_bstr HTP bstr to copy from
 * @param[in] fallback Fallback string (or NULL)
 * @param[in,out] ib_bstr Pointer to IronBee bytestring to fill
 *
 * @returns IronBee Status code
 */
static inline ib_status_t modhtp_set_bytestr(
    const ib_tx_t          *itx,
    const char             *label,
    bool                    force,
    const bstr             *htp_bstr,
    const char             *fallback,
    ib_bytestr_t          **ib_bstr)
{
    assert(itx != NULL);
    assert(label != NULL);
    assert(ib_bstr != NULL);

    ib_status_t    rc;
    const uint8_t *ptr = NULL;
    size_t         len = 0;

    /* If it's already set, do nothing */
    if ( (*ib_bstr != NULL) && (ib_bytestr_length(*ib_bstr) != 0) ) {
        if (! force) {
            return IB_OK;
        }
    }

    /* If it's not set in the htp bytestring, try the fallback. */
    if ( (htp_bstr == NULL) || (bstr_len(htp_bstr) == 0) ) {
        if (fallback == NULL) {
            ib_log_debug_tx(itx, "%s unknown: no fallback", label);
            return IB_OK;
        }
        ib_log_debug_tx(itx,
                        "%s unknown: using fallback \"%s\"", label, fallback);
        ptr = (const uint8_t *)fallback;
        len = strlen(fallback);
    }
    else {
        ptr = bstr_ptr(htp_bstr);
        len = bstr_len(htp_bstr);
    }

    /*
     * If the target bytestring is NULL, create it, otherwise
     * append to the zero-length bytestring.
     */
    if (*ib_bstr == NULL) {
        rc = ib_bytestr_dup_mem(ib_bstr, itx->mp, ptr, len);
    }
    else if (force) {
        void *new = ib_mpool_memdup(itx->mp, ptr, len);
        if (new == NULL) {
            rc = IB_EALLOC;
            goto done;
        }
        rc = ib_bytestr_setv(*ib_bstr, new, len);
    }
    else {
        rc = ib_bytestr_append_mem(*ib_bstr, ptr, len);
    }

done:
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Failed to set %s: %s",
                        label, ib_status_to_string(rc));
    }
    return rc;
}

/**
 * Set a NUL-terminated string if it's NULL or empty from a libhtp bstr.
 *
 * @param[in] itx IronBee transaction
 * @param[in] label Label for logging
 * @param[in] force Set even if value already set
 * @param[in] htp_bstr HTP bstr to copy from
 * @param[in] fallback Fallback string (or NULL)
 * @param[in,out] nulstr Pointer to NUL-terminated string to fill
 *
 * @returns Status code
 */
static inline ib_status_t modhtp_set_nulstr(
    const ib_tx_t          *itx,
    const char             *label,
    bool                    force,
    const bstr             *htp_bstr,
    const char             *fallback,
    const char            **nulstr)
{
    assert(itx != NULL);
    assert(label != NULL);
    assert(nulstr != NULL);

    const char  *ptr = NULL;
    size_t       len = 0;

    /* If it's already set, do nothing */
    if ( (*nulstr != NULL) && (**nulstr != '\0') ) {
        if (! force) {
            return IB_OK;
        }
    }

    /* If it's not set in the htp bytestring, try the fallback. */
    if ( (htp_bstr == NULL) || (bstr_len(htp_bstr) == 0) ) {
        if (fallback == NULL) {
            ib_log_debug_tx(itx, "%s unknown: no fallback", label);
            return IB_OK;
        }
        ib_log_debug_tx(itx,
                        "%s unknown: using fallback \"%s\"", label, fallback);
        ptr = fallback;
        len = strlen(fallback);
    }
    else {
        ptr = (const char *)bstr_ptr(htp_bstr);
        len = bstr_len(htp_bstr);
    }

    *nulstr = ib_mpool_memdup_to_str(itx->mp, ptr, len);
    return (*nulstr == NULL) ? IB_EALLOC : IB_OK;
}

/**
 * Get the modhtp context for an IronBee transaction
 *
 * @param[in] itx IronBee transaction
 *
 * @returns modhtp context
 */
static inline modhtp_context_t *modhtp_get_itx_context(
    const ib_tx_t *itx)
{
    assert(itx != NULL);
    assert(itx->conn != NULL);
    modhtp_context_t *ctx;

    ctx = (modhtp_context_t *)ib_conn_parser_context_get(itx->conn);
    assert(ctx != NULL);
    return ctx;
}

/**
 * Get the modhtp transaction for an IronBee transaction
 *
 * @param[in] itx IronBee transaction
 *
 * @returns modhtp transaction
 */
static inline htp_tx_t *modhtp_get_htx(
    const ib_tx_t *itx)
{
    assert(itx != NULL);
    assert(itx->conn != NULL);

    ib_status_t irc;
    htp_tx_t *htx;

    irc = ib_tx_get_module_data(itx, IB_MODULE_STRUCT_PTR, (void **)&htx);
    if (irc != IB_OK) {
        return NULL;
    }

    return htx;
}

/**
 * Check the modhtp connection parser status, get the related transactions
 *
 * @param[in] parser libhtp connection parser
 * @param[in] is_request True if this is a request, false if response
 * @param[in] label Label string (for logging)
 * @param[out] parse_error Error reported by parser?
 * @param[out] piconn Pointer to IronBee connection / NULL
 * @param[out] phtx Pointer to libhtp transaction / NULL
 * @param[out] pitx Pointer to IronBee transaction / NULL
 *
 * @returns libhtp status code
 */
static inline ib_status_t modhtp_check_parser(
    const htp_connp_t *parser,
    bool               is_request,
    const char        *label,
    bool              *parse_error,
    ib_conn_t        **piconn,
    htp_tx_t         **phtx,
    ib_tx_t          **pitx)
{
    assert(parser != NULL);
    assert(label != NULL);
    assert(parse_error != NULL);

    ib_conn_t               *iconn = NULL;
    htp_tx_t                *htx = NULL;
    ib_tx_t                 *itx = NULL;
    enum htp_stream_state_t  status;
    modhtp_context_t        *context = NULL;
    ib_status_t              rc = IB_OK;

    /* Check the connection parser status */
    status = (is_request ? parser->in_status : parser->out_status);
    if (status == HTP_STREAM_ERROR) {
        ib_log_error(modhtp_ib, "%s: HTP Parser Error", label);
        *parse_error = true;
    }
    else {
        *parse_error = false;
    }

    /* Get the current libhtp transaction */
    htx = (is_request ? parser->in_tx : parser->out_tx);
    if (htx == NULL) {
        ib_log_error(modhtp_ib, "%s: No HTP transaction", label);
        rc = (rc == IB_OK) ? IB_EINVAL : rc;
    }
    else {
        itx = htp_tx_get_user_data(htx);
        if (itx == NULL) {
            rc = (rc == IB_OK) ? IB_ENOENT : rc;
        }
    }

    /* Verify that the context is valid */
    context = htp_connp_get_user_data(parser);
    if (context == NULL) {
        ib_log_error(modhtp_ib, "%s:  to get connection context", label);
        rc = (rc == IB_OK) ? IB_EINVAL : rc;
    }
    else {
        iconn = context->iconn;
        if (iconn == NULL) {
            ib_log_error(modhtp_ib, "%s: No IronBee connection", label);
            rc = (rc == IB_OK) ? IB_ENOENT : rc;
        }
    }

    if (piconn != NULL) {
        *piconn = iconn;
    }
    if (pitx != NULL) {
        *pitx = itx;
    }
    if (phtx != NULL) {
        *phtx = htx;
    }

    return rc;
}

/* -- Field Generation Routines -- */


static ib_status_t modhtp_field_gen_bytestr(
    ib_data_t   *data,
    const char  *name,
    bstr        *bs,
    ib_field_t **pf)
{
    ib_field_t   *f;
    ib_bytestr_t *ibs;
    ib_status_t   rc;

    if (bs == NULL) {
        if (pf != NULL) {
            *pf = NULL;
        }
        return IB_EINVAL;
    }

    /* First lookup the field to see if there is already one
     * that needs the value set.
     */
    rc = ib_data_get(data, name, &f);
    if (rc == IB_OK) {
        rc = ib_field_mutable_value(f, ib_ftype_bytestr_mutable_out(&ibs));
        if (rc != IB_OK) {
            return rc;
        }

        rc = ib_bytestr_setv_const(ibs,
                                   (const uint8_t *)bstr_ptr(bs),
                                   bstr_len(bs));

        return rc;
    }

    /* If no field exists, then create one. */
    rc = ib_data_add_bytestr_ex(data, name, strlen(name),
                                (uint8_t *)bstr_ptr(bs),
                                bstr_len(bs), pf);
    return rc;
}

#define modhtp_field_gen_list(data, name, pf) \
    ib_data_add_list_ex((data), (name), strlen((name)), (pf))

/* -- Utility functions -- */


/*
 * Macro to check a single libhtp parser flag, set appropriate IB TX flag.
 *
 * This macro invokes the modhtp_check_flag function.  This is implemented as
 * a macro so that we can use IB_STRINGIFY().
 *
 * @param[in,out] itx IronBee transaction
 * @param[in] collection Collection name
 * @param[in,out] flags Libhtp flags
 * @param[in] name flag Flag to check
 */
#define MODHTP_PROCESS_PARSER_FLAG(itx,collection,flags,flag)           \
    do {                                                                \
        if ((flags) & (HTP_##flag)) {                                   \
            modhtp_parser_flag(itx,                                     \
                               collection,                              \
                               &flags,                                  \
                               (HTP_##flag),                            \
                               IB_XSTRINGIFY(flag));                    \
        }                                                               \
    } while(0)

/**
 * Process and handle a single libhtp parser flag
 *
 * @param[in,out] itx IronBee transaction
 * @param[in] collection Collection name
 * @param[in] pflags Pointer to libhtp flags
 * @param[in] flagbit Libhtp flag bit to check
 * @param[in] flagname Flag name
 */
static void modhtp_parser_flag(
    ib_tx_t    *itx,
    const char *collection,
    uint64_t   *pflags,
    uint64_t    flagbit,
    const char *flagname)
{
    assert(itx != NULL);
    assert(itx->mp != NULL);
    assert(itx->data != NULL);
    assert(flagname != NULL);

    ib_status_t rc;
    ib_field_t *field;
    ib_field_t *listfield;
    ib_num_t value = 1;

    (*pflags) ^= flagbit;

    rc = ib_data_get(itx->data, collection, &field);
    if (rc == IB_ENOENT) {
        rc = ib_data_add_list(itx->data, collection, &field);
        if (rc != IB_OK) {
            ib_log_warning_tx(itx,
                              "Failed to add collection \"%s\": %s",
                              collection, ib_status_to_string(rc));
            return;
        }
    }
    rc = ib_field_create(&listfield,
                         itx->mp,
                         IB_FIELD_NAME(flagname),
                         IB_FTYPE_NUM,
                         ib_ftype_num_in(&value));
    if (rc != IB_OK) {
        ib_log_warning_tx(itx, "Failed to create \"%s\" flag field: %s",
                          flagname, ib_status_to_string(rc));
        return;
    }
    rc = ib_field_list_add(field, listfield);
    if (rc != IB_OK) {
        ib_log_warning_tx(itx,
                          "Failed to add \"%s\" flag to collection \"%s\": %s",
                          flagname, collection, ib_status_to_string(rc));
        return;
    }
    return;
}

static ib_status_t modhtp_set_parser_flags(
    ib_tx_t       *itx,
    const char    *collection,
    uint64_t       flags)
{
    ib_status_t rc = IB_OK;

    /* FILED_xxxx */
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, FIELD_UNPARSEABLE);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, FIELD_INVALID);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, FIELD_FOLDED);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, FIELD_REPEATED);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, FIELD_LONG);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, FIELD_RAW_NUL);

    /* REQUEST_SMUGGLING */
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, REQUEST_SMUGGLING);

    /* INVALID_xxx */
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, INVALID_FOLDING);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, INVALID_CHUNKING);

    /* MULTI_PACKET_HEAD */
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, MULTI_PACKET_HEAD);

    /* HOST_xxx */
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, HOST_MISSING);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, HOST_AMBIGUOUS);

    /* PATH_xxx */
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, PATH_ENCODED_NUL);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, PATH_INVALID_ENCODING);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, PATH_INVALID);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, PATH_OVERLONG_U);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, PATH_ENCODED_SEPARATOR);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, PATH_UTF8_VALID);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, PATH_UTF8_INVALID);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, PATH_UTF8_OVERLONG);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, PATH_HALF_FULL_RANGE);

    /* STATUS_LINE_INVALID */
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, STATUS_LINE_INVALID);

    /* HOSTx_INVALID */
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, HOSTU_INVALID);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, HOSTH_INVALID);

    /* URLEN_xxx */
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, URLEN_ENCODED_NUL);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, URLEN_INVALID_ENCODING);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, URLEN_OVERLONG_U);
    MODHTP_PROCESS_PARSER_FLAG(itx, collection, flags, URLEN_HALF_FULL_RANGE);

    /* If flags is not 0 we did not handle one of the bits. */
    if (flags != 0) {
        ib_log_error_tx(itx, "HTP parser unknown flag: 0x%"PRIx64, flags);
        rc = IB_EUNKNOWN;
    }

    return rc;
}

/* -- LibHTP Callbacks -- */

static int modhtp_htp_request_start(
    htp_connp_t *connp)
{
    ib_status_t  irc;
    bool         perror = false;

    /* Check the parser status */
    irc = modhtp_check_parser(connp, true, "Request_line",
                              &perror, NULL, NULL, NULL);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    return HTP_OK;
}

static int modhtp_htp_request_line(
    htp_connp_t   *connp,
    unsigned char *line,
    size_t         len)
{
    htp_tx_t    *tx;
    ib_conn_t   *iconn;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror = false;

    /* Check the parser status */
    irc = modhtp_check_parser(connp, true, "Request_line",
                              &perror, &iconn, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    /* Store the request line if required */
    if ( (itx->request_line->raw == NULL) ||
         (ib_bytestr_length(itx->request_line->raw) == 0) )
    {
        irc = ib_bytestr_dup_mem(&itx->request_line->raw, itx->mp, line, len);
        if (irc != IB_OK) {
            return HTP_ERROR;
        }
    }

    /* Store the request method */
    irc = modhtp_set_bytestr(itx, "Request method", false,
                             tx->request_method, NULL,
                             &(itx->request_line->method));
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    /* Store the request URI */
    irc = modhtp_set_bytestr(itx, "Request URI", false,
                             tx->request_uri, NULL,
                             &(itx->request_line->uri));
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    /* Store the request protocol */
    irc = modhtp_set_bytestr(itx, "Request protocol", false,
                             tx->request_protocol, NULL,
                             &(itx->request_line->protocol));
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    /* Store the transaction URI path. */
    irc = modhtp_set_nulstr(itx, "URI Path", false,
                            tx->parsed_uri->path, "/",
                            &(itx->path));
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    return HTP_OK;
}

static int modhtp_htp_request_headers(
    htp_connp_t *connp)
{
    htp_tx_t    *tx;
    ib_conn_t   *iconn;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror;

    /* Check the parser status */
    irc = modhtp_check_parser(connp, true, "Request headers",
                              &perror, &iconn, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    /* Update the hostname that may have changed with headers. */
    irc = modhtp_set_nulstr(itx, "Hostname", true,
                            tx->parsed_uri->hostname,
                            iconn->local_ipstr,
                            &(itx->hostname));
    if (irc != IB_OK) {
        return irc;
    }

    return HTP_OK;
}

static int modhtp_htp_request_body_data(
    htp_tx_data_t *txdata)
{
    htp_tx_t    *tx;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror;

    /* Check the parser status */
    irc = modhtp_check_parser(txdata->tx->connp, true, "Request_body_data",
                              &perror, NULL, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flags(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    return HTP_OK;
}

static int modhtp_htp_request_trailer(
    htp_connp_t *connp)
{
    htp_tx_t    *tx;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror;

    /* Check the parser status */
    irc = modhtp_check_parser(connp, true, "Request_trailer",
                              &perror, NULL, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flags(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    return HTP_OK;
}

static int modhtp_htp_request_complete(
    htp_connp_t *connp)
{
    htp_tx_t    *tx;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror;

    /* Check the parser status */
    irc = modhtp_check_parser(connp, true, "Request_complete",
                              &perror, NULL, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flags(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    return HTP_OK;
}

static int modhtp_htp_response_line(
    htp_connp_t *connp)
{
    htp_tx_t    *tx;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror;

    /* Check the parser status */
    irc = modhtp_check_parser(connp, true, "Response_line",
                              &perror, NULL, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    /* Store the response protocol */
    irc = modhtp_set_bytestr(itx, "Response protocol", false,
                             tx->response_protocol, NULL,
                             &itx->response_line->protocol);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    /* Store the response status */
    irc = modhtp_set_bytestr(itx, "Response status", false,
                             tx->response_status, NULL,
                             &itx->response_line->status);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    /* Store the request URI */
    irc = modhtp_set_bytestr(itx, "Response message", false,
                             tx->response_message, NULL,
                             &itx->response_line->msg);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }


    if (tx->flags) {
        modhtp_set_parser_flags(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }


    return HTP_OK;
}

static int modhtp_htp_response_headers(
    htp_connp_t *connp)
{
    htp_tx_t    *tx;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror;

    /* Check the parser status */
    irc = modhtp_check_parser(connp, true, "Response_headers",
                              &perror, NULL, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flags(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    return HTP_OK;
}

static int modhtp_htp_response_body_data(
    htp_tx_data_t *txdata)
{
    htp_tx_t    *tx;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror;

    /* Check the parser status */
    irc = modhtp_check_parser(txdata->tx->connp, true, "Response_body_data",
                              &perror, NULL, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flags(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    return HTP_OK;
}

static int modhtp_htp_response_complete(
    htp_connp_t *connp)
{
    htp_tx_t    *tx;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror;

    /* Check the parser status */
    irc = modhtp_check_parser(connp, true, "Response_complete",
                              &perror, NULL, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flags(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    return HTP_OK;
}

static int modhtp_htp_response_trailer(
    htp_connp_t *connp)
{
    htp_tx_t    *tx;
    ib_tx_t     *itx;
    ib_status_t  irc;
    bool         perror;

    /* Check the parser status */
    irc = modhtp_check_parser(connp, true, "Response_trailer",
                              &perror, NULL, &tx, &itx);
    if (irc != IB_OK) {
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flags(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    return HTP_OK;
}


/**
 * Generate IronBee request header fields
 *
 * @param[in] htx libhtp transaction (NULL if not known)
 * @param[in,out] itx IronBee transaction
 *
 * @returns IronBee status code
 */
static ib_status_t modhtp_gen_request_header_fields(
    const htp_tx_t  *htx,
    ib_tx_t         *itx)
{
    ib_field_t *f;
    size_t param_count = 0;
    ib_status_t rc;

    /* If no libhtp transaction provided, get it from the context */
    if (htx == NULL) {
        modhtp_context_t *modctx;
        modctx = modhtp_get_itx_context(itx);
        htx = modctx->htp->in_tx;
        if (htx == NULL) {
            return IB_EINVAL;
        }
        htp_tx_set_user_data((htp_tx_t *)htx, itx);
    }

    modhtp_field_gen_bytestr(itx->data,
                             "request_line",
                             htx->request_line,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri",
                             htx->request_uri_normalized,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_scheme",
                             htx->parsed_uri->scheme,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_username",
                             htx->parsed_uri->username,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_password",
                             htx->parsed_uri->password,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_host",
                             htx->parsed_uri->hostname,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_host",
                             htx->parsed_uri->hostname,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_port",
                             htx->parsed_uri->port,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_path",
                             htx->parsed_uri->path,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_query",
                             htx->parsed_uri->query,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_fragment",
                             htx->parsed_uri->fragment,
                             NULL);

    rc = ib_data_add_list(itx->data, "request_cookies", &f);
    if ( (htx->request_cookies != NULL) &&
         htp_table_size(htx->request_cookies) &&
         (rc == IB_OK) )
    {
        rc = modhtp_table_iterator(itx, htx->request_cookies,
                                   modhtp_field_list_callback, f);
        if (rc != IB_OK) {
            ib_log_warning_tx(itx, "Error adding request cookies");
        }
    }
    else if (rc == IB_OK) {
        ib_log_debug3_tx(itx, "No request cookies");
    }
    else {
        ib_log_error_tx(itx,
                        "Failed to create request cookies list: %s",
                        ib_status_to_string(rc));
    }

    /* Extract the query parameters into the IronBee tx's URI parameters */
    rc = ib_data_add_list(itx->data, "request_uri_params", &f);
    if ( (rc == IB_OK) && (htx->request_params != NULL) ) {
        modhtp_param_iter_data_t idata =
            { f, HTP_SOURCE_QUERY_STRING, 0 };
        rc = modhtp_table_iterator(itx, htx->request_params,
                                   modhtp_param_iter_callback, &idata);
        if (rc != IB_OK) {
            ib_log_warning_tx(itx, "Failed to populate URI params: %s",
                              ib_status_to_string(rc));
        }
        param_count = idata.count;
    }

    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Failed to create request URI parameters: %s",
                        ib_status_to_string(rc));
    }
    else {
        ib_log_debug3_tx(itx, "%zd request URI parameters", param_count);
    }

    return IB_OK;
}

static ib_status_t modhtp_gen_request_fields(
    const htp_tx_t *htx,
    ib_tx_t        *itx)
{
    ib_field_t  *f;
    ib_status_t  rc;

    ib_log_debug3_tx(itx, "LibHTP: modhtp_gen_request_fields");
    if (htx == NULL) {
        return IB_OK;
    }

    /* Use the current parser transaction to generate fields. */
    /// @todo Check htp state, etc.
    size_t param_count = 0;

    rc = ib_data_add_list(itx->data, "request_body_params", &f);
    if ( (rc == IB_OK) && (htx->request_params != NULL) ) {
        modhtp_param_iter_data_t idata =
            { f, HTP_SOURCE_BODY, 0 };
        rc = modhtp_table_iterator(itx, htx->request_params,
                                   modhtp_param_iter_callback, &idata);
        if (rc != IB_OK) {
            ib_log_warning_tx(itx, "Failed to populate body params: %s",
                              ib_status_to_string(rc));
        }
        param_count = idata.count;
    }

    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Failed to create request body parameters: %s",
                        ib_status_to_string(rc));
    }
    else {
        ib_log_debug3_tx(itx, "%zd request body parameters", param_count);
    }

    return IB_OK;
}

static ib_status_t modhtp_gen_response_header_fields(
    htp_tx_t *htx,
    ib_tx_t  *itx)
{
    return IB_OK;
}

static ib_status_t modhtp_gen_response_fields(
    htp_tx_t *htx,
    ib_tx_t  *itx)
{
    return IB_OK;
}

/* -- Parser Provider Interface Implementation -- */

/*****************************************************************************
 * Dragons be here.
 *
 * The parser interface is called indirectly by the server in one of two modes:
 *
 * 1) The server will call the data_in/data_out functions to feed
 *    in raw HTTP data. This data is then parsed and the appropriate
 *    parsed versions of the ib_state_notify_*() functions are called
 *    by the parser.
 *
 * 2) The server will call the parsed versions of the ib_state_notify_*()
 *    functions directly with the parsed HTTP data. Because of limitations
 *    in libhtp, the parsed functions implemented here will reconstruct
 *    a normalized raw HTTP stream and call the data_in/data_out functions
 *    as if it was receiving a raw stream.
 *
 * The interfaces for the two modes are cyclic (raw data functions call
 * the parsed versions and vice versa) and some care is taken to detect
 * which mode is in use so that calls do not endlessly recurse. However,
 * no effort was taken to prevent an incorrectly written server from
 * causing problems and infinite recursion.
 *
 * TODO: Fix libhtp to have an interface that accepts pre-parsed HTTP
 *       data so that reconstructing the raw HTTP is not required. This
 *       will greatly simplify the interface.
 ****************************************************************************/

static ib_status_t modhtp_iface_init(
    ib_provider_inst_t *pi,
    ib_conn_t          *iconn)
{
    ib_engine_t *ib = iconn->ib;
    ib_context_t *ctx = iconn->ctx;
    modhtp_cfg_t *modcfg;
    modhtp_context_t *modctx;
    htp_time_t htv;
    ib_status_t rc;
    int personality;

    /* Get the module config. */
    rc = ib_context_module_config(ctx, IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to fetch module %s config: %s",
                     MODULE_NAME_STR, ib_status_to_string(rc));
        return rc;
    }

    ib_log_debug3(ib, "Creating LibHTP parser");

    /* Create a context. */
    modctx = ib_mpool_calloc(iconn->mp, 1, sizeof(*modctx));
    if (modctx == NULL) {
        return IB_EALLOC;
    }

    /* Figure out the personality to use. */
    personality = modhtp_personality(modcfg->personality);
    if (personality == -1) {
        personality = HTP_SERVER_APACHE_2;
    }

    /* Configure parser. */
    modctx->htp_cfg = htp_config_create();
    if (modctx->htp_cfg == NULL) {
        return IB_EALLOC;
    }
    htp_config_set_server_personality(modctx->htp_cfg, personality);
    /// @todo Make all these configurable???
    modctx->htp_cfg->log_level = HTP_LOG_DEBUG2;
    htp_config_set_tx_auto_destroy(modctx->htp_cfg, 0);
    htp_config_set_generate_request_uri_normalized(modctx->htp_cfg, 1);

    htp_config_register_urlencoded_parser(modctx->htp_cfg);
    htp_config_register_multipart_parser(modctx->htp_cfg);
    htp_config_register_log(modctx->htp_cfg, modhtp_callback_log);

    /* Cookies */
    modctx->htp_cfg->parse_request_cookies = 1;

    /* Setup context and create the parser. */
    modctx->ib = ib;
    modctx->iconn = iconn;
    modctx->modcfg = modcfg;
    modctx->htp = htp_connp_create(modctx->htp_cfg);
    if (modctx->htp == NULL) {
        return IB_EALLOC;
    }

    /* Open the connection */
    htp_connp_open(modctx->htp,
                   iconn->remote_ipstr, iconn->remote_port,
                   iconn->local_ipstr, iconn->local_port,
                   &htv);

    /* Store the context. */
    ib_conn_parser_context_set(iconn, modctx);
    htp_connp_set_user_data(modctx->htp, modctx);

    /* Register callbacks. */
    htp_config_register_request_start(modctx->htp_cfg,
                                      modhtp_htp_request_start);
    htp_config_register_request_line(modctx->htp_cfg,
                                     modhtp_htp_request_line);
    htp_config_register_request_headers(modctx->htp_cfg,
                                        modhtp_htp_request_headers);
    htp_config_register_request_body_data(modctx->htp_cfg,
                                          modhtp_htp_request_body_data);
    htp_config_register_request_trailer(modctx->htp_cfg,
                                        modhtp_htp_request_trailer);
    htp_config_register_request_complete(modctx->htp_cfg,
                                         modhtp_htp_request_complete);
    htp_config_register_response_line(modctx->htp_cfg,
                                      modhtp_htp_response_line);
    htp_config_register_response_headers(modctx->htp_cfg,
                                         modhtp_htp_response_headers);
    htp_config_register_response_body_data(modctx->htp_cfg,
                                           modhtp_htp_response_body_data);
    htp_config_register_response_trailer(modctx->htp_cfg,
                                         modhtp_htp_response_trailer);
    htp_config_register_response_complete(modctx->htp_cfg,
                                          modhtp_htp_response_complete);

    return IB_OK;
}

static ib_status_t modhtp_iface_disconnect(
    ib_provider_inst_t *pi,
    ib_conn_t          *iconn)
{
    ib_engine_t *ib = iconn->ib;
    modhtp_context_t *modctx;

    /* Fetch context from the connection. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(iconn);

    ib_log_debug3(ib, "Destroying LibHTP parser");

    /* Destroy the parser on disconnect. */
    htp_connp_destroy_all(modctx->htp);

    /* Destroy the configuration. */
    htp_config_destroy(modctx->htp_cfg);

    return IB_OK;
}

static ib_status_t modhtp_iface_tx_init(
    ib_provider_inst_t *pi,
    ib_tx_t            *itx)
{
    return IB_OK;
}

static ib_status_t modhtp_iface_tx_cleanup(
    ib_provider_inst_t *pi,
    ib_tx_t            *itx)
{
    modhtp_context_t *modctx;
    htp_tx_t *in_tx;
    htp_tx_t *out_tx;

    assert(itx != NULL);
    assert(itx->conn != NULL);

    /* Fetch context from the connection. */
    modctx = modhtp_get_itx_context(itx);

    /* Use the current parser transaction to generate fields. */
    out_tx = modctx->htp->out_tx;

    /* Reset libhtp connection parser. */
    if (out_tx != NULL) {
        ib_tx_t *tx_itx = htp_tx_get_user_data(out_tx);
        if (tx_itx == itx) {
            ib_log_debug_tx(itx,
                            "Destroying LibHTP outbound transaction=%p id=%s",
                            out_tx, itx->id);
            modctx->htp->out_status = HTP_STREAM_OPEN;
            modctx->htp->out_state = htp_connp_RES_IDLE;
            htp_tx_destroy(out_tx);
            modctx->htp->out_tx = NULL;
            if (out_tx == modctx->htp->in_tx) {
                modctx->htp->in_tx = NULL;
            }
        }
    }

    in_tx = modctx->htp->in_tx;
    if (in_tx != NULL) {
        ib_tx_t *tx_itx = htp_tx_get_user_data(in_tx);
        if (tx_itx == itx) {
            ib_log_debug_tx(itx,
                            "Destroying LibHTP inbound transaction=%p id=%s",
                            in_tx, itx->id);
            modctx->htp->in_status = HTP_STREAM_OPEN;
            htp_tx_destroy(in_tx);
            modctx->htp->in_tx = NULL;
            modctx->htp->in_state = htp_connp_REQ_IDLE;
            if (in_tx == modctx->htp->out_tx) {
                modctx->htp->out_tx = NULL;
            }
        }
    }

    htp_connp_clear_error(modctx->htp);
    return IB_OK;
}

static ib_status_t modhtp_iface_request_line(
    ib_provider_inst_t   *pi,
    ib_tx_t              *itx,
    ib_parsed_req_line_t *line)
{
    assert(pi != NULL);
    assert(itx != NULL);
    assert(line != NULL);

    modhtp_context_t *modctx;
    htp_connp_t      *htp;
    htp_tx_t         *htx;
    ib_status_t       irc;
    htp_status_t      hrc;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }
    if ( (line->method == NULL) || (line->uri == NULL) ) {
        return IB_EINVAL;
    }

    ib_log_debug_tx(itx,
                    "SEND REQUEST LINE TO LIBHTP: modhtp_iface_request_line");

    /* Fetch context from the connection */
    modctx = modhtp_get_itx_context(itx);
    htp = modctx->htp;

    /* Create the transaction */
    htx = htp_connp_tx_create(htp);
    if ( (htx == NULL) || (htp->in_tx != htx) ) {
        return IB_EUNKNOWN;
    }
    htp_tx_set_user_data(htx, itx);
    irc = ib_tx_set_module_data(itx, IB_MODULE_STRUCT_PTR, htx);
    if (irc != IB_OK) {
        return irc;
    }

    /* Start the request */
    hrc = htp_tx_state_request_start(htp->in_tx);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    /* Hand the whole request line to libhtp */
    hrc = htp_tx_req_set_line(htp->in_tx,
                              (const char *)ib_bytestr_const_ptr(line->raw),
                              ib_bytestr_length(line->raw),
                              HTP_ALLOC_COPY);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    /* Update the state */
    hrc = htp_tx_state_request_line(htx);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    return IB_OK;
}

static ib_status_t modhtp_iface_request_header_data(
    ib_provider_inst_t         *pi,
    ib_tx_t                    *itx,
    ib_parsed_header_wrapper_t *header)
{
    assert(pi != NULL);
    assert(itx != NULL);
    assert(header != NULL);

    ib_status_t   irc;
    htp_tx_t     *htx;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND REQUEST HEADER DATA TO LIBHTP: "
                    "modhtp_iface_request_header_data");

    /* Get the libhtp transaction */
    htx = modhtp_get_htx(itx);
    if (htx == NULL) {
        return IB_EUNKNOWN;
    }

    /* Hand the headers off to libhtp */
    irc = modhtp_set_header(itx, htx, header, htp_tx_req_set_header);
    if (irc != IB_OK) {
        return irc;
    }

    return IB_OK;
}

static ib_status_t modhtp_iface_request_header_finished(
    ib_provider_inst_t *pi,
    ib_tx_t            *itx)
{
    assert(pi != NULL);
    assert(itx != NULL);

    ib_status_t irc;
    htp_tx_t *htx;
    htp_status_t  hrc;

    /* Get the libhtp transaction */
    htx = modhtp_get_htx(itx);
    if (htx == NULL) {
        return IB_EUNKNOWN;
    }

    /* Update the state */
    hrc = htp_tx_state_request_headers(htx);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    /* Generate header fields. */
    irc = modhtp_gen_request_header_fields(htx, itx);
    if (irc != IB_OK) {
        return irc;
    }

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND REQUEST HEADER FINISHED TO LIBHTP: "
                    "modhtp_iface_request_header_finished");

    return IB_OK;
}

static ib_status_t modhtp_iface_request_body_data(
    ib_provider_inst_t *pi,
    ib_tx_t            *itx,
    ib_txdata_t        *txdata)
{
    assert(pi != NULL);
    assert(itx != NULL);
    assert(txdata != NULL);

    htp_status_t hrc;
    htp_tx_t *htx;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND REQUEST BODY DATA TO LIBHTP: "
                    "modhtp_iface_request_body_data");

    /* Get the libhtp transaction */
    htx = modhtp_get_htx(itx);
    if (htx == NULL) {
        return IB_EUNKNOWN;
    }

    /* Hand the request body data to libhtp. */
    hrc = htp_tx_req_process_body_data(htx, txdata->data, txdata->dlen);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    return IB_OK;
}

static ib_status_t modhtp_iface_request_finished(
    ib_provider_inst_t *pi,
    ib_tx_t            *itx)
{
    assert(pi != NULL);
    assert(itx != NULL);

    ib_status_t irc;
    htp_tx_t *htx;

    /* Get the libhtp transaction */
    htx = modhtp_get_htx(itx);
    if (htx == NULL) {
        return IB_EUNKNOWN;
    }

    /* Generate fields. */
    irc = modhtp_gen_request_fields(htx, itx);
    return irc;
}

static ib_status_t modhtp_iface_response_line(
    ib_provider_inst_t    *pi,
    ib_tx_t               *itx,
    ib_parsed_resp_line_t *line)
{
    assert(pi != NULL);
    assert(itx != NULL);

    htp_status_t      hrc;
    htp_tx_t         *htx;

    /* This is not valid for HTTP/0.9 requests. */
    if (line == NULL) {
        return IB_OK;
    }

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND RESPONSE LINE TO LIBHTP: "
                    "modhtp_iface_response_line");

    /* Get the libhtp transaction */
    htx = modhtp_get_htx(itx);
    if (htx == NULL) {
        return IB_EUNKNOWN;
    }

    /* Start the response transaction */
    hrc = htp_tx_state_response_start(htx);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    /* Hand off the status line */
    hrc = htp_tx_res_set_status_line(
        htx,
        (const char *)ib_bytestr_const_ptr(line->raw),
        ib_bytestr_length(line->raw),
        HTP_ALLOC_COPY);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    /* Set the state */
    hrc = htp_tx_state_response_line(htx);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    return IB_OK;
}

static ib_status_t modhtp_iface_response_header_data(
    ib_provider_inst_t         *pi,
    ib_tx_t                    *itx,
    ib_parsed_header_wrapper_t *header)
{
    assert(pi != NULL);
    assert(itx != NULL);
    assert(header != NULL);

    ib_status_t  irc;
    htp_tx_t    *htx;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND RESPONSE HEADER DATA TO LIBHTP: "
                    "modhtp_iface_response_header_data");

    /* Get the libhtp transaction */
    htx = modhtp_get_htx(itx);
    if (htx == NULL) {
        return IB_EUNKNOWN;
    }

    /* Hand the response headers off to libhtp */
    irc = modhtp_set_header(itx, htx, header, htp_tx_res_set_header);
    if (irc != IB_OK) {
        return irc;
    }

    return IB_OK;
}

static ib_status_t modhtp_iface_response_header_finished(
    ib_provider_inst_t *pi,
    ib_tx_t *itx)
{
    assert(pi != NULL);
    assert(itx != NULL);

    htp_status_t hrc;
    ib_status_t irc;
    htp_tx_t *htx;

    /* Get the libhtp transaction */
    htx = modhtp_get_htx(itx);
    if (htx == NULL) {
        return IB_EUNKNOWN;
    }

    /* Update the state */
    hrc = htp_tx_state_response_headers(htx);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    /* Generate header fields. */
    irc = modhtp_gen_response_header_fields(htx, itx);
    if (irc != IB_OK) {
        return irc;
    }

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND RESPONSE HEADER FINISHED TO LIBHTP: "
                    "modhtp_iface_response_header_finished");

    return IB_OK;
}

static ib_status_t modhtp_iface_response_body_data(ib_provider_inst_t *pi,
                                                   ib_tx_t *itx,
                                                   ib_txdata_t *txdata)
{
    assert(pi != NULL);
    assert(itx != NULL);
    assert(txdata != NULL);

    htp_tx_t *htx;
    htp_status_t hrc;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND RESPONSE BODY DATA TO LIBHTP: "
                    "modhtp_iface_response_body_data");

    /* Get the libhtp transaction */
    htx = modhtp_get_htx(itx);
    if (htx == NULL) {
        return IB_EUNKNOWN;
    }

    hrc = htp_tx_res_process_body_data(htx, txdata->data, txdata->dlen);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    return IB_OK;
}

static ib_status_t modhtp_iface_response_finished(ib_provider_inst_t *pi,
                                                  ib_tx_t *itx)
{
    assert(pi != NULL);
    assert(itx != NULL);

    ib_status_t irc;
    htp_tx_t *htx = modhtp_get_htx(itx);

    if (htx == NULL) {
        return IB_EUNKNOWN;
    }

    /* Generate fields. */
    irc = modhtp_gen_response_fields(htx, itx);

    return irc;
}

static IB_PROVIDER_IFACE_TYPE(parser) modhtp_parser_iface = {
    IB_PROVIDER_IFACE_HEADER_DEFAULTS,

    /* Connection Init/Cleanup */
    modhtp_iface_init,
    NULL,

    /* Connect/Disconnect */
    NULL,
    modhtp_iface_disconnect,

    /* Transaction Init/Cleanup */
    modhtp_iface_tx_init,
    modhtp_iface_tx_cleanup,

    /* Request */
    modhtp_iface_request_line,
    modhtp_iface_request_header_data,
    modhtp_iface_request_header_finished,
    modhtp_iface_request_body_data,
    modhtp_iface_request_finished,

    /* Response */
    modhtp_iface_response_line,
    modhtp_iface_response_header_data,
    modhtp_iface_response_header_finished,
    modhtp_iface_response_body_data,
    modhtp_iface_response_finished,
};


/* -- Module Routines -- */

static ib_status_t modhtp_init(ib_engine_t *ib,
                               ib_module_t *m,
                               void        *cbdata)
{
    ib_status_t rc;

    /* Register as a parser provider. */
    rc = ib_provider_register(ib, IB_PROVIDER_TYPE_PARSER,
                              MODULE_NAME_STR, NULL,
                              &modhtp_parser_iface,
                              NULL);
    if (rc != IB_OK) {
        ib_log_error(ib,
                     MODULE_NAME_STR ": Error registering htp parser provider: "
                     "%s", ib_status_to_string(rc));
        return IB_OK;
    }
    modhtp_ib = ib;

    return IB_OK;
}

static ib_status_t modhtp_context_close(ib_engine_t *ib,
                                        ib_module_t *m,
                                        ib_context_t *ctx,
                                        void *cbdata)
{
    ib_status_t rc;
    ib_provider_inst_t *pi;

    /* If there is not a parser set, then use this parser. */
    pi = ib_parser_provider_get_instance(ctx);
    if (pi == NULL) {
        ib_log_debug(ib, "Using \"%s\" parser by default in context %s.",
                     MODULE_NAME_STR, ib_context_full_get(ctx));

        /* Lookup/set this parser provider instance. */
        rc = ib_provider_instance_create(ib, IB_PROVIDER_TYPE_PARSER,
                                         MODULE_NAME_STR, &pi,
                                         ib_engine_pool_main_get(ib), NULL);
        if (rc != IB_OK) {
            ib_log_alert(ib, "Failed to create %s parser instance: %s",
                         MODULE_NAME_STR, ib_status_to_string(rc));
            return rc;
        }

        rc = ib_parser_provider_set_instance(ctx, pi);
        if (rc != IB_OK) {
            ib_log_alert(ib, "Failed to set %s as default parser: %s",
                         MODULE_NAME_STR, ib_status_to_string(rc));
            return rc;
        }
        pi = ib_parser_provider_get_instance(ctx);
    }

    return IB_OK;
}

static IB_CFGMAP_INIT_STRUCTURE(modhtp_config_map) = {
    IB_CFGMAP_INIT_ENTRY(
        MODULE_NAME_STR ".personality",
        IB_FTYPE_NULSTR,
        modhtp_cfg_t,
        personality
    ),
    IB_CFGMAP_INIT_LAST
};

/**
 * Module structure.
 *
 * This structure defines some metadata, config data and various functions.
 */
IB_MODULE_INIT(
    IB_MODULE_HEADER_DEFAULTS,           /**< Default metadata */
    MODULE_NAME_STR,                     /**< Module name */
    IB_MODULE_CONFIG(&modhtp_global_cfg),/**< Global config data */
    modhtp_config_map,                   /**< Configuration field map */
    NULL,                                /**< Config directive map */
    modhtp_init,                         /**< Initialize function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Finish function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Context open function */
    NULL,                                /**< Callback data */
    modhtp_context_close,                /**< Context close function */
    NULL,                                /**< Callback data */
    NULL,                                /**< Context destroy function */
    NULL                                 /**< Callback data */
);
