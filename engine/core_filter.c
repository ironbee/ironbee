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
 * @brief IronBee
 *
 * @author Sam Baskinger <sbaskinger@qualys.com>
 */

#include "core_filter_private.h"
#include "engine_private.h"

#include <ironbee/filter.h>
#include <ironbee/log.h>
#include <ironbee/core.h>

#include <assert.h>

static const char *CORE_FILTER_NAME = "raw";
static const char *CORE_FILTER_TYPE = "raw";

/**
 * The configuration data for a filter.
 */
struct filter_inst_t {
    ib_tx_t       *tx;
    ib_core_cfg_t *corecfg;
};
typedef struct filter_inst_t filter_inst_t;

/**
 * The argument passed to the ib_filter_inst_t creation function.
 */
struct filter_create_arg_t {
    ib_tx_t       *tx;      /**< The current transaction. */
    ib_core_cfg_t *corecfg; /**< The configuration of the module. */
};
typedef struct filter_create_arg_t filter_create_arg_t;

static ib_status_t req_filter_create_fn(
    void        *inst_data,
    ib_mm_t      mm,
    ib_filter_t *filter,
    void         *arg,
    void         *cbdata
)
{
    filter_inst_t       *inst;
    filter_create_arg_t *filter_arg = (filter_create_arg_t *)arg;

    inst = ib_mm_alloc(mm, sizeof(*inst));
    if (inst == NULL) {
        return IB_EALLOC;
    }
    inst->tx      = filter_arg->tx;
    inst->corecfg = filter_arg->corecfg;

    *(void **)inst_data = inst;

    return IB_OK;
}

static ib_status_t apply_buffering_to_limit(
    ib_tx_t                   *tx,
    ib_mpool_freeable_t       *mp,
    const ib_filter_data_t    *filter_data,
    const size_t               limit,
    ib_stream_t               *stream
)
{
    ib_status_t rc;
    const size_t data_length = ib_filter_data_len(filter_data);

    if (
        ib_filter_data_ptr(filter_data) == NULL ||
        ib_filter_data_len(filter_data) == 0    ||
        ib_filter_data_type(filter_data) != IB_FILTER_DATA
    ) {
        return IB_OK;
    }

    /* Already at the limit? */
    if (stream->slen >= limit) {
        /* Already at the limit. */
        ib_log_debug_tx(tx,
                        "Request body log limit (%zd) reached: Ignoring %zd bytes.",
                        limit,
                        data_length);
        return IB_OK;
    }
    else {
        /* Check remaining space, adding only what will fit. */
        const size_t remaining = limit - stream->slen;
        ib_filter_data_t *buffer_data;

        rc = ib_filter_data_slice(
            &buffer_data,
            mp,
            filter_data,
            0,
            (remaining >= data_length)?data_length : remaining);
        if (rc != IB_OK) {
            // FIXME - log error.
            return rc;
        }

        rc = ib_stream_push(
            stream,
            IB_STREAM_DATA,
            ib_filter_data_ptr(buffer_data),
            ib_filter_data_len(buffer_data));
        if (rc != IB_OK) {
            // FIXME - log error.
            return rc;
        }
    }

    return IB_OK;
}

static ib_status_t forward_data(
    ib_mpool_freeable_t    *mp,
    const ib_filter_data_t *filter_data,
    ib_list_t              *out
)
{
    ib_filter_data_t *new_filter_data;
    ib_status_t      rc;

    /* Alias to the ouptput the segment we're watching. */
    rc = ib_filter_data_slice(
        &new_filter_data,
        mp,
        filter_data,
        0,
        ib_filter_data_len(filter_data));
    if (rc != IB_OK) {
        // FIXME - log error.
        return rc;
    }

    /* Forward all incoming data out. */
    rc = ib_list_push(out, new_filter_data);
    if (rc != IB_OK) {
        // FIXME - log error.
        return rc;
    }

    return IB_OK;
}

static ib_status_t req_filter_exec_fn(
    ib_filter_inst_t    *filter_inst,
    void                *inst_data,
    ib_mpool_freeable_t *mp,
    ib_mm_t              mm_eval,
    const ib_list_t     *in,
    ib_list_t           *out,
    void                *cbdata
)
{
    filter_inst_t *inst = (filter_inst_t *)inst_data;
    ib_status_t    rc;
    const ib_list_node_t *node;

    assert(inst != NULL);
    assert(inst->tx != NULL);
    assert(inst->tx->request_body != NULL);
    assert(inst->corecfg != NULL);

    IB_LIST_LOOP_CONST(in, node) {
        const ib_filter_data_t *filter_data =
            (const ib_filter_data_t *)ib_list_node_data_const(node);

        /* Slice every data object into the out list. */
        rc = forward_data(mp, filter_data, out);
        if (rc != IB_OK) {
            return rc;
        }

        /* Buffer data into tx. */
        rc = apply_buffering_to_limit(
            inst->tx,
            mp,
            filter_data,
            inst->corecfg->limits.request_body_log_limit,
            inst->tx->request_body
        );
        if (rc != IB_OK) {
            return rc;
        }

    }

    return IB_OK;
}

static void req_filter_destroy_fn(
    void *inst_data,
    void *cbdata
)
{
}

static ib_status_t resp_filter_create_fn(
    void        *inst_data,
    ib_mm_t      mm,
    ib_filter_t *filter,
    void         *arg,
    void         *cbdata
)
{
    filter_inst_t       *inst;
    filter_create_arg_t *filter_arg = (filter_create_arg_t *)arg;

    inst = ib_mm_alloc(mm, sizeof(*inst));
    if (inst == NULL) {
        return IB_EALLOC;
    }
    inst->tx      = filter_arg->tx;
    inst->corecfg = filter_arg->corecfg;

    *(void **)inst_data = inst;

    return IB_OK;
}

static ib_status_t resp_filter_exec_fn(
    ib_filter_inst_t    *filter_inst,
    void                *inst_data,
    ib_mpool_freeable_t *mp,
    ib_mm_t              mm_eval,
    const ib_list_t     *in,
    ib_list_t           *out,
    void                *cbdata
)
{
    filter_inst_t *inst = (filter_inst_t *)inst_data;
    ib_status_t    rc;
    const ib_list_node_t *node;

    assert(inst != NULL);
    assert(inst->tx != NULL);
    assert(inst->tx->request_body != NULL);
    assert(inst->corecfg != NULL);

    IB_LIST_LOOP_CONST(in, node) {
        const ib_filter_data_t *filter_data =
            (const ib_filter_data_t *)ib_list_node_data_const(node);

        /* Slice every data object into the out list. */
        rc = forward_data(mp, filter_data, out);
        if (rc != IB_OK) {
            return rc;
        }

        /* Buffer data into tx. */
        rc = apply_buffering_to_limit(
            inst->tx,
            mp,
            filter_data,
            inst->corecfg->limits.response_body_log_limit,
            inst->tx->response_body
        );
        if (rc != IB_OK) {
            return rc;
        }

    }

    return IB_OK;
}

static void resp_filter_destroy_fn(
    void *inst_data,
    void *cbdata
)
{
}

ib_status_t ib_core_filter_init(
    ib_engine_t *ib,
    ib_mm_t      mm,
    ib_module_t *core_module
)
{
    assert(ib != NULL);
    assert(core_module != NULL);

    ib_status_t rc;
    ib_filter_t *request_body_raw;
    ib_filter_t *response_body_raw;

    rc = ib_filter_create(
        &request_body_raw,
        mm,
        CORE_FILTER_NAME,
        CORE_FILTER_TYPE,
        req_filter_create_fn, NULL,
        req_filter_exec_fn, NULL,
        req_filter_destroy_fn, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_filter_create(
        &response_body_raw,
        mm,
        CORE_FILTER_NAME,
        CORE_FILTER_TYPE,
        resp_filter_create_fn, NULL,
        resp_filter_exec_fn, NULL,
        resp_filter_destroy_fn, NULL
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_stream_pump_add(
        ib_engine_response_stream_pump(ib),
        response_body_raw
    );
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_stream_pump_add(
        ib_engine_request_stream_pump(ib),
        request_body_raw
    );
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}

ib_status_t ib_core_filter_tx_init(
    ib_tx_t       *tx,
    ib_core_cfg_t *corecfg
)
{
    ib_status_t rc;
    filter_create_arg_t arg = {
        .tx = tx,
        .corecfg = corecfg
    };

    rc = ib_stream_pump_inst_name_add(
        ib_tx_request_body_stream(tx),
        CORE_FILTER_NAME,
        &arg);
    if (rc != IB_OK) {
        return rc;
    }

    rc = ib_stream_pump_inst_name_add(
        ib_tx_response_body_stream(tx),
        CORE_FILTER_NAME,
        &arg);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}
