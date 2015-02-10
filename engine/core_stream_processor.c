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
 *
 */

#include "core_stream_processor_private.h"

#include <ironbee/stream_io.h>
#include <ironbee/stream_processor.h>
#include <ironbee/stream_pump.h>
#include <ironbee/log.h>
#include <ironbee/core.h>
#include <ironbee/engine.h>
#include <ironbee/mm_mpool_lite.h>

#include <assert.h>

static const char *CORE_PROCESSOR_NAME_REQ = "req_raw";
static const char *CORE_PROCESSOR_NAME_RESP = "resp_raw";
static const char *CORE_PROCESSOR_TYPE = "raw";

/**
 * The configuration data for a filter.
 */
struct inst_t {
    ib_core_cfg_t *corecfg; /**< The core configuration. */
    ib_stream_t   *stream;  /**< The stream to append data to. */
    size_t         limit;   /**< The limit of the tx to write to stream. */
    bool           is_request; /**< Is this request or response time? */
};
typedef struct inst_t inst_t;

/**
 * Common constructor code for a core filter intance.
 *
 * @param[out] inst_data Instance data. Set to
 *             a @ref inst_t.
 * @param[in] tx The transaction.
 * @param[in] is_request True if this is a request processor.
 *            False otherwise.
 * @returns
 * - IB_OK On success.
 * - IB_EALLOC On an allocation failure.
 * - Other on an unexpected API failure.
 */
static ib_status_t processor_create_common_fn(
    inst_t  **inst_data,
    ib_tx_t  *tx,
    bool      is_request
)
{
    assert(inst_data !=NULL);
    assert(tx != NULL);

    inst_t      *inst;
    ib_status_t  rc;
    ib_mm_t      mm = tx->mm;

    /* Create the processor instance data. */
    inst = ib_mm_alloc(mm, sizeof(*inst));
    if (inst == NULL) {
        return IB_EALLOC;
    }

    /* Record the current context. */
    rc = ib_core_context_config(tx->ctx, &inst->corecfg);
    if (rc != IB_OK) {
        return rc;
    }

    /* Here is the difference between a request and response processor. */
    if (is_request) {
        inst->stream = tx->request_body;
        inst->limit  = inst->corecfg->limits.request_body_log_limit;
    }
    else {
        inst->stream = tx->response_body;
        inst->limit  = inst->corecfg->limits.response_body_log_limit;
    }

    /* For traceability, record what type of processor we are, internally.
     * You can also strcmp() the name of the processor, but that's expensive. */
    inst->is_request = is_request;

    /* Hand back the configuration data. */
    *inst_data = inst;
    return IB_OK;
}

/**
 * Construct a request processor.
 *
 * @param[out] inst_data The instance data.
 * @param[in] tx The transaction.
 * @param[in] cbdata Unused.
 */
static ib_status_t processor_create_req_fn(
    void    *inst_data,
    ib_tx_t *tx,
    void    *cbdata
)
{
    assert(inst_data !=NULL);
    assert(tx != NULL);

    return processor_create_common_fn((inst_t **)inst_data, tx, true);
}

/**
 * Construct a response processor.
 *
 * @param[out] inst_data The instance data.
 * @param[in] tx The transaction.
 * @param[in] cbdata Unused.
 */
static ib_status_t processor_create_resp_fn(
    void    *inst_data,
    ib_tx_t *tx,
    void    *cbdata
)
{
    assert(inst_data !=NULL);
    assert(tx != NULL);

    return processor_create_common_fn((inst_t **)inst_data, tx, false);
}

/**
 * The logic of how to buffer @a filter_data into @a tx.
 *
 * This isolates the buffering logic to allow for easy
 * extending of this processor's functionality by other
 * static functions.
 *
 * @sa processor_exec_fn()
 *
 * @param[in] tx The transaction. For logging.
 * @param[in] io_tx IO Transaction. Used to reference memory.
 * @param[in] data The data segment. This is referenced if the
 *            data is required to be kept around.
 * @param[in] ptr The pointer to the data stored by @a data.
 * @param[in] ptr_len Length of the data at @a ptr.
 * @param[in] type The type of @a data. Must be IB_STREAM_IO_DATA or
 *            this does nothing.
 * @param[in] limit The limit of the data to capture.
 * @param[in] stream The stream object to buffer the data into.
 *            Data is aliased by stream's api, so we must increase
 *            the reference count of @a data if we bufffer.
 *
 * @returns
 * - IB_OK On success.
 * - Other on error.
 */
static ib_status_t apply_buffering_to_limit(
    ib_tx_t                    *tx,
    ib_stream_io_tx_t          *io_tx,
    ib_stream_io_data_t        *data,
    uint8_t                    *ptr,
    size_t                      ptr_len,
    ib_stream_io_type_t         type,
    const size_t                limit,
    ib_stream_t                *stream
)
{
    ib_status_t rc;

    /* If we are handed empty or non-data data (FLUSH data), return OK. */
    if (ptr == NULL || ptr_len == 0 || type != IB_STREAM_IO_DATA) {
        return IB_OK;
    }

    /* Already at the limit? */
    if (stream->slen >= limit) {
        /* Already at the limit. */
        ib_log_debug_tx(
            tx,
            "Request body log limit (%zd) reached: Ignoring %zd bytes.",
            limit,
            ptr_len);
        return IB_OK;
    }
    else {
        /* Check remaining space, adding only what will fit. */
        const size_t remaining = limit - stream->slen;

        /* "Say we want a copy of this data forever. */
        ib_stream_io_data_ref(io_tx, data);

        rc = ib_stream_push(
            stream,
            IB_STREAM_DATA,
            ptr,
            (remaining >= ptr_len)?ptr_len : remaining);
        if (rc != IB_OK) {
            ib_log_alert_tx(tx, "Failed to add stream data to tx buffer.");
            return rc;
        }
    }

    return IB_OK;
}

/**
 * The processor's implementation.
 *
 * @param[in] inst_data Instance data. A pointer to a @ref inst_t.
 * @param[in] tx The transaction.
 * @param[in] mm_eval Temporary memory pool for this evaluation of data only.
 * @param[in] in The data to process.
 * @param[in] out The resultant data. Unused as this always returns
 *            IB_DECLINED on success.
 * @param[in] cbdata Callback data. Unused.
 *
 * @returns
 * - IB_DECLINED On success (signalling the data to be passed down the stream).
 * - Other on error.
 * - IB_OK Is never returned.
 */
static ib_status_t processor_exec_fn(
    void                *inst_data,
    ib_tx_t             *tx,
    ib_mm_t              mm_eval,
    ib_stream_io_tx_t   *io_tx,
    void                *cbdata
)
{
    assert(inst_data != NULL);
    assert(tx != NULL);

    inst_t         *inst = (inst_t *)inst_data;
    ib_status_t     rc;

    /* Validate inst. */
    assert(inst != NULL);
    assert(inst->corecfg != NULL);
    assert(inst->stream != NULL);

    /* For all inputs... */
    while (ib_stream_io_data_depth(io_tx) > 0) {

        /* Unwrap the data segment. */
        ib_stream_io_data_t *data;

        uint8_t             *ptr;
        size_t               len;
        ib_stream_io_type_t  type;

        rc = ib_stream_io_data_take(io_tx, &data, &ptr, &len, &type);
        if (rc != IB_OK) {
            return rc;
        }

        /* Buffer data into tx. */
        rc = apply_buffering_to_limit(
            tx,
            io_tx,
            data,
            ptr,
            len,
            type,
            inst->limit,
            inst->stream
        );
        /* On error, pass the error back. */
        if (rc != IB_OK) {
            return rc;
        }

        /* Forward the data to the output. */
        rc = ib_stream_io_data_put(io_tx, data);
        if (rc != IB_OK) {
            return rc;
        }
    }

    /* Signal that we don't change the stream and @a out is not to be used. */
    return IB_OK;
}

ib_status_t ib_core_stream_processor_init(
    ib_engine_t *ib,
    ib_module_t *core_module
)
{
    assert(ib != NULL);
    assert(core_module != NULL);

    ib_mpool_lite_t *mpl;
    ib_mm_t          mml;
    ib_status_t      rc;
    ib_list_t       *core_types;

    rc = ib_mpool_lite_create(&mpl);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to create temporary mpool.");
        return rc;
    }
    mml = ib_mm_mpool_lite(mpl);

    rc = ib_list_create(&core_types, mml);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to create filter type list.");
        goto cleanup;
    }

    rc = ib_list_push(core_types, (void *)CORE_PROCESSOR_TYPE);
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to append to filter type list.");
        goto cleanup;
    }

    rc = ib_stream_processor_registry_register(
        ib_engine_stream_processor_registry(ib),
        CORE_PROCESSOR_NAME_REQ,
        core_types,
        processor_create_req_fn, NULL,
        processor_exec_fn, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to register core module's request processor.");
        goto cleanup;
    }

    rc = ib_stream_processor_registry_register(
        ib_engine_stream_processor_registry(ib),
        CORE_PROCESSOR_NAME_RESP,
        core_types,
        processor_create_resp_fn, NULL,
        processor_exec_fn, NULL,
        NULL, NULL
    );
    if (rc != IB_OK) {
        ib_log_alert(ib, "Failed to register core module's response processor.");
        goto cleanup;
    }

cleanup:
    ib_mpool_lite_destroy(mpl);
    return rc;
}

ib_status_t ib_core_stream_processor_tx_init(
    ib_tx_t       *tx,
    ib_core_cfg_t *corecfg
)
{
    assert(tx != NULL);
    assert(corecfg != NULL);

    ib_status_t rc;

    /* Create the response processor and place it at index 0. */
    rc = ib_stream_pump_processor_insert(
        ib_tx_response_body_pump(tx),
        CORE_PROCESSOR_NAME_RESP,
        0);
    if (rc != IB_OK) {
        return rc;
    }

    /* Create the request processor and place it at index 0. */
    rc = ib_stream_pump_processor_insert(
        ib_tx_request_body_pump(tx),
        CORE_PROCESSOR_NAME_REQ,
        0);
    if (rc != IB_OK) {
        return rc;
    }

    return IB_OK;
}
