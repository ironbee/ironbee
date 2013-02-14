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
    bool            parsed_data;  /**< Set when processing parsed data */
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

    switch(log->level) {
        /* Parsing issues are unusual but not IronBee failures. */
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
 * modhtp_table_iterator() callback to handle headers
 *
 * @param[in] tx IronBee transaction
 * @param[in] key Key of the key/value pair
 * @param[in] vptr Pointer to value of the key/value pair
 * @param[in] data Function specific data (nv pair list)
 *
 * @returns Status code (IB_OK)
 */
static ib_status_t modhtp_header_callback(
    const ib_tx_t *tx,
    const bstr    *key,
    void          *vptr,
    void          *data)
{
    assert(tx != NULL);
    assert(key != NULL);
    assert(vptr != NULL);
    assert(data != NULL);

    ib_parsed_header_wrapper_t *ibhdrs = (ib_parsed_header_wrapper_t *)data;
    const htp_header_t *hdr = (const htp_header_t *)vptr;
    ib_status_t rc;

    rc = ib_parsed_name_value_pair_list_add(ibhdrs,
                                            (const char *)bstr_ptr(key),
                                            bstr_len(key),
                                            (const char *)bstr_ptr(hdr->value),
                                            bstr_len(hdr->value));
    if (rc != IB_OK) {
        ib_log_error_tx(tx,
                        "Error adding request header name / value: %s",
                        ib_status_to_string(rc));
    }

    /* Always return IB_OK */
    return IB_OK;
}

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
 * @param[in] vptr Pointer to value of the key/value pair
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
 * @note If @fn returns an error, it will cause an error to returned
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
 * Set a generic request / response item for libhpt by creating a
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

    /* Hand it off to libhtp */
    hrc = fn(htx, data, dlen, HTP_ALLOC_COPY);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    return IB_OK;
}

/**
 * Set a generic request / response item for libhpt by creating a
 * c-style (nul terminated) string from the non-terminated string @a data
 * of length @a dlen and then calling @a fn with the new c string.
 *
 * @param[in] itx IronBee transaction
 * @param[in] htx HTP transaction
 * @param[in] bstr Byestring to set
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
    return modhtp_set_data(itx, htx, 
                           (const char *)ib_bytestr_const_ptr(bstr),
                           ib_bytestr_length(bstr),
                           fn);
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
        hrc = fn(htx,
                 (const char *)ib_bytestr_const_ptr(node->name),
                 ib_bytestr_length(node->name),
                 (const char *)ib_bytestr_const_ptr(node->value),
                 ib_bytestr_length(node->value),
                 HTP_ALLOC_COPY);
        if (hrc != HTP_OK) {
            return IB_EUNKNOWN;
        }
    }

    return IB_OK;
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
static ib_status_t modhtp_add_flag_to_collection(
    ib_tx_t    *itx,
    const char *collection_name,
    const char *flag
)
{
    ib_status_t rc;
    ib_field_t *f;

    if ( (itx == NULL) || (itx->data == NULL) ) {
        ib_log_error_tx(itx,
                        "Not adding flag %s field %s to NULL transaction",
                        collection_name, flag);
        return IB_EUNKNOWN;
    }

    rc = ib_data_get(itx->data, collection_name, &f);
    if (f == NULL) {
        rc = ib_data_add_list(itx->data, collection_name, &f);
    }
    if (rc == IB_OK && f != NULL) {
        ib_field_t *lf;
        ib_num_t value = 1;
        ib_field_create(&lf,
                        itx->mp,
                        IB_FIELD_NAME(flag),
                        IB_FTYPE_NUM,
                        ib_ftype_num_in(&value));
        rc = ib_field_list_add(f, lf);
        if (rc != IB_OK) {
            ib_log_warning_tx(itx, "Failed to add %s field: %s",
                              collection_name, flag);
        }
    }
    else {
        ib_log_warning_tx(itx, "Failed to add flag collection: %s",
                          collection_name);
    }

    return rc;
}

static ib_status_t modhtp_set_parser_flag(
    ib_tx_t       *itx,
    const char    *collection_name,
    unsigned int   flags)
{
    ib_status_t rc = IB_OK;

    if (flags & HTP_HOST_AMBIGUOUS) {
        flags ^= HTP_HOST_AMBIGUOUS;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "HOST_AMBIGUOUS");
    }
    if (flags & HTP_FIELD_INVALID) {
        flags ^= HTP_FIELD_INVALID;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "FIELD_INVALID");
    }
    if (flags & HTP_FIELD_LONG) {
        flags ^= HTP_FIELD_LONG;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "FIELD_LONG");
    }
    if (flags & HTP_FIELD_UNPARSEABLE) {
        flags ^= HTP_FIELD_UNPARSEABLE;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "FIELD_UNPARSEABLE");
    }
    if (flags & HTP_HOST_MISSING) {
        flags ^= HTP_HOST_MISSING;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "HOST_MISSING");
    }
    if (flags & HTP_INVALID_CHUNKING) {
        flags ^= HTP_INVALID_CHUNKING;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "INVALID_CHUNKING");
    }
    if (flags & HTP_INVALID_FOLDING) {
        flags ^= HTP_INVALID_FOLDING;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "INVALID_FOLDING");
    }
    if (flags & HTP_MULTI_PACKET_HEAD) {
        flags ^= HTP_MULTI_PACKET_HEAD;
        /* This will trigger for parsed data until LibHTP has a
         * proper parsed data API, so ignore it for parsed data
         * for now.
         *
         * FIXME: Remove when LibHTP has a proper API.
         */
        if (! ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
            rc = modhtp_add_flag_to_collection(itx, collection_name,
                                               "MULTI_PACKET_HEAD");
        }
    }
    if (flags & HTP_PATH_ENCODED_NUL) {
        flags ^= HTP_PATH_ENCODED_NUL;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "PATH_ENCODED_NUL");
    }
    if (flags & HTP_PATH_ENCODED_SEPARATOR) {
        flags ^= HTP_PATH_ENCODED_SEPARATOR;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "PATH_ENCODED_SEPARATOR");
    }
    if (flags & HTP_PATH_FULLWIDTH_EVASION) {
        flags ^= HTP_PATH_FULLWIDTH_EVASION;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "PATH_FULLWIDTH_EVASION");
    }
    if (flags & HTP_PATH_INVALID_ENCODING) {
        flags ^= HTP_PATH_INVALID_ENCODING;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "PATH_INVALID_ENCODING");
    }
    if (flags & HTP_PATH_OVERLONG_U) {
        flags ^= HTP_PATH_OVERLONG_U;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "PATH_OVERLONG_U");
    }
    if (flags & HTP_PATH_UTF8_INVALID) {
        flags ^= HTP_PATH_UTF8_INVALID;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "PATH_UTF8_INVALID");
    }
    if (flags & HTP_PATH_UTF8_OVERLONG) {
        flags ^= HTP_PATH_UTF8_OVERLONG;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "PATH_UTF8_OVERLONG");
    }
    if (flags & HTP_PATH_UTF8_VALID) {
        flags ^= HTP_PATH_UTF8_VALID;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "PATH_UTF8_VALID");
    }
    if (flags & HTP_REQUEST_SMUGGLING) {
        flags ^= HTP_REQUEST_SMUGGLING;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "REQUEST_SMUGGLING");
    }
    if (flags & HTP_STATUS_LINE_INVALID) {
        flags ^= HTP_STATUS_LINE_INVALID;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "STATUS_LINE_INVALID");
    }
    if (flags & HTP_HOST_INVALID) {
        flags ^= HTP_HOST_INVALID;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "HOST_INVALID");
    }

    /* If flags is not 0 we did not handle one of the bits. */
    if (flags != 0) {
        ib_log_error_tx(itx, "HTP parser unknown flag: 0x%08x", flags);
        rc = IB_EUNKNOWN;
    }

    return rc;
}

/* -- LibHTP Callbacks -- */

static int modhtp_htp_request_start(
    htp_connp_t *connp)
{
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;
    ib_status_t rc;
    htp_tx_t *tx;

    /* If this is a parsed data transaction, then use the existing
     * transaction structure, otherwise create one.
     */
    if (modctx->parsed_data) {
        itx = iconn->tx;
        if (itx == NULL) {
            ib_log_error(ib, "TX Start: No ironbee transaction available.");
            return HTP_ERROR;
        }
        ib_log_debug3(ib, "PARSED TX p=%p id=%s", itx, itx->id);
    }
    else {
        /* Create the transaction structure. */
        rc = ib_tx_create(&itx, iconn, NULL);
        if (rc != IB_OK) {
            /// @todo Set error.
            ib_log_debug3(ib, "Failed to create IronBee transaction for %p",
                          (void *)connp->in_tx);
            return HTP_ERROR;
        }
        ib_log_debug3(ib, "Created ironbee transaction %p structure for %p",
                      (void *)itx, (void *)connp->in_tx);
    }

    /* Store this as the current transaction. */
    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == HTP_STREAM_ERROR) {
        ib_log_error_tx(itx, "HTP Parser Error");
    }
    tx = connp->in_tx;
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Associate the ironbee transaction with the libhtp transaction. */
    htp_tx_set_user_data(tx, itx);

    return HTP_OK;
}

static int modhtp_htp_request_line(
    htp_connp_t *connp)
{
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_parsed_req_line_t *req_line;
    ib_tx_t *itx;
    ib_status_t rc;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Request Line: No ironbee transaction available.");
        return HTP_ERROR;
    }

    /* Store the transaction URI path. */
    if ((tx->parsed_uri != NULL) && (tx->parsed_uri->path != NULL)) {
        itx->path = ib_mpool_memdup_to_str(itx->mp, bstr_ptr(tx->parsed_uri->path), bstr_len(tx->parsed_uri->path));
    }
    else {
        ib_log_debug_tx(itx, "No uri path in the request line.");
    }

    if (itx->path == NULL) {
        ib_log_debug_tx(itx, "Unknown URI path - using /");
        /// @todo Probably should set a flag here
        itx->path = ib_mpool_strdup(itx->mp, "/");
    }

    /* Store the hostname if it was parsed with the URI. */
    if ((tx->parsed_uri != NULL) && (tx->parsed_uri->hostname != NULL)) {
        itx->hostname = ib_mpool_memdup_to_str(itx->mp, bstr_ptr(tx->parsed_uri->hostname), bstr_len(tx->parsed_uri->hostname));
    }
    else {
        ib_log_debug_tx(itx, "No hostname in the request line.");
    }

    if (itx->hostname == NULL) {
        ib_log_debug_tx(itx,
                        "Unknown hostname - using ip: %s",
                        iconn->local_ipstr);
        /// @todo Probably should set a flag here
        itx->hostname = ib_mpool_strdup(itx->mp, iconn->local_ipstr);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        return HTP_OK;
    }

    /* Allocate and fill the parsed request line object */
    // FIXME: libhtp bstr_{ptr,len} should work for NULL bstr
    rc = ib_parsed_req_line_create(itx,
                                   &req_line,
                                   (char *)bstr_ptr(tx->request_line),
                                   bstr_len(tx->request_line),
                                   (char *)bstr_ptr(tx->request_method),
                                   bstr_len(tx->request_method),
                                   (char *)bstr_ptr(tx->request_uri),
                                   bstr_len(tx->request_uri),
                                   (tx->request_protocol == NULL
                                    ? NULL
                                    : (char *)bstr_ptr(tx->request_protocol)),
                                   (tx->request_protocol == NULL
                                    ? 0
                                    : bstr_len(tx->request_protocol)));
    if (rc != IB_OK) {
        ib_log_error_tx(itx,
                        "Error creating parsed request line: %s",
                        ib_status_to_string(rc));
    }

    /* Tell the engine that the request started. */
    rc = ib_state_notify_request_started(ib, itx, req_line);
    if (rc != IB_OK) {
        ib_log_error_tx(itx,
                        "Error notifying request started: %s",
                        ib_status_to_string(rc));
        return HTP_ERROR;
    }
    else if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
        // TODO: Check flags for those that make further parsing impossible?
    }

    return HTP_OK;
}

static int modhtp_htp_request_headers(
    htp_connp_t *connp)
{
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;
    ib_status_t rc;
    ib_parsed_header_wrapper_t *ibhdrs;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction and notify the engine
     * that the request header is now available.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Request Headers: No ironbee transaction available.");
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    /* Update the hostname that may have changed with headers. */
    if ((tx->parsed_uri != NULL) && (tx->parsed_uri->hostname != NULL)) {
        itx->hostname = ib_mpool_memdup_to_str(
            itx->mp,
            bstr_ptr(tx->parsed_uri->hostname),
            bstr_len(tx->parsed_uri->hostname));
    }
    else {
        ib_log_debug_tx(itx, "No hostname in the request header.");
    }

    if (itx->hostname == NULL) {
        ib_log_debug_tx(itx,
                        "Unknown hostname - using ip: %s",
                        iconn->local_ipstr);
        /// @todo Probably should set a flag here
        itx->hostname = ib_mpool_strdup(itx->mp, iconn->local_ipstr);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        return HTP_OK;
    }

    /* Copy the request fields into a parse name value pair list object */
    rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, itx);
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Error creating header wrapper: %s",
                        ib_status_to_string(rc));
    }
    else {
        rc = modhtp_table_iterator(itx, tx->request_headers,
                                   modhtp_header_callback, ibhdrs);
        if (rc != IB_OK) {
            ib_log_error_tx(itx,
                            "Error adding request header name / value: %s",
                            ib_status_to_string(rc));
        }
    }

    rc = ib_state_notify_request_header_data(ib, itx, ibhdrs);
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Error notifying request header data: %s",
                        ib_status_to_string(rc));
    }

    rc = ib_state_notify_request_header_finished(ib, itx);
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Error notifying request header finished: %s",
                        ib_status_to_string(rc));
    }

    return HTP_OK;
}

static int modhtp_htp_request_body_data(
    htp_tx_data_t *txdata)
{
    htp_connp_t *connp = txdata->tx->connp;
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_status_t rc;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Request Body: No ironbee transaction available.");
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        return HTP_OK;
    }

    /* Notify the engine of any request body data. */
    if (txdata->data != NULL) {
        ib_txdata_t itxdata;
        itxdata.dlen = txdata->len;
        itxdata.data = (uint8_t *)txdata->data;
        rc = ib_state_notify_request_body_data(ib, itx, &itxdata);
        if (rc != IB_OK) {
            ib_log_error_tx(itx,
                            "ib_state_notify_request_body_data() failed: %s",
                            ib_status_to_string(rc));
        }
    }

    return HTP_OK;
}

static int modhtp_htp_request_trailer(
    htp_connp_t *connp)
{
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Request Trailer: No ironbee transaction available.");
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        return HTP_OK;
    }

    /// @todo Notify tx_datain_event w/request trailer
    ib_log_debug_tx(itx,
        "TODO: tx_datain_event w/request trailer: tx=%p", itx);

    return HTP_OK;
}

static int modhtp_htp_request_complete(
    htp_connp_t *connp)
{
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction, determine if this is a no-body
     * request and notify the engine that the request body is available
     * and is now finished.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Request: No ironbee transaction available.");
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        return HTP_OK;
    }

    ib_state_notify_request_finished(ib, itx);

    return HTP_OK;
}

static int modhtp_htp_response_line(
    htp_connp_t *connp)
{
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_parsed_resp_line_t *resp_line;
    ib_status_t rc;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->out_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Response Line: No ironbee transaction available.");
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        return HTP_OK;
    }

    /* Allocate and fill the parsed response line object */
    // FIXME: libhtp bstr_{ptr,len} should work for NULL bstr
    rc = ib_parsed_resp_line_create(itx,
                                    &resp_line,
                                    (char *)bstr_ptr(tx->response_line),
                                    bstr_len(tx->response_line),
                                    (char *)bstr_ptr(tx->response_protocol),
                                    bstr_len(tx->response_protocol),
                                    (char *)bstr_ptr(tx->response_status),
                                    bstr_len(tx->response_status),
                                    (tx->response_message == NULL
                                     ? NULL
                                     : (char *)bstr_ptr(tx->response_message)),
                                    (tx->response_message == NULL
                                     ? 0
                                     : bstr_len(tx->response_message)));
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Error creating parsed response line: %s",
                        ib_status_to_string(rc));
        return HTP_ERROR;
    }

    /* Tell the engine that the response started. */
    rc = ib_state_notify_response_started(ib, itx, resp_line);
    if (rc != IB_OK) {
        ib_log_error_tx(itx,
                        "Error notifying response started: %s",
                        ib_status_to_string(rc));
        return HTP_ERROR;
    }
    else if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
        if (tx->flags & HTP_STATUS_LINE_INVALID) {
            // FIXME: Why is this not an error???
            ib_log_error_tx(itx, "Error parsing response line.");
            return HTP_ERROR;
        }
    }

    return HTP_OK;
}

static int modhtp_htp_response_headers(
    htp_connp_t *connp)
{
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;
    ib_status_t rc;
    ib_parsed_header_wrapper_t *ibhdrs;

    /* Use the current parser transaction to generate fields. */
    if (connp->out_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction and notify the engine
     * that the response header is now available.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Response Headers: No ironbee transaction available.");
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        return HTP_OK;
    }

    /* Copy the response fields into a parse name value pair list object */
    rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, itx);
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Error creating header wrapper: %s",
                        ib_status_to_string(rc));
    }
    else {
        rc = modhtp_table_iterator(itx, tx->response_headers,
                                   modhtp_header_callback, ibhdrs);
        if (rc != IB_OK) {
            ib_log_error_tx(itx,
                            "Error adding response header name / value: %s",
                            ib_status_to_string(rc));
        }
    }

    rc = ib_state_notify_response_header_data(ib, itx, ibhdrs);
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Error notifying response header data: %s",
                        ib_status_to_string(rc));
    }

    rc = ib_state_notify_response_header_finished(ib, itx);
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Error notifying response header finished: %s",
                        ib_status_to_string(rc));
    }

    return HTP_OK;
}

static int modhtp_htp_response_body_data(
    htp_tx_data_t *txdata)
{
    htp_connp_t *connp = txdata->tx->connp;
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_status_t rc;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->out_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Response Body: No ironbee transaction available.");
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        return HTP_OK;
    }

    /* If this is connection data and the response has not yet
     * started, then LibHTP has interpreted this as the response
     * body. Instead, return an error.
     */
    else if (!ib_tx_flags_isset(itx, IB_TX_FHTTP09|IB_TX_FRES_STARTED)) {
        ib_log_info_tx(itx,
                       "LibHTP parsing error: "
                       "found response data instead of a response line");
        return HTP_ERROR;
    }

    /* Notify the engine of any response body data. */
    if (txdata->data != NULL) {
        ib_txdata_t itxdata;
        itxdata.dlen = txdata->len;
        itxdata.data = (uint8_t *)txdata->data;
        rc = ib_state_notify_response_body_data(ib, itx, &itxdata);
        if (rc != IB_OK) {
            ib_log_error_tx(itx,
                            "ib_state_notify_response_body_data() failed: %s",
                            ib_status_to_string(rc));
        }
    }

    return HTP_OK;
}

static int modhtp_htp_response_complete(
    htp_connp_t *connp)
{
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->out_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction and notify the engine
     * that the response body is available, the response
     * is finished and logging has begun.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Response: No ironbee transaction available.");
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        return HTP_OK;
    }

    ib_state_notify_response_finished(ib, itx);

    /* NOTE: ib_state_response_finished() triggers tx_destroy.  As a result, tx
     * is no longer valid */
    /* ib_state_notify_response_finished(ib, itx); */

    /* Destroy the transaction. */
    ib_log_debug3_tx(itx, "Destroying transaction structure");
    ib_tx_destroy(itx);

    /* NOTE: The htp transaction is destroyed in modhtp_tx_cleanup() */

    return HTP_OK;
}

static int modhtp_htp_response_trailer(
    htp_connp_t *connp)
{
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    /// @todo Check htp state, etc.
    if (connp->out_status == HTP_STREAM_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        return HTP_ERROR;
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "Response Trailer: No ironbee transaction available.");
        return HTP_ERROR;
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    /// @todo Notify tx_dataout_event w/response trailer
    ib_log_debug_tx(
        itx,
        "TODO: tx_dataout_event w/response trailer: tx=%p",
        itx
    );

    return HTP_OK;
}


static ib_status_t modhtp_gen_request_header_fields(
    ib_provider_inst_t *pi,
    ib_tx_t            *itx)
{
    ib_context_t *ctx = itx->ctx;
    ib_conn_t *iconn = itx->conn;
    ib_field_t *f;
    modhtp_cfg_t *modcfg;
    modhtp_context_t *modctx;
    size_t param_count = 0;
    htp_tx_t *tx;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(ctx, IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(itx, "Failed to fetch module %s config: %s",
                        MODULE_NAME_STR, ib_status_to_string(rc));
        return rc;
    }

    /* Fetch context from the connection. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(iconn);

    /* Use the current parser transaction to generate fields. */
    /// @todo Check htp state, etc.
    tx = modctx->htp->in_tx;
    if (tx == NULL) {
        return IB_OK;
    }

    htp_tx_set_user_data(tx, itx);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_scheme",
                             tx->parsed_uri->scheme,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_username",
                             tx->parsed_uri->username,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_password",
                             tx->parsed_uri->password,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_host",
                             tx->parsed_uri->hostname,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_host",
                             tx->parsed_uri->hostname,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_port",
                             tx->parsed_uri->port,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_path",
                             tx->parsed_uri->path,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_query",
                             tx->parsed_uri->query,
                             NULL);

    modhtp_field_gen_bytestr(itx->data,
                             "request_uri_fragment",
                             tx->parsed_uri->fragment,
                             NULL);

    rc = ib_data_add_list(itx->data, "request_cookies", &f);
    if (   (tx->request_cookies != NULL)
           && htp_table_size(tx->request_cookies)
           && (rc == IB_OK))
    {
        rc = modhtp_table_iterator(itx, tx->request_cookies,
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
    if ( (rc == IB_OK) && (tx->request_params != NULL) ) {
        modhtp_param_iter_data_t idata =
            { f, HTP_SOURCE_QUERY_STRING, 0 };
        rc = modhtp_table_iterator(itx, tx->request_params,
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
    ib_provider_inst_t *pi,
    ib_tx_t            *itx)
{
    ib_context_t *ctx = itx->ctx;
    ib_conn_t *iconn = itx->conn;
    ib_field_t *f;
    modhtp_cfg_t *modcfg;
    modhtp_context_t *modctx;
    htp_tx_t *tx;
    ib_status_t rc;

    ib_log_debug3_tx(itx, "LibHTP: modhtp_gen_request_fields");

    /* Get the module config. */
    rc = ib_context_module_config(ctx, IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(itx, "Failed to fetch module %s config: %s",
                        MODULE_NAME_STR, ib_status_to_string(rc));
        return rc;
    }

    /* Fetch context from the connection. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(iconn);

    /* Use the current parser transaction to generate fields. */
    /// @todo Check htp state, etc.
    tx = modctx->htp->in_tx;
    if (tx != NULL) {
        size_t param_count = 0;
        htp_tx_set_user_data(tx, itx);

        rc = ib_data_add_list(itx->data, "request_body_params", &f);
        if ( (rc == IB_OK) && (tx->request_params != NULL) ) {
            modhtp_param_iter_data_t idata =
                { f, HTP_SOURCE_BODY, 0 };
            rc = modhtp_table_iterator(itx, tx->request_params,
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
    }

    return IB_OK;
}

static ib_status_t modhtp_gen_response_header_fields(
    ib_provider_inst_t *pi,
    ib_tx_t            *itx)
{
    return IB_OK;
}

static ib_status_t modhtp_gen_response_fields(
    ib_provider_inst_t *pi,
    ib_tx_t            *itx)
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

static ib_status_t modhtp_iface_data_in(
    ib_provider_inst_t *pi,
    ib_conndata_t      *qcdata)
{
    ib_engine_t *ib = pi->pr->ib;
    ib_conn_t *iconn = qcdata->conn;
    modhtp_context_t *modctx;
    htp_connp_t *htp;
    htp_tx_t *tx;
    ib_tx_t *itx = NULL;
    struct timeval tv;
    int ec;

    /* Ignore any zero length data. */
    if (qcdata->dlen == 0) {
        ib_log_debug3(ib, "Ignoring zero length incoming data.");
        return IB_OK;
    }

    /* Fetch context from the connection. */
    modctx = ib_conn_parser_context_get(iconn);
    htp = modctx->htp;

    gettimeofday(&tv, NULL);

    ib_log_debug3(ib, "LibHTP incoming data status=%d", htp->in_status);
    ib_log_debug3(ib,
                  "DATA: %s:%d -> %s:%d len=%d %" IB_BYTESTR_FMT,
                  iconn->remote_ipstr, iconn->remote_port,
                  iconn->local_ipstr, iconn->local_port,
                  (int)qcdata->dlen,
                  IB_BYTESTRSL_FMT_PARAM(qcdata->data, qcdata->dlen));

    /* Lookup the current htp and ironbee transactions */
    tx = htp->in_tx;
    if (tx != NULL) {
        itx = htp_tx_get_user_data(tx);
    }

    if (itx == NULL) {
        ib_log_debug3(ib, "Data In: No IronBee transaction available.");
    }

    switch(htp->in_status) {
        case HTP_STREAM_NEW:
        case HTP_STREAM_OPEN:
        case HTP_STREAM_DATA:
            /* Let the parser see the data. */
            ec = htp_connp_req_data(htp, &tv, qcdata->data, qcdata->dlen);
            if (ec == HTP_STREAM_DATA_OTHER) {
                ib_log_notice(ib, "LibHTP parser blocked: %d", ec);
                /// @todo Buffer it for next time?
            }
            else if (ec != HTP_STREAM_DATA) {
                ib_log_info(ib, "LibHTP request parsing error: %d", ec);
            }
            break;
        case HTP_STREAM_ERROR:
            ib_log_info(ib, "LibHTP parser in \"error\" state");
            break;
        case HTP_STREAM_DATA_OTHER:
            ib_log_notice(ib, "LibHTP parser in \"other\" state");
            break;
        default:
            ib_log_error(ib, "LibHTP parser in unhandled state %d",
                         htp->in_status);
    }

    return IB_OK;
}

static ib_status_t modhtp_iface_data_out(
    ib_provider_inst_t *pi,
    ib_conndata_t      *qcdata)
{
    ib_engine_t *ib = pi->pr->ib;
    ib_conn_t *iconn = qcdata->conn;
    modhtp_context_t *modctx;
    htp_connp_t *htp;
    htp_tx_t *tx;
    ib_tx_t *itx = NULL;
    struct timeval tv;
    int ec;

    /* Ignore any zero length data. */
    if (qcdata->dlen == 0) {
        ib_log_debug3(ib, "Ignoring zero length outgoing data.");
        return IB_OK;
    }

    gettimeofday(&tv, NULL);

    /* Fetch context from the connection. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(iconn);
    htp = modctx->htp;

    ib_log_debug3(ib, "LibHTP outgoing data status=%d", htp->out_status);
    ib_log_debug3(ib,
                  "DATA: %s:%d -> %s:%d len=%d %" IB_BYTESTR_FMT,
                  iconn->local_ipstr, iconn->local_port,
                  iconn->remote_ipstr, iconn->remote_port,
                  (int)qcdata->dlen,
                  IB_BYTESTRSL_FMT_PARAM(qcdata->data, qcdata->dlen));

    /* Lookup the current htp and ironbee transactions */
    tx = htp->out_tx;
    if (tx != NULL) {
        itx = htp_tx_get_user_data(tx);
    }

    if (itx == NULL) {
        ib_log_debug3(ib, "Data Out: No IronBee transaction available.");
    }

    switch(htp->out_status) {
        case HTP_STREAM_NEW:
        case HTP_STREAM_OPEN:
        case HTP_STREAM_DATA:
            /* Let the parser see the data. */
            ec = htp_connp_res_data(htp, &tv, qcdata->data, qcdata->dlen);
            if (ec == HTP_STREAM_DATA_OTHER) {
                ib_log_notice(ib, "LibHTP parser blocked: %d", ec);
                /// @todo Buffer it for next time?
            }
            else if (ec != HTP_STREAM_DATA) {
                ib_log_info(ib, "LibHTP response parsing error: %d", ec);
            }
            break;
        case HTP_STREAM_ERROR:
            ib_log_info(ib, "LibHTP parser in \"error\" state");
            break;
        case HTP_STREAM_DATA_OTHER:
            ib_log_notice(ib, "LibHTP parser in \"other\" state");
            break;
        default:
            ib_log_error(ib, "LibHTP parser in unhandled state %d",
                         htp->out_status);
    }

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

/**
 * Set the request URI for libhtp
 *
 * This may involve splitting the URI up into the URI portion and the query
 * portion.
 *
 * @param[in] itx IronBee transaction
 * @param[in] htx libhtp transaction
 * @param[in] uri_bstr URI as an IronBee byte string
 *
 * @returns Status code
 */
static ib_status_t modhtp_set_request_uri(
    const ib_tx_t      *itx,
    htp_tx_t           *htx,
    const ib_bytestr_t *uri_bstr)
{
    const char  *uri;
    size_t       uri_len; 
    ib_status_t  irc;
    ssize_t      qoff;

    uri = (const char *)ib_bytestr_const_ptr(uri_bstr);
    uri_len = ib_bytestr_length(uri_bstr);

    /* Do we have a query portion? */
    irc = ib_strchr_nul_ignore(uri, uri_len, '?', &qoff);
    if (irc != IB_OK) {
        return irc;
    }
    else if (qoff == 0) {
        return IB_EINVAL;
    }
    else if (qoff != -1) {
        size_t len;
        const char *qmark = uri + qoff;

        len = qoff;
        irc = modhtp_set_data(itx, htx,
                              uri, len,
                              htp_tx_req_set_uri);
        if (irc != IB_OK) {
            return irc;
        }

        len = uri_len - qoff;
        if (len > 0) {
            irc = modhtp_set_data(itx, htx,
                                  qmark + 1, len,
                                  htp_tx_req_set_query_string);
            if (irc != IB_OK) {
                return irc;
            }
        }
    }
    else {
        irc = modhtp_set_bstr(itx, htx,
                              uri_bstr, htp_tx_req_set_uri);
        if (irc != IB_OK) {
            return irc;
        }
    }

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

    /* Fetch context from the connection and mark this
     * as being a parsed data request. */
    modctx = modhtp_get_itx_context(itx);
    modctx->parsed_data = true;
    htp = modctx->htp;

    /* Create the request transaction */
    htp->in_tx = htp_tx_create(htp);
    if (htp->in_tx == NULL) {
        return IB_EUNKNOWN;
    }
    htp_tx_set_user_data(htp->in_tx, itx);

    /* Start the request transaction */
    hrc = htp_tx_state_request_start(htp->in_tx);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    /* Hand the method to libhtp */
    irc = modhtp_set_bstr(itx, htp->in_tx, line->method, htp_tx_req_set_method);
    if (irc != IB_OK) {
        return irc;
    }

    /* Set the URI (maybe split up URI into URI and query) */
    irc = modhtp_set_request_uri(itx, htp->in_tx, line->uri);
    if (irc != IB_OK) {
        return irc;
    }

    /* Tell libhtp the protocol */
    if ( (line->protocol != NULL) || (ib_bytestr_length(line->protocol) == 0)) {
        htp_tx_req_set_protocol_0_9(htp->in_tx, 1);
    }
    else {
        irc = modhtp_set_bstr(itx, htp->in_tx,
                              line->method, htp_tx_req_set_protocol);
        if (irc != IB_OK) {
            return irc;
        }
        htp_tx_req_set_protocol_0_9(htp->in_tx, 0);
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

    ib_status_t irc;
    modhtp_context_t *modctx;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND REQUEST HEADER DATA TO LIBHTP: "
                    "modhtp_iface_request_header_data");

    modctx = modhtp_get_itx_context(itx);
    modctx->parsed_data = true;
    irc = modhtp_set_header(itx, modctx->htp->in_tx,
                            header, htp_tx_req_set_header);

    return irc;
}

static ib_status_t modhtp_iface_request_header_finished(
    ib_provider_inst_t *pi,
    ib_tx_t            *itx)
{
    assert(pi != NULL);
    assert(itx != NULL);

    ib_status_t irc;

    /* Generate header fields. */
    irc = modhtp_gen_request_header_fields(pi, itx);
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
    modhtp_context_t *modctx;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND REQUEST BODY DATA TO LIBHTP: "
                    "modhtp_iface_request_body_data");

    /* Write request body data to libhtp. */
    modctx = modhtp_get_itx_context(itx);
    modctx->parsed_data = true;
    hrc = htp_tx_req_process_body_data(modctx->htp->in_tx,
                                       txdata->data, txdata->dlen);
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

    /* Generate fields. */
    irc = modhtp_gen_request_fields(pi, itx);
    return irc;
}

/**
 * @todo This function will be replaced
 */
static ib_status_t modhtp_set_response_protocol(
    const ib_tx_t      *itx,
    htp_tx_t           *htx,
    const ib_bytestr_t *proto_buf)
{
    int proto;

    bstr *buf;
    buf = bstr_dup_mem(ib_bytestr_const_ptr(proto_buf),
                       ib_bytestr_length(proto_buf));
    if (buf == NULL) {
        return IB_EALLOC;
    }
    proto = htp_parse_protocol(buf);
    bstr_free(buf);
    if (proto == HTP_PROTOCOL_UNKNOWN) {
        return IB_EINVAL;
    }

    htp_tx_res_set_protocol_number(htx, proto);
    return IB_OK;
}

/**
 * @todo This function will be replaced
 */
static ib_status_t modhtp_set_response_status(
    const ib_tx_t      *itx,
    htp_tx_t           *htx,
    const ib_bytestr_t *status_buf)
{
    ib_num_t status_code;
    ib_status_t irc;

    irc = ib_string_to_num_ex((const char *)ib_bytestr_const_ptr(status_buf),
                              ib_bytestr_length(status_buf),
                              10,
                              &status_code);
    if (irc != IB_OK) {
        return IB_EINVAL;
    }
    htp_tx_res_set_status_code(htx, (int)status_code);
    return IB_OK;
}

static ib_status_t modhtp_iface_response_line(
    ib_provider_inst_t    *pi,
    ib_tx_t               *itx,
    ib_parsed_resp_line_t *line)
{
    assert(pi != NULL);
    assert(itx != NULL);

    modhtp_context_t *modctx;
    htp_connp_t      *htp;
    ib_status_t       irc;
    htp_status_t      hrc;

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

    /* Fetch context from the connection and mark this
     * as being a parsed data request. */
    modctx = modhtp_get_itx_context(itx);
    modctx->parsed_data = true;
    htp = modctx->htp;

    /* Create the response transaction */
    htp->out_tx = htp_tx_create(htp);
    if (htp->out_tx == NULL) {
        return IB_EUNKNOWN;
    }
    htp_tx_set_user_data(htp->out_tx, itx);

    /* Start the response transaction */
    hrc = htp_tx_state_response_start(htp->out_tx);
    if (hrc != HTP_OK) {
        return IB_EUNKNOWN;
    }

    /* No funcion to set the protocol string for response; we need to parse
     * it here ourselves. <sigh> */
    irc = modhtp_set_response_protocol(itx, htp->out_tx, line->protocol);
    if (irc != IB_OK) {
        return irc;
    }

    irc = modhtp_set_response_status(itx, htp->out_tx, line->status);
    if (irc != IB_OK) {
        return irc;
    }

    irc = modhtp_set_bstr(itx, htp->out_tx, line->msg,
                          htp_tx_res_set_status_message);
    if (irc != IB_OK) {
        return irc;
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

    ib_status_t irc;
    modhtp_context_t *modctx;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND RESPONSE HEADER DATA TO LIBHTP: "
                    "modhtp_iface_response_header_data");

    modctx = modhtp_get_itx_context(itx);
    modctx->parsed_data = true;
    irc = modhtp_set_header(itx, modctx->htp->out_tx,
                           header, htp_tx_res_set_header);

    return irc;
}

static ib_status_t modhtp_iface_response_header_finished(
    ib_provider_inst_t *pi,
    ib_tx_t *itx)
{
    assert(pi != NULL);
    assert(itx != NULL);

    ib_status_t irc;

    /* Generate header fields. */
    irc = modhtp_gen_response_header_fields(pi, itx);
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

    modhtp_context_t *modctx;
    htp_status_t hrc;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        return IB_OK;
    }

    ib_log_debug_tx(itx,
                    "SEND RESPONSE BODY DATA TO LIBHTP: "
                    "modhtp_iface_response_body_data");

    /* Write request body data to libhtp. */
    modctx = modhtp_get_itx_context(itx);
    modctx->parsed_data = true;
    hrc = htp_tx_res_process_body_data(modctx->htp->in_tx,
                                       txdata->data, txdata->dlen);
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

    /* Generate fields. */
    irc = modhtp_gen_response_fields(pi, itx);

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

    /* Required Parser Functions */
    modhtp_iface_data_in,
    modhtp_iface_data_out,

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
