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
#include <ironbee/debug.h>
#include <ironbee/engine.h>
#include <ironbee/field.h>
#include <ironbee/hash.h>
#include <ironbee/module.h>
#include <ironbee/mpool.h>
#include <ironbee/provider.h>
#include <ironbee/state_notify.h>
#include <ironbee/string.h>

#include <dslib.h>
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

/** Module Context Structure */
struct modhtp_context_t {
    ib_engine_t    *ib;           /**< Engine handle */
    ib_conn_t      *iconn;        /**< Connection structure */
    modhtp_cfg_t   *modcfg;       /**< Module config structure */
    htp_cfg_t      *htp_cfg;      /**< Parser config handle */
    htp_connp_t    *htp;          /**< Parser handle */
    int            parsed_data;   /**< Set when processing parsed data */
};

/** Module Configuration Structure */
struct modhtp_cfg_t {
    const char     *personality;  /**< libhtp personality */
};

/* Instantiate a module global configuration. */
static modhtp_cfg_t modhtp_global_cfg = {
    "generic" /* personality */
};

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
    { "tomcat_6_0", HTP_SERVER_TOMCAT_6_0 },
    { "apache",     HTP_SERVER_APACHE },
    { "apache_2_2", HTP_SERVER_APACHE_2_2 },
    { NULL, 0 }
};

/* Lookup a numeric personality from a name. */
static int modhtp_personality(const char *name)
{
    IB_FTRACE_INIT();
    const modhtp_nameval_t *rec = modhtp_personalities;

    if (name == NULL) {
        IB_FTRACE_RET_INT(-1);
    }

    while (rec->name != NULL) {
        if (strcasecmp(name, rec->name) == 0) {
            IB_FTRACE_RET_INT(rec->val);
        }

        ++rec;
    }

    IB_FTRACE_RET_INT(-1);
}

/* Log htp data via ironbee logging. */
static int modhtp_callback_log(htp_log_t *log)
{
    IB_FTRACE_INIT();
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

    IB_FTRACE_RET_INT(0);
}


/* -- Field Generation Routines -- */


static ib_status_t modhtp_field_gen_bytestr(ib_provider_inst_t *dpi,
                                            const char *name,
                                            bstr *bs,
                                            ib_field_t **pf)
{
    ib_field_t *f;
    ib_bytestr_t *ibs;
    ib_status_t rc;

    if (bs == NULL) {
        if (pf != NULL) {
            *pf = NULL;
        }
        return IB_EINVAL;
    }

    /* First lookup the field to see if there is already one
     * that needs the value set.
     */
    rc = ib_data_get(dpi, name, &f);
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
    rc = ib_data_add_bytestr_ex(dpi, name, strlen(name),
                                (uint8_t *)bstr_ptr(bs),
                                bstr_len(bs), pf);
    if (rc != IB_OK) {
        ib_log_error(dpi->pr->ib,
                     "Failed to generate \"%s\" field: %s",
                     name, ib_status_to_string(rc));
    }

    return rc;
}

#define modhtp_field_gen_list(dpi, name, pf) \
    ib_data_add_list_ex((dpi), (name), strlen((name)), (pf))

/* -- Utility functions -- */
static ib_status_t modhtp_add_flag_to_collection(
    ib_tx_t *itx,
    const char *collection_name,
    const char *flag
)
{
    IB_FTRACE_INIT();
    ib_status_t rc;
    ib_field_t *f;

    rc = ib_data_get(itx->dpi, collection_name, &f);
    if (f == NULL) {
        rc = ib_data_add_list(itx->dpi, collection_name, &f);
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
            ib_log_debug3_tx(itx, "Failed to add %s field: %s",
                             collection_name, flag);
        }
    }
    else {
        ib_log_debug3_tx(itx, "Failed to add flag collection: %s",
                         collection_name);
    }

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modhtp_set_parser_flag(ib_tx_t *itx,
                                          const char *collection_name,
                                          unsigned int flags)
{
    IB_FTRACE_INIT();
    ib_status_t rc = IB_OK;

    if (flags & HTP_AMBIGUOUS_HOST) {
        flags ^= HTP_AMBIGUOUS_HOST;
        rc = modhtp_add_flag_to_collection(itx, collection_name,
                                           "AMBIGUOUS_HOST");
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

    /* If flags is not 0 we did not handle one of the bits. */
    if (flags != 0) {
        ib_log_error_tx(itx, "HTP parser unknown flag: 0x%08x", flags);
        rc = IB_EUNKNOWN;
    }

    IB_FTRACE_RET_STATUS(rc);
}

/* -- LibHTP Callbacks -- */

static int modhtp_htp_tx_start(htp_connp_t *connp)
{
    IB_FTRACE_INIT();
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;
    ib_status_t rc;
    htp_tx_t *tx;

    /* If this is a parsed data transaction, then use the existing
     * transaction structure, otherwise create one.
     */
    if (modctx->parsed_data != 0) {
        itx = iconn->tx;
        if (itx == NULL) {
            ib_log_error(ib, "No ironbee transaction available.");
            IB_FTRACE_RET_INT(HTP_ERROR);
        }
        ib_log_debug3(ib, "PARSED TX p=%p id=%s", itx, itx->id);
    }
    else {
        /* Create the transaction structure. */
        ib_log_debug3(ib, "Creating ironbee transaction structure");
        rc = ib_tx_create(&itx, iconn, NULL);
        if (rc != IB_OK) {
            /// @todo Set error.
            IB_FTRACE_RET_INT(HTP_ERROR);
        }
    }

    /* Store this as the current transaction. */
    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == STREAM_STATE_ERROR) {
        ib_log_error_tx(itx, "HTP Parser Error");
    }
    tx = connp->in_tx;
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Associate the ironbee transaction with the libhtp transaction. */
    htp_tx_set_user_data(tx, itx);

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_request_line(htp_connp_t *connp)
{
    IB_FTRACE_INIT();
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_parsed_req_line_t *req_line;
    ib_tx_t *itx;
    ib_status_t rc;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
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
        IB_FTRACE_RET_INT(HTP_OK);
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
        IB_FTRACE_RET_INT(HTP_ERROR);
    }
    else if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
        // TODO: Check flags for those that make further parsing impossible?
    }

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_request_headers(htp_connp_t *connp)
{
    IB_FTRACE_INIT();
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;
    ib_status_t rc;
    ib_parsed_header_wrapper_t *ibhdrs;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction and notify the engine
     * that the request header is now available.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    /* Update the hostname that may have changed with headers. */
    if ((tx->parsed_uri != NULL) && (tx->parsed_uri->hostname != NULL)) {
        itx->hostname = ib_mpool_memdup_to_str(itx->mp, bstr_ptr(tx->parsed_uri->hostname), bstr_len(tx->parsed_uri->hostname));
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
        IB_FTRACE_RET_INT(HTP_OK);
    }

    /* Copy the request fields into a parse name value pair list object */
    rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, itx);
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Error creating header wrapper: %s",
                        ib_status_to_string(rc));
    }
    else {
        htp_header_t *hdr = NULL;
        table_iterator_reset(tx->request_headers);
        while (table_iterator_next(tx->request_headers, (void *)&hdr) != NULL)
        {
            rc = ib_parsed_name_value_pair_list_add(ibhdrs,
                                                    bstr_ptr(hdr->name),
                                                    bstr_len(hdr->name),
                                                    bstr_ptr(hdr->value),
                                                    bstr_len(hdr->value));
            if (rc != IB_OK) {
                ib_log_error_tx(itx,
                                "Error adding request header name / value: %s",
                                ib_status_to_string(rc));
                continue;
            }
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

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_request_body_data(htp_tx_data_t *txdata)
{
    IB_FTRACE_INIT();
    htp_connp_t *connp = txdata->tx->connp;
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_status_t rc;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        IB_FTRACE_RET_INT(HTP_OK);
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

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_request_trailer(htp_connp_t *connp)
{
    IB_FTRACE_INIT();
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        IB_FTRACE_RET_INT(HTP_OK);
    }

    /// @todo Notify tx_datain_event w/request trailer
    ib_log_debug_tx(itx,
        "TODO: tx_datain_event w/request trailer: tx=%p", itx);

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_request(htp_connp_t *connp)
{
    IB_FTRACE_INIT();
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->in_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->in_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction, determine if this is a no-body
     * request and notify the engine that the request body is available
     * and is now finished.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_REQUEST_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        IB_FTRACE_RET_INT(HTP_OK);
    }

    ib_state_notify_request_finished(ib, itx);

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_response_line(htp_connp_t *connp)
{
    IB_FTRACE_INIT();
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_parsed_resp_line_t *resp_line;
    ib_status_t rc;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->out_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        IB_FTRACE_RET_INT(HTP_OK);
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
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Tell the engine that the response started. */
    rc = ib_state_notify_response_started(ib, itx, resp_line);
    if (rc != IB_OK) {
        ib_log_error_tx(itx,
                        "Error notifying response started: %s",
                        ib_status_to_string(rc));
        IB_FTRACE_RET_INT(HTP_ERROR);
    }
    else if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
        if (tx->flags & HTP_STATUS_LINE_INVALID) {
            // FIXME: Why is this not an error???
            ib_log_error_tx(itx, "Error parsing response line.");
            IB_FTRACE_RET_INT(HTP_ERROR);
        }
    }

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_response_headers(htp_connp_t *connp)
{
    IB_FTRACE_INIT();
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;
    ib_status_t rc;
    ib_parsed_header_wrapper_t *ibhdrs;

    /* Use the current parser transaction to generate fields. */
    if (connp->out_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction and notify the engine
     * that the response header is now available.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        IB_FTRACE_RET_INT(HTP_OK);
    }

    /* Copy the response fields into a parse name value pair list object */
    rc = ib_parsed_name_value_pair_list_wrapper_create(&ibhdrs, itx);
    if (rc != IB_OK) {
        ib_log_error_tx(itx, "Error creating header wrapper: %s",
                        ib_status_to_string(rc));
    }
    else {
        htp_header_t *hdr = NULL;
        table_iterator_reset(tx->response_headers);
        while (table_iterator_next(tx->response_headers, (void *)&hdr) != NULL)
        {
            rc = ib_parsed_name_value_pair_list_add(
                ibhdrs,
                bstr_ptr(hdr->name),
                bstr_len(hdr->name),
                bstr_ptr(hdr->value),
                bstr_len(hdr->value));
            if (rc != IB_OK) {
                ib_log_error_tx(
                    itx,
                    "Error adding response header name / value: %s",
                    ib_status_to_string(rc)
                );
                continue;
            }
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

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_response_body_data(htp_tx_data_t *txdata)
{
    IB_FTRACE_INIT();
    htp_connp_t *connp = txdata->tx->connp;
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_status_t rc;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->out_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        IB_FTRACE_RET_INT(HTP_OK);
    }

    /* If this is connection data and the response has not yet
     * started, then LibHTP has interpreted this as the response
     * body. Instead, return an error.
     */
    else if (!ib_tx_flags_isset(itx, IB_TX_FHTTP09|IB_TX_FRES_STARTED)) {
        ib_log_info_tx(itx,
                       "LibHTP parsing error: "
                       "found response data instead of a response line");
        IB_FTRACE_RET_INT(HTP_ERROR);
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

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_response(htp_connp_t *connp)
{
    IB_FTRACE_INIT();
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    if (connp->out_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction and notify the engine
     * that the response body is available, the response
     * is finished and logging has begun.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    if (tx->flags) {
        modhtp_set_parser_flag(itx, "HTP_RESPONSE_FLAGS", tx->flags);
    }

    /* The engine may have already been notified if the parser is
     * receiving already parsed data.  In this case the engine
     * must not be notified again and instead return.
     */
    if (ib_tx_flags_isset(itx, IB_TX_FPARSED_DATA)) {
        IB_FTRACE_RET_INT(HTP_OK);
    }

    ib_state_notify_response_finished(ib, itx);

    /* Destroy the transaction. */
    ib_log_debug3_tx(itx, "Destroying transaction structure");
    ib_tx_destroy(itx);

    /* NOTE: The htp transaction is destroyed in modhtp_tx_cleanup() */

    IB_FTRACE_RET_INT(HTP_OK);
}

static int modhtp_htp_response_trailer(htp_connp_t *connp)
{
    IB_FTRACE_INIT();
    modhtp_context_t *modctx = htp_connp_get_user_data(connp);
    htp_tx_t *tx = connp->out_tx;
    ib_conn_t *iconn = modctx->iconn;
    ib_engine_t *ib = iconn->ib;
    ib_tx_t *itx;

    /* Use the current parser transaction to generate fields. */
    /// @todo Check htp state, etc.
    if (connp->out_status == STREAM_STATE_ERROR) {
        ib_log_error(ib, "HTP Parser Error");
    }
    if (tx == NULL) {
        /// @todo Set error.
        IB_FTRACE_RET_INT(HTP_ERROR);
    }

    /* Fetch the ironbee transaction and notify the engine
     * that more transaction data has arrived.
     */
    itx = htp_tx_get_user_data(tx);
    if (itx == NULL) {
        ib_log_error(ib, "No ironbee transaction available.");
        IB_FTRACE_RET_INT(HTP_ERROR);
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

    IB_FTRACE_RET_INT(HTP_OK);
}


static ib_status_t modhtp_gen_request_header_fields(ib_provider_inst_t *pi,
                                                    ib_tx_t *itx)
{
    IB_FTRACE_INIT();
    ib_context_t *ctx = itx->ctx;
    ib_conn_t *iconn = itx->conn;
    ib_field_t *f;
    modhtp_cfg_t *modcfg;
    modhtp_context_t *modctx;
    htp_tx_t *tx;
    ib_status_t rc;

    /* Get the module config. */
    rc = ib_context_module_config(ctx, IB_MODULE_STRUCT_PTR, (void *)&modcfg);
    if (rc != IB_OK) {
        ib_log_alert_tx(itx, "Failed to fetch module %s config: %s",
                        MODULE_NAME_STR, ib_status_to_string(rc));
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Fetch context from the connection. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(iconn);

    /* Use the current parser transaction to generate fields. */
    /// @todo Check htp state, etc.
    tx = modctx->htp->in_tx;
    if (tx != NULL) {
        htp_tx_set_user_data(tx, itx);

        modhtp_field_gen_bytestr(itx->dpi,
                                 "request_uri_scheme",
                                 tx->parsed_uri->scheme,
                                 NULL);

        modhtp_field_gen_bytestr(itx->dpi,
                                 "request_uri_username",
                                 tx->parsed_uri->username,
                                 NULL);

        modhtp_field_gen_bytestr(itx->dpi,
                                 "request_uri_password",
                                 tx->parsed_uri->password,
                                 NULL);

        modhtp_field_gen_bytestr(itx->dpi,
                                 "request_uri_host",
                                 tx->parsed_uri->hostname,
                                 NULL);

        modhtp_field_gen_bytestr(itx->dpi,
                                 "request_host",
                                 tx->parsed_uri->hostname,
                                 NULL);

        modhtp_field_gen_bytestr(itx->dpi,
                                 "request_uri_port",
                                 tx->parsed_uri->port,
                                 NULL);

        modhtp_field_gen_bytestr(itx->dpi,
                                 "request_uri_path",
                                 tx->parsed_uri->path,
                                 NULL);

        modhtp_field_gen_bytestr(itx->dpi,
                                 "request_uri_query",
                                 tx->parsed_uri->query,
                                 NULL);

        modhtp_field_gen_bytestr(itx->dpi,
                                 "request_uri_fragment",
                                 tx->parsed_uri->fragment,
                                 NULL);

        rc = ib_data_add_list(itx->dpi, "request_cookies", &f);
        if (   (tx->request_cookies != NULL)
            && table_size(tx->request_cookies)
            && (rc == IB_OK))
        {
            bstr *key = NULL;
            bstr *value = NULL;

            /// @todo Make this a function
            table_iterator_reset(tx->request_cookies);
            while ((key = table_iterator_next(tx->request_cookies,
                                              (void *)&value)) != NULL)
            {
                ib_field_t *lf;

                /* Create a list field as an alias into htp memory. */
                rc = ib_field_create_bytestr_alias(&lf,
                                                   itx->mp,
                                                   bstr_ptr(key),
                                                   bstr_len(key),
                                                   (uint8_t *)bstr_ptr(value),
                                                   bstr_len(value));
                if (rc != IB_OK) {
                    ib_log_debug3_tx(itx,
                                     "Failed to create field: %s",
                                     ib_status_to_string(rc));
                }

                /* Add the field to the field list. */
                rc = ib_field_list_add(f, lf);
                if (rc != IB_OK) {
                    ib_log_debug3_tx(itx,
                                     "Failed to add field: %s",
                                     ib_status_to_string(rc));
                }
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

        rc = ib_data_add_list(itx->dpi, "request_uri_params", &f);
        if (   (tx->request_params_query != NULL)
            && table_size(tx->request_params_query)
            && (rc == IB_OK))
        {
            bstr *key = NULL;
            bstr *value = NULL;

            /// @todo Make this a function
            table_iterator_reset(tx->request_params_query);
            while ((key = table_iterator_next(tx->request_params_query,
                                              (void *)&value)) != NULL)
            {
                ib_field_t *lf;

                /* Create a list field as an alias into htp memory. */
                rc = ib_field_create_bytestr_alias(&lf,
                                                   itx->mp,
                                                   bstr_ptr(key),
                                                   bstr_len(key),
                                                   (uint8_t *)bstr_ptr(value),
                                                   bstr_len(value));
                if (rc != IB_OK) {
                    ib_log_debug3_tx(itx,
                                     "Failed to create field: %s",
                                     ib_status_to_string(rc));
                }

                /* Add the field to the field list. */
                rc = ib_field_list_add(f, lf);
                if (rc != IB_OK) {
                    ib_log_debug3_tx(itx,
                                     "Failed to add field: %s",
                                     ib_status_to_string(rc));
                }
            }
        }
        else if (rc == IB_OK) {
            ib_log_debug3_tx(itx, "No request URI parameters");
        }
        else {
            ib_log_error_tx(itx,
                            "Failed to create request URI parameters: %s",
                            ib_status_to_string(rc));
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_gen_request_fields(ib_provider_inst_t *pi,
                                             ib_tx_t *itx)
{
    IB_FTRACE_INIT();
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
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Fetch context from the connection. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(iconn);

    /* Use the current parser transaction to generate fields. */
    /// @todo Check htp state, etc.
    tx = modctx->htp->in_tx;
    if (tx != NULL) {
        htp_tx_set_user_data(tx, itx);

        rc = ib_data_add_list(itx->dpi, "request_body_params", &f);
        if (   (tx->request_params_body != NULL)
            && table_size(tx->request_params_body)
            && (rc == IB_OK))
        {
            bstr *key = NULL;
            bstr *value = NULL;

            /// @todo Make this a function
            table_iterator_reset(tx->request_params_body);
            while ((key = table_iterator_next(tx->request_params_body,
                                              (void *)&value)) != NULL)
            {
                ib_field_t *lf;

                /* Create a list field as an alias into htp memory. */
                rc = ib_field_create_bytestr_alias(&lf,
                                                   itx->mp,
                                                   bstr_ptr(key),
                                                   bstr_len(key),
                                                   (uint8_t *)bstr_ptr(value),
                                                   bstr_len(value));
                if (rc != IB_OK) {
                    ib_log_debug3_tx(itx,
                                     "Failed to create field: %s",
                                     ib_status_to_string(rc));
                }

                /* Add the field to the field list. */
                rc = ib_field_list_add(f, lf);
                if (rc != IB_OK) {
                    ib_log_debug3_tx(itx,
                                     "Failed to add field: %s",
                                     ib_status_to_string(rc));
                }
            }
        }
        else if (rc == IB_OK) {
            ib_log_debug3_tx(itx, "No request body parameters");
        }
        else {
            ib_log_error_tx(itx,
                            "Failed to create request body parameters: %s",
                            ib_status_to_string(rc));
        }
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_gen_response_header_fields(ib_provider_inst_t *pi,
                                                     ib_tx_t *itx)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_gen_response_fields(ib_provider_inst_t *pi,
                                              ib_tx_t *itx)
{
    IB_FTRACE_INIT();

    IB_FTRACE_RET_STATUS(IB_OK);
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

static ib_status_t modhtp_iface_init(ib_provider_inst_t *pi,
                                     ib_conn_t *iconn)
{
    IB_FTRACE_INIT();
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
        IB_FTRACE_RET_STATUS(rc);
    }

    ib_log_debug3(ib, "Creating LibHTP parser");

    /* Create a context. */
    modctx = ib_mpool_calloc(iconn->mp, 1, sizeof(*modctx));
    if (modctx == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    /* Figure out the personality to use. */
    personality = modhtp_personality(modcfg->personality);
    if (personality == -1) {
        personality = HTP_SERVER_APACHE_2_2;
    }

    /* Configure parser. */
    modctx->htp_cfg = htp_config_create();
    if (modctx->htp_cfg == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
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
        IB_FTRACE_RET_STATUS(IB_EALLOC);
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
    htp_config_register_transaction_start(modctx->htp_cfg,
                                          modhtp_htp_tx_start);
    htp_config_register_request_line(modctx->htp_cfg,
                                     modhtp_htp_request_line);
    htp_config_register_request_headers(modctx->htp_cfg,
                                        modhtp_htp_request_headers);
    htp_config_register_request_body_data(modctx->htp_cfg,
                                          modhtp_htp_request_body_data);
    htp_config_register_request_trailer(modctx->htp_cfg,
                                        modhtp_htp_request_trailer);
    htp_config_register_request(modctx->htp_cfg,
                                modhtp_htp_request);
    htp_config_register_response_line(modctx->htp_cfg,
                                      modhtp_htp_response_line);
    htp_config_register_response_headers(modctx->htp_cfg,
                                         modhtp_htp_response_headers);
    htp_config_register_response_body_data(modctx->htp_cfg,
                                           modhtp_htp_response_body_data);
    htp_config_register_response_trailer(modctx->htp_cfg,
                                         modhtp_htp_response_trailer);
    htp_config_register_response(modctx->htp_cfg,
                                 modhtp_htp_response);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_iface_disconnect(ib_provider_inst_t *pi,
                                           ib_conn_t *iconn)
{
    IB_FTRACE_INIT();
    ib_engine_t *ib = iconn->ib;
    modhtp_context_t *modctx;

    /* Fetch context from the connection. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(iconn);

    ib_log_debug3(ib, "Destroying LibHTP parser");

    /* Destroy the parser on disconnect. */
    htp_connp_destroy_all(modctx->htp);

    /* Destroy the configuration. */
    htp_config_destroy(modctx->htp_cfg);

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_iface_data_in(ib_provider_inst_t *pi,
                                        ib_conndata_t *qcdata)
{
    IB_FTRACE_INIT();
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
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    gettimeofday(&tv, NULL);

    /* Fetch context from the connection. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(iconn);
    htp = modctx->htp;

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
        ib_log_debug3(ib, "No IronBee transaction available.");
    }

    switch(htp->in_status) {
        case STREAM_STATE_NEW:
        case STREAM_STATE_OPEN:
        case STREAM_STATE_DATA:
            /* Let the parser see the data. */
            ec = htp_connp_req_data(htp, &tv, qcdata->data, qcdata->dlen);
            if (ec == STREAM_STATE_DATA_OTHER) {
                ib_log_notice(ib, "LibHTP parser blocked: %d", ec);
                /// @todo Buffer it for next time?
            }
            else if (ec != STREAM_STATE_DATA) {
                ib_log_info(ib, "LibHTP request parsing error: %d", ec);
            }
            break;
        case STREAM_STATE_ERROR:
            ib_log_info(ib, "LibHTP parser in \"error\" state");
            break;
        case STREAM_STATE_DATA_OTHER:
            ib_log_notice(ib, "LibHTP parser in \"other\" state");
            break;
        default:
            ib_log_error(ib, "LibHTP parser in unhandled state %d",
                         htp->in_status);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_iface_data_out(ib_provider_inst_t *pi,
                                         ib_conndata_t *qcdata)
{
    IB_FTRACE_INIT();
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
        IB_FTRACE_RET_STATUS(IB_OK);
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
        ib_log_debug3(ib, "No IronBee transaction available.");
    }

    switch(htp->out_status) {
        case STREAM_STATE_NEW:
        case STREAM_STATE_OPEN:
        case STREAM_STATE_DATA:
            /* Let the parser see the data. */
            ec = htp_connp_res_data(htp, &tv, qcdata->data, qcdata->dlen);
            if (ec == STREAM_STATE_DATA_OTHER) {
                ib_log_notice(ib, "LibHTP parser blocked: %d", ec);
                /// @todo Buffer it for next time?
            }
            else if (ec != STREAM_STATE_DATA) {
                ib_log_info(ib, "LibHTP response parsing error: %d", ec);
            }
            break;
        case STREAM_STATE_ERROR:
            ib_log_info(ib, "LibHTP parser in \"error\" state");
            break;
        case STREAM_STATE_DATA_OTHER:
            ib_log_notice(ib, "LibHTP parser in \"other\" state");
            break;
        default:
            ib_log_error(ib, "LibHTP parser in unhandled state %d",
                         htp->out_status);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_iface_tx_init(ib_provider_inst_t *pi,
                                        ib_tx_t *itx)
{
    IB_FTRACE_INIT();
    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_iface_tx_cleanup(ib_provider_inst_t *pi,
                                           ib_tx_t *itx)
{
    IB_FTRACE_INIT();
    ib_conn_t *iconn = itx->conn;
    modhtp_context_t *modctx;
    htp_tx_t *in_tx;
    htp_tx_t *out_tx;

    assert(itx != NULL);
    assert(itx->conn != NULL);

    /* Fetch context from the connection. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(iconn);

    /* Use the current parser transaction to generate fields. */
    out_tx = modctx->htp->out_tx;

    /* Reset libhtp connection parser. */
    if (out_tx != NULL) {
        ib_tx_t *tx_itx = htp_tx_get_user_data(out_tx);
        if (tx_itx == itx) {
            ib_log_debug_tx(itx, "Destroying LibHTP outbound transaction=%p id=%s", out_tx, itx->id);
            modctx->htp->out_status = STREAM_STATE_OPEN;
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
            ib_log_debug_tx(itx, "Destroying LibHTP inbound transaction=%p id=%s", in_tx, itx->id);
            modctx->htp->in_status = STREAM_STATE_OPEN;
            htp_tx_destroy(in_tx);
            modctx->htp->in_tx = NULL;
            modctx->htp->in_state = htp_connp_REQ_IDLE;
            if (in_tx == modctx->htp->out_tx) {
                modctx->htp->out_tx = NULL;
            }
        }
    }

    htp_connp_clear_error(modctx->htp);
    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_iface_request_line(ib_provider_inst_t *pi,
                                             ib_tx_t *itx,
                                             ib_parsed_req_line_t *line)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);
    assert(line != NULL);

    ib_conndata_t conndata = {0};
    modhtp_context_t *modctx;
    ib_status_t rc;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug_tx(itx, "SEND REQUEST LINE TO LIBHTP: modhtp_iface_request_line");

    /* Fetch context from the connection and mark this
     * as being a parsed data request. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(itx->conn);
    modctx->parsed_data = 1;

    conndata.conn = itx->conn;

    /* Write request line to libhtp. */
    conndata.dlen = ib_bytestr_length(line->raw);
    conndata.data = ib_bytestr_ptr(line->raw);
    rc = modhtp_iface_data_in(pi, &conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Write request line end-of-line to libhtp. */
    conndata.dlen = 2;
    conndata.data = (uint8_t *)"\r\n";
    rc = modhtp_iface_data_in(pi, &conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* User data structure for header iteration. */
typedef struct modhtp_header_data {
    ib_provider_inst_t *pi;
    ib_conndata_t *conndata;
    IB_PROVIDER_FUNC(
        ib_status_t,
        write_fn,
        (ib_provider_inst_t *pi, ib_conndata_t *cdata)
    );
} modhtp_header_data;

/* Send header data to libhtp via this header iteration callback. */
static ib_status_t modhtp_send_header_data(const char *name,
                                           size_t name_len,
                                           const char *value,
                                           size_t value_len,
                                           void *user_data)
{
    assert(user_data != NULL);

    const modhtp_header_data *data = (const modhtp_header_data *)user_data;
    ib_status_t rc;

    /* Write header name to libhtp. */
    data->conndata->dlen = name_len;
    data->conndata->data = (uint8_t *)name;
    rc = data->write_fn(data->pi, data->conndata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Write header name/value delimiter to libhtp. */
    data->conndata->dlen = 2;
    data->conndata->data = (uint8_t *)": ";
    rc = data->write_fn(data->pi, data->conndata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Write header value to libhtp. */
    data->conndata->dlen = value_len;
    data->conndata->data = (uint8_t *)value;
    rc = data->write_fn(data->pi, data->conndata);
    if (rc != IB_OK) {
        return rc;
    }

    /* Write header end-of-line to libhtp. */
    data->conndata->dlen = 2;
    data->conndata->data = (uint8_t *)"\r\n";
    rc = data->write_fn(data->pi, data->conndata);

    return rc;
}


static ib_status_t modhtp_iface_request_header_data(ib_provider_inst_t *pi,
                                                    ib_tx_t *itx,
                                                    ib_parsed_header_wrapper_t *header)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);
    assert(header != NULL);

    ib_conndata_t conndata = {0};
    modhtp_header_data cbdata;
    ib_status_t rc;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug_tx(itx, "SEND REQUEST HEADER DATA TO LIBHTP: modhtp_iface_request_header_data");

    conndata.conn = itx->conn;
    cbdata.pi = pi;
    cbdata.conndata = &conndata;
    cbdata.write_fn = modhtp_iface_data_in;

    rc = ib_parsed_tx_each_header(header,
                                  modhtp_send_header_data,
                                  &cbdata);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modhtp_iface_request_header_finished(ib_provider_inst_t *pi,
                                                        ib_tx_t *itx)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);

    ib_conndata_t conndata = {0};
    ib_status_t rc;

    /* Generate header fields. */
    rc = modhtp_gen_request_header_fields(pi, itx);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug_tx(itx, "SEND REQUEST HEADER FINISHED TO LIBHTP: modhtp_iface_request_header_finished");

    conndata.conn = itx->conn;

    /* Write request header separator to libhtp. */
    conndata.dlen = 2;
    conndata.data = (uint8_t *)"\r\n";
    rc = modhtp_iface_data_in(pi, &conndata);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modhtp_iface_request_body_data(ib_provider_inst_t *pi,
                                                  ib_tx_t *itx,
                                                  ib_txdata_t *txdata)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);
    assert(txdata != NULL);

    ib_conndata_t conndata = {0};
    ib_status_t rc;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug_tx(itx, "SEND REQUEST BODY DATA TO LIBHTP: modhtp_iface_request_body_data");

    conndata.conn = itx->conn;

    /* Write request body data to libhtp. */
    conndata.dlen = txdata->dlen;
    conndata.data = txdata->data;
    rc = modhtp_iface_data_in(pi, &conndata);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modhtp_iface_request_finished(ib_provider_inst_t *pi,
                                                 ib_tx_t *itx)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);

    ib_status_t rc;

    /* Generate fields. */
    rc = modhtp_gen_request_fields(pi, itx);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modhtp_iface_response_line(ib_provider_inst_t *pi,
                                              ib_tx_t *itx,
                                              ib_parsed_resp_line_t *line)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);

    ib_conndata_t conndata = {0};
    modhtp_context_t *modctx;
    ib_status_t rc;

    /* This is not valid for HTTP/0.9 requests. */
    if (line == NULL) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug_tx(itx, "SEND RESPONSE LINE TO LIBHTP: modhtp_iface_response_line");

    /* Fetch context from the connection and mark this
     * as being a parsed data request. */
    modctx = (modhtp_context_t *)ib_conn_parser_context_get(itx->conn);
    modctx->parsed_data = 1;

    conndata.conn = itx->conn;

    /* Write response line to libhtp. */
    conndata.dlen = ib_bytestr_length(line->raw);
    conndata.data = ib_bytestr_ptr(line->raw);
    rc = modhtp_iface_data_out(pi, &conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Write response line end-of-line to libhtp. */
    conndata.dlen = 2;
    conndata.data = (uint8_t *)"\r\n";
    rc = modhtp_iface_data_out(pi, &conndata);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modhtp_iface_response_header_data(ib_provider_inst_t *pi,
                                                     ib_tx_t *itx,
                                                     ib_parsed_header_wrapper_t *header)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);
    assert(header != NULL);

    ib_conndata_t conndata = {0};
    modhtp_header_data cbdata;
    ib_status_t rc;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug_tx(itx, "SEND RESPONSE HEADER DATA TO LIBHTP: modhtp_iface_response_header_data");

    conndata.conn = itx->conn;
    cbdata.pi = pi;
    cbdata.conndata = &conndata;
    cbdata.write_fn = modhtp_iface_data_out;

    rc = ib_parsed_tx_each_header(header,
                                  modhtp_send_header_data,
                                  &cbdata);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modhtp_iface_response_header_finished(
    ib_provider_inst_t *pi,
    ib_tx_t *itx
)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);

    ib_conndata_t conndata = {0};
    ib_status_t rc;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug_tx(itx, "SEND RESPONSE HEADER FINISHED TO LIBHTP: modhtp_iface_response_header_finished");

    conndata.conn = itx->conn;

    /* Write response header separator to libhtp. */
    conndata.dlen = 2;
    conndata.data = (uint8_t *)"\r\n";
    rc = modhtp_iface_data_out(pi, &conndata);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    /* Generate header fields. */
    rc = modhtp_gen_response_header_fields(pi, itx);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modhtp_iface_response_body_data(ib_provider_inst_t *pi,
                                                   ib_tx_t *itx,
                                                   ib_txdata_t *txdata)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);
    assert(txdata != NULL);

    ib_conndata_t conndata = {0};
    ib_status_t rc;

    /* This is required for parsed data only. */
    if (ib_conn_flags_isset(itx->conn, IB_CONN_FSEENDATAIN)) {
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    ib_log_debug_tx(itx, "SEND RESPONSE BODY DATA TO LIBHTP: modhtp_iface_response_body_data");

    conndata.conn = itx->conn;

    /* Write request body data to libhtp. */
    conndata.dlen = txdata->dlen;
    conndata.data = txdata->data;
    rc = modhtp_iface_data_out(pi, &conndata);

    IB_FTRACE_RET_STATUS(rc);
}

static ib_status_t modhtp_iface_response_finished(ib_provider_inst_t *pi,
                                                  ib_tx_t *itx)
{
    IB_FTRACE_INIT();

    assert(pi != NULL);
    assert(itx != NULL);

    ib_status_t rc;

    /* Generate fields. */
    rc = modhtp_gen_response_fields(pi, itx);

    IB_FTRACE_RET_STATUS(rc);
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
    IB_FTRACE_INIT();
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
        IB_FTRACE_RET_STATUS(IB_OK);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

static ib_status_t modhtp_context_close(ib_engine_t *ib,
                                        ib_module_t *m,
                                        ib_context_t *ctx,
                                        void *cbdata)
{
    IB_FTRACE_INIT();
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
            IB_FTRACE_RET_STATUS(rc);
        }

        rc = ib_parser_provider_set_instance(ctx, pi);
        if (rc != IB_OK) {
            ib_log_alert(ib, "Failed to set %s as default parser: %s",
                         MODULE_NAME_STR, ib_status_to_string(rc));
            IB_FTRACE_RET_STATUS(rc);
        }
        pi = ib_parser_provider_get_instance(ctx);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
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
