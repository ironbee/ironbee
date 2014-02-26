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
 * @brief IronBee --- Filter Interface
 *
 * @author Brian Rectanus <brectanus@qualys.com>
 */

#include "ironbee_config_auto.h"

#include "engine_private.h"

#include <ironbee/engine.h>
#include <ironbee/log.h>
#include <ironbee/mpool.h>

#include <inttypes.h>
#include <string.h>

ib_status_t ib_fctl_tx_create(ib_fctl_t **pfc,
                              ib_tx_t *tx,
                              ib_mm_t mm)
{
//    ib_engine_t *ib = tx->ib;
    ib_status_t rc = IB_OK;

    /* Create the main structure. */
    *pfc = (ib_fctl_t *)ib_mm_calloc(mm, 1, sizeof(**pfc));
    if (*pfc == NULL) {
        rc = IB_EALLOC;
        goto failed;
    }
    (*pfc)->ib = tx->ib;
    (*pfc)->mm = mm;
    (*pfc)->fdata.udata.tx = tx;

    /* Create streams */
    rc = ib_stream_create(&(*pfc)->source, mm);
    if (rc != IB_OK) {
        goto failed;
    }
    rc = ib_stream_create(&(*pfc)->sink, mm);
    if (rc != IB_OK) {
        goto failed;
    }

    return rc;

failed:
    /* Make sure everything is cleaned up on failure */
    *pfc = NULL;

    return rc;
}

ib_status_t ib_fctl_config(ib_fctl_t *fc,
                           ib_context_t *ctx)
{
//    ib_engine_t *ib = fc->ib;
    ib_status_t rc;

    /* Use filters from context. */
    fc->filters = ctx->filters;

    rc = ib_fctl_process(fc);
    return rc;
}

static ib_status_t filter_exec(ib_filter_t *f,
                               ib_fdata_t *fdata)
{
//    ib_engine_t *ib = f->ib;
    ib_context_t *ctx;
    ib_mm_t mm;
    ib_flags_t flags;
    ib_status_t rc;

    switch (f->type) {
        case IB_FILTER_TX:
            ctx = fdata->udata.tx->ctx;
            mm = fdata->udata.tx->mm;
            break;
        case IB_FILTER_CONN:
            ctx = fdata->udata.conn->ctx;
            mm = fdata->udata.conn->mm;
            break;
        default:
            return IB_EINVAL;
    }

    rc = f->fn_filter(f, fdata, ctx, mm, &flags);
    if (rc != IB_OK) {
        return rc;
    }

    /// @todo Handle flags

    return rc;
}

ib_status_t ib_fctl_process(ib_fctl_t *fc)
{
    ib_engine_t *ib = fc->ib;
    ib_list_node_t *node;
    ib_status_t rc = IB_OK;

    if (fc->filters == NULL) {
        return IB_OK;
    }

    /* Prepare data for filtering. */
    fc->fdata.stream = fc->source;

    /* Filter if there are filters. */
    if (ib_list_elements(fc->filters) > 0) {
        IB_LIST_LOOP(fc->filters, node) {
            ib_filter_t *f = (ib_filter_t *)ib_list_node_data(node);

            rc = filter_exec(f, &fc->fdata);
            if (rc != IB_OK) {
                /// @todo Handle errors
                ib_log_error(ib,
                             "Error processing filter idx=%zd \"%s\": %s",
                             f->idx, f->name, ib_status_to_string(rc));
            }
        }
    }

    /* Buffer if there is a buffer filter. */
    if (fc->fbuffer) {
        rc = filter_exec(fc->fbuffer, &fc->fdata);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Move anything remaining in the stream to the sink. */
    if (fc->fdata.stream->nelts) {
        ib_sdata_t *sdata;

        /// @todo Need API to move data between streams.
        rc = ib_stream_pull(fc->fdata.stream, &sdata);
        while (rc == IB_OK) {
            rc = ib_stream_push_sdata(fc->sink, sdata);
            if (rc == IB_OK) {
                rc = ib_stream_pull(fc->fdata.stream, &sdata);
            }
        }
        if (rc != IB_ENOENT) {
            return rc;
        }
    }


    return IB_OK;
}

ib_status_t ib_fctl_data_add(ib_fctl_t *fc,
                             void *data,
                             size_t dlen)
{
//    ib_engine_t *ib = fc->ib;
    ib_status_t rc;

    rc = ib_stream_push(fc->source, IB_STREAM_DATA, data, dlen);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_fctl_process(fc);
    return rc;
}

ib_status_t ib_fctl_meta_add(ib_fctl_t *fc,
                             ib_sdata_type_t stype)
{
//    ib_engine_t *ib = fc->ib;
    ib_status_t rc;

    rc = ib_stream_push(fc->source, stype, NULL, 0);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_fctl_process(fc);
    return rc;
}

ib_status_t ib_fctl_drain(ib_fctl_t *fc,
                          ib_stream_t **pstream)
{
    if (pstream != NULL) {
        *pstream = fc->sink;
    }

    return IB_OK;
}

ib_status_t ib_filter_register(ib_filter_t **pf,
                               ib_engine_t *ib,
                               const char *name,
                               ib_filter_type_t type,
                               ib_flags_t options,
                               ib_filter_fn_t fn_filter,
                               void *cbdata)
{
    ib_status_t rc;

    *pf = (ib_filter_t *)ib_mm_calloc(
        ib_engine_mm_main_get(ib), 1, sizeof(**pf)
    );
    if (*pf == NULL) {
        return IB_EALLOC;
    }

    (*pf)->ib = ib;
    (*pf)->name = ib_mm_strdup(ib_engine_mm_main_get(ib), name);
    (*pf)->type = type;
    (*pf)->options = options;
    (*pf)->idx = ib_array_elements(ib->filters);
    (*pf)->fn_filter = fn_filter;
    (*pf)->cbdata = cbdata;

    rc = ib_array_setn(ib->filters, (*pf)->idx, *pf);
    if (rc != IB_OK) {
        ib_log_error(ib,
            "Error registering filter %s: %s",
            (*pf)->name, ib_status_to_string(rc)
        );
        *pf = NULL;
        return rc;
    }

    return rc;
}
